# Map Legacy Retirement - Context & Decisions

**Last Updated:** 2026-08-10 21:30
**Current Phase:** ✅ Complete — all 7 phases + verification gate
**Status:** ✅ **COMPLETE — built and play-tested green**

---

## Quick Status

**What's Done:**
- ✅ **All seven build phases** (0–6), each gated at compile 0 and Fast 44
- ✅ **Final sign-off gate**: compile **0 / 5958**, Fast **44**, All **79** — every Phase 0 invariant held
- ✅ **Phase 7 combined verification gate DISCHARGED 2026-08-10** — the user ran it in full and reported
  all green, **including 7c two-client MP/JIP and 7e pre-change save load**, and regenerated the six
  localization exports
- ✅ **`map/location-types` Phase 7 (V-3 … V-7) discharged with it** — that was the epic's stated hard
  gate and this phase was designed to fold it in (settled decision D-4)

**What's Next:**
- 📋 Nothing in this feature. `map/respawn` is the next epic feature and inherits the legacy-free map.

**Blockers:**
- None. Feature complete.

---

## The invariants (Phase 0 baseline, captured 2026-08-10)

Recorded **before** the first deletion. These are the numbers every phase gate is checked against.

| Gate | Baseline | Notes |
|---|---|---|
| `tools/compile-check.sh` | **exit 0 · 5959 files** | Game module, ~6 s warm |
| `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) | **exit 0 · 44 tests** | ~16 s |
| `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) | **exit 0 · 79 tests** | ~22 s |
| Working tree | clean at **`28c2f957`** | *(fix) various map/core bugs* |

⚠️ **A changed test count is a finding to investigate, never a number to update.** This feature adds
no test and deletes no covered behaviour, so 44/79 must hold at every gate through sign-off.

---

## Key Files

### Deleted by this feature
- `Scripts/Game/UI/Map/OVT_MapIcons.c` — 846-line hand-rolled parallel-array icon layer (P3.1)
- `UI/Layouts/Map/MapIcon.layout` + `.meta` — `{F5E0CFFFC9F27B19}` (P3.3)
- `UI/Layouts/Map/MapInfo.layout` + `.meta` — `{0EC60966C99CE954}` (P3.4)
- `UI/Layouts/Map/MapInfo/Modifier.layout` + `.meta` — `{7BAC7637E5744768}` (P3.5)

### Stripped, not deleted
- `Scripts/Game/UI/Context/OVT_MapContext.c` — 592 → <80 lines; keeps `GetMap`/`ShowMap`/`HideMap`/`OpenMap` (P2)
- `Scripts/Game/UI/Context/OVT_MainMenuContext.c` — two rows + two methods out (P1)
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` — four unvalidated fast-travel RPCs out (P4)
- `Configs/Map/MapFullscreen.conf` — the `OVT_MapIcons` block only (P3.2)
- `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` — four attributes inside the retained `OVT_MapContext` block (P3.6)
- `Language/localization_Overthrow.st` — one `CustomStringTableItem` (P5.1)
- `Configs/overthrowBroadcastMessages.conf` — one `SCR_SimpleMessagePreset` (P5.2)

### Explicitly retained (do not touch)
- `OVT_MapRestrictedAreas` (live — FOB rings, BUG-070), `OVT_MapThreatGrid` (shipped disabled), `OVT_MapPlayerLocation` (live) — all `SCR_MapConfig` modules on a **different rendering path** from the marker widgets
- `m_DescriptorDefaultsConfig` in `MapFullscreen.conf` — `map/location-types`' bus-stop duplicate suppression, nothing to do with `OVT_MapIcons`
- `UI/Layouts/Map/MapPlayerLocation.layout`
- `Scripts/Game/UserActions/OVT_CatchBusAction.c` — **unmodified**; already re-pointed by `map/fast-travel` Phase 4
- `docs/archive/OverthrowMapSystem.md`

---

## Important Decisions

Carried in from `implementation.md` §5 — recorded here so a reader of this file alone does not
undo them.

### K-1 — Caller-first ordering, gated per phase
Callers are cut before callees. `OVT_MainMenuContext.c:218` is the **only** remaining thing that arms
`m_bFastTravelActive`, so after Phase 1 every legacy mode is *provably* unreachable before one line of
map code is deleted. No intermediate state has a reachable path calling a deleted symbol.

### K-3 — `HideMap()` is retained despite zero callers
All eight in-file callers die in P2 and grep confirms no external one (`OVT_OverthrowMapUI` has its
**own private** `HideMap()`/`GetMap()`). It survives because (a) it carries the play-test-derived
`ToggleFocused(false)`-then-stow ordering with its dead check and held-gadget guard — found by
play-test, not by any gate — and (b) `map/respawn` needs exactly this behaviour. **P2.5 requires a
`//!` block saying so, or the next reader cuts it as an orphan.**

### K-7 — Default to KEEP on every string and preset
The orphan sweep started from nine candidate ids and deleted **one**. `MustHaveMap` is the instructive
case: it looks like a fast-travel-mode string, but its surviving call site is inside the **retained**
`OpenMap()`. Deleting a string the live path still shows is a silent, player-visible defect no gate
catches.

### K-8 — `MapFullscreen.conf` is a same-GUID **delta** over vanilla's
Removing a block changes what merges with vanilla rather than replacing a file. This is why
`SCR_MapRadialUI` is live on Overthrow's map despite our conf never mentioning it.

---

## The two findings this feature adds

Neither is in `requirements.md`. Both are real, player-visible regressions if missed, and neither is
visible from the symbol being deleted.

> **🔴 FINDING A — deleting the "Map Info" block removes the main menu's only initial-focus call.**
> `OVT_MainMenuContext.c:108`'s `SetFocusedWidget(comp.GetRootWidget())` sits *inside* the `if (comp)`
> block for "Map Info" and is the **only** such call in the whole context (grep-verified). Delete the
> block naively and the Overthrow main menu opens with **no focused widget** — a controller user has
> nothing to navigate from. P1.2 relocates it to the new first entry, **"Place"**. Tested by F-2 and
> P7d step 22.

> **🟡 FINDING B — the in-game Field Manual documents both deleted entries.**
> `FieldManualConfigRoot.conf:19-30` holds two header/text pairs describing the deleted click-anywhere
> workflow verbatim. Both become **false** the moment Phase 1 lands. Handled in P6.4 via
> `help-docs-sync`, with every replacement sentence fact-checked against a `file:line`.

**Consequence:** `#OVT-MainMenu_MapInfo` and `#OVT-MainMenu_FastTravel` are **both retained** in the
`.st` master — the Field Manual, `OVT_OverthrowMapUI.c:929` and `OVT_MapInfoPanel.layout:198` still
consume them.

---

## Findings raised during the build (not in the plan)

> **🟠 FINDING C — "Place" was not the first row, so the relocated focus landed mid-list.**
> The layout's **visible row order** and `OnShow()`'s **wiring order** are different things, and they
> only coincided before by accident (Map Info happened to be both first-wired and first-in-layout).
> After Phase 1 the visible order was `Resistance, Jobs, Place, …`, so the plan's instruction to focus
> "Place, the new first entry" opened the menu with the **third** row highlighted. Gamepad navigation
> worked either way — only the starting row differed. **Raised to the user, who chose to reorder the
> layout** (see Decision D-5). *Lesson: a `SetFocusedWidget` target is a layout-order fact, not a
> code-order fact.*

> **🔴 FINDING E — prose rot is broader than the plan's model.**
> `implementation.md` K-9 / §2 goal 8 / P3.7 assume the only stale `file:line` comments are pointers
> into `OVT_MapIcons.c` and `OVT_MapContext.c`. Phase 2 found `OVT_FastTravelService.c`'s
> `BUS_STOP_RADIUS` comment citing `OVT_TownManagerComponent.c:881` for bus-stop discovery — and that
> file contains **no bus code at all** (`grep -n "BusStop\|bus"` → zero hits); line 881 is now a
> `\param[out]` on `GetTownsWithinDistance`. It was already dangling before this feature. Dropped in
> passing. **A general `file:line`-in-comments audit is warranted across the codebase** — not this
> feature's job, but worth filing. *Lesson: a `file:line` in a comment is a reference with a human
> reader instead of a compiler, and nothing ever re-checks it.*

> **🟡 FINDING F — a Phase 5 acceptance grep passes by coincidence.**
> `implementation.md` P5's criterion expects "only the two `OVT_TEST_InitSuite.c` prose mentions" of
> `NeedBusStop`. There were actually **three** (`:2004, :2094, :2160`). Phase 2 corrected `:2004` as
> part of its named scope, leaving exactly two — so the grep will pass, but *by accident, not because
> the file is correct*. Both survivors are factually stale: the live refusal is `#OVT-NotAtBusStop`
> via `OVT_TravelResult.NOT_AT_BUS_STOP` (`OVT_FastTravelService.c:139,182`), and one of them
> (`:2095`) is a `SetResultFailure` message string. **Phase 5 must correct them rather than rely on
> the count.**

> **🟢 FINDING G — the plan's Q-4 expected count was wrong (harmlessly).**
> `implementation.md` Q-4 expects **nine** `OVT_MapIcons` prose mentions across six files, listing
> `OVT_MapMarkerComponent.c:20` among them. That comment says "the legacy map-icon layer's static POI
> registry" *without naming the class*, so it never appears in the grep at all. The true post-P3.7 set
> is **eight mentions across five files**. Q-4's expected result should be corrected to 8/5.

> **🟢 FINDING H — P3.2 was provably a zero-runtime-behaviour-change deletion.**
> Verified against the Reforger reference tree rather than assumed: `SCR_MapEntity.c:1284` does
> `if (component.IsConfigDisabled()) continue;`, so a config-disabled component is never inserted into
> `m_aLoadedComponents`, never `SetActive(true)`, never `Init()`ed. The deleted block carried
> `m_bDisableComponent 1` and was therefore already inert. `m_aUIComponents` is also consumed as a
> whole array with lookups by typename (`GetMapUIComponent(typename)`), never by index, so removing an
> entry cannot shift anything. The retained `OVT_MapThreatGrid` is skipped by the identical mechanism.

> **🟡 FINDING I — a pre-existing orphaned script `.meta`, adjacent to this work but not caused by it.**
> `Scripts/Game/UI/Map/OVT_MapThreatGrid.c.meta` declares
> `Name "{B8F4C6A8C9D3E4F1}Scripts/Game/UI/Map/OVT_MapThreatGrid.c"`, but that file now lives at
> `Scripts/Game/UI/Map/Visualization/OVT_MapThreatGrid.c` (which has no `.meta` of its own). Committed
> in `96e6da4d Vehicle patrols`, untouched by this feature. **If P7a's "zero missing-resource errors"
> check trips on `{B8F4C6A8C9D3E4F1}`, this is the cause, not the deletions** — and misattribution is
> easy, because `OVT_MapThreatGrid` is one of the modules this feature deliberately *retained*.

> **🟡 FINDING J — more prose rot, out of scope, logged not fixed.**
> `OVT_MapLocationWaypoint.c:4-5` cites `OVT_JobsContext.ShowOnMap:109` (actually `:107`) and
> `OVT_RecruitsContext.ShowOnMap:489` (actually `:471` — off by 18). A different topical paragraph from
> the ones P3.7 rewrote, pointing into files this feature did not touch. Also `OVT_JobManagerComponent.c:67`
> cited `RplSave/RplLoad at :780/:810` when they are at `:794`/`:824`; that one *was* in a block being
> rewritten, so the unstable self-pointer was dropped and the rationale kept. Together with FINDING E,
> this is now three independent instances — **a codebase-wide `file:line`-in-comments audit is
> warranted.**

> **🟡 FINDING D — the `Options` row has no handler.** `MainMenu.layout` contains a `ButtonWidgetClass`
> named `Options`, and `grep '"Options"'` in `OVT_MainMenuContext.c` returns nothing — nothing is wired
> to it. **Pre-existing and unrelated to this feature**; the menu already rendered one dead row before
> any of these deletions. Not touched. Worth filing separately.

---

## Important Decisions (made during the build)

### D-5 — Main-menu rows reordered by frequency of use *(user-directed, 2026-08-10)*
**Context:** FINDING C — relocating the menu's only `SetFocusedWidget` call to `Place` left focus on the
third visible row.
**Options put to the user:** focus the first visible row (`Resistance`) to preserve the exact
pre-change behaviour; keep focus on `Place` mid-list as the plan literally says; or reorder the layout
so `Place` is genuinely first.
**Decision:** **reorder**, and further — `Build` second, `Resistance` third — so the menu reads in
order of how often each entry is actually used. Final order: `Place, Build, Resistance, Jobs, Real
Estate, Manage Recruits, Character Sheet, Save, Options`.
**Note:** this is a deliberate, user-directed **widening** of a deletion-only feature. It was flagged
as out of scope (`implementation.md` §2: "reworking the main menu beyond the two rows" is an explicit
non-goal) and the user made the call anyway. Recorded here rather than folded in silently. The moves
are pure block relocations — every moved `ButtonWidgetClass` is byte-identical to its original.
**Impact:** `implementation.md`'s F-2 and P7d step 22 should now read "the highlight starts on
**Place**, which is the first row" — the ambiguity FINDING C exposed is resolved rather than
documented around.

---

## The P5.5 orphan sweep — recorded so it is not re-run

Every notification id raised from a deleted `MapClick` branch was audited. **Nine candidates, one
deletion.**

| Id | Verdict | Live consumer after this feature |
|---|---|---|
| `MustHaveMap` | **KEEP** | The **retained** `OVT_MapContext.OpenMap()` still raises it |
| `CannotFastTravelThere` | **KEEP** | `OVT_FastTravelService.c:187,360` + 4 location types |
| `CannotFastTravelDistance` | **KEEP** | `OVT_FastTravelService.ReasonKeyFor` |
| `CannotFastTravelWanted` | **KEEP** | `ReasonKeyFor` |
| `CannotFastTravelDuringQRF` | **KEEP** | `ReasonKeyFor` |
| `CannotFastTravelToQRF` | **KEEP** | `ReasonKeyFor` |
| `MustBeDriver` | **KEEP** | `ReasonKeyFor` |
| `MustExitVehicle` | **KEEP** | `ReasonKeyFor` |
| `CannotAfford` | **KEEP** | 14 live call sites across build, place, shop, resistance, travel |
| `MainMenu_MapInfo` | **KEEP** | `FieldManualConfigRoot.conf:20` |
| `MainMenu_FastTravel` | **KEEP** | Field Manual + `OVT_OverthrowMapUI.c:929` + `OVT_MapInfoPanel.layout:198` |
| `NeedBusStop` | **DELETE** | Only consumer was the deleted bus branch |

Also **not** to be deleted: `#OVT-NotAtBusStop` — a new, live id from `map/fast-travel`.

---

## Gotchas & Learnings

### 1. Four file classes are invisible to every automated gate
`.layout`, `.conf`, `.et` and `.st` are read by neither `compile-check.sh` nor either test group, and
**four of the seven phases edit nothing else**. A deleted layout GUID still referenced from a prefab,
a config block removed with something else by accident, or a partially-deleted `.st` block all give
compile 0 + green tests and fail at runtime. The real gate for Phase 3 is **P7a**, the Workbench load.

### 2. Never delete by line range
Every `file:line` in the plan is a pointer for a human to look at. The hand-off list has already been
wrong once — it named a 4-line range *inside* the 18-line `OVT-NeedBusStop` `CustomStringTableItem`,
and acting on it would have corrupted the `.st` master silently. Locate by **symbol name** or **`Id`
string**, confirm boundaries by eye, delete whole syntactic units.

### 3. Read the whole enclosing block before removing it
FINDING A is exactly this failure mode: a `SetFocusedWidget` call hiding inside a block that *looks*
purely legacy. Ask what else lives there.

### 4. `compile-check.sh` exit 0 does not mean the scripts load
Two known blind spots (`Rpc()` arity; argument-count overflow on base-class methods with defaulted
params). Always read `run-tests.sh`'s verdict too — an indeterminate (exit 2) with no `junit.xml` is
the signature of a runtime compiler rejection the static check passed.

---

## Known epic tech debt this feature must NOT fix

⚠️ **This list was stale on arrival.** `implementation.md` §8 names BUG-133, BUG-134, BUG-136 and the
unreachable `OnLocationClicked` as live traps. A **concurrent session closed all five `map/core` bugs
(BUG-133 … BUG-137) on 2026-08-10**, after this plan was written — see `docs/features/map/epic-overview.md`.
So the only genuinely open item is:

- **T1 — three manager-access idioms across the ten location types** (`map/location-types`): inherited
  cache vs own shadowing member vs per-call `OVT_Global` lookup. Not this feature's to fix.

**One live consequence for this feature.** BUG-136's fix added per-type `m_fRefreshInterval` polling —
`OVT_OverthrowMapUI.TickRefresh` → reconcile → `OVT_MapLocationElement.SetLocationData` — which
**destroys and re-creates marker elements while the map is open** (5 s for Town/Base/RadioTower/FOB/Camp,
2 s for Vehicle). That is a new, runtime-unverified path through the same file Phase 2 edits, and it
means Phase 7's marker sweep must leave the map open **>5 s with a panel pinned**. It does not change
what this feature deletes — `IsInfoPanelVisible()`'s only caller was in `OVT_MapContext` either way.

If retirement surfaces a **gap**, it still goes back to `map/location-types` or `map/fast-travel`.

If retirement surfaces a **gap**, it goes back to `map/location-types` or `map/fast-travel` — fixing
it here would turn a clean, revertable deletion into a mixed change that cannot be bisected.

---

## Verification gate — ✅ DISCHARGED 2026-08-10

The user ran the **full** combined Phase 7 gate and reported all green, then regenerated the six
localization exports. **All 27 boxes ticked**, including the two that a single session most often
skips and that this project's regressions historically hide in:

- **7c — two-client MP/JIP.** Two clients on `launch-server.sh`, B joining *after* A had accumulated
  state; per-player marker isolation (A's house, vehicle and private camp invisible to B, warehouse
  and FOB visible), B's own purchases invisible to A, **both clients fast-travelling concurrently**
  each arriving correctly and charged once, and a JIP client executing the server travel path.
- **7e — save compatibility.** A save created *before* these changes loads with all markers rendering.

Also covered: 7a Workbench load (zero missing-resource errors — so no deleted GUID broke a prefab,
the risk no automated gate in this project can see), 7b single-player sweep incl. the Catch Bus and
fast-travel money paths, and 7d gamepad — which is what confirms **FINDING A**, the regression that
would otherwise have shipped a controller-unusable main menu.

**This gate discharged two features.** Per settled decision D-4, `map/location-types`' outstanding
Phase 7 (V-3 … V-7) was folded in here — it was the epic's stated hard gate, and it is now closed.

**Localization exports confirmed regenerated** (verified in the tree, not just reported): all six
`.conf` files updated, `OVT-NeedBusStop` now absent from **all six**, and the rewritten Field Manual
text present. That was the precondition for P7b step 5 being meaningful at all.

---

## Session Notes

### 2026-08-10 18:50 — Phase 0
- Baseline captured and it matches the plan's prediction exactly: compile **exit 0 / 5959 files**,
  Fast **44**, All **79**. Tree clean at `28c2f957`.
- `tasks.md` scaffolded from `implementation.md` §4 — 49 build tasks across Phases 0–6, plus 27
  user-driven boxes in Phase 7.
- ⚠️ **Deviation from the plan, deliberate:** `implementation.md` §4 and Q-10 call for a **commit per
  phase**. This autorun does not touch git under any circumstances, so all phases land as one
  uncommitted working-tree change. The revert path the plan relies on (`git revert` of a single
  phase) is therefore **not** available unless the user commits per phase themselves — recommended,
  since P7 is where a gap would surface.

### 2026-08-10 19:10 — Phase 1
- Both dead main-menu rows removed (`OnShow()` blocks + `MapInfo()`/`FastTravel()` methods + two
  `ButtonWidgetClass` units). **FINDING A handled first**, before either block was deleted.
- Gate: 4/4 acceptance greps clean; layout braces **333/333** (was 389/389 — a delta of exactly
  2 × 28, and 80 lines = 2 × 40, consistent with two whole units and nothing else); compile **exit 0 /
  5959 files**; Fast **44**. Baselines held.
- Both deleted blocks were otherwise purely legacy — comment + `GetButtonText` + `if (comp)` with only
  the `m_OnClicked.Insert`, and two two-line methods calling `CloseLayout()` + the doomed
  `Enable*`. The `SetFocusedWidget` call was the single non-legacy inhabitant.
- **FINDING C** raised and resolved by user decision **D-5** (menu reordered by frequency of use).
  **FINDING D** logged, untouched.

### 2026-08-10 19:35 — Phase 2 (advanced)
- `OVT_MapContext.c` **591 → 79 lines**, exactly four methods. All 11 members/constants and all 15
  methods deleted. The four survivors were extracted with `sed` and **machine-diffed byte-identical**
  against the originals before reassembly, trailing tabs included — they were not retyped.
- Gate: greps 2/3/4 all empty; compile **exit 0 / 5959**; Fast **44**. Baselines held.
- **BUG-069 part 4 confirmed structurally closed** — `grep -rn "SCR_MapEntity.GetOn"` returns nothing,
  so no Overthrow context subscribes to any static map invoker and the pattern cannot be reintroduced
  by an existing file.
- **Two non-legacy inhabitants found in deleted blocks, both proven inert:**
  1. `OnMapExit` called the base-class `CloseLayout()` and was subscribed to the *static*
     `GetOnMapClose()` invoker, so it fired on **every** map close — holstering, death, other menus —
     not only from the legacy modes. Inert because `CloseLayout()` returns immediately when `!m_wRoot`,
     and `m_wRoot` is only ever set by `ShowLayout()`, whose sole caller was `EnableMapInfo()`
     (unreachable since Phase 1).
  2. `RegisterInputs` bound `MapExit` to **`MenuBack` and `GadgetMap`** — two live, globally meaningful
     inputs. Safe because `MapExit`'s first statement is the three-flag guard, so it early-returned
     unconditionally after Phase 1, and `AddActionListener` does not consume the input from other
     handlers. ⚠️ **This is the deletion to re-check first if map open/close misbehaves** in P7b step 12.
- **FINDING E** and **FINDING F** raised (above).
- ⚠️ **Intermediate state, closed by Phase 3:** `Character_Player.et` now sets four attributes whose
  script members no longer exist. Expected — but it means the plan's claim that *"the tree is shippable
  at every phase boundary"* does **not** hold between P2 and P3. Do not open that prefab in the
  Workbench until Phase 3 lands.

### 2026-08-10 19:55 — Phase 3 (advanced)
- **The legacy map is gone.** Deleted `OVT_MapIcons.c` (846 lines), `MapIcon.layout`, `MapInfo.layout`,
  `MapInfo/Modifier.layout` and all three `.meta` files, plus the now-empty `UI/Layouts/Map/MapInfo/`.
  Edited the `OVT_MapIcons` block out of `MapFullscreen.conf` and the four dead attributes out of
  `Character_Player.et`.
- Gate: Q-4 grep **8 mentions / 5 files** (see FINDING G); Q-5 GUID grep **empty** repo-wide outside
  `docs/`; `MapFullscreen.conf` braces 24/24 with all keepers intact; `.et` braces 140/140; compile
  **exit 0 / 5958** — exactly the predicted −1; Fast **44**.
- **Death was grep-proven before each cut, not after.** `RegisterPOI` had exactly two hits repo-wide
  (its own definition + one prose mention). The three layout GUIDs returned exactly the predicted
  self-contained sets. **Bidirectional check caught a near-miss:** the deleted config block cited
  `overthrow_mapicons.imageset`, which has 20+ other live consumers — retained, not orphaned along
  with the block.
- **The eight `OVT_MapIcons.c:<line>` pointers were all accurate** — each was resolved against the live
  file *before* deleting it, so every preserved rationale is factually correct.
- **FINDING G, H, I, J** raised (above). The P2→P3 dangling-attribute window is now closed.

### 2026-08-10 20:10 — Phase 4 (security fix)
- Four RPCs deleted from `OVT_PlayerCommsComponent` — **including their `[RplRpc]` attribute lines**, so
  the wire surface is gone, not just the callers. 67 deletions, 0 insertions; file 2045 → 1978 lines.
- Gate: RPC symbol list **61 → 59** with a diff showing exactly the two expected `RpcAsk_` names and
  nothing else; braces 233/233; compile **exit 0 / 5958** (no file added or removed); Fast **44**, with
  `junit.xml` produced — so the scripts genuinely *loaded*, not merely compiled.
- **Safety proven before deletion, not asserted.** The old recruit ring-placement loop and
  `OVT_TravelRequestComponent.TeleportRecruits` were extracted and diffed with only the two renamed
  identifiers normalised: **verbatim identical**, including all three inline comments, the ring maths,
  and the `FindSafeSpawnPosition(..., skipSpawnPointSearch = true)` call. The agent also checked the
  *non-loop* half unprompted — persistent-ID resolution, null-actor guard, `originPos` captured before
  teleporting, radius query against the departure point — all reproduced. **The only behavioural deltas
  in the replacement are additive: validation and payment.**
- What was actually deleted, for the record: `RpcAsk_RequestFastTravel` was a **two-line unvalidated
  teleport** (`ResolveSenderPlayerId` → `SCR_Global.TeleportPlayer`) with no distance, ownership,
  wanted-level or QRF check and no payment, callable by any client with an arbitrary `vector`.
- **P4.3 correction:** `OVT_TravelRequestComponent.c:291` had become false — it carried a `(:1529-1547)`
  pointer into a now-deleted method and said the legacy path "**is** the only implementation that has
  ever run". Rewritten to past tense with the pointer dropped (same K-9 treatment as P3.7). `:9` was
  already correct and was left alone.
- 📋 **Stale generated docs:** `generated-docs/html/class_o_v_t___player_comms_component.html` still
  lists the deleted RPCs. Doxygen output, not source — stale for Phases 1–3 too. Worth a regeneration
  pass at the end of the feature.

### 2026-08-10 20:25 — Phase 5
- `OVT-NeedBusStop` deleted from the `.st` master (whole 22-line `CustomStringTableItem`,
  `:4353-4374`) and its `SCR_SimpleMessagePreset` from `overthrowBroadcastMessages.conf` (`:396-403`,
  including the nested `m_UIInfo`). **FINDING F** discharged — the two remaining stale
  `OVT_TEST_InitSuite.c` mentions were corrected to `#OVT-NotAtBusStop` / `NOT_AT_BUS_STOP` rather
  than left to make the acceptance grep pass by luck. Comments and one `SetResultFailure` string only;
  no assertion, condition, constant or `[Test]` case touched.
- Gate: `.st` braces **1048/1048**, all 11 KEEP ids present exactly once; broadcast conf **322/322**
  with `MustBeDriver` and `MustExitVehicle` intact; **generated exports zero diff, verified twice**;
  compile **exit 0 / 5958**; Fast **44**.
- Boundaries were confirmed by reading `:4320-4390` first, and the edit was anchored on the full block
  text **plus the following item's opening line**, so an ambiguous match would have errored rather
  than cut blind.

#### Three errors found *in the plan itself* (all in `implementation.md` §4 P5)
1. 🔴 **"~18 lines" understates the `.st` block, which is 22.** The boundaries named were correct; only
   the length was wrong. Trusting "~18" as a range would have left four metadata lines dangling —
   precisely the silent-corruption failure this phase exists to avoid.
2. **P5's acceptance grep can never pass as written.** It excludes with `^./Language/…`, assuming
   `grep -r .` emits a `./` path prefix; this build does not, so six generated exports show through as
   false positives. Correct form: `grep -vE "Language/localization_Overthrow\.[a-z_-]+\.conf"`.
3. **"brace delta must be exactly 1 pair" is wrong** — GUID string literals contain braces that
   `grep -o` counts. Real deltas: `.st` −2 pairs (1 structural + 1 GUID), conf −5 (2 structural + 3
   GUID). **Balance is the signal; the pair count is not 1.**

#### 📌 `CLAUDE.md` is stale on test counts
It records **Fast 38 / All 66**. The real counts are **44 / 79**, and were before this feature began —
nothing here changed a test. A future session following `CLAUDE.md` would read the correct count as a
regression. Worth correcting at the project level.

### 2026-08-10 20:40 — Phase 6a (Field Manual, FINDING B)
- **Decision: rewrite both entries, keep both headers.** Deleting them would have removed the only
  in-game explanation of markers, info panels and fares — and the map is *more* capable than the thing
  the old text described. Only the **route** (a main-menu row) and the **interaction** (click empty map)
  were false. `#OVT-MainMenu_MapInfo` and `#OVT-MainMenu_FastTravel` still read correctly as headers,
  so no header re-pointing, no new ids, and **`FieldManualConfigRoot.conf` is unmodified** — the whole
  change is two `Target_en_us` values in the `.st` master.
- Gate: `.st` braces 1048/1048; Field Manual conf braces 29/29 and untouched; generated exports **zero
  diff**; compile **exit 0 / 5958**; Fast **44**.
- **Every factual sentence is backed by a `file:line`** — ~25 of them, covering what opens the map, the
  fourteen marker types, hover-vs-pin-vs-unpin, which four location types allow travel, fare = km ×
  `m_Difficulty.fastTravelCost` with a 1 km floor, recruits within `RECRUIT_TRAVEL_RADIUS = 50.0`
  travelling **opt-out** at a full fare each, every refusal reason, and the bus flow (no km floor, must
  be on foot).
- **Six sentences were cut because they could not be proven** — this is the valuable half:
  - *"Click the panel's close button"* — `SetupCloseButton` exists, but **BUG-134** is an open
    widget-name mismatch for exactly that button. Documented the empty-map click instead, which was
    verified.
  - *"…including any intel known about the area"* (from the old text) — no intel field is surfaced by
    any panel builder. Dropped.
  - *Points of interest as a fast-travel destination* — `OVT_MapLocationPOI` has no `CanFastTravel`
    override and no `m_bCanFastTravel 1`. It is a marker only.
  - *"Travel is free in debug mode"* — true, but a dev switch, not player help.
  - *Any concrete price or distance* — all come from `m_Difficulty` at runtime; the old text's "a small
    fee" was an unprovable characterisation.
  - *"Markers appear as you zoom in"* — zoom-gating exists, but no rule precise enough to state.
- ⚠️ **Ten stale translations.** Both blocks keep their old-workflow `Target_fr_fr/ru_ru/ko_kr/zh_cn/uk_ua`
  values, which now describe deleted UI. **Not machine-translated, deliberately** — flagging is correct,
  silently shipping a mistranslation is not. `Status DEVELOPMENT_DONE` on both blocks is now optimistic
  and should be reset when they go for translation.
- ⚠️ **Ordering constraint for P7:** until the exports are regenerated the Field Manual shows the **old
  English text** (the ids are unchanged, so nothing renders as a raw key — the stale export simply
  wins). **P7b step 5 can only be checked after regeneration.**

### 2026-08-10 20:55 — Phase 6b (documentation reorganisation)
- `towns/map-info` archived to `docs/archive/towns-map-info-{context,implementation,tasks}.md`, each
  carrying a banner at the **top of the file itself** rather than a sibling README — a reader who opens
  one file directly is the case that matters, and a README next to it would never be seen.
  `docs/archive/OverthrowMapSystem.md` untouched, as required.
- `towns/epic-overview.md` 5/5 → 4/4 with the map clause dropped from Purpose, the build-order entry and
  dependency line removed, and four Tech Debt bullets retargeted. `docs/overview.md` towns row → 4/4,
  map row updated. `map/epic-overview.md` feature-4 row rewritten with the gate numbers.
- **Two dependency lines the plan did not name** were also retargeted, because criterion 2 could not
  pass otherwise: `towns/core/implementation.md:179` and `towns/stability/implementation.md:167`.
- **Concurrency handled correctly.** Every file was re-read immediately before editing and only targeted
  `Edit` calls were used — no `Write` over an existing doc. The other session's work in
  `map/epic-overview.md` (the rewritten `core` row, six resolved `[x]` tech-debt bullets, the
  "Reconciled 2026-08-10" paragraph, and the already-resolved "Legacy static is a hard dependency"
  bullet) was preserved and verified present afterwards.

#### Deliberate exception to the P6 acceptance grep
`docs/overview.md:37` — the **v1.7 changelog entry** still says "towns epic created — 5/5 features …
`map-info`". That is a dated record of what happened on 2026-08-03. **Rewriting it would falsify the
changelog**, so it stays. If the grep must come clean, the correct fix is a *new* version entry, not an
edit to the old one.

#### 🐛 Possible BUG found while rewriting (verified, not assumed)
`OVT_MapLocationTown.c:272` does `m_Config.m_aModifiers[modifierData.id]` **with no bounds check** — the
following `if (!modifierConfig)` catches a null entry, not an out-of-range id. This is the **same defect
the deleted `OVT_MapContext` had**, so it was moved rather than fixed by the map rewrite. Out of scope
here (removal only); worth filing.

---

## Deviations from the plan, and why

Recorded so a reviewer does not have to reconstruct them.

1. **No commits.** `implementation.md` §4 and **Q-10** require a commit per phase, and the plan's whole
   remediation story (`git revert` one phase if P7 finds a gap) depends on it. This autorun never writes
   git state, so **all seven phases are one uncommitted working-tree change**. Q-10 is therefore *not
   satisfiable by this run* — the user should commit per phase if they want the revert path.
2. **`help-docs-sync` agent is not installed.** P6.4 names it. The available agent set does not include
   it, so P6.4 ran on a standard `component-developer` with the fact-checking discipline written into
   the prompt explicitly (every sentence backed by a `file:line`; unprovable sentences cut). Result was
   good — see the Phase 6a note — but the specialised agent was not used. `/upgrade-beast-mode` may
   install it.
3. **Menu reorder (D-5)** — a user-directed widening of a removal-only feature. See Decision D-5.
4. **P5.6 added** — correcting the two stale `NeedBusStop` test-suite mentions rather than letting the
   plan's acceptance grep pass by coincidence (FINDING F).
5. **Q-4's expected result is 8 mentions in 5 files**, not the plan's 9 in 6 (FINDING G).

### 2026-08-10 21:30 — Phase 7 discharged, FEATURE COMPLETE
- Final automated sign-off before handing over: compile **exit 0 / 5958**, Fast **44**, All **79**.
  Every Phase 0 invariant held from first deletion to last; **no test count moved at any point**,
  which is exactly what a pure-removal feature should produce.
- **The user ran the full Phase 7 gate and reported all green**, including 7c two-client MP/JIP and
  7e pre-change save load, and regenerated the six localization exports. All 27 boxes ticked.
- **Export regeneration verified in the tree, not merely reported**: all six `.conf` files show a
  diff, `OVT-NeedBusStop` is absent from every one, and the rewritten Field Manual text is present.
  This mattered — until it happened, the Field Manual would have rendered the *old* English text and
  P7b step 5 would have been checking the thing this feature was supposed to have fixed.
- **`map/location-types` Phase 7 (V-3 … V-7) is discharged with it** (settled decision D-4). That was
  the map epic's stated hard gate.
- **What this gate proves that nothing automated could.** Four of seven phases edited `.layout`,
  `.conf`, `.et` and `.st` files — invisible to `compile-check.sh` *and* to both test groups. A
  dangling GUID, an orphaned config entry or a half-deleted string block would have passed every gate
  and failed in the world. 7a's clean Workbench load is the only evidence those deletions were sound.
  7d is the only evidence FINDING A's fix works. 7c is the only evidence the map's per-player
  isolation survives a second client.

---

*Update this file at the end of each work session.*
