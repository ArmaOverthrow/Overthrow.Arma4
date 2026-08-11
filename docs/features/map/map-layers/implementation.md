# Map Layers & Legend — Implementation Plan

**Status:** Ready for Review (code-complete, unobserved) · **Started:** 2026-08-11 · **Target Completion:** TBD · **Last Updated:** 2026-08-11

**Epic:** `map` (feature **7 of 8** — stretch goal, sequenced after `territory-overlay`)
**Requirements:** `docs/features/map/map-layers/requirements.md`

> 🔴 **READ THIS BEFORE `requirements.md`.** Three of that document's premises went stale between it being
> written (2026-08-10) and this plan (2026-08-11), and one of its requirements has been overruled by a user
> decision. All four corrections are recorded as **K1 – K6** in §6. Where the two documents disagree,
> **this plan is the design.** The corrections are: `ModuleGameSettings` now has an in-tree precedent
> (K1); a display-name field already exists and a _third_ field is what is needed (K2); Overthrow has no
> live tool-menu registration and this feature is the first (K3); and the legend's "colour meaning" clause
> is deliberately **not** shipped (K4).

---

## 1. Executive Summary

The map is now crowded. Fourteen configured location types draw markers, a weighted-Voronoi territory
overlay shades the whole landmass, restricted-area rings ring every base and radio tower, and player
markers sit on top of all of it. Everything the previous six features added is drawn at once, and there is
no way for a player to ask the map a narrower question than "show me everything".

This feature adds **one docked panel with one list of rows**. Each row is simultaneously the legend entry
(icon + localized name) and the control (an on/off checkbox). Rows come from three sources and nothing is
hardcoded:

1. the **14 configured location types** in `Configs/Map/OverthrowMap.conf`,
2. every **canvas layer** registered with `OVT_MapCanvasCompositor` (`territory`, `restricted` today),
3. one **hand-built row** for `OVT_MapPlayerLocation`, which is neither of the above (K5).

The panel is entered through **vanilla's own map tool menu** — the icon strip already live on the left of
Overthrow's fullscreen map — so it costs **no new keybinding**, and gamepad operability comes from the
already-bound `MapToolMenuFocus` (D-pad Left) plus the engine's own focus navigation. That single decision
closes the riskiest part of any new Overthrow map UI: `MapContext` carries 41 live actions, `KC_H` is taken
three times over, and the repo's input-conflict checker cannot see inline `ActionContext` actions.

Preferences persist per **profile** through vanilla `ModuleGameSettings`, reusing the accessor pattern the
`new-player-experience` feature shipped for tutorial state — no EPF, no `.conf` registration, no new file
I/O, console-safe.

**Default: everything on.** A player who never opens the panel sees exactly the map they see today.

**What this feature is not.** It adds no replicated state, no RPC, no persistence of campaign data and no
write to any campaign record. A toggle is a **client-side presentation preference** and is deliberately
named as such throughout — the future intel epic's "what the campaign chooses to reveal" is a different
concept and must never share a field with this one.

---

## 2. Goals

### Primary

- **G1 — One row per thing that draws.** Icon, localized plural name, on/off. Adding a location type to
  `OverthrowMap.conf`, or a canvas layer to `MapOverthrow.conf`, adds a row with **zero code change**.
- **G2 — Toggling is cheap and immediate.** No element is destroyed, no layout is rebuilt, no map UI is torn
  down. Hiding a type must make the visibility sweep _cheaper_, not more expensive.
- **G3 — Fully operable on gamepad.** D-pad Left to focus the tool menu, D-pad Up/Down to reach the entry,
  A to open, D-pad Up/Down to walk rows, A to toggle, D-pad Left to leave. No new binding anywhere.
- **G4 — Preferences survive.** Across map open/close, across a session, and across a restart — per profile.
- **G5 — Non-occluding.** Docked into vanilla's `ToolFramesOverlay`, beside the tool strip, dismissible from
  the same entry that opened it.

### Secondary

- **G6 — Player-facing labels only.** A new, plural, localized `m_sCategoryName` on `OVT_MapLocationType`
  (K2). No raw editor string reaches a player.
- **G7 — Zero art dependency.** The tool-menu glyph is an existing vanilla quad, exposed as an attribute so
  it can be swapped from config later (K6). The row toggle is vanilla's own `WLib_Checkbox`, which is
  already localized in every shipped language. This epic has been blocked on art before; it will not be
  again.
- **G8 — A generic row source.** `shared-markers` (feature 8) will want to register a third category.
  Do not build for it — but do not build a two-category structure that has to be rewritten either.

### Explicit non-goals (hard, from `requirements.md`)

- Search by name. Per-marker (individual) hiding. Presets beyond "everything on". Restyling the map.
- Server or admin control of what a player may see.
- Changing any `m_fVisibilityZoom` / `m_fShowNameZoom` value.
- A read-only colour key (**overruled — see K4**).
- Reviving `OVT_MapThreatGrid`. It ships `m_bDisableModule 1` and stays that way; see §3.4.

---

## 3. Architecture Overview

### 3.1 Component hierarchy

```
SCR_MapConfig  (Configs/Map/MapOverthrow.conf — same-GUID DELTA over vanilla's)
│
├── m_aModules                              [vanilla + Overthrow, MERGED]
│   ├── SCR_MapCursorModule                 (vanilla — proves the merge, see §3.5)
│   ├── OVT_MapTerritoryLayer               m_sLayerId "territory"   ─┐
│   ├── OVT_MapRestrictedAreas              m_sLayerId "restricted"  ─┤ register with
│   └── OVT_MapThreatGrid   [DISABLED]      no id, never activates   ─┘ OVT_MapCanvasCompositor
│                                                                        on OnMapOpen
└── m_aUIComponents                         [vanilla + Overthrow, MERGED]
    ├── SCR_MapToolMenuUI                   (vanilla) ── the entry point
    ├── OVT_MapPlayerLocation               (existing) + m_bMarkersVisible   ← NEW (K5)
    ├── OVT_OverthrowMapUI                  (existing) + RefreshAllVisibility ← NEW
    └── OVT_MapLayersUI                     ← NEW: the panel component
          │
          ├─ Init()        register the tool-menu entry  (ONCE — see K7)
          ├─ OnMapOpen()   resolve ToolFramesOverlay, apply preferences
          ├─ Update()      one-shot re-apply on the first tick (ordering safety net, K8)
          ├─ OnMapClose()  flush preferences, drop widget refs
          └─ owns  m_wPanel ──> N × OVT_MapLayerRowComponent
                                   (one per type / layer / the player row)

Preference storage — three files, split exactly as the tutorial store is (K1)
  OVT_MapLayerPrefsStore       pure set logic, no engine call        → Logic tier pins it
  OVT_MapLayerSettings         ModuleGameSettings subclass + entry struct
  OVT_MapLayerSettingsAccessor GetModule / WriteToInstance / ReadFromInstance / flush
```

### 3.2 Data flow, one map session

```
map opens
   │
   ├─ modules SetActive(true) ──► canvas layers subscribe OnMapOpen  ─┐  (modules subscribe
   ├─ components SetActive(true) ─► OVT_MapLayersUI subscribes       ─┘   BEFORE components)
   │
   ├─ s_OnMapOpen.Invoke()
   │     ├─ layers register with OVT_MapCanvasCompositor
   │     ├─ OVT_OverthrowMapUI populates + creates elements
   │     └─ OVT_MapLayersUI.OnMapOpen
   │           ├─ Accessor.Load(store)                    (once per session, cached)
   │           └─ ApplyPreferences()                      ← types + layers + player markers
   │
   ├─ first Update() tick ─► ApplyPreferences() again, idempotent   (K8 safety net)
   │
   ├─ player presses the tool-menu entry
   │     └─ BuildPanel() ─► BuildRows() from the THREE sources, live state read from the
   │                        objects themselves — never from the store
   │
   ├─ player toggles a row
   │     ├─ store.SetHidden(key, hidden)                  (memory only — no flush)
   │     └─ ApplyOne()  ──► type.SetPlayerVisible(b) + mapUI.RefreshAllVisibility()
   │                    ──► layer.SetLayerVisible(b)
   │                    ──► playerLoc.SetMarkersVisible(b)
   │
   ├─ panel closes  ──► Accessor.Save(store)              (flush point 1)
   └─ map closes    ──► Accessor.Save(store)              (flush point 2)
```

### 3.3 The visibility gate

`OVT_MapLocationElement.SetVisible` already ANDs three gates — the caller's `visible`, a zoom gate through
`GetEffectiveVisibilityZoom()`, and `m_LocationType.ShouldShowLocation(...)`. The player toggle becomes a
**fourth gate, inserted first**, immediately after the existing `!m_LocationType` guard:

```
if (!m_LocationType.IsPlayerVisible())
{
    super.SetVisible(false);
    return;
}
```

Placing it first is not cosmetic. `SetVisible` is a hot path (it runs for every element on every zoom
change, alongside `ShouldUseSmallIcon`), and an early return **skips** the zoom lookup and the
`ShouldShowLocation` manager reads entirely — so a hidden type costs _less_ than a shown one. One virtual
call and one boolean compare is the whole cost when nothing is hidden.

🔴 **The gate does NOT go in `ShouldShowLocation`.** That is a per-record virtual with live manager lookups
and it is the wrong place for a per-type constant. And no element is ever destroyed or recreated by a
toggle — that would violate G2 and would walk straight into BUG-136's reconciliation hazards.

The re-apply sweep copies `OnMapZoom` verbatim in shape: iterate `m_mIcons`, cast to
`OVT_MapLocationElement`, call a new one-line `RefreshVisibility()` (which is `SetVisible(m_bVisible)`).
`OnZoomChanged()` is refactored to call the same method so there is one implementation, not two.

### 3.4 Row sources, and what does _not_ produce a row

| Source         | Enumerated by                                                                           | Key                       | Label                    | Toggle                                    |
| -------------- | --------------------------------------------------------------------------------------- | ------------------------- | ------------------------ | ----------------------------------------- |
| Location types | `OVT_OverthrowMapUI.GetLocationTypes()` (new accessor over `m_Config.m_aLocationTypes`) | `"type:" + ClassName()`   | `GetCategoryName()`      | `SetPlayerVisible` / `IsPlayerVisible`    |
| Canvas layers  | `OVT_MapCanvasCompositor.GetInstance().GetLayers()` — **read it, never mutate it**      | `"layer:" + GetLayerId()` | `GetDisplayName()`       | `SetLayerVisible` / `IsLayerVisible`      |
| Player markers | the one hand-built row (K5)                                                             | `"layer:players"`         | `#OVT-Map_Layer_Players` | `SetMarkersVisible` / `AreMarkersVisible` |

**Keys are namespace-prefixed** so a layer id can never collide with a class name. This costs nothing and
removes a whole category of question.

**Nothing produces a blank row.** Four filters, each with a one-time WARNING naming what was skipped:

1. **A disabled module never appears at all, structurally.** `SCR_MapEntity.ActivateModules` skips any
   module whose `IsConfigDisabled()` is true _before_ inserting it into `m_aActiveModules` or calling
   `SetActive(true)` — and `SetActive(true)` is what subscribes `OnMapOpen`, which is what registers with
   the compositor. So `OVT_MapThreatGrid` (`m_bDisableModule 1`) can never be in `GetLayers()` and needs
   no special case. This is worth knowing rather than discovering.
2. **A layer with an empty `m_sLayerId` is skipped** — it cannot be addressed, so it cannot be persisted.
3. **A layer with an empty `m_sDisplayName` is skipped** — it cannot be labelled.
4. **A duplicate key is skipped** — two config entries of the same location-type class would otherwise
   share one row and one preference. There are none today (all 14 classes are distinct); the guard exists
   so that stops being true loudly rather than silently.

**Label fallback.** An `m_sCategoryName` left empty falls back to `GetDisplayName()`, then to `ClassName()`,
with a one-time WARNING. The row **always appears** — G1 says adding a config entry adds a toggle, and a
missing translation is a content gap, not a structural one. The fallback is unreachable for all 14 shipped
types because Phase 3 sets `m_sCategoryName` on every one of them.

### 3.5 Where the boundary is

The map is a **read-only projection of replicated campaign state** and this feature does not change that.
Phase 6 re-runs `territory-overlay`'s I-4 boundary greps as an explicit verification step:

- no `[RplProp]`, no `[RplRpc]`, no `Rpc(`, no EPF, in any new or changed file;
- nothing added to `OVT_PlayerCommsComponent` (legacy/deprecated);
- no write to any campaign record — the only mutable state this feature introduces is a set of strings in
  a profile settings block and three runtime booleans on client-side UI objects;
- every new file under `Scripts/Game/UI/Map/`, `Scripts/Game/Data/`, `Scripts/Game/Global/` or
  `Scripts/Game/Tests/`.

**Two clients must be able to hold opposite preferences with no effect on each other, and no effect on
what the server sends.** That is a structural property here — nothing crosses a wire — but it is the MP
gate's job to confirm it, because "structurally impossible" has been wrong in this epic before.

**Corroborating evidence that the `MapOverthrow.conf` delta really merges:** `SCR_MapCursorModule` is
listed **only** in vanilla's `m_aModules`, and it is the sole caller of `SCR_MapEntity.InvokeOnSelect`,
which raises the `GetOnSelection()` invoker that `OVT_OverthrowMapUI.OnMapSelection` subscribes to. Clicking
a marker to pin its info panel was play-tested green on 2026-08-10 (BUG-137). Therefore vanilla's module
list demonstrably merges into Overthrow's delta at runtime. That is a proxy for `m_aUIComponents`, not a
proof of it — which is why Phase 1's first task is to _look at the map_ (§5).

### 3.6 Layout ↔ code name contract

Every name introduced by this feature, in the format `map/core/context.md` uses. **This table is the
deliverable, not documentation of one** — `FindAnyWidget` returning null is a silent no-op that
`compile-check.sh` cannot see, and this epic lost two configured features to exactly that (BUG-133
`IconLayout`, BUG-134 `CloseButton`). Every lookup below must be null-guarded and must log an ERROR naming
the widget it could not find.

| Name                | Layout                                                                                           | Read by                                                                                                                                  |
| ------------------- | ------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `ToolFramesOverlay` | vanilla `UI/layouts/Map/MapMenu.layout` (**read-only — Overthrow does not edit it**, see K9)     | `OVT_MapLayersUI.ResolveDockParent` — `m_RootWidget.FindAnyWidget(...)`; the panel is created **into** it                                |
| `ToolMenuContainer` | vanilla `UI/layouts/Map/MapMenu.layout`                                                          | `OVT_MapLayersUI.ResolveDockParent` — first fallback when `ToolFramesOverlay` is absent (`FastTravelMapMenu.layout` has no such overlay) |
| `LayersPanel`       | `UI/Layouts/Map/Core/OVT_MapLayersPanel.layout` (the root widget's own `Name`)                   | held as `m_wPanel`; not looked up                                                                                                        |
| `PanelTitle`        | `OVT_MapLayersPanel.layout`                                                                      | `OVT_MapLayersUI.BuildPanel` — `SetText("#OVT-Map_Layers_Title")`                                                                        |
| `OverlaysHeader`    | `OVT_MapLayersPanel.layout`                                                                      | `OVT_MapLayersUI.BuildPanel` — `SetText`                                                                                                 |
| `MarkersHeader`     | `OVT_MapLayersPanel.layout`                                                                      | `OVT_MapLayersUI.BuildPanel` — `SetText`                                                                                                 |
| `OverlayRows`       | `OVT_MapLayersPanel.layout`                                                                      | `OVT_MapLayersUI.BuildRows` — parent for overlay + player rows                                                                           |
| `TypeRows`          | `OVT_MapLayersPanel.layout`                                                                      | `OVT_MapLayersUI.BuildRows` — parent for location-type rows                                                                              |
| `FocusProxy`        | `OVT_MapLayersPanel.layout`                                                                      | `OVT_MapLayersUI.OnPanelBuilt` — `FindHandler(SCR_EventHandlerComponent)` then `GetOnFocus().Insert(FocusFirstRow)` (K10)                |
| `RowIcon`           | `UI/Layouts/Map/Core/OVT_MapLayerRow.layout`                                                     | `OVT_MapLayerRowComponent.Init` — `LoadImageFromSet`; **hidden when the row has no imageset**                                            |
| `RowLabel`          | `OVT_MapLayerRow.layout`                                                                         | `OVT_MapLayerRowComponent.Init` — `SetText`                                                                                              |
| `RowCheckbox`       | `OVT_MapLayerRow.layout` (inherits `{?}UI/layouts/WidgetLibrary/ToolBoxes/WLib_Checkbox.layout`) | `OVT_MapLayerRowComponent.Init` — `SCR_CheckboxComponent.GetCheckboxComponent("RowCheckbox", m_wRoot)`, then `m_OnChanged.Insert(...)`   |

⚠️ **`RowCheckbox` inherits a vanilla layout, so its `SCR_CheckboxComponent` override MUST reuse the base
layout's component GUID `{546A9B7B0A8AD927}`.** A fresh GUID adds a second, unconfigured component and the
checkbox goes dead — the single most common layout bug in this codebase.

### 3.7 Contract-table rows to add to `map/core/context.md`

Per the epic's standing rule, the two contract tables are never changed silently. Phase 6 adds these rows.

**To `The OVT_MapLocationType Contract`:**

| Member                                                                  | Kind                           | Purpose                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| ----------------------------------------------------------------------- | ------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **`m_sCategoryName`**                                                   | attribute                      | **Added by `map/map-layers`.** The **plural, localized, player-facing** category name shown on the map layer-filter row. Deliberately a **third** field: `m_sName` stays the Workbench editor-tree label (`OVT_MapLocationTypeTitle._WB_GetCustomTitle`) and `m_sDisplayName` stays the **singular** type line on the info panel. Both are left doing exactly their existing jobs. Empty ⇒ falls back to `GetDisplayName()`, then `ClassName()`, with a one-time WARNING.                                                                        |
| **`GetCategoryName()`**                                                 | getter                         | **Added by `map/map-layers`.** Read only by `OVT_MapLayersUI` when building rows. Not a hot path.                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| **`m_bPlayerVisible` / `SetPlayerVisible(bool)` / `IsPlayerVisible()`** | runtime member + setter/getter | **Added by `map/map-layers`.** A **client-side presentation preference**, deliberately _not_ an attribute — it is never authored in config and is always applied from the persisted profile store at map open. Read as the **first** gate in `OVT_MapLocationElement.SetVisible`, which early-returns, so a hidden type skips the zoom lookup and `ShouldShowLocation` entirely. 🔴 **This is not campaign visibility.** What the campaign chooses to _reveal_ belongs to the future intel epic and must never share this field. Default `true`. |

**To `The OVT_MapCanvasLayer Contract`:** **no rows.** `territory-overlay` Phase 1 designed the layer half
of this feature's contract correctly and completely — `m_sLayerId`, `m_sDisplayName`, `SetLayerVisible` /
`IsLayerVisible` and `GetLayers()` are consumed exactly as specified with no extension. Record that as a
one-line note under the table; a contract that needed nothing added when its first consumer arrived is
worth saying out loud.

**One documented exception, to be recorded as a note beneath the canvas-layer table:**
`OVT_MapPlayerLocation` is a `SCR_MapUIBaseComponent`, not an `OVT_MapCanvasLayer`, so it does not appear
in `GetLayers()` and its row is **hand-built** (K5). It gains `m_bMarkersVisible` / `SetMarkersVisible(bool)`
/ `AreMarkersVisible()` and `IsAvailableThisSession()`. It is the only non-generic row in the panel, and
the reason is that reparenting a working, retained component to satisfy a list builder is a worse trade
than one special case with a comment on it.

---

## 4. Settled Planning Decisions

These were settled before this plan and are **not open**. They are restated so the plan is self-contained.

- **D1 — Entry point is a vanilla tool-menu entry, not a new keybinding.** `SCR_MapToolMenuUI` is listed in
  vanilla's `Configs/Map/MapOverthrow.conf:53`, and Overthrow's file is a same-GUID delta over it.
  `SCR_MapJournalUI.Init()` (`:47-54`) is the exact registration idiom to copy.
- **D2 — One list; each row is both the legend entry and the toggle.** No separate read-only key.
- **D3 — Default preset: everything on.** Purely additive. Existing per-type `m_fVisibilityZoom` thresholds
  already keep houses, shops and bus stops off the zoomed-out view.
- **D4 — Overlay rows are generically registered**, one per `GetLayers()` entry.
- **D5 — 🔴 The toggle primitive is `SetLayerVisible`, never `SetActive`.**
  `SCR_MapModuleBase.SetActive(false)` calls `m_MapEntity.DeactivateModule(this)`, and both
  `ActivateModules` and `m_aActiveModules` are `protected` on `SCR_MapEntity`. **`SetActive(false)` is
  one-way from script** — a toggle built on it turns a layer off permanently for the session.
- **D6 — Persistence is vanilla `ModuleGameSettings`.** Per profile, console-safe, no `.conf` registration,
  no new file I/O. **Accepted consequence: preferences are per profile, not per campaign** — they follow
  the player across saves and servers. That is the right granularity for a presentation preference.

---

## 5. Implementation Phases

Effort is **S / M / L** relative to a single focused session. "Agent" is the routing hint for `/proceed`.
**Phases 3 and 4 need the advanced variants** — Phase 3 because it edits the base class every location type
inherits, on a hot path, and extends a published contract; Phase 4 because it is the whole UI surface,
gamepad-critical, and carries the entire `FindAnyWidget` failure class.

---

### Phase 0 — Baseline — **S — no agent (already measured)**

Measured on `new-map` at `ecf1a696`, working tree clean, **2026-08-11**. These numbers were **run, not
quoted.**

| Gate                                             | Baseline                                                                     |
| ------------------------------------------------ | ---------------------------------------------------------------------------- |
| `tools/compile-check.sh`                         | **exit 0, 5988 files, Game module, 5 s**                                     |
| `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) | **OK, 87 tests, 34 s**                                                       |
| `tools/run-tests.sh "{6A6E2A002F53A581}"` (All)  | **OK, 125 tests, 40 s**                                                      |
| Highest allocated bug id                         | **BUG-145**                                                                  |
| Free GUID series                                 | **`{6A85…}`** — zero uses anywhere in the tree (`{6A86}`/`{6A87}` also free) |

⚠️ **`CLAUDE.md` says Fast 38 / All 66. `territory-overlay` says Fast 66 / All 101. Both are stale — do not
quote either.** Parallel sessions commit to this tree. Re-check `git status`, the highest `docs/bugs/` id
and all three gate numbers at **every** phase boundary. A changed count is a **finding to investigate**,
never a number to update.

Expected end state: compile **5988 + 6 = 5994 files**, Fast **89**, All **127** (+2 Logic cases, Phase 2).

---

### Phase 1 — 🔴 Entry-point and gamepad spike — **S — `ui-developer`, then user-run**

> **A deliberately throwaway-shaped spike, run before anything depends on it.** Three things this feature's
> whole shape rests on cannot be established by reading: whether `SCR_MapToolMenuUI` is actually live on
> Overthrow's fullscreen map, whether a panel created into `ToolFramesOverlay` renders where it should, and
> whether a gamepad can reach and operate a row inside it. `territory-overlay` Phase 2 spent one session on
> a render probe and retired its largest risk outright; this is the same move.

**Tasks**

1. **P1 (zero code).** Open the fullscreen map in a started campaign and look for vanilla's tool-menu icon
   strip on the left (ruler, watch, compass, journal, tasks…). Record what is there.
   - **If absent:** add `SCR_MapToolMenuUI "{599C7D68E8F6B9A8}" { }` to `m_aUIComponents` in Overthrow's
     `Configs/Map/MapOverthrow.conf`. Because the GUID matches vanilla's entry this is a **delta on that
     entry, not a duplicate** — safe whether the array merges or replaces. Re-run P1.
2. Create `Scripts/Game/UI/Map/OVT_MapLayersUI.c` as a stub `SCR_MapUIBaseComponent` that:
   - in `Init()` resolves `SCR_MapToolMenuUI` via `m_MapEntity.GetMapUIComponent(SCR_MapToolMenuUI)` and
     registers **once** (K7) with `RegisterToolMenuEntry(SCR_MapToolMenuUI.s_sToolMenuIcons,
m_sToolMenuIcon, m_iSortPriority, m_bIsExclusive)`, then `m_OnClick.Insert(TogglePanel)`,
     `GetOnDisableMapUIInvoker().Insert(ClosePanel)`, `SetEnabled(true)`;
   - `[Attribute] string m_sToolMenuIcon` defaulting to **`"filters"`** (K6),
     `[Attribute] int m_iSortPriority` defaulting to **`3`** (K11);
   - in `OnMapOpen` resolves the dock parent (`ToolFramesOverlay`, then `ToolMenuContainer`, then
     `m_RootWidget`, ERROR-logging which one it settled on) and creates a **stub panel layout** with a
     title and **two hardcoded checkbox rows** that print on toggle.
3. Register the component in `Configs/Map/MapOverthrow.conf` `m_aUIComponents` with
   `m_bIsExclusive 1` (K12).
4. Author `UI/Layouts/Map/Core/OVT_MapLayersPanel.layout` + `.meta` and
   `UI/Layouts/Map/Core/OVT_MapLayerRow.layout` + `.meta` in their **final widget-name shape** (§3.6) but
   with placeholder content. GUIDs from the `{6A85…}` series. All six platform configurations in each
   `.meta`.

**Acceptance — the four spike questions, each answered by observation and recorded verbatim in `context.md`**

- **P1 — Does the tool menu exist on Overthrow's map?** Yes / no, and which fallback (if any) was needed.
- **P2 — Does the entry appear, in the right place, with a legible glyph?** Confirm sort position relative
  to the journal/task entries and that `"filters"` reads as a filter control at 64 px.
- **P3 — Does the panel dock where it should, and does it occlude the map?** It must sit beside the icon
  strip, not over the middle of the map.
- **P4 — 🔴 The gamepad round trip.** D-pad Left → tool menu focused → D-pad Up/Down → entry → **A** →
  panel opens **with focus already on the first row** → D-pad Up/Down walks rows → **A** toggles → D-pad
  Left returns to the tool menu → **A** closes the panel. Record every step that fails.
  - Also record whether the **left stick still pans the map** while the panel is open. It is expected to
    (`MenuUp`/`MenuDown` are bound to both `pad_up`/`pad_down` **and** `left_thumb_vertical`, while
    `MapPanVGamepad` owns the stick). D-pad navigation and stick panning is a clean division and is
    probably the right behaviour; if the stick also moves row focus, say so — that is the one finding that
    would force a redesign, and the fallback is to hold the cursor module's sub-menu state while the panel
    is open.

**Gates:** compile **exit 0**, file count **5988 + 1**. Fast **87**, All **125** — unchanged; nothing here
is assertable. ⚠️ `.layout` / `.conf` / `.meta` edits are invisible to every gate. **The play-test is the
only evidence this phase produces**, and it is the phase's entire point.

---

### Phase 2 — The preference store — **S — `component-developer`**

> The one part of this feature that is genuinely testable, and it is split so that it can be. Mirror
> `OVT_TutorialSeenStore` / `OVT_TutorialSettings` / `OVT_TutorialSettingsAccessor` exactly (K1) — same
> three-file split, same rules, same guards.

**Tasks**

1. `Scripts/Game/Data/OVT_MapLayerPrefsStore.c` — **pure, no engine call, no BaseContainer, no widget.**
   - Holds a `ref set<string>` of **hidden** keys. **Absent ⇒ visible** (K13).
   - `bool IsVisible(string key)`, `void SetHidden(string key, bool hidden)`,
     `void LoadFrom(array<string> keys, int version)`, `void WriteTo(out array<string> keys)`,
     `int Count()`, `void Clear()`.
   - `static string TypeKey(string className)` → `"type:" + className`;
     `static string LayerKey(string layerId)` → `"layer:" + layerId`.
   - `CURRENT_VERSION = 1`; a version mismatch **clears** rather than half-trusting.
   - `MAX_HIDDEN` cap with a warn-once, refusing rather than evicting.
   - Empty keys refused. `LoadFrom(null, …)` treated as empty.
2. `Scripts/Game/Global/OVT_MapLayerSettings.c`:
   - `[BaseContainerProps()] class OVT_MapHiddenLayerEntry { [Attribute()] string m_sKey; }` — **the type
     name carries the meaning**; presence _is_ "hidden" (K13).
   - `class OVT_MapLayerSettings : ModuleGameSettings` with `[Attribute("1")] int m_iVersion` and
     `[Attribute()] ref array<ref OVT_MapHiddenLayerEntry> m_aHidden`.
   - 🔴 **Nested `[BaseContainerProps()]` struct in an object array, never a top-level `ref array<string>`**
     — the latter is unproven in a settings module; the former round-trips on every server-browser filter
     save. And **never parallel arrays** (K1b).
3. `Scripts/Game/Global/OVT_MapLayerSettingsAccessor.c`:
   - `static bool Load(notnull OVT_MapLayerPrefsStore store)` /
     `static bool Save(notnull OVT_MapLayerPrefsStore store)` / `static bool Reset()`.
   - `System.IsConsoleApp()` early-out; null-guard `GetGameUserSettings()` **and** `GetModule()`; neither
     is worth a log line.
   - **Read-modify-write against the live container**, so a member a future version adds is preserved
     rather than blanked. Allocate `m_aHidden` when the loader hands back null (it legitimately does).
   - Write the **whole record** every time. Version-mismatch on load rewrites the block immediately.
   - ⚠️ Mind the naming direction: `WriteToInstance` is the **load**, `ReadFromInstance` is the **store**.
4. `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_MapLayerPrefs.c` — **two cases**, each proven able to
   fail with the inversion recorded (§9).

**Acceptance**

- compile **exit 0**, file count **5993** (5989 + 4: the store, the settings module, the accessor, the test
  file).
- Fast **89**, All **127** — the +2 are the new Logic cases and are the **only** expected change.
- Both inversions recorded in `context.md`.

---

### Phase 3 — Toggle primitives and labels — **M — `component-developer-advanced`**

> **Advanced.** It edits `OVT_MapLocationType` (the base class all 14 types inherit and a published
> contract), `OVT_MapLocationElement.SetVisible` (a hot path), `OVT_OverthrowMapUI`, and
> `OVT_MapPlayerLocation` — four integration points, one of them performance-sensitive and one of them a
> retained legacy component marked do-not-touch by `legacy-retirement`.

**Tasks**

1. `OVT_MapLocationType`:
   - `[Attribute(defvalue: "")] protected string m_sCategoryName` + `string GetCategoryName()` with the
     documented fallback chain and warn-once.
   - `protected bool m_bPlayerVisible = true;` + `SetPlayerVisible(bool)` / `IsPlayerVisible()`.
     ⚠️ **Do not touch `m_sName` or `m_sDisplayName`** (K2). ⚠️ **Do not reset `m_bPlayerVisible` in
     `Init()`** — `Init()` runs on **every** map open, and the store is re-applied at open anyway.
2. `OVT_MapLocationElement`:
   - The fourth gate at the top of `SetVisible` (§3.3).
   - `void RefreshVisibility()` → `SetVisible(m_bVisible)`; `OnZoomChanged()` and `OnLocationDataChanged()`
     both call it instead of duplicating the line.
3. `OVT_OverthrowMapUI`:
   - `array<ref OVT_MapLocationType> GetLocationTypes()` (null-safe accessor over `m_Config`).
   - `void RefreshAllVisibility()` — the `m_mIcons` sweep, shaped exactly like `OnMapZoom`.
   - Generalise `IsSelectionOnInfoPanel` into `IsSelectionInsideWidget(Widget, vector)`, keep
     `IsSelectionOnInfoPanel` as the thin wrapper, and have `OnMapSelection` **also** test the layers
     panel's widget so a click on a filter row does not unpin the info panel (K14).
4. `OVT_MapPlayerLocation` (K5):
   - `protected bool m_bMarkersVisible = true;` + `SetMarkersVisible(bool)` / `AreMarkersVisible()`.
     `SetMarkersVisible` loops `m_Widgets` and calls `SetVisible`; `Update()` early-returns when hidden so
     the two cannot fight.
   - `protected bool m_bAvailableThisSession` + `bool IsAvailableThisSession()`, set **true** only past
     both of `OnMapOpen`'s early returns (no controlled entity; `m_Difficulty.showPlayerOnMap` false), and
     reset **false** at the top of `OnMapOpen`. A difficulty-disabled marker must not present a dead
     toggle.
   - ⚠️ **Leave the class otherwise as-is.** Do not delete the vestigial `m_ToolMenuEntry` /
     `ZoomInOnPlayer` here — that is a separate finding for the user (K3).
5. `Language/localization_Overthrow.st` — **18 new `CustomStringTableItem` blocks**, GUIDs from `{6A85…}`,
   `Target_en_us` only, `Comment` filled in for the translator:
   - `OVT-Map_Layers_Title` "Map Layers", `OVT-Map_Layers_Overlays` "Overlays",
     `OVT-Map_Layers_Markers` "Markers", `OVT-Map_Layer_Players` "Players";
   - 14 × `OVT-Map_Category_*` — **English text taken verbatim from each type's existing `m_sName`**, which
     is already the correct plural in all 14 cases: Towns, Bases, Radio Towers, FOBs, Ports, Camps, Houses,
     Shops, Gun Dealers, Warehouses, Bus Stops, Vehicles, Points of Interest, Waypoints.
   - ❌ **NEVER touch `Language/localization_Overthrow.<lang>.conf`.** Those are Workbench-generated
     exports; the user regenerates them. Their `Ids{}`/`Texts{}` blocks are neither parallel nor
     same-length and hand-editing has corrupted all six before.
6. `Configs/Map/OverthrowMap.conf` — add `m_sCategoryName "#OVT-Map_Category_*"` to all 14 entries.
   ⚠️ **`.conf` files carry no comments** — no `.conf` in this repo or the whole vanilla tree has one.

**Acceptance**

- compile **exit 0**, file count **5993** — unchanged (no new `.c`; `.st` and `.conf` are invisible to it).
- Fast **89**, All **127** — unchanged from Phase 2.
- **By inspection:** with no panel yet, the map is byte-for-byte unchanged (every `m_bPlayerVisible`
  defaults true, `IsAvailableThisSession` is read by nobody).
- 🔴 **The new ids render as raw `#OVT-Map_*` keys on screen until the user regenerates the exports in
  Workbench.** That is expected, is not a defect, and must **not** be worked around by hardcoding literals.
  The two `#OVT-Map_Layer_*` ids `territory-overlay` shipped are already in exactly this state.

---

### Phase 4 — The panel — **M — `ui-developer-advanced`**

> **Advanced.** This is the whole player-facing surface, it is console-critical, and it is where the
> `FindAnyWidget` failure class lives. Nothing in it is visible to any automated gate.

**Tasks**

1. `UI/Layouts/Map/Core/OVT_MapLayersPanel.layout` (+ `.meta`) — final content: a fixed-width vertical
   layout, top-left aligned inside its `OverlayWidgetSlot`, carrying `PanelTitle`, `OverlaysHeader`,
   `OverlayRows`, `MarkersHeader`, `TypeRows` and `FocusProxy` (§3.6). A background at the shop menu's
   opacity so labels read against terrain, and a scroll container around the rows — 17 rows will not fit a
   short screen.
2. `UI/Layouts/Map/Core/OVT_MapLayerRow.layout` (+ `.meta`) — `RowIcon` (13×13, matching the info-row
   convention the epic settled on), `RowLabel`, `RowCheckbox` inheriting `WLib_Checkbox.layout` with the
   component GUID **copied, not generated**.
3. `Scripts/Game/UI/Map/Core/OVT_MapLayerRowComponent.c : SCR_ScriptedWidgetComponent`:
   - `void Init(string key, string label, ResourceName imageset, string icon, bool visible,
OVT_MapLayersUI owner)` — **`Init()` does the wiring, not `HandlerAttached()`** (the ordering of
     `HandlerAttached` against sibling handlers is not guaranteed).
   - Wire `SCR_CheckboxComponent.m_OnChanged` once, guarded by a `m_bWired` flag; call back into the owner
     with the key and the new state. **Rows never mutate state themselves.**
   - Hide `RowIcon` when the imageset is empty (overlay and player rows have no icon source).
4. `OVT_MapLayersUI` — replace the Phase 1 stub:
   - `BuildRows()` from the three sources (§3.4) with the four skip filters; clear children first
     (`while (container.GetChildren()) container.RemoveChild(container.GetChildren())`) so a rebuild cannot
     stack duplicates.
   - **Row state is read live from the objects** (`IsPlayerVisible` / `IsLayerVisible` /
     `AreMarkersVisible`), never from the store, so the panel cannot disagree with the map.
   - `OnRowToggled(key, visible)` → apply immediately, then `store.SetHidden(key, !visible)`.
   - `TogglePanel()` copies `SCR_MapJournalUI.ToggleVisible`: flip visibility, `m_ToolMenuEntry.SetActive`,
     and **focus the first row when becoming visible** (K10).
   - `ClosePanel()` on the `GetOnDisableMapUIInvoker()` subscription, and `HandlerDeattached`-equivalent
     teardown that removes exactly what was inserted.
   - `Widget GetPanelWidget()` returning null when closed — consumed by `OVT_OverthrowMapUI` for K14.
5. Rebuild rows on every panel open (17 rows is nothing, and it is the only construction that is always
   correct given layers register per map open).

**Acceptance**

- compile **exit 0**, file count **5994** (+1, `OVT_MapLayerRowComponent.c`). Fast **89**, All **127** —
  unchanged.
- **Play-test (user):** the panel opens, shows 3 overlay/marker rows and 14 type rows with icons and
  labels, and every toggle takes effect on the map **within one frame and without any flicker or
  repopulation**. Toggle a type off with an info panel pinned and leave the map open for >10 s — the
  refresh tick (2–5 s for six types) must not resurrect the hidden markers, and must not drop the pin.

---

### Phase 5 — Persistence wiring and resilience — **S — `component-developer`**

**Tasks**

1. `OVT_MapLayersUI` owns one `ref OVT_MapLayerPrefsStore`, loaded **once** on the first `OnMapOpen` and
   cached for the process (the profile cannot change under a running client).
2. `ApplyPreferences()` — walk all three sources and push `store.IsVisible(key)` into each object.
   Idempotent by construction.
3. **Call it twice per map open** (K8): once from `OnMapOpen`, and once from the first `Update` tick after
   an open (a one-shot bool). The first covers the normal case with no visible flash; the second is
   provably after every module and component has run its `OnMapOpen`, whatever the subscription order.
4. **Flush points: panel close and map close only** (K15). Never per toggle.
5. Degrade cleanly: on console-app/headless, or when the settings module is unavailable, the store stays
   in memory and every toggle still works for the session.

**Acceptance**

- compile **exit 0**, file count unchanged (**5994**). Fast **89**, All **127**.
- **Play-test (user):** hide Houses and the Territory overlay → close the map → reopen → both still hidden.
  Quit to the main menu and rejoin → both still hidden. Re-enable both → close → reopen → both back.

---

### Phase 6 — Boundary audit, contract records, docs — **S — `component-developer`**

**Tasks**

1. Re-run `territory-overlay`'s I-4 boundary greps over every new and changed file (§3.5) and record the
   output.
2. Write the contract rows from §3.7 into `docs/features/map/core/context.md`, including the "no rows
   added to the canvas-layer contract" note and the `OVT_MapPlayerLocation` exception.
3. Add the layout↔code table from §3.6 to the same file, following the two tables already there.
4. Update `docs/features/map/epic-overview.md` — feature 7 row, rollup, and any tech-debt movement.
5. **File the incidental findings as bugs** (§11 F1–F3) — do not fix them here.
6. ❌ **No `file:line` pointers in shipped code comments** (epic K-9: keep the rationale, name the symbol,
   drop the line number). `docs/` citations are exempt and expected.

**Acceptance:** compile **exit 0**, Fast **89**, All **127** — all unchanged; this is a docs-and-audit
phase and any movement is a finding.

---

### Phase 7 — Verification gate — **M — user-driven, no agent**

The full Verification Method in §8. ⚠️ **Warn the user before launching anything** — `tools/launch-game.sh`
opens a window on their desktop and can orphan. Always pass `--timeout 3600`.

---

### Phase 8 — Help & documentation sync — **S — `help-docs-sync`**

This feature changes what players see and do, so the closing phase is mandatory: tutorial popups
(`Configs/Tutorials/`), the Field Manual (`Configs/FieldManual/`) and the public wiki must describe the
panel, how to open it on both keyboard and pad, and that preferences are per profile rather than per
campaign. ⚠️ Every sentence must be backed by shipped behaviour — two tips have shipped inventing mechanics
in this project, and no gate can catch a well-formed lie.

---

## 6. Key Technical Decisions

### K1 — `ModuleGameSettings` has an **in-tree precedent**, and it is reused rather than re-derived

**Corrects `requirements.md`**, which says _"Overthrow does not use this mechanism anywhere yet — this would
be its first."_ **That has been false since the `new-player-experience` feature.** The precedent is
`Scripts/Game/Global/OVT_TutorialSettings.c` + `OVT_TutorialSettingsAccessor.c` +
`Scripts/Game/Data/OVT_TutorialSeenStore.c`, and it carries four hard-won facts this feature inherits
wholesale rather than rediscovering:

- **(a)** A top-level `ref array<string>` in a settings module is **unproven**; a nested
  `[BaseContainerProps()]` struct inside a module's object array **is** proven (`SCR_FilterSetStorage`
  round-trips one on every server-browser filter save). Adopt the proven shape.
- **(b)** **Never parallel arrays.** `SCR_HintSettings` keeps ids and counts in two arrays and has to erase
  _both_ when their lengths disagree. One array of self-describing structs cannot get out of step with
  itself.
- **(c)** **`SaveUserSettings()` is throttled by the engine.** Measured 2026-08-07: two calls microseconds
  apart leave only the **first** on disk — the second is _dropped_, not deferred — while two calls six
  seconds apart both land. See K15 for what this feature does about it, because a filter panel is far more
  exposed to it than the tutorial store was.
- **(d)** **Guard `System.IsConsoleApp()`** and degrade to an in-memory store; a headless server has no
  profile. Null-guard `GetGameUserSettings()` (null very early in startup) and `GetModule()` (null for an
  unregistered class). Neither deserves a log line every frame.

Also inherited: the **read-modify-write against the live container**, so a member a future version adds is
preserved rather than blanked, and the schema-version-clears-rather-than-half-trusts rule.

### K2 — A display-name field already exists; this feature adds a **third**, `m_sCategoryName` — **USER DECISION**

**Corrects `requirements.md`**, which offers _"either promote `m_sName` properly or introduce a separate
display field"_ as if no separate field existed. One does: `OVT_MapLocationType.m_sDisplayName`, with
`GetDisplayName()` at `:389`, already driving the info panel's type line. It is unsuitable for a filter row
for two independent reasons:

1. It is **singular** — "Town", "#OVT-House" — where a category row wants a plural.
2. **5 of its 14 configured values are raw English literals**, not localization keys: Town `"Town"`
   (`OverthrowMap.conf:6`), Bus Stop `"Bus Stop"` (`:143`), Vehicle `"Vehicle"` (`:155`), POI
   `"Point of Interest"` (`:166`), Waypoint `"Waypoint"` (`:176`).

**The user chose: add a new `m_sCategoryName` attribute** (plural, localized), leaving `m_sName` (the
Workbench editor-tree label read only by `OVT_MapLocationTypeTitle._WB_GetCustomTitle`) and
`m_sDisplayName` (the info-panel type line) **doing exactly their existing jobs, untouched**. Three fields
is one more than feels tidy, and it is still right: they have three different audiences (an editor tree, an
info panel, a filter list) and three different grammatical forms, and merging any two of them would drag a
second surface into this feature's blast radius.

Convenient consequence: the English text for all 14 new ids is already authored — `m_sName` is _already_
the correct plural for every one of the 14 types ("Towns", "Radio Towers", "Points of Interest"). The new
ids transcribe it rather than inventing it.

### K3 — Overthrow has **no** live tool-menu registration; this feature is the first — **CORRECTION**

`Scripts/Game/UI/Map/Visualization/OVT_MapPlayerLocation.c:3` declares
`protected SCR_MapToolEntry m_ToolMenuEntry`, and it is **never assigned**: `Init()` is empty, the only
other mention is a commented-out line, and `ZoomInOnPlayer()` has **zero callers** — grep-verified across
`Scripts/`. It is vestigial and must not be planned around as a working precedent. The only working
precedents are vanilla's nine registrants.

**Do not delete it as part of this feature either.** It is flagged as finding **F1** (§11) for the user to
decide on — deleting dead code in a file this feature is already editing is exactly how a small change
becomes an ambiguous one.

### K4 — **No colour key.** The legend's "colour meaning" clause is deliberately not shipped — **USER DECISION**

`requirements.md` asks the legend to identify _"each active overlay by its colour meaning (faction
territory colours, restriction rings, threat shading)"_. The settled one-row-per-layer shape gives an on/off
row, not a swatch. **The user chose: ship on/off rows only.**

Rationale, recorded so this reads as a decision rather than an omission: the territory overlay draws the
**same faction colours Overthrow already uses everywhere else** — on markers, on restricted rings, in the
info panels — through one shared colour helper that `territory-overlay` K6 deliberately unified. A key
would restate what the map is already showing, in a panel whose whole job is to reduce what the map shows.
If it turns out players cannot tell the factions apart, that is a _colour_ problem in `territory-overlay`,
not a missing legend here. **No swatch column is designed, and none should be added speculatively.**

### K5 — `OVT_MapPlayerLocation` gets a **hand-built** row — **USER DECISION**

It is a `SCR_MapUIBaseComponent`, not an `OVT_MapCanvasLayer`, so it will never appear in `GetLayers()`.
Three options existed: reparent it onto `OVT_MapCanvasLayer`, drop the row, or special-case it. **The user
chose the hand-built row.** Reparenting a working, retained component (which `legacy-retirement`
deliberately kept) to satisfy a list builder buys nothing and risks a live feature; dropping the row leaves
a visible marker with no control, which is exactly the inconsistency a filter panel exists to remove.

The exception is documented in the contract file (§3.7) so the next reader knows why one row of seventeen
is not generic. It also carries an availability rule the generic rows do not need: `OnMapOpen`
early-returns when there is no controlled entity or when `m_Difficulty.showPlayerOnMap` is false, so
`IsAvailableThisSession()` is set only past both returns and **a difficulty-disabled marker presents no
row at all** rather than a dead toggle.

### K6 — Tool-menu icon: reuse a vanilla glyph, exposed as an attribute — **USER DECISION**

Pass `SCR_MapToolMenuUI.s_sToolMenuIcons` — `{2EFEA2AF1F38E7F0}UI/Textures/Icons/icons_wrapperUI-64.imageset`
— with an existing quad, exactly as seven of the nine vanilla registrants do. The set carries **204 quads**;
the closest fit is **`"filters"`** (candidates considered and rejected: `gridView`, `listView`,
`terrainIcon`, `structures`, `settings`).

**Zero art dependency — the feature must be shippable and play-testable the day it compiles.** This epic has
been blocked on art before (the shop caret icons) and will not be again. The quad name is an `[Attribute]`
so the user can swap it from config with no code change, and `SCR_MapMarkerEntrySquadLeader` proves an
arbitrary imageset can be passed — so a bespoke Overthrow glyph stays a later, config-only change.

### K7 — 🔴 Register the tool-menu entry in `Init()`, **never** in `OnMapOpen`

`SCR_MapToolMenuUI.m_aMenuEntries` is **only ever inserted into** — there is no unregister API anywhere in
the class — and `OnMapClose` does not clear it. `OnMapOpen` re-runs `PopulateToolMenu()`, which rebuilds
the button widgets and re-binds each entry's `m_ButtonComp`. So registering in `OnMapOpen` **duplicates the
entry on every single map open**, forever, within one session.

⚠️ **This is a genuine trap, because the two `Init()`s in play have opposite lifetimes.**
`SCR_MapUIBaseComponent.Init()` runs once per map-config load; `OVT_MapLocationType.Init()` — documented in
`map/core`'s gotchas — runs on **every** map open. A developer who has internalised the second rule will
put the registration in the wrong place. Say so at the call site.

### K8 — Apply preferences **twice**, because the registration order is an assumption

`OVT_MapCanvasCompositor.GetLayers()` is only populated while a map is open: layers register in `OnMapOpen`
and unregister in `OnMapClose` / `SetActive(false)`. So a persisted "territory hidden" preference can only
be applied to a layer that has already registered — and "my saved toggles didn't stick" is the single most
likely bug in this feature.

Reading `SCR_MapEntity.OnMapOpen` settles the normal case: `ActivateModules(config.Modules)` runs at `:355`
and `ActivateComponents(config.Components)` at `:356`, and each one's `SetActive(true)` is what _subscribes_
`OnMapOpen`; the invoke itself is at `:361`, after both. ScriptInvoker fires in insertion order, so **every
module's `OnMapOpen` runs before every component's** — the layers are registered by the time
`OVT_MapLayersUI.OnMapOpen` runs.

That is true and it is still an assumption: it depends on subscription order, and the reactivation path
(`m_bDoReload == false`) re-subscribes in a different loop. So `ApplyPreferences()` is called **from
`OnMapOpen` and again from the first `Update` tick after an open**. The first avoids a one-frame flash of a
layer the player has hidden; the second is provably after every registration whatever the order. The call
is idempotent — it writes the same values — so the two cannot fight, and the ongoing cost is one boolean
compare per frame.

### K9 — Create the panel **into** `ToolFramesOverlay`; do not override the vanilla layout

`SCR_MapJournalUI` finds a frame **authored into vanilla's `UI/layouts/Map/MapMenu.layout`**
(`JournalFrame`, alongside `MapTaskList`, `MapSuppliesTransportSystem` and `MapWeatherFrame` inside
`ToolFramesOverlay`). Copying that literally would mean Overthrow shipping a same-GUID delta over a **vanilla
`.layout`** — and while same-GUID `.conf` and `.et` deltas are proven in this project, a same-GUID
**layout** delta merging a child into an existing widget's children block is **not**.

So the panel is **created from script into the existing overlay**:
`workspace.CreateWidgets(m_PanelLayout, m_RootWidget.FindAnyWidget("ToolFramesOverlay"))`. That is the
`CreateWidgets(layout, parent)` idiom used throughout Overthrow's UI, it needs no vanilla file touched at
all, and it degrades through a named fallback chain (`ToolFramesOverlay` → `ToolMenuContainer` →
`m_RootWidget`) with an ERROR naming which one it settled on. The fallback is not hypothetical:
`FastTravelMapMenu.layout` has no `ToolFramesOverlay` and parents `JournalFrame` directly onto
`ToolMenuContainer`.

Consequence to respect: `ToolFramesOverlay`'s children are stacked full-stretch overlays, so the **panel's
own content layout owns its footprint**. Author it fixed-width and top-left aligned so it hugs the icon
strip. Do not try to reason about `ToolMenuContainer`'s negative `SizeX -1820 / SizeY -1050` — author it
the way `JournalFrame` is authored and size the content from the layout.

### K10 — 🔴 The panel must grab focus itself, and the order that makes it work is fragile

`SCR_MapToolEntry`'s constructor inserts its own `OnClick` into `m_OnClick` **first**, and that handler
clears focus on a gamepad (`if (!IsUsingMouseAndKeyboard()) m_OwnerMenu.SetToolMenuFocused(false);`, which
calls `SetFocusedWidget(null)`). A toggle handler inserted afterwards therefore runs _after_ focus has been
nulled and must re-establish it — which is exactly what `SCR_MapJournalUI.ToggleVisible` does with
`FocusOnFirstEntry()`.

**If our handler ran first, or if anything cleared focus after it, the panel would open unfocused and be
completely dead on a controller** — while looking perfect on a mouse. Two consequences:

- `TogglePanel` must call `SetFocusedWidget(firstRow)` when becoming visible, and Phase 1's P4 is what
  proves it.
- The panel needs a **focus-proxy button** (`FocusProxy` + `SCR_EventHandlerComponent.GetOnFocus()` →
  bounce to the first row), copying `SCR_MapJournalUI`'s `m_wFocusButton`. Engine focus navigation can land
  on the panel's outer widget; without the proxy the player is focused on a container that does nothing.

There is deliberately **no new "close the panel" action**. The tool-menu entry toggles it — D-pad Left back
to the strip, A to close — which is the vanilla-native path and, decisively, needs **no free key or pad
button on `MapContext`**. `MapContext` already carries 41 live actions, `KC_H` is taken three times over,
and the repo's input-conflict checker cannot see the base game's 197 inline `ActionContext` actions. Not
needing a binding is worth more than a slightly shorter close gesture.

### K11 — Sort priority is **ascending**, and negatives silently clamp to 0

`SCR_MapToolMenuUI.PopulateToolMenu` bubble-sorts on `m_iSortPriority` with **lower first**, and the bar
(`ToolMenuHoriz`) is a `VerticalLayoutWidget` despite the name, so lower priority sits **higher** on screen.
Vanilla uses 0 (supplies), 1 (journal), 2 (tasks), 10 (ruler), 11 (compass), 12 (watch), 13 (drawing), 20
(weather), 20 (squad). The entry ctor clamps `sortPriority <= 0` to `0`, so **negative values cannot sort
above supplies** — they tie with it and fall back to registration order.

Default **3**: grouped with the content entries (supplies/journal/tasks) rather than the measuring tools,
which is what a map-content filter is. Exposed as an `[Attribute]` so it is a config change, not a code
change.

### K12 — The entry is **exclusive**, and that is a rendering requirement, not a preference

`isExclusive` makes clicking this entry fire `GetOnDisableMapUIInvoker()` on every _other_ exclusive entry —
a mutual-exclusion group among opt-in members. In vanilla's fullscreen map exactly two components opt in:
`SCR_MapJournalUI` and `SCR_MapTaskListUI`, both via `m_bIsExclusive 1` in `MapOverthrow.conf`.

Both of those dock into the same `ToolFramesOverlay`, whose children are **full-stretch overlays that
overlap by construction**. Vanilla gets away with it only because at most one exclusive panel is visible.
An Overthrow panel in the same overlay that was _not_ exclusive would render on top of an open journal.
So `m_bIsExclusive 1` in the conf entry, and the `GetOnDisableMapUIInvoker()` subscription that makes it
mean something.

⚠️ Note that `SCR_MapToolEntry.SetEnabled` is **cosmetic only** — it sets the border colour and nothing
consults `m_bIsEnabled` to block a click. Every vanilla caller calls `SetEnabled(true)` immediately after
registering purely to get the orange border; copy that and do not mistake it for a gate. Likewise
`SCR_MapToolEntry.GetImageSet()` returns `m_sIconQuad` — a vanilla bug. Do not use it.

### K13 — Store **hidden** keys, not a key→bool map

The record is a set of hidden keys; **absent means visible**. Given D3 ("everything on"), this is the
minimal representation that is also the most robust:

- A location type or canvas layer added later is absent from every existing record, and therefore defaults
  **visible** with no migration and no schema bump.
- The entry type is named `OVT_MapHiddenLayerEntry`, so the meaning of presence lives in the type name
  rather than in a comment.
- It cannot express a contradiction. A key→bool map can hold `visible = true` for a key that also appears
  in a hidden list; a set cannot disagree with itself, which is the same argument as K1b one level down.

Rejected: `{ string m_sKey; bool m_bVisible; }`. It buys the ability to record "explicitly on", which is
indistinguishable from "absent" under an always-default-on model, and it buys a future tri-state nobody has
asked for.

**Keys are namespace-prefixed** (`"type:OVT_MapLocationHouse"`, `"layer:territory"`) so the two id spaces
cannot collide.

**On class names as ids.** `OVT_OverthrowMapUI.TickRefresh` already keys its timer map by `ClassName()`
rather than by an array parallel to `m_aLocationTypes`, with the reason written at the method; reusing that
convention costs nothing and adds no new idiom. It survives config reordering, which an index would not.
**It does not survive a class rename** — and the failure is benign and worth stating: the stored key stops
matching, the type reverts to visible (the default), and the player loses one preference once. The
alternative — an authored `m_sFilterId` attribute — is more stable but needs 14 config values that can be
left empty, mistyped or duplicated, which trades a benign failure for a silent one. **Class name wins.**
The duplicate-key skip filter (§3.4) is what makes the one real hazard — two config entries of the same
class — loud instead of silent.

### K14 — The panel inherits the info-panel unpin hazard, and gets the same guard

`OVT_OverthrowMapUI.OnMapSelection`'s `else` branch treats "no hovered element" as "clicked empty map" and
unpins the info panel. A click landing on a UI panel over the map looks exactly like that. `map/fast-travel`
added `IsSelectionOnInfoPanel` for precisely this, for precisely the same reason (a control that must leave
the panel open).

Generalise it to `IsSelectionInsideWidget(Widget, vector)` and test the layers panel too.
⚠️ **Honest status, copied from R6:** it is still unverified whether the button widget consumes the click
before the map's selection handler sees it at all. If it does, the guard is inert. It is applied anyway
because it is two lines and the alternative is a bug that only appears when a player happens to have a
pinned panel open.

### K15 — Flush on **panel close and map close**, not per toggle

The engine throttles `SaveUserSettings()` (K1c): two calls microseconds apart leave only the first on disk.
The tutorial store's answer was "flush on every mutation, survivable because we write the whole record" —
and that reasoning does **not** transfer unchanged, because a filter panel is a _burst_ surface. A player
flipping five rows in three seconds generates five flushes, most of which the engine will drop.

So: **mutate in memory on every toggle and apply to the map immediately; flush at panel close and at map
close.** Both write the **whole record**, so a dropped intermediate flush loses nothing — the next one
carries every key. The two flush points are separated by distinct user actions, so they cannot collapse
into one throttle window.

**Residual exposure, accepted and stated:** a player who alt-F4s or crashes with the map still open loses
that session's filter choices. That costs a few seconds of re-toggling a cosmetic preference — strictly
less than the tutorial store's failure mode (a dismissed tip reappearing), which is why that store made the
opposite trade. Applying to the map is **decoupled** from flushing: a toggle takes visible effect
immediately regardless of whether anything reached disk.

### K16 — Manager access: none, and that is the point (epic tech debt T1)

This feature reads no manager singleton except through `OVT_MapPlayerLocation`'s existing
`OVT_Global.GetConfig()` call, which is already there and is not moved. It therefore **adds no fourth
manager-access idiom** to T1's existing three. The panel's entire input is the config's type list, the
compositor's layer list and a profile settings block.

---

## 7. Quality Bar

This is a **UI-heavy, console-critical** feature with a persistence half. Correctness is necessary and
nowhere near sufficient: a panel that filters perfectly but cannot be operated on a pad has failed the
requirement it exists to satisfy, and a preference that silently fails to save is the most likely
real-world complaint.

**Gamepad (the primary bar — a mouse-only filter panel is worse than no filter panel).**

- The full round trip in P4 works with **no mouse touched at all**: open the map, D-pad Left, reach the
  entry, open, walk every row, toggle several, leave, close.
- Focus is **visible** on the row the pad is on. A row with no focus visual is unusable on a pad.
- Focus lands **inside the panel** on open, not on a container, not nowhere.
- The panel is reachable and dismissible from the same entry; no new binding was added to `MapContext`.

**Non-occlusion.**

- The panel docks beside the tool strip and never covers the centre of the map.
- With the panel open, markers and terrain outside its footprint remain fully interactive — hover, pin and
  travel still work.
- 17 rows on a short screen scroll rather than overflowing off-screen.

**Immediate response.**

- A toggle takes effect **within one frame**. No repopulation, no flicker, no marker destroyed or
  recreated, no info panel dismissed.
- Hiding a type must not make the zoom sweep slower. The gate early-returns; that is a design property, not
  an optimisation to add later.

**Readable labels.**

- Every row label is a localized `#OVT-` key. **No raw editor string reaches a player.**
- Labels are plural category names, not singular type names.
- ⚠️ Raw `#OVT-Map_*` keys on screen before the user regenerates the exports are **expected**, not a defect,
  and must not be worked around by hardcoding literals.

**Persistence reliability.**

- Preferences survive map close/open, a session, and a restart.
- On a headless/dedicated server, or with no settings module, every toggle still works for the session and
  nothing errors.
- The stored record is whole-record, self-describing, and cannot half-load.

**Structural.**

- No `[RplProp]`, no RPC, no EPF, no write to any campaign record, nothing in
  `OVT_PlayerCommsComponent`.
- The `OVT_MapLocationType` extension is **additive with safe defaults**: with no panel and no store, every
  type behaves byte-for-byte as it does today.
- Player _presentation preference_ and campaign _visibility_ are separately named from day one.
- No `file:line` pointers in new code comments. Every new Logic case is **proven able to fail**, with the
  method recorded. ❌ No `maxAttempts`.

---

## 8. Definition of Done

Written so an evaluator with **no implementation context** can verify each item.

### Functional

**F-1 — The entry exists.** Open the fullscreen map in a started campaign. Vanilla's tool-menu icon strip is
on the left, and it carries an Overthrow entry with a filter glyph, positioned above the ruler/compass/watch
tools.

**F-2 — The panel opens and closes from that entry.** Clicking it opens a docked panel titled "Map Layers";
clicking it again closes it. Opening the journal or task list closes the layers panel, and vice versa.

**F-3 — Every drawn thing has exactly one row.** The panel lists **14 location-type rows** — Towns, Bases,
Radio Towers, FOBs, Ports, Camps, Houses, Shops, Gun Dealers, Warehouses, Bus Stops, Vehicles, Points of
Interest, Waypoints — plus **Territory**, **Restricted Areas** and **Players**. Seventeen rows. No blank
row, no duplicate, and no row for the disabled threat grid.

**F-4 — Every row is labelled and iconed.** Each location-type row shows that type's map icon and a plural
localized name. Overlay and player rows show a name and no icon. Nothing reads as a raw editor string.
(Before the exports are regenerated, the names render as `#OVT-Map_*` keys — that is expected, and is what
F-11 checks.)

**F-5 — Every toggle works, immediately.** Turn each row off and confirm the corresponding thing disappears
from the map **within one frame**; turn it back on and confirm it returns. Territory shading, restriction
rings and player markers all respond.

**F-6 — Toggling does not disturb the map.** With an info panel pinned on a marker, toggle three unrelated
rows. The pin survives, the panel stays open, no marker flickers or moves, and the map does not re-centre.

**F-7 — The refresh tick does not resurrect hidden markers.** Hide Towns (5 s refresh) and Vehicles (2 s
refresh) and leave the map open for **30 seconds**. Neither reappears.

**F-8 — Default is everything on.** On a profile that has never opened the panel, the map looks exactly as
it did before this feature and every row reads on.

**F-9 — Preferences survive.** Hide Houses and Territory. Close the map, reopen — still hidden. Quit to the
main menu, rejoin the campaign, open the map — still hidden. Re-enable both, close and reopen — both back.

**F-10 — The panel does not occlude the map.** It sits beside the icon strip. With it open, markers in the
rest of the map still hover, pin and offer travel.

**F-11 — Localization.** After the user regenerates the six exports in Workbench, all 18 new ids render as
English text with no `#OVT-` key visible anywhere in the panel.

### Quality

**Q-1 — Gamepad round trip, no mouse.** With a controller only: open the map, D-pad Left, D-pad Up/Down to
the entry, A. The panel opens **with focus already on the first row**. D-pad Up/Down walks every row with a
visible focus highlight; A toggles the focused row and the map responds. D-pad Left returns to the strip; A
closes the panel.

**Q-2 — Scrolling.** With 17 rows the list is fully reachable on a 1080p screen, by pad and by mouse wheel.

**Q-3 — No new binding.** `git diff` on `Configs/System/chimeraInputCommon.conf` is **empty** for this
feature. Confirm with
`python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` → exit 0.

**Q-4 — Hidden is cheaper.** By code reading: `OVT_MapLocationElement.SetVisible` returns before the zoom
lookup and before `ShouldShowLocation` when the type is hidden.

**Q-5 — Every new Logic case is proven able to fail.** Both inversions recorded in `context.md`, each
applied alone to a pristine copy, reverted, and the clean gate re-run.

**Q-6 — Gates.** compile **exit 0 / 5994 files**; Fast **89**; All **127**. Any other number is a finding to
investigate, not a number to update.

### Integration

**I-1 — Boundary greps clean.** No `[RplProp]`, `[RplRpc]`, `Rpc(`, `EPF_`, or `OVT_PlayerCommsComponent`
in any new or changed file. Every new file under `Scripts/Game/UI/Map/`, `Scripts/Game/Data/`,
`Scripts/Game/Global/` or `Scripts/Game/Tests/`.

**I-2 — 🔴 Two clients, opposite preferences.** Two clients on one dedicated server. Client A hides Houses
and Territory; client B hides nothing. **B's map is unchanged.** B then hides Bases; **A's map is
unchanged, and A's houses and territory stay hidden.** Neither client's markers, territory or info panels
differ in any way other than by their own filters.

**I-3 — JIP.** Client B joins _after_ A has accumulated state and toggled filters. B's map is complete and
unfiltered; A's filters persist across B's join.

**I-4 — Nothing else regressed.** With every row on, the map is indistinguishable from the pre-feature map:
markers, info panels, fast travel, bus travel, territory shading and restriction rings all behave as
before.

**I-5 — Contract records written.** `docs/features/map/core/context.md` carries the three new
`OVT_MapLocationType` rows, the "no rows added" note on the canvas-layer contract, the
`OVT_MapPlayerLocation` exception, and the layout↔code table.

### Verification Method

Run in order. Steps 1–4 are automated; 5–8 need a human; 9 needs two clients.

1. `git status` — clean, on `new-map`. Record the commit.
2. `tools/compile-check.sh` → **exit 0**, note the file count.
3. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → **exit 0**, note the count.
4. `tools/run-tests.sh "{6A6E2A002F53A581}"` → **exit 0**, note the count.
   Compare all three against Phase 0. Investigate any difference other than the predicted +6 files / +2
   tests.
5. **Workbench clean load** — open the project in Workbench and confirm no unresolved-resource or GUID
   errors. This is the only gate in the project that can see a dangling GUID in a new `.layout` or `.meta`.
   ⚠️ A pre-existing orphaned `.meta` for `OVT_MapThreatGrid` (`{B8F4C6A8C9D3E4F1}`) is **not** this
   feature's.
6. **Single-player pass** — start or load a campaign, open the map, and walk **F-1 … F-10** in order.
7. **Gamepad pass** — with a controller and the mouse untouched, walk **Q-1** and **Q-2**.
8. **Persistence pass** — F-9, including a full quit to the main menu and rejoin. Then quit the game
   entirely and relaunch: the preferences must still be there.
9. **Two-client MP pass** — ⚠️ _warn the user first: each client opens a window on their desktop and can
   orphan._
   ```
   tools/launch-server.sh
   tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001
   tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001
   ```
   Walk **I-2** and **I-3**. Two profiles is also what proves the preferences are **per profile**: client 1's
   filters must not appear on client 2.
10. Record every observation in `context.md` — including the ones that passed. A play-test report is this
    feature's only evidence for everything in §7.

---

## 9. Testing Strategy

**Be honest about what this is.** A filter panel is overwhelmingly UI, and UI is explicitly outside this
project's automated test spine. There is no widget tier, no input tier and no rendering tier; `.layout`,
`.conf`, `.meta` and `.st` edits are invisible to `tools/compile-check.sh` **and** to both test groups. Most
of this feature can only be verified by a human looking at a screen with a controller in their hands.

**Do not invent UI test cases.** The automated gates here are regression guards for everything _else_, plus
two genuine assertions about the one part that is pure.

### What genuinely can be pinned: the preference store (Logic tier)

`OVT_MapLayerPrefsStore` makes no engine call, touches no widget, and knows nothing about `BaseContainer` —
the settings module is injected as a plain array of strings and read back as one. That split exists for
exactly this reason, and it is the same split `OVT_TutorialSeenStore` uses.

| Case                                         | What it pins                                                                                                                                                                                                                                                                                                                                                      | Candidate inversion                                                                                                                                                            |
| -------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `OVT_TEST_Logic_MapLayerPrefs_HiddenSet`     | Absent ⇒ **visible** (the default that makes D3 work); hide then show is idempotent both ways; an empty key is refused; `LoadFrom` at a **mismatched version discards** rather than half-loading; `LoadFrom(null)` is an empty list, not a crash; `WriteTo` round-trips exactly the hidden set into a reused, pre-populated buffer with nothing stale left behind | Invert the default (`return m_aHidden.Contains(key)` instead of `!…`) — every type ships hidden; **or** drop the version guard in `LoadFrom` and confirm a v0 list is imported |
| `OVT_TEST_Logic_MapLayerPrefs_KeyNamespaces` | `TypeKey`/`LayerKey` produce distinct prefixed keys; **a location type and a canvas layer with the same bare name do not collide**; hiding one does not hide the other                                                                                                                                                                                            | Drop the prefix from `LayerKey` — a layer called `territory` and a type class called `territory` share one preference                                                          |

Both must be **proven able to fail** before shipping: apply the inversion alone to a pristine copy, compile,
run that one case, revert, re-run the clean gate, and record the exact failure message in `context.md`.
❌ **No `maxAttempts`.**

### What no automated gate can see

- The tool-menu entry existing, rendering, or being clickable.
- The panel docking, its position, its size, whether it occludes anything, whether it scrolls.
- Every widget name in §3.6 — `FindAnyWidget` returning null is a silent no-op.
- **All gamepad behaviour**, including the focus grab in K10, which is the single most likely thing to ship
  looking correct and being dead on a controller.
- Whether a toggle visibly changes the map, and how fast.
- The `ModuleGameSettings` round trip, the engine's flush throttle, and anything about the profile on disk.
- Localization rendering, and whether an id resolves or shows as a raw key.
- MP isolation between two clients.

The autotest world has no players, no map and no UI. Phases 1, 4, 5 and 7 produce evidence that exists
**only** as a user's play-test report.

### Debugging: three signatures

Written before the play-test, on the pattern `map/respawn` established.

| Symptom                                | Most likely cause                                                                                   | First check                                                                                                                                                                                                                                |
| -------------------------------------- | --------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **No entry on the tool strip**         | `SCR_MapToolMenuUI` not merged into Overthrow's `m_aUIComponents`, or `Init()` never ran            | Is vanilla's icon strip there at all? If the strip is missing entirely it is the conf merge (P1's fallback). If the strip is there and only our entry is missing, `GetMapUIComponent` returned null or `Init()` bailed.                    |
| **Panel opens but a row does nothing** | `FindAnyWidget` name mismatch, or a fresh `SCR_CheckboxComponent` GUID instead of the inherited one | The ERROR log from the null-guarded lookup names the widget. If there is no ERROR, the lookup succeeded and the component GUID is wrong — the checkbox is a second, unconfigured instance.                                                 |
| **Preferences do not stick**           | The store never flushed, or the apply ran before the layers registered                              | Toggle, close the panel, close the map, and check the profile settings block. If the record is on disk but the map comes back unfiltered, it is K8's ordering; if the record is absent, it is K15's flush point or a null settings module. |
| **Panel is dead on gamepad**           | Focus never landed inside it (K10)                                                                  | Does the first row show a focus highlight the instant the panel opens? If not, `SetFocusedWidget` did not run, ran before the entry's own `OnClick` nulled focus, or the row is not a real focusable button.                               |

---

## 10. Dependencies

### Internal (code — all read-only unless noted)

| Depends on                                                                                    | Why                                                      | Changed?                                              |
| --------------------------------------------------------------------------------------------- | -------------------------------------------------------- | ----------------------------------------------------- |
| `OVT_MapCanvasCompositor.GetLayers()`                                                         | enumerate overlay rows                                   | **No** — read only, never mutated                     |
| `OVT_MapCanvasLayer` (`m_sLayerId` / `m_sDisplayName` / `SetLayerVisible` / `IsLayerVisible`) | overlay row identity, label, toggle                      | **No** — the contract already fits exactly            |
| `OVT_MapLocationType`                                                                         | type list, category name, player-visible flag            | **Yes** — 3 additive members (§3.7)                   |
| `OVT_MapLocationElement.SetVisible`                                                           | the fourth gate                                          | **Yes** — one early return + one new method           |
| `OVT_OverthrowMapUI`                                                                          | type accessor, refresh sweep, unpin guard                | **Yes** — 3 additive methods                          |
| `OVT_MapPlayerLocation`                                                                       | the hand-built row                                       | **Yes** — 2 additive members + availability flag (K5) |
| `Configs/Map/OverthrowMap.conf`                                                               | 14 `m_sCategoryName` values                              | **Yes**                                               |
| `Configs/Map/MapOverthrow.conf`                                                               | register `OVT_MapLayersUI` (+ the P1 fallback if needed) | **Yes**                                               |
| `OVT_TutorialSettings` / `…Accessor` / `OVT_TutorialSeenStore`                                | the persistence pattern                                  | **No** — copied, not shared                           |

### Vanilla (read-only, never modified)

`SCR_MapToolMenuUI` / `SCR_MapToolEntry`; `SCR_MapJournalUI` (the reference implementation);
`SCR_MapUIBaseComponent`; `UI/layouts/Map/MapMenu.layout` (`ToolFramesOverlay`, resolved by name — **not
overridden**, K9); `UI/layouts/WidgetLibrary/ToolBoxes/WLib_Checkbox.layout` + `SCR_CheckboxComponent`;
`SCR_EventHandlerComponent`; `ModuleGameSettings`, `UserSettings.GetModule`, `BaseContainerTools`.

### External — user / Workbench work

1. **Regenerate the six localization exports** after Phase 3 (18 new ids). Until then the panel shows raw
   keys — expected, not a defect.
2. **The play-test gate** (§8 steps 5–9), including the two-client MP pass and the gamepad pass. Neither
   can be run autonomously.
3. **Optional, later:** a bespoke tool-menu glyph. `m_sToolMenuIcon` is an `[Attribute]`, so this is a
   config change with no code change, and the feature ships and is play-testable without it (K6).

### Downstream (this feature unblocks)

- **`shared-markers` (feature 8)** will register a third row category. `BuildRows` iterates three sources in
  a loop with a shared row builder; adding a fourth is one more source, not a rewrite. **Do not build for
  it** — no registration API, no plugin interface, nothing speculative.
- **The future intel epic** will build on this toggle machinery. That is exactly why `m_bPlayerVisible` is
  named and documented as a _presentation preference_ rather than as "visibility": the two concepts must
  stay separately named from day one, and nothing here may assume the map always shows everything.

### New and changed files

```
NEW — script (6)
  Scripts/Game/Data/OVT_MapLayerPrefsStore.c                       pure store, Logic-tier pinned
  Scripts/Game/Global/OVT_MapLayerSettings.c                       ModuleGameSettings + entry struct
  Scripts/Game/Global/OVT_MapLayerSettingsAccessor.c               engine-touching load/save/flush
  Scripts/Game/UI/Map/OVT_MapLayersUI.c                            the panel component
  Scripts/Game/UI/Map/Core/OVT_MapLayerRowComponent.c              one row
  Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_MapLayerPrefs.c   2 cases

NEW — content (4)
  UI/Layouts/Map/Core/OVT_MapLayersPanel.layout  (+ .meta)
  UI/Layouts/Map/Core/OVT_MapLayerRow.layout     (+ .meta)

CHANGED
  Scripts/Game/UI/Map/Core/OVT_MapLocationType.c        +3 members (contract extension)
  Scripts/Game/UI/Map/Core/OVT_MapLocationElement.c     +1 gate, +1 method
  Scripts/Game/UI/Map/OVT_OverthrowMapUI.c              +3 methods, 1 generalisation
  Scripts/Game/UI/Map/Visualization/OVT_MapPlayerLocation.c  +3 members (K5)
  Configs/Map/OverthrowMap.conf                         14 × m_sCategoryName
  Configs/Map/MapOverthrow.conf                        1 component entry (+ P1 fallback if needed)
  Language/localization_Overthrow.st                    18 new items (MASTER ONLY)
  docs/features/map/core/context.md                     contract rows + layout table
  docs/features/map/epic-overview.md                    feature 7 row + rollup

NEVER TOUCHED
  Language/localization_Overthrow.<lang>.conf           Workbench-generated; the user regenerates
  Configs/System/chimeraInputCommon.conf                no new binding (K10)
  UI/layouts/Map/MapMenu.layout                         vanilla; resolved by name, not overridden (K9)
```

**GUID allocation:** the `{6A85…}` series — zero uses anywhere in the tree as of 2026-08-11.

---

## 11. Risks & Mitigation

| #       | Risk                                                                                                                          | Likelihood | Impact | Mitigation                                                                                                                                                                                                                                                                                                                                                                      |
| ------- | ----------------------------------------------------------------------------------------------------------------------------- | ---------- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **R1**  | 🔴 **`SCR_MapToolMenuUI` is not actually live on Overthrow's fullscreen map**, and the whole entry-point design collapses     | Low        | High   | Phase 1 P1 is a **zero-code look at the map**, before anything is built on it. Strong prior: vanilla's `SCR_MapCursorModule` is module-list-only and is the sole raiser of the selection invoker Overthrow's play-tested click-to-pin depends on, so the delta demonstrably merges. Fallback is a **one-line same-GUID conf entry**, safe whether the array merges or replaces. |
| **R2**  | 🔴 **The panel is dead on a controller** — focus never lands inside it, or the tool menu's own focus-clearing runs after ours | Medium     | High   | K10 documents the exact insertion-order dependency and the focus-proxy pattern. Phase 1 P4 tests the full round trip **before** the real panel is built. If it fails, the fallback is to hold the cursor module's sub-menu state while the panel is open.                                                                                                                       |
| **R3**  | **A widget name does not match and a row silently does nothing** (BUG-133 / BUG-134's failure class)                          | Medium     | Medium | Every name tabulated in §3.6 before a line is written; every lookup null-guarded and ERROR-logged naming the widget; the inherited `SCR_CheckboxComponent` GUID called out explicitly as copy-not-generate.                                                                                                                                                                     |
| **R4**  | **Preferences do not stick** — applied before the layers registered, or the flush was throttled away                          | Medium     | Medium | K8 applies twice (open + first tick), idempotently, so no subscription-order assumption is load-bearing. K15 flushes at two user-separated points and always writes the whole record. The debug table names the two signatures apart.                                                                                                                                           |
| **R5**  | **`OVT_MapLayersUI` registers its tool-menu entry on every map open** and the strip grows an entry per open                   | Medium     | Medium | K7 — registration is in `Init()`, and the trap is called out at the call site because the _other_ `Init()` in this subsystem (`OVT_MapLocationType.Init()`) has the opposite lifetime. Phase 1's play-test catches it immediately: open and close the map three times and count the entries.                                                                                    |
| **R6**  | **The panel occludes the map or overlaps the journal**                                                                        | Low        | Medium | `m_bIsExclusive 1` (K12) makes the journal/task-list/layers group mutually exclusive, which is what vanilla relies on for the same overlap. Panel authored fixed-width, top-left, hugging the icon strip. F-10 and P3 both check it.                                                                                                                                            |
| **R7**  | **17 rows overflow a short screen**                                                                                           | Medium     | Low    | Scroll container from the start (Phase 4 task 1); Q-2 verifies by pad and wheel.                                                                                                                                                                                                                                                                                                |
| **R8**  | **Toggling fights the refresh tick** — six types re-populate every 2–5 s and could resurrect hidden markers                   | Low        | Medium | The gate is on the **type**, not on the element, so a recreated element reads the same flag. F-7 verifies it directly with a 30 s soak on the 2 s and 5 s types.                                                                                                                                                                                                                |
| **R9**  | **A future class rename loses a preference**                                                                                  | Low        | Low    | Benign and stated (K13): the key stops matching, the type reverts to the default (visible), one preference is forgotten once. The duplicate-key skip filter makes the one non-benign case loud.                                                                                                                                                                                 |
| **R10** | **The gates cannot see 80 % of this feature**, so it ships green and broken                                                   | High       | High   | Accepted and designed around: Phase 1 is a play-tested spike _before_ the build; §9 states plainly what no gate can see; §8's Verification Method is written for an evaluator with no context; the debug-signature table is written before the play-test rather than after it.                                                                                                  |
| **R11** | Scope creep into a colour key, presets, search, or a `shared-markers` registration API                                        | Medium     | Medium | K4 records the colour key as a **user decision to defer**, not an oversight. G8 says the row source stays generic but explicitly forbids building for feature 8. Everything else is a hard non-goal in §2.                                                                                                                                                                      |

### Incidental findings — file as bugs, do not fix here

| #      | Finding                                                                                                                                                                                                                                                                                | Why not here                                                                                                                                                                                                                                                  |
| ------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **F1** | `OVT_MapPlayerLocation` carries a **vestigial `SCR_MapToolEntry m_ToolMenuEntry`** that is never assigned, an empty `Init()`, and a `ZoomInOnPlayer()` with **zero callers** (grep-verified).                                                                                          | K3 — it is dead code in a file this feature already edits, and deleting it here would make a small, reviewable change ambiguous. The user should decide: delete, or wire `ZoomInOnPlayer` to a second tool-menu entry (it looks like it was meant to be one). |
| **F2** | **5 of 14 `m_sDisplayName` values are raw English literals** (`OverthrowMap.conf:6,143,155,166,176`) and render untranslated in the info panel **today**, independently of this feature.                                                                                               | It is a different surface with its own play-test. Fixing it is 5 conf edits + up to 5 `.st` ids, and it is **not** free — it changes the info-panel type line for five types. K2 keeps `m_sDisplayName` untouched deliberately.                               |
| **F3** | `OVT_MapPlayerLocation.Update()` sets `SetOpacity(0)` for a player with no controlled entity and **never restores it**, and `m_Widgets` is not cleared on map close (only at the start of the next successful open, past two early returns), so it can hold refs to destroyed widgets. | Pre-existing, unrelated to filtering, and in a component `legacy-retirement` deliberately retained. Phase 3's edits must not make it worse — `SetMarkersVisible` uses `SetVisible`, not opacity, precisely so the two do not interact.                        |

---

_Plan written 2026-08-11. Phase 0 baselines were **measured**, not quoted. Every vanilla API, line number
and behavioural claim in this document was verified against the actual files in
`/mnt/n/Projects/Arma 4/ArmaReforger` or `/mnt/n/Projects/Arma 4/Overthrow.Arma4`._
