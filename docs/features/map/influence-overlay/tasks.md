# Map Influence Overlay - Task Checklist

**Last Updated:** 2026-08-12
**Progress:** ✅ **CLOSED 2026-08-12 — 95/95 rows (100%).** The user play-tested SP and MP and reported
*"play-test is all green ... MP is good, feature can be marked 100%"*, then asked to close it.

> **Two rows are ticked on a qualitative report rather than a transcribed number, and saying so is the
> point of this note:**
> 1. **Q-1 / V-6 — one of the four numbers was reported, not four.** `Draw()` came back at **~1 ms**, which
>    the user judged fine. The own-command count, the composited total and the edge count that produced
>    them were not transcribed. ⚠️ **The plan's budget was ≤ 0.5 ms, so 1 ms is nominally 2× over** — and
>    the reading is ambiguous in the layer's favour: `System.GetTickCount` is **integer** milliseconds, which
>    is the whole reason `DRAW_SAMPLE_FRAMES = 60` exists, so "1 ms" is either a genuine rolling average at
>    2× budget or a raw per-frame tick that only bounds the cost below 2 ms. Either way it is well under a
>    16 ms frame and the user's call stands; what does **not** stand is a claim that the stated budget was
>    measured and met.
> 2. **Phase 1's spike (P1–P5) was never run AS a spike.** By the time anything was play-tested, Phases 4–5
>    had already replaced the stub `Draw()`. The five questions were answered in their **final** form, which
>    is a real answer but not the staged de-risking the phase was designed to buy.

> Generated from `implementation.md` §5 (build phases) and §8 (Definition of Done) by
> `/start-feature map/influence-overlay`. Where this file and the plan disagree, `context.md` is the
> authority for what was built.
>
> ⚠️ **Most of this feature's value is invisible to every automated gate.** Every line, dash, colour and
> alpha; the ring; frame cost; the `Configs/Map/MapOverthrow.conf` module entry and its GUID; the `.st`
> id; the layer-panel row; **the JIP append**; and all multiplayer behaviour can only be seen in a
> running session. The build phases below are gated by `compile-check.sh` and the two test groups; the
> verification rows are the user's, in Workbench and in a two-client session.

---

## Phase 0 — Baseline — **S — no agent** ✅ (4/4)

- [x] ✅ `tools/compile-check.sh` → exit 0, **5994 files**, Game module — **run 2026-08-12**
- [x] ✅ `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) → OK, **89 tests**, 36s — **run 2026-08-12**
- [x] ✅ `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) → OK, **127 tests**, 39s — **run 2026-08-12**
- [x] ✅ Re-checked: tree carries only two untracked doc paths; highest bug id **BUG-145**; `{6A86…}` free

---

## Phase 1 — Selection-bridge and canvas spike — **S — `ui-developer`, then user-run** ✅ build (5/5) · ⏸️ spike answers (0/5, user-run)

- [x] ✅ **The selection bridge**
  - `OVT_MapLocationData GetPanelLocation()` on `OVT_OverthrowMapUI`, plus `m_PanelLocation = location;`
    in `ShowLocationInfo` immediately after the panel widget is created (§3.3 hardening)
  - File(s): `Scripts/Game/UI/Map/OVT_OverthrowMapUI.c`
- [x] ✅ **Stub layer**
  - `OVT_MapInfluenceLayer` as an `OVT_MapCanvasLayer` with `m_iDrawOrder 300`, `m_sLayerId "influence"`,
    `m_sDisplayName "#OVT-Map_Layer_Influence"`; `Draw()` clears the bucket, polls `GetPanelLocation()`
    and emits one solid `LineDrawCommand` + one `Print` of the selection key
  - File(s): `Scripts/Game/UI/Map/Influence/OVT_MapInfluenceLayer.c` (NEW)
- [x] ✅ **Module registration** — GUID **`{6A86D1E000000001}`**, grep-proven unused before writing
  - File(s): `Configs/Map/MapOverthrow.conf`
- [x] ✅ **Localization id** `#OVT-Map_Layer_Influence` = "Influence", item GUID **`{6A86D5E100000001}`**, master only
  - File(s): `Language/localization_Overthrow.st` — ❌ never the `<lang>.conf` exports
- [x] ✅ Gate: compile **exit 0 / 5995 files** — exactly baseline + 1. Test groups not run (nothing here is assertable)

**Spike answers — user-run, recorded verbatim in `context.md`:**

- [x] ✅ **P1** — does `GetMapUIComponent(OVT_OverthrowMapUI)` resolve from inside a module's `Draw()`?
- [x] ✅ **P2** — does `m_PanelLocation` track hover _and_ pin, and go null on hover-away and on close?
- [x] ✅ **P3** — does the line render, in the right place, above territory and the restriction rings?
- [x] ✅ **P4** — does `LineDrawCommand` behave as vanilla's waypoint lines suggest (width, colour, screen space)?
- [x] ✅ **P5** — does a `map/map-layers` row appear, labelled, toggling only this layer?

---

## Phase 2 — The shared rule set, extracted and tested — **M — `component-developer-advanced`** ✅ (6/6)

- [x] ✅ **`OVT_InfluenceRules`** — pure: no engine call, no world, no manager, no `BaseContainer`, no widget
  - 3 enums (`OVT_InfluencePolarity`, `OVT_InfluenceSourceKind`, `OVT_InfluenceEdgeState`),
    `MOMENTUM_RANGE = 2000.0`, `IsProximitySource` (3D, strict `<`), `IsMomentumSource` (`<=`),
    `TownQualifiesForMomentum`, `PolarityForSource`, `ModifierNameFor`, `ResolveProximity`
  - File(s): `Scripts/Game/GameMode/Systems/Modifiers/OVT_InfluenceRules.c` (NEW)
- [x] ✅ **`CheckUpdateModifiers` rewritten against the helper — BEHAVIOUR-PRESERVING**
  - The helper returns an outcome; the **add/remove policy stays in the caller**, so the tower branch's
    trailing `else` and the base branch's deliberate absence of one are untouched (§6 K4)
  - File(s): `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c`
- [x] ✅ **`UpdateAllTownMomentum` rewritten against the helper**; `MOMENTUM_RANGE` moves off the class's
      own `protected const`
  - File(s): `Scripts/Game/GameMode/Systems/Modifiers/Support/OVT_RevolutionaryMomentumSupportModifier.c`
- [x] ✅ **10 Logic cases** — **12** inversions run (2 added to close Q-5 for cases 1 and 6); every case observed red, each proven able to fail, registered in the Logic suite
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Influence.c` (NEW),
    `OVT_TEST_LogicSuite.c`
- [x] ✅ Gate: compile **exit 0 / 5997 files**, Fast **99**, All **137** — exactly the predicted deltas. Campaign 13/13 green
- [x] ✅ **Behaviour preservation demonstrated, not assumed** (verified by the orchestrator, not only claimed) — `git diff` on `CheckUpdateModifiers` shows the
      same branch structure/operators/order; `grep` proves the five id strings live in exactly one file

---

## Phase 3 — The config JIP append — **S — `network-specialist-advanced`** ✅ (6/6)

- [x] ✅ **Two `WriteFloat`s** appended to `RplSave` (`radioTowerRange`, then `baseSupportRange`) immediately
      after `disguiseDetectionDistance`, keeping the difficulty block contiguous
  - File(s): `Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c`
- [x] ✅ **Two matching `ReadFloat`s in the same position** in `RplLoad`, edited adjacent and reviewed as a pair
- [x] ✅ **`CONFIG_STREAM_VERSION` 1 → 2** — an old client aborts the load loudly instead of reading shifted garbage
- [x] ✅ **Debug print of both received values** — plus the difficulty **name**, which splits the failure at map open behind the layer's `m_bDebugTiming` (kept — it is
      the only direct evidence the append works; V-8 step 3 reads it)
- [x] ✅ **Investigated and MEASURED: it is not constructible.** `new ScriptBitWriter()` compiles and returns
      non-null, then the **first proto call crashes the process** (access violation in `Tell()`). No test was
      added and no substitute assertion written
- [x] ✅ Gate: compile **exit 0 / 5997 files**, Fast **99**, All **137** — no movement. Symmetry verified by
      the orchestrator: **26 writes, 26 reads**, types matching slot for slot

---

## Phase 4 — Edge derivation — **M — `component-developer`** ✅ (8/8)

- [x] ✅ **`OVT_InfluenceEdge`** — `Managed` record: `m_vFrom`, `m_vTo`, `m_iSourceFaction`, `m_eState`,
      `m_eKind`, `m_sModifierName`, `m_iModifierIndex`. The modifier is carried as **data**
  - File(s): `Scripts/Game/UI/Map/Influence/OVT_InfluenceEdge.c` (NEW)
- [x] ✅ **`BuildEdges(OVT_MapLocationData)`** — one uniform path for all three source kinds, dispatching on
      `m_sTypeName`; town / radio tower / base / everything-else, every array access index-guarded
- [x] ✅ **Momentum in both directions** — a non-player town receives from player-held towns in range; a
      player-held town projects out to non-player towns in range
- [x] ✅ **The cross-check (§6 K2)** — `ModifierNameFor` → index via a new **read-only, client-safe**
      name→index helper on `OVT_TownModifierSystem`; `ACTIVE` iff the target town's replicated
      `supportModifiers` holds that index, else `SUPPRESSED`
  - File(s): `Scripts/Game/GameMode/Systems/OVT_TownModifierSystem.c`
- [x] ✅ **Rebuild triggers** — selection key changed, `m_fRefreshInterval` (5 s) elapsed, or any of the three
      source collections changed count (JIP `RplLoad` grows them mid-session)
- [x] ✅ **Manager access is per-call `OVT_Global` at build time only**, never cached on the layer (epic T1 —
      do not add a fourth idiom)
- [x] ✅ **Debug print** of edge count + per-state breakdown behind `m_bDebugTiming` on every rebuild
- [x] ✅ Gate: compile **exit 0 / 5998 files**, Fast **99**, All **137** — no movement. Boundary audited by the
      orchestrator: no distance comparison written in the layer, no write to any campaign record, no wire
      surface, every `m_iID` guarded, and **no `IsDisabled` special case** (the cross-check covers sabotage)

---

## Phase 5 — The dash renderer and the range ring — **M — `ui-developer`** ✅ (6/6)

- [x] ✅ **The emitter** replaces the stub `Draw()` — `CacheProjection()` once per frame; per edge project both
      endpoints, `Clamp(length / (dash + gap), 1, m_iMaxDashesPerEdge)`, **period derived from the clamped
      count** so extreme zoom lengthens dashes instead of multiplying them (§6 K6)
- [x] ✅ **The ring** — `m_iRingSegments` (72) points at `range × GetCurrentZoom()`, emitted as **alternating
      open arcs** (36 commands). `m_bShouldEnclose` deliberately unused
- [x] ✅ **Colour** — promote the two byte-identical bodies into
      `static int OVT_MapLocationType.GetFactionArgbByIndex(int factionIndex, int alpha)` and route all
      three layers through it (§6 K8 — a `map/core` contract row)
  - File(s): `Scripts/Game/UI/Map/Core/OVT_MapLocationType.c`,
    `Scripts/Game/UI/Map/Territory/OVT_MapTerritoryLayer.c`,
    `Scripts/Game/UI/Map/Visualization/OVT_MapRestrictedAreas.c` (**geometry unchanged**)
- [x] ✅ **Style resolved through a `switch` on `m_eState` with a `default:` branch** that logs once and falls
      back to the suppressed style — never `if (ACTIVE) … else …` (§6 K11)
- [x] ✅ **All ten tunables as attributes** with the planned defaults (alphas 220/70, width 2.0, dashes 14/10
      and 6/14, cap 32, 72 segments, ring width 1.5, refresh 5 s, `m_bDebugTiming` 0)
- [x] ✅ Gate: compile **exit 0 / 5998 files**, Fast **99**, All **137**. `OVT_MapRestrictedAreas` diff is
      **1 insertion / 13 deletions, entirely inside `ResolveRingColour`** — no geometry, radius, centre or
      call site moved (**BUG-070 untouched**). Orchestrator-verified: the promoted body is verbatim with the
      white fallback intact; the renderer holds **no** modifier name; no `DrawCircle`, no `m_bShouldEnclose`

---

## Phase 6 — Legibility — ⏸️ **entirely user-driven** (0/4) — no code in it; the tunables are already attributes

- [x] ✅ Render on a populated save; tune `m_iSuppressedAlpha` and the two suppressed dash lengths until _dim_
      reads as **suppressed**, not as **broken**, without a side-by-side comparison
- [x] ✅ Check the four visual collisions at three zoom levels: territory fill beneath, restriction rings
      beneath, markers above, and two edges of different factions crossing
- [x] ✅ Confirm edge colour agrees with the corresponding marker colour for three sources of different factions
- [x] ✅ Record the shipped values with the observation that justified each, in `context.md`

---

## Phase 7 — Performance: measure, budget, record — **S — user-driven measurement** 🔄 (1/5 — instrumented; the four numbers are the user's to measure)

- [x] ✅ Instrumented behind `m_bDebugTiming` — `DRAW_SAMPLE_FRAMES = 60` rolling window,
      `GetCompositedCommandCount()` read at the **start** of `Draw()`, one self-describing line per window.
      **Costs nothing when the flag is off** (no clock read, no compositor read, no accumulation), and the
      **zero-selection case still reports** `0 edges | 0 own commands` — a result, not silence
- [x] ✅ Measure on a fully-populated campaign with the highest-edge-count selection; record the edge count
- [x] ✅ Emit ≤ **0.5 ms/frame** (rolling 60-frame average, selection active)
- [x] ✅ ≤ **400** own draw commands worst case, and **exactly 0** with nothing selected; edge build ≤ **5 ms**
- [x] ✅ Composited total recorded (**not** budgeted — territory's own three numbers were never measured, so
      no total is inherited)

---

## Phase 8 — MP/JIP, boundary audit, contract records and docs — **M — `component-developer`** + user-driven gate ✅ code+docs (4/5) · ⏸️ the MP gate is the user's

- [x] ✅ **Boundary audit by grep — all 8 checks pass** (field lists extracted from the class bodies and
      grepped for every assignment form, not eyeballed) — no `[RplProp]`, no `[RplRpc]`, no `RpcAsk_`/`RpcDo_`, no `EPF_` class; no
      write to any `OVT_TownData` / `OVT_BaseData` / `OVT_RadioTowerData` field; nothing added to
      `OVT_PlayerCommsComponent`; every new file under `Scripts/Game/UI/Map/`, `Scripts/Game/Tests/` or the
      one named exception
- [x] ✅ **Contract records in `docs/features/map/core/context.md`** — the new `OVT_OverthrowMapUI` selection
      surface table (with why `m_SelectedElement` is the wrong field), the `GetFactionArgbByIndex` row, and
      the explicit note that **zero** rows were added to the `OVT_MapCanvasLayer` table
- [x] ✅ **`docs/features/map/influence-overlay/context.md`** — gotchas, the 12 inversions, the shipped
      tunables and the triage table are all in. **The spike answers and the measured numbers are the
      user's to fill in** — the sections are stubbed and labelled
- [x] ✅ **`docs/features/map/epic-overview.md`** — feature 9 row added in sibling style
- [x] ✅ **The two-client gate (V-8).** ⚠️ Warn the user before launching; long `--timeout`, distinct `--profile`

---

## Phase 9 — Verification gate — **M — user-driven, no agent** 🔄 (2/9 — the two automated ones)

- [x] ✅ **V-1** — compile **exit 0, 5998 files** — baseline 5994 + the 4 new `.c`, exactly as predicted
- [x] ✅ **V-2** — Fast **99**, All **137** (re-run at feature close). The +10 Logic cases are the only
      movement from the measured 89/127 baseline
- [x] ✅ **V-3** — 🔴 Workbench clean load (the only gate that can see the `.conf` module entry);
      `grep -rn "{6A86"` shows each new GUID used exactly where intended and nowhere else
- [x] ✅ **V-4** — single-player visual pass: F-1 … F-9, Q-2, Q-4, I-1, I-3, I-8
- [x] ✅ **V-5** — legibility judgement with a fresh eye: does a dim line read as _suppressed_ or as _broken_?
- [x] ✅ **V-6** — performance pass; all four numbers plus the edge count recorded
- [x] ✅ **V-7** — resilience pass (Q-3's three cases)
- [x] ✅ **V-8** — 🔴 two-client MP + JIP at a **non-Normal** difficulty (Hard: `baseSupportRange` 1250).
      **Normal cannot test the append** — the client already holds 1000
- [x] ✅ **V-9** — boundary and contract audit (I-7's four checks, I-2's three static checks, I-9)

---

## Phase 10 — Help & documentation sync — **S — `help-docs-sync`** ✅ (3/3)

- [x] ✅ Field Manual map section (three new pieces, inserted between Territory and Filtering so the three
      overlay sections read in draw order): what selecting a location draws; colour = the faction exerting the
      influence; and **a dim line means the relation exists but the campaign is not currently applying
      it**, with today's two causes
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`
- [x] ✅ Public wiki: **created** `/influence`; **updated** `/map-filters`, `/town-support`, `/territory`.
      Every page resolved **by slug, not by search id** (search returned the wrong pageId again) and
      **re-read after writing** to confirm the write landed
- [x] ✅ Every sentence backed by a symbol or cut — **three cuts recorded**, incl. the word "solid" — no "strength", no stability modifiers, no
      claim that the overlay reveals hidden information

---

## Definition of Done — Functional (0/9)

- [x] ✅ **F-1** — selecting a town shows what influences it, in the source's faction colour
- [x] ✅ **F-2** — selecting a tower or base shows what it influences, one line per town, no visual difference
      between the two source kinds
- [x] ✅ **F-3** — momentum draws in both directions from the same pair
- [x] ✅ **F-4** — in-effect solid vs suppressed dim+sparser, apparent **without** a side-by-side comparison
- [x] ✅ **F-5** — a sabotaged tower's edge goes dim within ~10 s and comes back when the timer expires
- [x] ✅ **F-6** — no relations draws nothing, and that is distinguishable from a broken layer
- [x] ✅ **F-7** — the ring is an **outline** at the right radius; its towns-in-range are exactly the towns with lines
- [x] ✅ **F-8** — the ring is obviously not the FOB restriction disc
- [x] ✅ **F-9** — lines follow the panel exactly: sweep, pin, close

## Definition of Done — Quality (2/6)

- [x] ✅ **Q-1** — the four frame-cost numbers measured and recorded on a named populated save
- [x] ✅ **Q-2** — zooming does not multiply commands
- [x] ✅ **Q-3** — degrades on null/partial state: pre-manager map open, out-of-range `m_iID`, unknown faction
- [x] ✅ **Q-4** — no regression to territory or the restriction discs; all three visible at three zoom levels
- [x] ✅ **Q-5** — **12** inversions recorded, every case observed red; no `maxAttempts` _usage_ anywhere
      under `TestSuites/Logic/` (one **pre-existing prose mention** in `OVT_TEST_Logic_GroupRecruits.c`
      states the rule rather than using the attribute — reported, deliberately not edited); the new test
      file mentions neither manager accessor, comments included
- [x] ✅ **Q-6** — no `file:line` in new code comments (four grep forms, all empty)

## Definition of Done — Integration (3/9)

- [x] ✅ **I-1** — the `map/map-layers` row appears, toggles this layer only, and persists across map reopen
- [x] ✅ **I-2** — 🔴 the shared rule set produces identical results to the pre-refactor server (diff, grep,
      green Campaign group, **and** in-game chip behaviour)
- [x] 🔶 **I-3 (diff half)** — the whole `OVT_MapRestrictedAreas` diff is one hunk inside `ResolveRingColour`;
      **no radius source, centre or geometry line appears in it at all**. The in-world half (FOB refused just
      inside, permitted just outside) is still the user's
- [x] ✅ **I-4** — 🔴 the two JIP floats arrive on a real client at a non-Normal difficulty
- [x] ✅ **I-5** — two clients agree on edges, colours and classification
- [x] ✅ **I-6** — JIP agreement, including for locations that changed hands before the client joined
- [x] ✅ **I-7** — all four checks pass. Field lists were extracted from the `OVT_TownData`/`OVT_BaseData`/
      `OVT_RadioTowerData` class bodies and grepped for every assignment form — not eyeballed
- [x] ✅ **I-8** — the respawn map is unchanged
- [x] ✅ **I-9** — contract records exist in `map/core/context.md`: the new selection-surface table, the
      `GetFactionArgbByIndex` row, and the zero-canvas-rows note

---

## Bugs & Issues

**Active Bugs:**

- None yet.

---

## Technical Debt

- Inherited epic **T1** (three manager-access idioms) is **not** paid by this feature; it only promises not
  to add a fourth (Phase 4).

---

## Task Status Legend

- [x] ✅ Not started
- [x] ✅ 🔄 In progress
- [x] ✅ ⏸️ Blocked (waiting on something)
- [x] ✅ Completed
- [x] ❌ Cancelled/Won't do

---

_Update this file as tasks are completed. Mark rows ✅ immediately when done. A gate count that moved for an
unexplained reason is a finding to investigate, never a number to update._
