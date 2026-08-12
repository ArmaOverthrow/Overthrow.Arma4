# Map Influence Overlay - Context & Decisions

**Last Updated:** 2026-08-12
**Current Phase:** ✅ Complete — closed 2026-08-12
**Status:** ✅ **COMPLETE — built and play-tested green (SP + MP) 2026-08-12, closed on the user's
instruction** (*"play-test is all green ... MP is good, feature can be marked 100%"*). Every build phase
landed in one autorun the same day; gates compile **exit 0 / 5998**, Fast **99**, All **137**.
⚠️ **Q-1 was closed on one number, not four** — see `tasks.md`'s header note. `Draw()` measured **~1 ms**
against a stated ≤ 0.5 ms budget; the user judged it fine and that call stands, but the budget was not
*met as written* and the other three numbers were never transcribed.

> **Read `implementation.md` §6 K2 first.** The feature's central decision is the **cross-check**: an edge is
> drawn ACTIVE **if and only if** (a) the geometric relation qualifies under the shared range test **and**
> (b) the corresponding modifier is actually present in the target town's replicated `supportModifiers`
> list. Otherwise it is SUPPRESSED. That single rule is what makes client-side re-derivation safe, and it
> is why the client never re-implements the enemy-wins resolution rule at all.
>
> ⚠️ **This feature's output is invisible to every automated gate.** `compile-check.sh` cannot see a pixel,
> a colour, an alpha, a dash, a `.conf` entry or an `.st` id, and neither can either test group. The build
> phases can be gated; the _feature_ cannot be. See `tasks.md`'s verification rows.

---

## Quick Status

**What's Done:**

- ✅ Planning complete — `implementation.md` records four settled decisions (D1 client re-derives + the
  two-float JIP append, D2 lines follow the **selected** element, D3 solid vs dimmed, D4 a stroke-outline
  reach ring) and eleven key technical decisions (K1–K11).
- ✅ **Phase 0** — baselines re-measured on `new-map` with the working tree as found:
  compile **exit 0 / 5994 files**, Fast **89**, All **127** (pending), highest bug id **BUG-145**,
  GUID series **`{6A86…}`** free. **Identical to the plan's recorded values.**

- ✅ **Phase 1 (build half)** — `GetPanelLocation()` + the `ShowLocationInfo` assignment; the stub
  `OVT_MapInfluenceLayer`; the `MapOverthrow.conf` module entry (**`{6A86D1E000000001}`**, draw order
  300); the `#OVT-Map_Layer_Influence` id (**`{6A86D5E100000001}`**, master only). Gate: compile
  **exit 0 / 5995 files**. **Zero rows added to the `OVT_MapCanvasLayer` contract** — it covered
  everything the spike needed.

- ✅ **Phase 2** — `OVT_InfluenceRules` (pure), `CheckUpdateModifiers` and `UpdateAllTownMomentum`
  rewritten against it **behaviour-preservingly**, and 10 Logic cases with **12** inversions run. Gate:
  compile **exit 0 / 5997 files**, Fast **99**, All **137**, Campaign 13/13 green.

- ✅ **Phase 3** — the two range floats appended to the config JIP stream (`radioTowerRange` then
  `baseSupportRange`, last in the difficulty block), matching reads in the same position,
  `CONFIG_STREAM_VERSION` **1 → 2**, and the `m_bDebugTiming` range print. Gate: compile
  **exit 0 / 5997 files**, Fast **99**, All **137** — no movement, no new files.

- ✅ **Phase 4** — `OVT_InfluenceEdge`, `BuildEdges` (one uniform path, three dispatch arms), the
  cross-check, the rebuild triggers, and `OVT_TownModifierSystem.GetModifierIndexByName`. Gate: compile
  **exit 0 / 5998 files**, Fast **99**, All **137**.

- ✅ **Phase 5** — the real emitter (clamped dashes + alternating ring arcs), the ten tunables at the
  planned defaults, the K11 `switch`/`default` style resolution, and **K8's colour promotion**:
  `OVT_MapLocationType.GetFactionArgbByIndex` now serves all three canvas layers. Gate: compile
  **exit 0 / 5998 files**, Fast **99**, All **137**.

- ✅ **Phase 7 task 1** — the rolling 60-frame `Draw()` instrumentation, the own-command count, and the
  composited total read at the **start** of `Draw()`. Costs nothing when `m_bDebugTiming` is 0, and the
  **zero-selection case still reports** (`0 edges | 0 own commands`) — a result, not silence.
- ✅ **Phase 8 code + docs** — all 8 boundary checks pass; the two `map/core` contract records and the epic
  row are written.

**What's Next — and all of it is yours, not an agent's:**

- ⏸️ **Phase 1's five spike answers** (P1–P5) — see "The spike, as it must be run" below.
- ⏸️ **Phase 6** — legibility. No code in it; the tunables are already attributes.
- ⏸️ **Phase 7 tasks 2–4** — the four measurements. **An unrecorded number fails Q-1 even if the overlay
  feels fine.**
- ⏸️ **Phase 8 task 5 / Phase 9 V-8** — the two-client MP + JIP gate, at **Hard or above, never Normal**.
- ⏸️ **Phase 9** — the whole verification gate (V-1…V-9).

### The shipped tunables (Phase 5 defaults — Phase 6 may retune the suppressed three)

| Attribute                                            | Shipped    | Why                                                                  |
| ---------------------------------------------------- | ---------- | -------------------------------------------------------------------- |
| `m_iActiveAlpha` / `m_iSuppressedAlpha`              | 220 / 70   | cue **one** for the suppressed class                                 |
| `m_fActiveDashLength` / `m_fActiveGapLength`         | 14 / 10 px | the in-effect rhythm                                                 |
| `m_fSuppressedDashLength` / `m_fSuppressedGapLength` | 6 / 14 px  | cue **two** — shorter _and_ sparser                                  |
| `m_fLineWidth` / `m_fRingWidth`                      | 2.0 / 1.5  | the ring is context; the edges are the message                       |
| `m_iMaxDashesPerEdge`                                | 32         | the command-count bound                                              |
| `m_iRingSegments`                                    | 72         | 36 emitted arcs in DASHED mode; irrelevant to command count in SOLID |
| `m_eRingMode`                                        | DASHED     | **added 2026-08-12 at the user's request** — DASHED / SOLID / NONE   |

**Two independent cues carry the suppressed class — alpha AND rhythm.** That redundancy is the whole
mitigation for R1 (a dim line reading as a _rendering fault_); do not collapse them into one.

**How the clamp bounds the command count.** The configured `dash + gap` is an _ideal_ period;
`dashes = ClampInt(screenLength / idealPeriod, 1, m_iMaxDashesPerEdge)`; the period **actually drawn** is
then `screenLength / dashes`, and the dash is `period × (dash / idealPeriod)` so the dash-to-gap
proportion — the second cue — survives the clamp. Once the cap bites, dashes **lengthen** instead of
multiplying.

🔴 **Predicted worst case for Phase 7 to measure against: `32 × E + 36` commands**, E = the selection's
edge count. Against the ≤ 400 budget that holds to **E = 11**; **at E = 12 it is 420 and the budget is
missed**, whereupon K6's ordered fallback starts with lowering `m_iMaxDashesPerEdge`. A tower reaching
8 towns = 292; a dense-cluster town with 8 edges and no ring = 256.

### `m_eRingMode` — the ring is switchable, and SOLID is nearly free (added 2026-08-12)

The user asked whether the ring should be dropped or made switchable. It is now a three-way attribute
rather than a bool, because **the cheap option is not "off"**:

| Mode                 | Commands                       | Notes                                                                                                                                                                                                                                                 |
| -------------------- | ------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **DASHED** (default) | `m_iRingSegments / 2` = **36** | matches the edges' idiom; what D4 asks for                                                                                                                                                                                                            |
| **SOLID**            | **1**, at any segment count    | one enclosed polyline via `m_bShouldEnclose` — K6's recorded degradation. **Still an OUTLINE**: a `LineDrawCommand` strokes its vertices and never fills them, so the map stays visible through the interior either way. Costs only the dashed rhythm |
| **NONE**             | **0**                          | also removes **F-6's aliveness affordance** — the ring is what tells a viewer the layer is working when a source influences nothing                                                                                                                   |

🔴 **The ring is not where the cost is.** It is a flat 36 commands; the _edges_ are up to 32 **each**.
Dropping the ring saves 36; dropping `m_iMaxDashesPerEdge` from 32 to 16 saves `16 × E`. If the budget is
ever missed, **switch the ring to SOLID (−35) and halve the dash cap** rather than reaching for NONE.

Switched with a `default:` branch that warns once and falls back to DASHED, per K11 requirement 2.

### Three Phase 5 calls beyond the plan's letter

1. **`m_iRingFaction` was added to the derived state.** The plan requires a faction-coloured ring but
   Phase 4 stored only centre and radius. It initialises/resets to `-1`, which falls back to white through
   the shared helper.
2. **The ring draws at `m_iActiveAlpha`** — no ring-alpha attribute was invented (K11 requirement 5). The
   ring is the selected source's own reach and is never "suppressed". If Phase 6 wants it dimmer, that is
   a **new attribute**, not a tuning.
3. **Two guard constants, deliberately not tunables:** `MIN_SCREEN_LENGTH 2.0` (skip a degenerate edge or
   ring that projects to a point when fully zoomed out) and `MIN_DASH_PERIOD 1.0` (a zeroed dash+gap pair
   would divide by zero). Floors on config, not style.

### Five calls made where the plan was underspecified (Phase 4)

1. **The support modifier system is unresolvable.** The plan says "never skip the cross-check" but not what
   to do when the _system itself_ is null. Edges are built anyway; every index resolves to `-1`, so every
   edge comes out **SUPPRESSED** — the conservative direction, since the overlay under-claims rather than
   asserting an influence it cannot confirm — plus **one unconditional `WARNING`**, because that state is
   otherwise indistinguishable from a styling bug.
2. **Where the rebuild decision lives.** §3.4 says no manager read on the per-frame path, but the
   collection-count trigger _is_ by definition a per-frame manager read. `ShouldRebuild` does three static
   accessor calls + `Count()` per frame. Judged cheaper than the alternative — folding the count check into
   the 5 s tick would make the trigger useless. **A deliberate, small breach of §3.4's letter**, recorded
   rather than hidden.
3. **Momentum polarity is derived, not asserted.** Rather than hardcoding POSITIVE for `MOMENTUM_TOWN`, the
   momentum arm runs the identical `sourceFaction == occupyingFaction` test as towers and bases; a
   player-held source town is never the occupier, so POSITIVE falls out. One uniform path, and **K11
   requirement 3 holds with no source-kind ⇒ polarity assumption anywhere in the layer**.
4. **"Is player-held" is expressed as `!TownQualifiesForMomentum(...)`** rather than a fresh `==` against
   the player faction index. Slightly indirect, but it keeps the layer free of a second implementation.
5. **A source that influences no town still draws its ring.** Not stated in the plan; drawn, because
   **F-6 relies on the ring as the "the layer is alive" affordance**.

The three existing inline name scans in `OVT_TownModifierSystem` (`TryAddByName`, `RemoveByName`,
`GetModifierSpaceByName`) were **deliberately not** rewritten through the new helper — that is server-tick
code and this feature is not the place to touch it.

> ⚠️ **V-8 step 3 needs one config edit that is easy to forget.** `m_bDebugTiming` defaults to **0**, so
> the range print is silent. Add `m_bDebugTiming 1` inside the `OVT_MapInfluenceLayer "{6A86D1E000000001}"`
> block of `Configs/Map/MapOverthrow.conf` before the MP run. **If it is missed, an absent print reads as
> a failed append.** Server and client both load the conf from the working tree, so one edit covers both.
>
> The print includes the **difficulty name**, deliberately — it travels in the same stream ahead of both
> ranges, so it splits the failure: name `Hard` with range 1000 means _these two appended values_ are at
> fault; name `Normal` on a Hard server means _the whole stream_ never landed. Without it a failed V-8 is
> a guess.
>
> ```
> [OVT_Influence] ranges: radioTowerRange 1500 m, baseSupportRange 1250 m (difficulty 'Hard')
> ```

- ⏸️ **Phase 1's five spike answers are user-run** and are the phase's entire point. See
  "The spike, as it must be run" below.

**Blockers:**

- None. The user-driven observation phases (1's spike answers, 6, 7, 9 and V-8 in 8) cannot be run
  headlessly and are tracked as verification rows rather than as blockers.

---

## Key Files

### Core Implementation

- `Scripts/Game/GameMode/Systems/Modifiers/OVT_InfluenceRules.c` — **NEW.** The shared, pure rule set:
  range tests, polarity, the source→modifier name mapping and the enemy-wins resolution. Deliberately
  filed under the campaign's own folder, not under `UI/Map/` — **it is the campaign's rule set and the
  map is its second caller, not its owner** (§3.5).
- `Scripts/Game/UI/Map/Influence/OVT_MapInfluenceLayer.c` — **NEW.** Poll → derive → emit.
- `Scripts/Game/UI/Map/Influence/OVT_InfluenceEdge.c` — **NEW.** A `Managed` record that carries its
  modifier as **data**, so the renderer holds no modifier name and no source-kind ⇒ modifier assumption.
- `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Influence.c` — **NEW.** The feature's only
  automatable surface.

### Modified

- `Scripts/Game/UI/Map/OVT_OverthrowMapUI.c` — `GetPanelLocation()`; `m_PanelLocation` also set in
  `ShowLocationInfo`.
- `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c` — `CheckUpdateModifiers` via the rules,
  **behaviour-preserving**.
- `Scripts/Game/GameMode/Systems/Modifiers/Support/OVT_RevolutionaryMomentumSupportModifier.c`
- `Scripts/Game/GameMode/Systems/OVT_TownModifierSystem.c` — one read-only, client-safe name→index helper.
- `Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c` — +2 floats, `CONFIG_STREAM_VERSION` 1→2.
- `Scripts/Game/UI/Map/Core/OVT_MapLocationType.c` — `+ static GetFactionArgbByIndex` (K8).
- `Scripts/Game/UI/Map/Territory/OVT_MapTerritoryLayer.c`,
  `Scripts/Game/UI/Map/Visualization/OVT_MapRestrictedAreas.c` — routed through it. **Restricted-area
  geometry is untouched** (BUG-070 must not regress).
- `Configs/Map/MapOverthrow.conf`, `Language/localization_Overthrow.st` (master only).

### Related

- `docs/features/map/core/context.md` — owes two contract records (Phase 8).
- `docs/features/map/territory-overlay/context.md` — the canvas-layer/compositor machinery this stands on.
- `docs/features/map/map-layers/context.md` — the free toggle row this claims to get for nothing.

---

## Important Decisions

Settled with the user before planning and restated in `implementation.md` §4. **They are not open.**

- **D1** — the client **re-derives**; no source attribution is recorded server-side. Plus the two-float
  versioned append to the config JIP stream.
- **D2** — lines follow the **selected** element (hover or pin), so lines and the info panel always agree.
  Shipping with **no fade and no dwell** — a timing behaviour on the lines alone would make them disagree
  with the panel, which is the one property D2 exists to guarantee.
- **D3** — in-effect edges **solid**, suppressed edges **dimmed**; both dashed, both faction-coloured. The
  suppressed class is defined by the **general rule** (K3), never by today's two causes.
- **D4** — a selected tower or base also draws its reach as a **stroke outline only**, no fill.
- **D-JIP** — 🔴 **a client holds `Difficulty_Normal.conf`'s values, not the attribute defaults**, because
  the game-mode prefab instantiates `m_Difficulty` from that file. So client and server **agree at Normal
  and disagree everywhere else** — the MP verification must run at **Hard** (1250) or Extreme/Insane
  (1500). A run at Normal proves nothing.

---

## Gotchas & Learnings

### 1. `m_SelectedElement` is the wrong field to drive the lines from

**Problem:** it is **sticky** — neither `HideLocationInfo` nor `ForceHideLocationInfo` clears it, so lines
driven from it would outlive the panel.
**Solution:** `m_PanelLocation`, exposed as `GetPanelLocation()`. It _is_ the panel's location: set when a
panel is built, nulled by both hide paths and by `OnMapClose`, and re-pointed at the fresh record by the
BUG-136 refresh reconciliation.
**Lesson:** agreement with the panel is worth getting **by construction** rather than by parallel reasoning.

### 2. The `NearbyBase*` branch's missing `else` is deliberate and correct

**Problem:** it reads like a bug next to the tower branch, which has one.
**Solution:** `m_Bases` is insert-only, `OVT_BaseData` has no disabled state and bases do not move, so
"in range → not in range" is unreachable. The tower branch needs its `else` precisely because sabotage
_can_ take a tower off the air.
**Lesson:** do not file it, do not code around it, and do not assert the invariant in a code comment
(K11 requirement 1) — a future job that disables a base must not have to undo anything.

---

### 3. `LineDrawCommand.m_UVScale` is a **vector**, not a float

**Problem:** `PolygonDrawCommand.m_fUVScale` — which `territory-overlay` dialled in by eye for its hatch —
is a float. `LineDrawCommand`'s equivalent is a **`vector`** (confirmed in vanilla's `EnWidgets.c`).
**Lesson:** §6 K6's rejected textured-single-command fallback **cannot** port the territory layer's
scale idiom directly. It was already a probe rather than a tweak; this makes it slightly more of one.
Everything else the plan assumes about `LineDrawCommand` (`m_iColor`, `m_fWidth`, `m_fOutlineWidth`, the
flat float `m_Vertices`, `m_bShouldEnclose`) is exactly as written.

### 4. `ShowLocationInfo` nulls `m_PanelLocation` at the top before rebuilding the panel

**Consequence:** a hover sweep produces **null → value** transitions rather than value → value ones. Good
for observing P2, but a future caller must not treat a single null frame as "the user deselected".

---

### 5. 🔴 `vector.Distance` is **not** a correctly-rounded square root — and it changed two test cases

**Problem:** measured on this build, `Distance` returns **exactly** the true value at 1, 5, 100, 1024,
1500 and 2048 m — but at **1000 m** and **2000 m** it returns the true value **+ 1 ULP**.
Two consequences, both real:

1. **Case 2 as planned was toothless.** At range 1000 the measured 1000.000061 fails `<` _and_ `<=`, so
   the `<` → `<=` inversion would not have reddened it. The case now uses **1500 m**, a separation
   measured exact, and the inversion does redden it.
2. **Case 4's boundary is unreachable through world geometry.** No arrangement of points makes `Distance`
   return exactly 2000.0 — the Pythagorean form `(1200, 0, 1600)` gives the identical result. Through
   `IsMomentumSource(vector, vector)` alone, `<=` and `<` are **the same function**, so a case built that
   way pins nothing while looking authoritative. The comparison was split out as
   `IsWithinMomentumRange(float distance)`, with `IsMomentumSource` delegating to it; case 4 asserts
   inclusiveness there, plus the two-position form at 1999/2001.
   **This is one function beyond the API `implementation.md` §5 Phase 2 specifies**, and it is why.
   **A campaign fact falls out of it, and it is not a defect:** two towns exactly 2000.000 m apart do **not**
   exchange momentum, because the measurement puts them just outside. That was already true before this
   feature; nothing changed.
   **Lesson:** a boundary case is only evidence if the boundary is _reachable_. Probe the engine's actual
   return before writing a test that asserts an exact-equality edge.

### 6. `SetResultFailure` has a hard parameter limit

Five format args gives `error: Too many parameters`. Four is fine. (Hit independently in two phases.)

### 7. 🔴 `ScriptBitWriter` / `ScriptBitReader` **cannot** be round-tripped from script — measured, not inferred

**Do not spend another session on this.** `new ScriptBitWriter()` **compiles clean** and comes back
**non-null**, which is exactly why it looks testable. The first proto call then kills the process:

```
[PROBE] step 2: writer non-null = 1
ENGINE (F): Crashed
Access violation. Illegal read ... at 0x28   ← in Execute, at writer.Tell()
```

The script object is a wrapper with no engine-side storage behind it, and the first dereference of that
storage crashes. A round-trip case would be a **hard crash on every run**, not a red test.
Three corroborating facts from vanilla's `EnNetwork.c`:

- Neither class exposes any way to move bits between them — no shared buffer type, no factory, no
  `Rewind`/`Reset`/`Seek`, only `Tell()`. Even a working writer's bits could never reach a reader.
- The engine ships that pattern where it _is_ intended: `SSnapSerializer.MakeWriter/MakeReader` over a
  shared `SSnapshot`, with `SSnapSerializerBase`'s constructor `private`. The bit streams have no
  equivalent because they are not meant to be constructed.
- `grep` for `new ScriptBit` across the base game, EPF and EDF: **zero** hits.

**Consequence:** the JIP append has **no automated coverage and cannot have any.** Its evidence is V-8
step 3 — reading the printed value off a real client. The symmetry lint was deliberately **not** built
(it is a plausible future `dev-ops` tool, not this feature's job). What was done instead was a one-off
inline check: the full positional type sequence extracted from both methods and diffed slot by slot —
**26 writes, 26 reads, every type matching**, the two new floats at slots 20 and 21.

### 8. 🔴 "SOLID" in D3 means **full alpha**, not a solid line — and player-facing text must not say "solid"

**Both** edge classes are dashed; D3's own wording says so ("Both dashed, both faction-coloured") and §1's
phrase is "solid-**alpha**". `ResolveStyle` gives ACTIVE the **longer 14/10 rhythm** at alpha 220 and
SUPPRESSED the **shorter, sparser 6/14** at alpha 70. The shipped code is correct.
**Two consequences:**

1. **Do not "fix" the renderer to draw an unbroken line for active edges.** It is not a defect.
2. **The Field Manual and wiki say "bright, in long dashes" vs "dim, in shorter and sparser dashes"** —
   never "solid". A player told "solid" and shown a dashed line concludes the overlay is broken, which is
   precisely R1, the feature's highest risk, arriving through the help text instead of the render.

### 9. The version bump aborts the **entire** config load on a mismatched client

Not just the two new floats — the whole difficulty block, `mobileFOBOfficersOnly` and all three item
limits, with **one ERROR line as the only symptom**. That is the designed BUG-078 behaviour and it is what
makes a mid-stream insert safe, but it hard-requires **server and client on the same build**.
`tools/launch-server.sh` runs the working tree, so V-8 gets that for free. A stale client presents as
**"nothing replicated"**, not as "the ranges are wrong" — worth knowing before debugging the wrong thing.

---

## The spike, as it must be run (Phase 1, P1–P5)

The `.conf` and `.st` edits are invisible to `compile-check.sh`. In Workbench Play mode, on a populated
campaign, watch the console for `[OVT_Influence] selection: <typeName>|<id>` (or `none`), printed **once
per selection change**:

1. **P1** — open the map. If it prints `none` forever while a panel is visibly up, the lazy re-resolve of
   `OVT_OverthrowMapUI` is not recovering. That is the bridge failing, not the derivation.
2. **P2** — sweep the cursor over town / base / radio-tower markers: one line per new panel; hovering off
   prints `none`. Then **pin** a marker and move away — the printed selection must **stay**. Press the
   panel's close button — it must go to `none`.
3. **P3** — with a selection up, a **magenta** straight line runs 1000 m east of the marker, **above** the
   territory wash and the restriction rings, with both still visible. (Magenta is deliberate: no other
   layer draws it, so "is that mine?" is never a question.)
4. **P4** — zoom fully out and fully in: the endpoints separate and converge with the map, but the line's
   **thickness must not change**.
5. **P5** — open the map-layers panel: an **"Influence"** row is present (it renders as the raw key
   `#OVT-Map_Layer_Influence` until the localization exports are regenerated). Toggling it off hides the
   magenta line **only** — territory and the restriction discs keep drawing.

---

## Testing Approach

| Tier            | Covers this feature?                                                                                                                          |
| --------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| **Logic**       | ✅ **The only automatable surface.** 10 world-free cases over `OVT_InfluenceRules`.                                                           |
| **Init**        | ⚠️ Declined — YAGNI. A case would assert the config component, which already has coverage.                                                    |
| **Campaign**    | ⚠️ Existing cases only — but they are the **only** automated evidence that Phase 2's rewrite of the 10-second modifier tick did not break it. |
| **Persistence** | ❌ None, deliberately. This feature persists nothing; a relevant case would mean the boundary had been breached.                              |

**The inversions — 12 run, every one of the 10 cases observed red.** Each was applied alone,
compile-checked, run, then reverted and md5-verified before the next.

| Inversion                                                      | Cases red                     |
| -------------------------------------------------------------- | ----------------------------- |
| `<` → `<=` in `IsProximitySource`                              | **2** only (1 green)          |
| `vector.Distance` → `vector.DistanceXZ`                        | **3** only (1, 2 green)       |
| `<=` → `<` in the momentum comparison                          | **4** only                    |
| swap `hasEnemy`/`hasFriendly` precedence in `ResolveProximity` | **5** only (6 green)          |
| invert `PolarityForSource`                                     | **5 and 7** — it does compose |
| typo `"NearbyBaseNegative"` → `"NearbyBaseNegatvie"`           | **8** only                    |
| `!=` → `==` in `TownQualifiesForMomentum`                      | **9** only                    |
| `MOMENTUM_RANGE` → 2500                                        | **4 and 10**                  |
| `<` → `>` in `IsProximitySource` _(added)_                     | **1, 2, 3**                   |
| fold NONE into POSITIVE in `ResolveProximity` _(added)_        | **6** only                    |

The plan's list of eight left **cases 1 and 6 never seen red**; the last two inversions close that.

⚠️ **One flake, recorded rather than smoothed over:** the `polarity_invert` run timed out at 300 s with no
junit on its first attempt — **INDETERMINATE, not a result**. Re-run unchanged it completed in 8 s with the
result above. Nothing in that inversion is reachable outside the test; a stale client process, not a
property of the code.

**Where the five id string literals live:** `OVT_InfluenceRules.c` is the single **implementation** home
(five `static const string`). `OVT_TEST_Logic_Influence.c` carries case 8's **expected values** as literals
deliberately — comparing `ModifierNameFor`'s answer to the constant it returns would assert nothing, so the
test's independent copy is what makes a rename detectable. The criterion's intent (no second
_implementation_ can drift) holds. Three job files mention the names **in comments** — prose, not
literals, left untouched.

---

## Debugging: where to look when it doesn't work

| Symptom                                                        | Most likely cause                                                                                                                            | First check                                                                                                                               |
| -------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| **Nothing draws, ever**                                        | `GetPanelLocation()` returns null — the UI component was not resolved, or the getter was never wired                                         | The Phase 4 debug print of the selection key. Null every frame ⇒ the bridge; non-null with zero edges ⇒ the derivation                    |
| **Everything draws dim**                                       | The cross-check fails for every edge — name→index returned `-1`, or the **stability** modifier system was fetched instead of the support one | Print the resolved index for one known-good edge                                                                                          |
| **Edges on the host but not the client**                       | The range values disagree — the Phase 3 append is missing, mis-ordered, or the client is a stale build                                       | The `m_bDebugTiming` range print on both machines                                                                                         |
| **Lines render but territory or the restriction discs vanish** | The **compositor** is not composing                                                                                                          | `GetCompositedCommandCount()` at frame start. **This reads exactly like a broken influence layer and will be debugged in the wrong file** |

---

## Session Notes

### 2026-08-12 — CLOSED. Play-tested green, SP and MP, on the day it was built.

The user ran the gate and reported *"play-test is all green. I ran the timing and it was 1ms so no problem
there. MP is good, feature can be marked 100%"*, then asked to close it.

**What that discharges, and it is nearly everything:** the selection bridge and all five spike questions
(in their final form — the stub was long gone by then), the render, the two-cue suppressed class, the ring,
the layer-toggle row, the JIP append on a real client, and every multiplayer criterion. **The Phase 5
tunable defaults shipped unchanged** — the legibility loop the plan budgeted for, and that
`territory-overlay` needed twice, was not needed once here.

**What it does not discharge, recorded rather than rounded up:** three of Q-1's four numbers. Only
`Draw()` ~1 ms was reported, against a stated ≤ 0.5 ms budget. The user's "no problem" is a sound
engineering call — 1 ms is a sixteenth of a 60 fps frame — but it is a *judgement*, not the measurement the
criterion asked for, and `System.GetTickCount`'s integer resolution makes the single figure ambiguous
anyway. **Consequence for whoever builds canvas layer four: there is still no recorded frame-cost baseline
in this epic.** `territory-overlay` left the same gap and this feature has now repeated it.

**One change landed after the build and before the play-test**, at the user's request: `m_eRingMode`
(DASHED / SOLID / NONE), because they were weighing dropping the ring on cost grounds. Worth keeping in
mind that the premise was off — the ring is a flat 36 commands while the *edges* are up to 32 **each**.

**A Reforger-side finding also came out of that exchange** and is worth carrying into
`docs/bugs/reforger/`: a stroked circle is **already possible** (`LineDrawCommand.m_bShouldEnclose`), so
that is not worth requesting. The genuine gap is that **`LineDrawCommand` has no dash/gap/phase parameter
at all** — one field would collapse this whole feature to roughly one command per edge. The cheaper ask is
merely to **document `m_UVScale`'s semantics** (whether UVs derive from screen-space vertices, which
decides whether a dash texture slides as the map pans) — `territory-overlay` hit that same unknown
independently, so it is a two-feature complaint rather than a one-off.

### 2026-08-12 — the whole build, one autorun (`/autorun-feature map/influence-overlay`)

Phases 0–5, 7's instrumentation, 8's code+docs and 10 landed in sequence, each gated before the next
started. Final gates re-run at close: compile **exit 0 / 5998** (baseline 5994 + the 4 new `.c`), Fast
**99**, All **137** — the +10 Logic cases are the only movement from the measured baseline.

**Three things a later reader should not have to rediscover:**

1. **`vector.Distance` is not correctly rounded** (gotcha 5) — it reshaped two test cases and added one
   function to the rule set.
2. **`ScriptBitWriter` cannot be round-tripped from script** (gotcha 7) — measured by crashing the game,
   so nobody has to do it twice. The JIP append has **no** automated coverage and cannot have any.
3. **"Solid" means full alpha, not an unbroken line** (gotcha 8) — both edge classes are dashed. Do not
   "fix" the renderer, and never write "solid" in player-facing text.

**What the next session must do:** nothing in the code. Run the verification gate — the spike answers
first (they are cheap and they de-risk everything after them), then the SP visual pass, then the two-client
MP run at **Hard**. Record the numbers as they come; an unrecorded number fails Q-1 even if the overlay
feels fine.

### 2026-08-12 — Phase 0

- `/autorun-feature map/influence-overlay` started. Resolved to the nested epic path; epic context loaded
  (`epic-overview.md` + siblings).
- Baselines **run, not quoted**: compile **exit 0 / 5994 files**, Fast **89**. Tree as found carries two
  untracked doc paths (`docs/features/logistics/`, this feature's `implementation.md`) and nothing else.
- `tasks.md` and `context.md` scaffolded from `implementation.md` §5 and §8.

---

_Update this file at the end of each work session. Where this file and `implementation.md` disagree, **this
file is the built feature**._
