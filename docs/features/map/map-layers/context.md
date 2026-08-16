# Map Layers & Legend - Context & Decisions

**Last Updated:** 2026-08-13
**Current Phase:** Phase 7 discharged 2026-08-13 (SP + gamepad + persistence + 4b probes, all green) — **only the two-client MP rows (I-2 … I-4) remain**
**Status:** 🟢 Built and play-tested green in SP and on gamepad; MP isolation (I-2 … I-4) still unobserved

---

## Quick Status

**What's Done:**

- ✅ **Phase 0 — baselines re-measured, not quoted** (2026-08-11, HEAD `ecf1a696`, tree clean but for this feature's own untracked docs): compile **exit 0 / 5988 files / Game module / 5 s**, Fast **87 / 35 s**, All **125 / 39 s**, highest bug id **BUG-145**, `{6A85…}` GUID series confirmed **zero hits** across the tree. Every one of these matches `implementation.md` §5 Phase 0 exactly — the plan's baselines were real measurements and are still current

- ✅ **Phases 1, 2, 3, 4, 4b, 5 and 6 all DONE** — see the session notes at the foot of this file. Final gate state: compile **exit 0 / 5994 files**, Fast **89**, All **127**
- ✅ **Phase 6 (2026-08-11)** — boundary greps clean and recorded verbatim below; the two `map/core` contract tables and the layout↔code table written; `epic-overview.md` updated; **F1 – F6** written up for filing from **BUG-146** onward *(actually filed 2026-08-13 as BUG-149…155 — 146…148 were taken)*

- ✅ **Phase 7 discharged 2026-08-13 — the user ran the SP, gamepad, persistence and Phase-4b probe passes and reported all green**, including the two load-bearing rows: **P1** (the same-GUID tool-menu delta renders — D2's fallback was the right call) and **Q-1** (the full pad round trip works). Two observations transcribed: **A on a focused row does nothing** (so `MenuLeft` stays load-bearing), and **D-pad Left unticks the focused row AND leaves the panel** — worse than 4b predicted, filed as **F7** in `tasks.md`. F-11 ticking also means the six localization exports were regenerated

**What's Next:**

- 🔴 **The two-client MP rows I-2 … I-4 are the feature's last open evidence** — per-profile preference isolation, JIP, and the all-rows-on parity check. They were explicitly not part of the 2026-08-13 session
- 🔴 **That session gained a row (I-5, 2026-08-16):** two player-reported MP-only map defects were diagnosed and fixed blind — **BUG-176** (clients had NO base markers at all: `OVT_BaseData.entId` is `[NonSerialized]` and its only writer was the server's world query, so `OVT_MapLocationBase.PopulateLocations` skipped every base on a client; entity now optional + entId backfilled in `CheckBaseAndSetFaction`) and **BUG-177** (`RpcDo_RegisterCamp` re-derived the owner from the client's player-id table, racing its replication ⇒ `owner ""` ⇒ the private-camp filter hid the camp even from its owner; the RPC now carries the owner string + `isPrivate`, and the client filter treats unknown identity as "show"). Both fixes are code-complete, gated (compile 0 / 6057, Fast 125, All 166) and **entirely unobserved** — I-5 is their only possible evidence
- ✅ ~~File F1 – F7 as bugs~~ — **filed 2026-08-13 as BUG-149 … BUG-155** (BUG-146…148 were already taken by parallel sessions). F7 = BUG-155 is the only one found by observation rather than code reading; the mapping table is in `findings.md`
- **Retest the marker hover hitbox** — the gamepad pass surfaced hard-to-hover markers; fixed same-day at epic level (see the 2026-08-13 session note), but the fix's second iteration is unobserved
- ✅ **Phase 8 (`help-docs-sync`) is DONE** — and its most valuable output is what it **refused** to write. It cut five claims rather than ship them unverified: the **entire gamepad walkthrough** (the plan's P4 round trip is known-wrong at the D-pad-Left step since D4), the left-stick and character-movement consequences (hand-derived, unobserved), the journal/task-list exclusivity behaviour (unresolved until P1 says whether Overthrow's `m_aUIComponents` merges or replaces), K15's crash-with-panel-open caveat, and §3.3's hidden-is-cheaper property. 3 new `.st` ids (`{6A85D1E000000080}`–`{…0082}`), a Field Manual section, one edited tutorial sentence, and three wiki pages — each resolved **by slug and confirmed with `wikijs_get_page`**, never by search, which did return a wrong pageId exactly as the known hazard predicts
- ✅ ~~The user must regenerate the six localization exports in Workbench~~ — **discharged by 2026-08-13**: F-11 passed (all ids render as English), which is only possible with the exports regenerated

**Blockers:**

- None. **Phase 7 is user-driven by design** and is not a blocker on the build.

---

## Key Files

### Core Implementation (new)

- `Scripts/Game/UI/Map/OVT_MapLayersUI.c` — the panel component; owns the tool-menu entry, the dock parent, the store and the three-source row build
- `Scripts/Game/UI/Map/Core/OVT_MapLayerRowComponent.c` — one row (icon + label + checkbox); **never mutates state itself**
- `Scripts/Game/Data/OVT_MapLayerPrefsStore.c` — pure hidden-key set, no engine call; Logic-tier pinned
- `Scripts/Game/Global/OVT_MapLayerSettings.c` — `ModuleGameSettings` subclass + `OVT_MapHiddenLayerEntry`
- `Scripts/Game/Global/OVT_MapLayerSettingsAccessor.c` — the only engine-touching persistence code
- `UI/Layouts/Map/Core/OVT_MapLayersPanel.layout` · `OVT_MapLayerRow.layout` (+ `.meta`)

### Changed

- `Scripts/Game/UI/Map/Core/OVT_MapLocationType.c` — +3 members (published contract extension)
- `Scripts/Game/UI/Map/Core/OVT_MapLocationElement.c` — the fourth visibility gate + `RefreshVisibility()`
- `Scripts/Game/UI/Map/OVT_OverthrowMapUI.c` — type accessor, refresh sweep, unpin guard generalisation
- `Scripts/Game/UI/Map/Visualization/OVT_MapPlayerLocation.c` — the one hand-built row's toggle (K5)
- `Configs/Map/OverthrowMap.conf` (14 × `m_sCategoryName`) · `Configs/Map/MapOverthrow.conf` (1 entry)
- `Language/localization_Overthrow.st` — **21** new ids (18 Phase 3 + 3 Phase 8), **master only**
- `Configs/FieldManual/Categories/FM_Overthrow.conf` — new "Filtering the Map" section (Phase 8)

### Never touched

- `Language/localization_Overthrow.<lang>.conf` — Workbench-generated; the user regenerates
- `UI/layouts/Map/MapMenu.layout` — vanilla; `ToolFramesOverlay` is resolved **by name**, not overridden (K9)

### ⚠️ Touched after all — `Configs/System/chimeraInputCommon.conf`

This file was listed as **never touched** when the plan was written (K10/Q-3). **Phase 4b changed that** by
user decision **D4**: one new `ActionContext OverthrowMapLayersContext` (+11 lines). **No new key or pad
button is consumed** — it only re-references `Menu*` actions that already exist, which is what Q-3 was
actually protecting. The stale "never touched" line is corrected here rather than left to mislead.

---

## Important Decisions

### D1: Phase 1's play-tested spike is folded into the Phase 7 gate

**Date:** 2026-08-11
**Context:** `implementation.md` shapes Phase 1 as a deliberately throwaway spike **observed before anything
depends on it** — four questions (P1 – P4) that cannot be answered by reading. This run is autonomous:
nobody is at the screen between phases, and this project has no browser/MCP path to a running Arma session.
**Decision:** Build Phase 1's deliverables in their **final** widget-name shape (which the plan already
asks for) and carry **P1 – P4 verbatim into the Phase 7 gate**, plus an R5 row (open and close the map three
times, count the tool-menu entries).
**Rationale:** The spike's code deliverables are a strict subset of Phase 4's, so a stub-then-rewrite buys
nothing when no observation happens in between. Both spike risks have documented, cheap fallbacks — R1's is
a one-line same-GUID conf entry, R2's is holding the cursor module's sub-menu state — so a failed answer
costs rework, not a redesign from zero. This also matches how every other feature in this epic actually
shipped: `respawn` built eight phases and then gated once.
**Impact:** Phase 7 is the only evidence for the entry point, the dock position and all gamepad behaviour —
which was already true of most of this feature (§9 of the plan).

### D2: 🔴 R1 was real — vanilla's tool menu was **not** reaching Overthrow's map, and the fallback was needed

**Date:** 2026-08-11 (Phase 1)
**Context:** The plan rated R1 (_"`SCR_MapToolMenuUI` is not actually live on Overthrow's fullscreen map,
and the whole entry-point design collapses"_) as **Low** likelihood, on the strong prior that vanilla's
`SCR_MapCursorModule` demonstrably merges into Overthrow's delta.
**Finding:** That prior held for `m_aModules` and **did not transfer to `m_aUIComponents`**. Vanilla's
`Configs/Map/MapOverthrow.conf` lists `SCR_MapToolMenuUI "{599C7D68E8F6B9A8}"`; Overthrow's same-GUID
delta carried **only** `OVT_MapPlayerLocation` and `OVT_OverthrowMapUI`. There was no tool-menu entry.
**Decision:** Added the vanilla entry as a delta on that GUID, placed **first** in the array — its
constructor is the only thing that assigns the static `SCR_MapToolMenuUI.s_sToolMenuIcons` that our
`RegisterToolMenuEntry` call reads, so it must be constructed before ours. Safe whether the array merges
or replaces: if it merges, this is a delta on vanilla's entry; if it replaces, it is the entry.
**Impact:** The plan's one-line fallback was exercised on its first day. **This does not yet prove the strip
renders** — §3.5's evidence was about `m_aModules`, and the `m_aUIComponents` merge is still unobserved.
**P1 is now a load-bearing play-test question, not a formality**, and it is the first row of the Phase 7
gate. If the strip is still absent, the next thing to check is whether Overthrow's `m_aUIComponents`
_replaces_ rather than merges — in which case every vanilla UI component the map needs must be listed.

### D3: `m_bIsExclusive` needed no new field, and `FocusProxy`'s shape came from vanilla

**Date:** 2026-08-11 (Phase 1)
**Context:** The plan specified both from first principles.
**Finding:** `m_bIsExclusive` is **already** `protected bool` on `SCR_MapUIBaseComponent`, fed by the conf
attribute, so `m_bIsExclusive 1` flows straight into `RegisterToolMenuEntry` with nothing redeclared. And
vanilla authors its journal focus button (`SCR_JournalWidgets.m_wFocusButton`) as a **full-stretch**
`ButtonWidgetClass` with `Opacity 0`, `style blank` and `SCR_EventHandlerComponent`, placed as the **first**
overlay child so content renders on top and mouse clicks still reach the rows — not the small corner button
a first reading suggests.
**Impact:** Both copied from the vanilla files rather than invented. Recorded because "the plan said X and
vanilla already did X better" is the cheap half of this epic's `FindAnyWidget` lesson.

### D4: 🔴 The plan's gamepad story was half right — a dedicated input context is being built (**USER DECISION**)

**Date:** 2026-08-11 (after Phase 4)
**Context:** Settled decision **D1** held that a vanilla tool-menu entry _"closes the gamepad requirement for
free"_, because `MapToolMenuFocus` (D-pad Left) is already bound. Phase 4 found that this covers **reaching
and opening** the panel and says nothing about **walking the rows inside it** — a different input surface.
**Finding (verified independently, not taken on the agent's word):** `ActionContext MapContext` declares 28
inline actions plus 10 `ActionRefs` and **not one is a `Menu*` navigation action**; `chimeraMenus.conf`
gives `MenuPreset MapMenu` exactly one `ActionContext "MapContext"`. So D-pad Up/Down/Left/Right almost
certainly does not drive rows in a panel docked on the map.
**Corroboration that this is a real pattern rather than an oversight:** vanilla hit it **twice** and solved
it identically both times — `TaskListMapContext` (priority 70, above `MapContext`'s 50) re-activated **every
frame** by `SCR_UITaskManagerComponent` while the task list is up, and `MapMarkerEditContext` likewise by
`SCR_MapMarkersUI`. Notably `SCR_MapJournalUI` does **not** do this, which suggests vanilla's own journal
list is cursor-only on the map today. Overthrow already records the consequence in `OVT_OverthrowMapUI`'s
R6/N12 comment: _"on a controller pressing MapSelect over the toggle is its ONLY input path."_
**Decision:** The user chose to **build the fix now** rather than wait for the play-test — a new Overthrow
`ActionContext` re-referencing the existing `Menu*` actions, activated per frame **only while the panel is
open**. Phase **4b**.
**Why this does not violate what Q-3 was protecting:** Q-3 says the `chimeraInputCommon.conf` diff must be
empty, and this breaks that **letter**. Its _reason_ — stated at K10 — is that `MapContext` already carries
41 live actions, `KC_H` is taken three times over, and the repo's conflict checker is blind to the base
game's 197 inline `ActionContext` actions, so **finding a free key is the risky part**. No new key or pad
button is consumed here; only actions that already exist are re-referenced. The reason survives intact and
the deviation is recorded rather than quietly absorbed.
**Impact:** This is the requirement most at risk in the whole feature — `requirements.md` says a filter
panel that only works with a mouse is **worse than no filter panel**. **Q-1 in Phase 7 is still the only
evidence it works**, because `.conf` edits are invisible to all three automated gates.

---

## Gotchas & Learnings

Carried in from the plan's §6 so they are visible at the point of work rather than only in the plan. These
are **not** discoveries from this session — they are inherited constraints, each already verified against
the vanilla tree.

### 1. The two `Init()`s in this subsystem have **opposite lifetimes**

`SCR_MapUIBaseComponent.Init()` runs **once per map-config load**; `OVT_MapLocationType.Init()` runs on
**every map open**. `SCR_MapToolMenuUI.m_aMenuEntries` is only ever inserted into — there is no unregister
API anywhere in the class, and `OnMapClose` does not clear it. So registering the tool-menu entry in
`OnMapOpen` **duplicates it on every single map open, forever, within one session** (K7/R5).
**Lesson:** a developer who has internalised the location-type rule will put it in the wrong place. The
trap is commented at the call site.

### 2. `SetActive(false)` is **one-way from script**

`SCR_MapModuleBase.SetActive(false)` calls `m_MapEntity.DeactivateModule(this)`, and both `ActivateModules`
and `m_aActiveModules` are `protected` on `SCR_MapEntity`. A toggle built on it turns a layer off
**permanently for the session**. The toggle primitive is `SetLayerVisible` — which also happens to be what
"cheap and immediate" requires (D5).

### 3. `SaveUserSettings()` is **throttled by the engine**

Measured 2026-08-07 by `new-player-experience`: two calls microseconds apart leave only the **first** on
disk — the second is _dropped_, not deferred — while two calls six seconds apart both land. A filter panel
is a **burst** surface (five rows flipped in three seconds), so the tutorial store's "flush on every
mutation" does **not** transfer. Flush at panel close and map close only, always writing the whole record
(K15).

### 4. An inherited layout's component GUID must be **copied, not generated**

`RowCheckbox` inherits `WLib_Checkbox.layout`, so its `SCR_CheckboxComponent` override must reuse
`{546A9B7B0A8AD927}`. A fresh GUID adds a second, unconfigured component and the checkbox goes dead — the
single most common layout bug in this codebase.

### 5. `FindAnyWidget` returning null is a **silent no-op the compiler cannot see**

This epic lost two configured features to exactly that (BUG-133 `IconLayout`, BUG-134 `CloseButton`). Every
name in §3.6 of the plan is tabulated **before** a line is written, and every lookup is null-guarded and
ERROR-logs the widget it could not find.

### 6. `SCR_MapToolEntry.SetEnabled` is **cosmetic only**

It sets the border colour; nothing consults `m_bIsEnabled` to block a click. Every vanilla caller calls
`SetEnabled(true)` immediately after registering purely to get the orange border. Do not mistake it for a
gate. Likewise `SCR_MapToolEntry.GetImageSet()` returns `m_sIconQuad` — a vanilla bug; do not use it.

---

## Testing Approach

**Automated (regression guards, plus two genuine assertions):**
`tools/compile-check.sh`, Fast `{6A6E29FF47ECB840}`, All `{6A6E2A002F53A581}` at **every** phase boundary.
The only new assertions are the two Logic cases over `OVT_MapLayerPrefsStore` — the one part of this feature
that makes no engine call, touches no widget and knows nothing about `BaseContainer`. **Both must be proven
able to fail**, each inversion applied alone to a pristine copy, with the exact failure message recorded
below. ❌ No `maxAttempts`.

**What no automated gate can see** (§9 of the plan, restated because it drives Phase 7):
the tool-menu entry existing/rendering/being clickable; the panel docking, its position, size, occlusion or
scrolling; **every widget name in §3.6**; **all gamepad behaviour**, including K10's focus grab — the single
most likely thing to ship looking correct and being dead on a controller; whether a toggle visibly changes
the map and how fast; the `ModuleGameSettings` round trip and the engine's flush throttle; localization
rendering; MP isolation between two clients.

### Inversion log (Phase 2 — complete)

Each inversion was applied **alone** to a pristine copy of `Scripts/Game/Data/OVT_MapLayerPrefsStore.c`,
compiled (exit 0 both times — the inversions are semantic, not syntactic), run as **that one case**, then
reverted and the full clean gate re-run.

| Case                                         | Inversion                                                                                                                              | Result                               | Exact failure message                                                                                                                            |
| -------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `OVT_TEST_Logic_MapLayerPrefs_HiddenSet`     | `IsVisible` returns `m_aHidden.Contains(key)` instead of `!…` — every type ships hidden                                                | `run-tests: FAILED (1 of 1)`, exit 1 | `A key that was never hidden reported as hidden; absent must mean visible, or every marker vanishes for a player who has never opened the panel` |
| `OVT_TEST_Logic_MapLayerPrefs_KeyNamespaces` | `LayerKey` returns `layerId` instead of `LAYER_PREFIX + layerId` — a layer and a type class of the same bare name share one preference | `run-tests: FAILED (1 of 1)`, exit 1 | `LayerKey('territory') returned the bare name; a preference key must be namespaced or a layer id and a class name can address the same record`   |

**A third, unplanned red was produced by the first clean run and is worth recording**, because it is the
case earning its keep before it was ever inverted: `_HiddenSet` failed on
`A prefix of a hidden key reported as hidden; the lookup must be exact`. The store was correct; **the
assertion was inverted**. `IsVisible` has the **opposite polarity** to `OVT_TutorialSeenStore.HasSeen`, and
two negative lookups copied from the tutorial case's shape (`if (store.HasSeen(nearMiss))`) had to become
`if (!store.IsVisible(nearMiss))`. Anything porting further assertions from that precedent must flip every
lookup — a copied `if (store.IsVisible(x))` asserts the exact opposite of what it reads like.

---

## Play-test observations

_Record the observations that **passed** as well as the ones that failed — a play-test report is this
feature's only evidence for most of §7._

### 2026-08-11 — first sighting (user, partial)

**Report:** _"the labels aren't lining up with the switches, the labels are all too low."_

🎉 **This is the feature's first observed evidence, and most of it is good news carried implicitly.** For a
misalignment to be visible at all, the following must already be true — so these rows move from _unproven_
to _observed_:

- **P1 / F-1 — the tool-menu strip exists on Overthrow's map and carries our entry.** This was the single
  biggest open risk (**D2**: vanilla's `SCR_MapToolMenuUI` was genuinely absent from
  `m_aUIComponents` and had to be added back blind). **The `m_aUIComponents` delta merges.**
- **F-2 — the entry opens the panel**, and **K9's dock resolution found a real parent** (no
  `ToolFramesOverlay` ERROR was reported).
- **F-3 / F-4 (partial) — rows are being built with both labels and switches.** So `BuildRows` runs, the
  three row sources enumerate, `CreateWidgets` on the row layout succeeds, and **`RowLabel` and
  `RowCheckbox` both resolve by name** — meaning the `FindAnyWidget` failure class (BUG-133 / BUG-134),
  which was risk **R3** and the reason §3.6 exists, **did not bite**.

**Cause — a real defect, and the sibling layout is why it slipped through.** `RowIcon`, `RowLabel` and
`RowCheckbox` were authored with `VerticalAlign 2` on their `LayoutSlot`s, copied from the known-good
`OVT_MapInfoRow.layout`, which uses the same value. In the Enfusion slot alignment enum (`0` top, `1`
center, `2` bottom, `3` stretch — `3` independently confirmed by this feature's own full-stretch
`RowHighlight` and `FocusProxy`), **`2` is bottom, not centre.**

It is harmless in `OVT_MapInfoRow` because every child there is roughly one text-line tall, so bottom-aligning
them is indistinguishable from centring them. It is **not** harmless here: `m_fSizeWithoutLabel 30` makes
`RowCheckbox` a **30 px** widget that sets the row height, while `RowLabel` is 11 pt text. Bottom-aligned,
the text sinks to the floor of the row while the switch graphic sits centred inside its own 30 px box —
exactly the reported gap.

**Why vanilla never hits this:** vanilla's own checkbox rows (`BasesMapSettings.layout`) set
`m_bUseLabel 1` and let `SCR_CheckboxComponent` render the label **inside** the component, which centres it
for them. This feature deliberately set `m_bUseLabel 0` to fit an icon and a plural localized category name
alongside the switch — **so it owns the vertical alignment, and vanilla offers no precedent to copy.**

**Fix:** `VerticalAlign 2` → `VerticalAlign 1` on all three children. The two genuinely full-stretch slots
(`RowHighlight`, `RowContent`) keep `3`.

⚠️ **`compile-check.sh` and both test groups cannot see this change** — it is a `.layout` edit. Re-open the
map to confirm.

**Lesson worth carrying:** _"copied from the known-good sibling"_ is weaker evidence than it feels. The
sibling was correct **for its own child heights**, and the value silently became wrong the moment a 30 px
widget joined the row. Alignment values are only as valid as the tallest sibling they sit beside.

---

## Session Notes

### 2026-08-11 — `/autorun-feature map/map-layers`

- Re-measured all three gates rather than quoting them: **5988 / 87 / 125**, identical to the plan's
  Phase 0. `git status` clean at `ecf1a696` apart from this feature's own docs; highest bug id **BUG-145**;
  `{6A85…}` still free.
- Scaffolded `tasks.md` (87 rows across 9 phases) and this file. Followed the epic's sibling shape rather
  than `.claude/templates/`, whose sections (Zustand stores, WebSocket events, `*.test.ts`) describe a web
  app and not this project.
- Recorded **D1** — Phase 1's spike questions move to the Phase 7 gate.

**Phase 1 — entry point + layout skeletons — DONE.** Gate re-verified by the orchestrator, not just
reported: **compile exit 0 / 5989 files** (5988 + 1, exactly the predicted `.c`).

- 🔴 **R1 fired on day one** — see **D2**. Vanilla's tool menu was genuinely absent from Overthrow's
  `m_aUIComponents`, so the plan's "fallback" was the actual path, and **P1 is now a load-bearing play-test
  question rather than a formality.**
- Registration sits in `Init()` behind an `m_bEntryRegistered` guard with the K7 trap named at the call
  site. The agent confirmed the trap is **stronger** than the plan stated: `SCR_MapEntity.ActivateComponents`
  calls `Init()` only for components inserted during `m_bDoReload` (once per config load), while
  `OnMapOpen` re-runs `PopulateToolMenu()` unconditionally.
- **No deviations from the §3.6 widget-name table.** Three structural names were added that §3.6 does not
  mention and script never resolves: `PanelOverlay`, `PanelBackground`, `PanelContent` (plus the row root
  `LayerRow`).
- `RowCheckbox` inherits `{5D5055E10FD00549}…/WLib_Checkbox.layout` and reuses the base
  `SCR_CheckboxComponent` GUID `{546A9B7B0A8AD927}` — verified against the base layout **and** cross-checked
  against vanilla's own two instantiations in `UI/layouts/Map/BasesMapSettings.layout`.
- Both `.meta` files carry all six platform configurations, copied from `OVT_MapInfoRow.layout.meta`.

**GUID ledger — the `{6A85D1E0…}` block** (kept here so it stays correct as phases allocate more):

| Range                 | Phase | Allocated to                                                                             |
| --------------------- | ----- | ---------------------------------------------------------------------------------------- |
| `{…0001}`             | 1     | the `OVT_MapLayersUI` entry in `MapOverthrow.conf`                                       |
| `{…0010}` – `{…0024}` | 1     | `OVT_MapLayersPanel.layout` resource, its widgets, slots, `FocusProxy` and event handler |
| `{…0030}` – `{…0037}` | 1     | `OVT_MapLayerRow.layout` resource, its widgets and slots                                 |
| `{…0038}` – `{…003F}` | —     | **permanent gap** (Phase 3 was told to start at `…0040`)                                 |
| `{…0040}` – `{…0051}` | 3     | the 18 new `.st` string items                                                            |
| `{…0060}` –           | 4     | the panel's final content (see the Phase 4 note)                                         |

**Phase 3 — contract extension, hot-path gate, 18 string ids — DONE.** Gates **unchanged** exactly as
predicted: compile **exit 0 / 5993** (no new `.c`), Fast **89**, All **127**.

- The gate sits immediately after the `!m_LocationType` guard and above everything else. Both things below
  it are **per-record** work — `GetEffectiveVisibilityZoom()` is a `GetDataFloat` lookup plus a compare, and
  `ShouldShowLocation` is a virtual whose overrides reach into live managers — while `IsPlayerVisible()` is
  a **per-type constant**. Answering the cheap constant first is what makes **Q-4 ("hidden is cheaper") a
  structural property of the ordering rather than an optimisation anyone has to remember.**
- `m_sName` and `m_sDisplayName` proven untouched by `git diff`: the only diff lines mentioning them are
  _added comment prose_ explaining why there are now three name fields. The vestigial `m_ToolMenuEntry` /
  empty `Init()` / zero-caller `ZoomInOnPlayer()` are bit-identical — **F1 is preserved intact**.
- The 14 English category strings were **transcribed from `OverthrowMap.conf`, not invented**, anchored on
  each block's `m_fVisibilityZoom` to avoid the Shop block's five _nested_ `OVT_ShopTypeInfo.m_sDisplayName`
  values. `.st` verified: 690 → 708 items, zero duplicate `Id`s, brace depth balanced, all 14
  `#OVT-Map_Category_*` keys used in the conf resolve to an `Id` in the master.
- 🔴 **A hazard the plan did not anticipate, fixed inside the class.** `m_bMarkersVisible` is component
  state that survives a map close, but `OnMapOpen` builds a **fresh, visible** widget set — and with
  `Update()` early-returning while hidden, nothing would ever reposition them. A player who hid the player
  markers, closed the map and reopened it would have seen a new set of visible markers parked wherever an
  unpositioned `FrameSlot` widget lands. One `SetMarkersVisible(m_bMarkersVisible)` after the creation loop
  closes it. Phase 5's `ApplyPreferences()` would have masked this **eventually** — but not before at least
  one frame, and not at all if its ordering assumption ever failed.
- `OnMapOpen` has a **third** early return the plan does not enumerate (`if (!fac) return;` after resolving
  the player faction), which also produces a session with zero markers. `m_bAvailableThisSession` is set
  past **all three**, which serves the stated intent — no dead toggles — strictly better than the literal
  "past both".
- **Honest caveat, recorded rather than smoothed over:** the claim "the map is byte-for-byte unchanged" is
  now _almost_ true. Every new symbol has zero callers except the gate (which never fires, since nothing
  writes `m_bPlayerVisible` yet) — but `OnMapSelection` also tests the layers panel rect, and Phase 1 did
  register a working tool-menu entry. So a player who opens the stub panel and clicks inside it will no
  longer unpin a pinned info panel. That is K14 arriving a phase early, not a regression.
- Out of scope and worth stating so nobody later reads "all 14 entries" as "all entries in the tree":
  `Configs/Map/OverthrowMapRespawn.conf` instantiates **four more** `OVT_MapLocationType` subclasses with no
  `m_sCategoryName`. Correct — that config carries no `OVT_MapLayersUI`, so `GetCategoryName()` is never
  called there and the warn-once can never fire.

**Phase 4 — the panel — DONE.** Gates: compile **exit 0 / 5994**, Fast **89**, All **127**; input-conflict
checker **exit 0** and `git diff Configs/System/chimeraInputCommon.conf` **empty** at that point.
GUIDs `{…0060}`–`{…006A}`; **next free `{6A85D1E00000006B}`**. `RowCheckbox` still carries the inherited
`SCR_CheckboxComponent` GUID `{546A9B7B0A8AD927}`, verified against the base layout _and_ against vanilla's
own two instantiations in `BasesMapSettings.layout`.

- 🔴 **`SCR_CheckboxComponent.SetChecked` raises `m_OnChanged`** (via `SetCurrentItem` → `SetToggled` →
  `OnElementChanged`). So a row **seeds its value before subscribing**. Wiring first would have reported a
  player-driven toggle for every row that starts hidden — and Phase 5's store would then have switched the
  map back on the instant the panel opened. This is the kind of defect that looks like "persistence doesn't
  work" and is actually an event-ordering bug one layer down.
- **Focus visual is layered twice on purpose**, so one failing still leaves the other: vanilla's own
  `SCR_ButtonEffectColor` line highlight on `WLib_Checkbox` (free, nothing written to get it), plus a
  row-wide `RowHighlight` wash driven from the row component's focus/mouse events. It works because every
  widget-library handler in the chain returns `false` from those events, so a focus change on the child
  checkbox propagates to the row root.
- Two safety measures beyond the plan, both aimed at the pad: a **next-frame focus retry** that refuses to
  act if anything already holds focus (so it can only fill a gap, never steal one), and
  `ReleaseFocusInsidePanel` before every teardown — a focused widget that is then destroyed strands the pad
  on nothing _and_ robs `SCR_ToolboxComponent` of the `OnFocusLost` it uses to remove the listeners it
  registers on focus, leaking a pair per row per rebuild.
- **`HandlerDeattached` is unreachable for this component** — `SCR_MapUIBaseComponent` only adds itself as a
  widget handler when `m_bHookToRoot` is true, and it is false here. Vanilla's `SCR_MapJournalUI` has the
  same dead teardown. All real teardown lives in `OnMapClose`.
- **Deliberate deviation from the plan's "shop menu opacity"**: `ShopMenu.layout`'s background is 50 % black
  _over a full-screen blur_. This panel sits over bare terrain with no blur, so 11 pt labels at that value
  would be marginal — which is the very thing the instruction was protecting. Matched
  `OVT_MapInfoPanel.layout`'s `InfoBackground` instead: the existing Overthrow panel that already floats
  over this exact map.
- **Two things to watch at the play-test that no gate can see:** whether the `Fill`-inside-`SizeLayout`
  scroll idiom collapses to zero height (a `MinDesiredHeight 320` floor is in place as insurance), and
  whether the mouse wheel over the panel scrolls the list _and_ zooms the map — `MapWheelUp`/`MapWheelDown`
  are bound at the action level and a widget handler cannot swallow an action.

**Phase 5 — persistence wiring — DONE.** Gates: compile **exit 0 / 5994**, Fast **89**, All **127**. One
file touched.

- `ApplyPreferences()` runs from `OnMapOpen` (before `BuildPanel`, the no-flash path) and again from the
  first `Update` tick, behind a one-shot **armed in `OnMapOpen` and disarmed in two places** — the tick that
  consumes it, **and `OnMapClose`**. That second reset is not tidiness: a map closed before its first tick
  would otherwise fire the armed application into the _next_ session, against a layer list already rebuilt
  from scratch.
- **`FlushPrefs()` has exactly two call sites** — `OnMapClose`, and the `if (!visible)` branch of
  `SetPanelVisible`, which is the single funnel for both close routes (`TogglePanel` closing and
  `ClosePanel` firing off the disable-map-UI invoker). **`OnRowToggled` contains no flush at all**, and the
  reasoning is commented there so a future reader does not "fix" it.
- A **dirty guard** on `FlushPrefs` earns its keep: `ClosePanel` fires from the invoker whenever _any_ other
  exclusive tool opens, panel open or not — without it, opening the journal twice would spend the engine's
  throttle window on two no-change writes. The flag clears **only on a successful save**, so a session with
  no writable profile re-offers the write at the next flush point rather than silently forgetting it.
- The `IsAvailableThisSession()` hazard is handled with the **same guard in the same position** that
  `BuildPlayerRow` uses, so an unavailable session gets neither a row nor a `SetMarkersVisible` write — which
  matters because `m_bMarkersVisible` survives a map close and would otherwise leak into a later session
  that _does_ draw markers.
- **Two places the plan's §3.2 diagram is now the odd one out**, both unobservable: it shows load+apply as
  the whole of `OnMapOpen` (Phase 1/4 had already put `BuildPanel` there), and it shows the toggle path as
  store-then-apply where §5 and the shipped code do **apply-then-store**. Apply-first is strictly better
  under a failure inside the store.

**Phase 4b — gamepad row navigation (D4) — DONE.** Gates: compile **exit 0 / 5994**, Fast **89**, All
**127**, conflict checker **exit 0**. `OverthrowMapLayersContext`, **priority 70, `Flags 0x26 0`** — both
copied verbatim from vanilla's `TaskListMapContext`, not invented (there is no script-side enum for the flag
word anywhere in the reference tree, so it is opaque). Re-references `MenuUp`/`MenuDown`/`MenuLeft`/
`MenuRight`/`MenuSelect`, **all five of which already existed** in the base game's top-level `Actions`
block. `git diff -U0 | grep -E '^\+.*(Input |InputSource)'` returns **nothing** — zero new keys, zero new
pad buttons, exactly as D4 requires.

- ✅ **The checker was proven able to see the new context before its clean exit was believed.** A copy of
  the conf in the scratchpad had a deliberate collision added (`MenuSelectDouble` alongside `MenuSelect`)
  and the checker reported 3 errors, exit 1. **Zero findings from an instrument that cannot see you is not
  a pass** — the real conf was never modified for the probe.
- **Activation is a single-frame lease.** `ActivateContext`'s default duration is 0, so _not calling it_ is
  the entire teardown: a closed panel, a map close and a destroyed component all drop the context within one
  frame with no special case and nothing to remove in `OnMapClose`.
- **The one-frame grace delay is copied from `SCR_MapMarkersUI`**, whose own field carries the comment that
  it exists so the dialog "doesn't trigger inputs used to open it". It matters here for exactly the same
  reason: the panel is opened by **A**, and this context puts `MenuSelect` on **A**.

🔴 **Three consequences of D4 that are real costs, hand-derived and untested.** They are written here rather
than discovered at the play-test:

1. **`pad_left` is shadowed.** `MapToolMenuFocus` is the only pad route to the tool strip, so **D-pad Left
   will untick the focused row rather than return to the strip** while the panel is open. `MenuLeft` is
   included anyway because `SCR_ToolboxComponent` registers listeners for `MenuLeft`/`MenuRight` only, and
   with `m_bCycleMode` false, stepping YES→NO is `MenuLeft` **alone** — **without it a pad could turn layers
   on but never off.** Vanilla accepts the identical shadow in `MapMarkerEditContext`. The escape hatch
   survives: `MenuBack` was deliberately **not** claimed, so `MapEscape` (`gamepad0:b`, priority 55, renewed
   every frame by the gadget manager) still closes the map.
2. **The left stick stops panning the map** while the panel is open and walks/toggles rows instead. This
   **directly contradicts the plan's P4 expectation**, which is corrected in `tasks.md` rather than left to
   mislead the play-tester.
3. **Character movement freezes** while the panel is open — `CharacterMovementContext` is priority 10.
   Consistent with all 20 other Overthrow menus, but **new for the map**.

**The context activates on panel visibility, not on focus** (mirroring both vanilla precedents), but
**nothing consumes the actions unless focus is inside the panel**, because `SCR_ToolboxComponent` registers
its listeners in `OnFocus`. So K10's focus grab is load-bearing for D4 as well as for itself. The failure
mode to watch is **panel open, focus lost, pad stranded** — the context stays live while nothing listens.
One-line fix if seen: also require the focused widget to be inside the panel before activating.

**Phase 6 — boundary audit, contract records, docs — DONE.** Gates re-run and **all three unchanged**, which
is the whole acceptance criterion for a docs-and-audit phase: compile **exit 0 / 5994 files / Game module /
5 s**, Fast **OK, 89 tests, 34 s**, All **OK, 127 tests, 39 s**.

**The changed set was derived from `git status` + `git diff HEAD`, not taken from the phase brief — and the
two agree exactly** (6 new `.c`, 2 new `.layout` + 2 `.meta`, 4 changed `.c`, 3 changed `.conf`, 1 changed
`.st`; plus this feature's own three untracked `docs/` files, which are not code). `git diff HEAD --stat`:
644 insertions / 14 deletions across the eight tracked-and-changed files.

### I-4 boundary greps — actual output, over all 18 new and changed files

```
=== grep -n "\[RplProp"  ===   (no output, exit 1)
=== grep -n "\[RplRpc"   ===   (no output, exit 1)
=== grep -n "Rpc("       ===   (no output, exit 1)
=== grep -n "EPF_"       ===   (no output, exit 1)
=== grep -n "PlayerComms" ===  4 hits, ALL in Language/localization_Overthrow.st, all inside
                               pre-existing Field Manual translator Comment prose (fact-check
                               citations for the Capturing Bases, Recruits, Skills and Your Home
                               pages). Confirmed not ours:
                                 git diff HEAD -- Language/localization_Overthrow.st \
                                   | grep '^+' | grep -c "PlayerComms"   ->  0
=== git status --porcelain -- Scripts/Game/Components/OVT_PlayerCommsComponent.c ===
                               (no output — the file is untouched at HEAD)
=== grep -n "Replication\." over the 5 new .c files ===   (no output, exit 1)
=== grep -n "OVT_Global\." over the 5 new .c files   ===   (no output, exit 1)
```

**No write to any campaign record.** Every mutating call on every added line across the four changed `.c`
files, extracted mechanically from the diff:

```
git diff HEAD -- 'Scripts/*.c' | grep '^+' | grep -oE '\b(Set|Add|Remove|Write|Insert)[A-Za-z]*\(' | sort | uniq -c
      4 SetVisible(
      2 SetMarkersVisible(
      1 SetPlayerVisible(
```

Three client-side booleans on three client-side UI objects, and nothing else. The same grep for
`Manager|OVT_Global|Save|Persist|EPF|Rpc` over added lines returns **nothing**.

**File placement — all 10 new files are inside the sanctioned trees**, no exceptions:
`Scripts/Game/Data/` (1), `Scripts/Game/Global/` (2), `Scripts/Game/UI/Map/` + `UI/Map/Core/` (2),
`Scripts/Game/Tests/TestSuites/Logic/` (1), `UI/Layouts/Map/Core/` (2 `.layout` + 2 `.meta`).

**K-9 check clean.** `git diff HEAD -- '*.c' | grep -E '^\+.*\.c:[0-9]+'` → nothing, and a direct grep for
`.c:N` / `.conf:N` / `.layout:N` across all six new `.c` files → nothing. **No `file:line` pointer was
introduced into any shipped code comment by this feature.**

### What was written where

- `docs/features/map/core/context.md` — the **three new `OVT_MapLocationType` contract rows**
  (`m_sCategoryName`, `GetCategoryName()`, `m_bPlayerVisible`/`SetPlayerVisible`/`IsPlayerVisible`) with the
  🔴 _this is a presentation preference, not campaign visibility — the intel epic must never share this
  field_ note preserved; the **"no rows added to the canvas-layer contract"** note and the
  **`OVT_MapPlayerLocation` exception (K5)** beneath the `OVT_MapCanvasLayer` table; and the **completed
  layout ↔ code name table** (13 resolved names + 7 structural names script never resolves).
- `docs/features/map/epic-overview.md` — feature 7's row, the feature-8-is-unblocked note, a new rollup
  bullet, the header status line, the one-line master summary, and a sentence on the `file:line` debt item.
- The findings (**F1 – F6**) are written up in full for filing as **BUG-146** onward. *(Actually filed 2026-08-13, with F7, as BUG-149…155.)*

### The layout ↔ code table was verified against source, not transcribed — and it moved

Every row was re-derived from the `Name "..."` lines in the two `.layout` files and the `WIDGET_*` constants
in `OVT_MapLayersUI` / `OVT_MapLayerRowComponent`. **All 13 resolved names matched.** Three corrections to
what the docs had implied:

1. **The structural-name list was incomplete.** Phase 1's note recorded three (`PanelOverlay`,
   `PanelBackground`, `PanelContent`); the shipped layouts carry **seven** — Phase 4's final content added
   `TitleStripe`, `RowsScroll`, `RowsContent` (the scroll container) and `RowContent`. All are unresolved by
   script and are now listed as such.
2. **`RowHighlight` is in the shipped row layout and is resolved by `OVT_MapLayerRowComponent.Init`** — it
   is **not** in the plan's §3.6 table, having been added in Phase 4 as the second of two independently
   sufficient focus visuals. It is now a first-class row.
3. **`FocusProxy` is read by `OnPanelBuilt`**, as planned — but it is authored as a _full-stretch_ opacity-0
   blank `ButtonWidget` placed **first** in the overlay (vanilla's journal shape, D3), not the corner button
   §3.6 implied. Recorded in the table so nobody "fixes" its size.

### Two small drifts found while auditing, neither worth a finding of its own

- **F2's line numbers in `tasks.md` are already stale.** It cites `OverthrowMap.conf:6,143,155,166,176`; the
  five raw literals now sit at `:6,153,166,178,189`, shifted by Phase 3's own 14 `m_sCategoryName` additions.
  The _symbols_ (Town / Bus Stop / Vehicle / Point of Interest / Waypoint) are unchanged. **This is K-9
  demonstrating itself inside a single feature, in under a day** — the findings write-up names symbols only.
- **`ZoomInOnPlayer` has exactly one tree-wide hit, its own declaration** — F1's "zero callers" claim
  re-verified rather than inherited.

---

## Session Note — 2026-08-13 — Phase 7 discharged (MP rows excepted) + hover-hitbox finding fixed at epic level

**Phase 7:** the user ran the SP/Workbench pass (row 5, F-1 … F-11), the persistence pass (row 8), the
gamepad pass (Q-1, Q-2), the folded-in spike questions (P1 – P4) and all eleven Phase-4b probe rows, and
reported **all green**. Scope honesty is recorded in `tasks.md`'s Phase 7 note: the rows were confirmed as
a block, with two observations transcribed individually — **A on a focused row does nothing** (the
expected outcome; `MenuLeft` stays load-bearing) and **D-pad Left unticks the focused row AND leaves the
panel** (worse than 4b's prediction, which expected only the untick; filed as **F7**). The **two-client MP
rows I-2 … I-4 were not part of the session and stay open** — they are now this feature's only unobserved
surface.

**The one non-green finding was not this feature's:** moving the map cursor with the left stick is
imprecise enough that hovering the 12–32 px Overthrow markers is hard. **It took three iterations the
same day, and the first two are recorded so nobody retries them** (full write-up in `map/core`'s
context file, which owns the changed code):

1. ❌ An invisible hit widget sized to **overflow** the icon box (center-aligned child keeps its desired
   size) — compiled, looked right, did nothing: **the engine does not trace hover outside a parent's
   arranged bounds**. Play-test-caught ("hover only works directly over the icon and its square").
2. ❌ Growing `IconContainer` and padding the visuals back in — kept the hit surface inside bounds by
   construction and **collapsed every icon to a slim white sliver**: the element's outer `SizeLayout0`
   hard-caps content at 32×60, so the growth clamped and the inward padding consumed what remained.
   Play-test-caught immediately.
3. ✅ **Shipped: a cursor-proximity hover magnet, no widget sizing at all.** The layout and
   `OVT_MapLocationType` were restored byte-identical to HEAD; `OVT_OverthrowMapUI.TickHoverMagnet`
   hovers the nearest visible marker within `OVT_OverthrowMapConfig.m_fHoverRadius` (default **32**
   reference px, tweakable on `Configs/Map/OverthrowMap.conf`, 0 disables) of the cursor's world
   position — vanilla's own descriptor-selection model, working identically for the pad's virtual
   cursor. Element guards keep the magnet and real widget events from double-firing.

Gates after iteration 3: compile **exit 0 / 6007 files**, Fast **101** — both moved from this feature's
Phase 7 baselines (5994 / 89) because parallel sessions have committed since; re-measured, not quoted.
⚠️ **Iteration 3 is unobserved** — the user should retest hover with the pad (markers should light up
within ~32 px of the cursor, nearest wins) and tune `m_fHoverRadius` to taste.

---

_Update this file at the end of each work session. Run `/update-feature map/map-layers` before compacting._
