# Map Territory Overlay - Context & Decisions

**Last Updated:** 2026-08-11
**Current Phase:** ✅ Complete — closed 2026-08-11
**Status:** ✅ **COMPLETE** — play-tested green (SP + MP) by the user 2026-08-11 (_"play-test and MP all green, no issues, looks great"_), closed on their instruction. ⚠️ **Q-1's three frame-cost numbers were never recorded** — the play-test discharges Q-2, not Q-1. See `tasks.md` for the three rows deliberately left open.

> 🔴 **READ D12 FIRST. The core geometric representation was REPLACED on 2026-08-11.** Territory is no
> longer a star polygon of ray radii about each site — it is an **ownership grid**, with the fill merged
> out of it and the border traced through it by marching squares and rounded with Chaikin corner
> cutting. Everything about rays, march steps, closed-form takeover radii and smoothing exemptions is
> **deleted**, along with the seven test cases that pinned it. **Any sentence anywhere in this file about
> a ray, a radius, a march step or a smoothing exemption describes code that no longer exists** — it is
> kept because the _reasons_ still matter and D12 explains which of them survived the change.
>
> **Read this next if you are new to the feature.** Everything below Phase 4 was shaped by user
> decisions taken _after_ looking at a render — **D6** (no maximum influence radius), **D7** (neutral
> bands only on inter-faction frontiers), **D9** (flat alpha, same-faction cells tessellate) and **D10**
> (**the overlay draws occupier ground only**, contested is support-driven, coastlines are not smoothed)
> and **D11** (**contested ground is HATCHED**, restricted rings take the **faction's** colour, and the QRF
> is distinguished by hatch _scale_ rather than by hue).
> They **override `implementation.md` and `requirements.md`**, which still describe the planned feature —
> D10 in particular **overrides §6 K8 by name**. Where the plan and this file disagree, **this file is the
> built feature.** **D6, D7, D9's intent, D10 and D11 all survive D12** — see D12 for exactly how each one
> is now expressed.

---

## Quick Status

**What's Done:**

- Planning complete — `implementation.md` §4 records five decisions settled with the user on 2026-08-11
  (D1 weighted sites incl. FOBs/towers, D2 coastline clipping in, D3 threat grid **deferred**,
  D4 textured fills for both rings and bands, D5 no in-feature toggle).
- ✅ **Phase 0** — baselines re-measured on `new-map` + working tree; **identical to the plan's recorded
  values** (compile exit 0 / 5964 files, Fast 54, All 89, BUG-144, `{6A84…}` free).
- ✅ **Phase 1** — `OVT_MapCanvasCompositor` + the additive `OVT_MapCanvasLayer` contract
  (`m_iDrawOrder` / `m_sLayerId` / `m_sDisplayName` / `m_bVisible` + `SetLayerVisible`,
  `CacheProjection` / `ProjectWorld`, optional texture on `DrawCircle`, register/unregister lifecycle,
  the `Count() > 0` guard removed). `MapOverthrow.conf` gives the restricted-areas layer
  `m_iDrawOrder 200` / `m_sLayerId "restricted"`. Gates: compile **exit 0 / 5965 files**, Fast **54**,
  All **89**. **The compositor has never been executed** — its proof is I-1/I-1b/I-2, all user-driven.

- ✅ **Phase 2 — run by the user 2026-08-11 and settled.** P4 PASS (max error 0–1.41 px, pure integer
  truncation, **zero basis error** — R4 retired), P3 PASS (`TriMeshDrawCommand` implemented ⇒ **rung 1**),
  P1 PASS (`m_pTexture`/`m_fUVScale` work ⇒ hatching viable). P2 rendered but its decisive detail was not
  distinguished — recorded as unknown, and made irrelevant by rung 1.
- ✅ **Phase 3** — `OVT_TerritorySite`, `OVT_TerritoryCell` + `OVT_TerritoryStop`, `OVT_TerritorySolver`
  (weighted `OwnsPoint`, `SolveRay` march + bisection refine, shrink-only circular `SmoothRadii`,
  candidate rival lists) and **10 Logic cases, every one proven able to fail** with the inversion recorded
  below. Gates: compile **exit 0 / 5970 files**, Fast **64**, All **99**.

- ✅ **Phase 4** — `OVT_MapTerritoryLayer` + `OVT_TerritorySiteConfig`, the full config surface, the four
  collectors (camps excluded, with the privacy reason in the comment), `HashSites` + the O(sites) recolour
  tick, and `EmitCell` over all three rungs with the TriMesh fan as the shipped default. `MapOverthrow.conf`
  carries the layer entry (`{6A84C7D2E1F30A64}`) and four site-type records — **base 1.6 > town 1.0 >
  radiotower 0.7 > FOB 0.55**, reach derived at 1500 m per weight unit. Gates: compile **exit 0 / 5972
  files**, Fast **64**, All **99**. I-4's boundary greps all clean. ⚠️ **The 1500 m-per-weight reach
  derivation was removed by D6** — `m_fRadiusPerWeight` no longer exists and the weights above are now
  purely _relative_, deciding where boundaries sit rather than how far a cell reaches.

- ✅ **D6 applied** (user decision after the first render, see below) — cells are no longer clipped to a
  reach. `m_fMaxRayLength` on the solver is a **world-diagonal safety bound** derived in `SetWorld` from
  `BaseWorld.GetBoundBox`; `m_fMaxRadius` on a site type is an optional cap where **0 = unlimited** and
  nothing shipped sets it; `m_fRadiusPerWeight` is deleted. The ownership test is now **sqrt-free**
  (cross-multiplied squared distances and squared weights) and the candidate bound is the **proven**
  `d(S,R) <= maxRadius × (1 + w_R/w_S)`, which is exhaustive at the world diagonal and only bites when a
  type is capped. Rays also bail on leaving the world bound box. Gates: compile **exit 0 / 5971 files**,
  Fast **64**, All **99** — all three identical to the pre-D6 baseline.

- ✅ **Phase 5** — the K6 shared colour helper (one implementation, three fallbacks preserved), contested
  alpha 30→95, neutral bands off the `RIVAL` stop reason, the texture path behind `m_bUseTextures 0`, the
  restricted-ring restyle with geometry untouched, and two `.st` ids (master only). Gates: compile
  **exit 0 / 5971 files**, Fast **64**, All **99**.
- ✅ **D7 applied** (user finding after the second render, see below) — the solver now records **which
  site** stopped each ray (`OVT_TerritoryCell.m_aStopSite`, parallel to the radii, `-1` where no rival
  won) and the band rule became the pure static `OVT_TerritorySolver.IsFrontierStop`, which the layer
  delegates to. A `RIVAL` stop only earns a band when the winner's faction differs from the cell's, so
  same-faction neighbours read as one continuous territory. **No geometry moved** — a ray stops at the
  same radius whichever neighbour won it. Gates: compile **exit 0 / 5971 files**, Fast **65**, All
  **100** — the +1 is the new case 11 and is the only expected count change.
- ✅ **Phase 8 task 1 pulled forward** — the Phase 2 probe layer and its conf entry deleted 2026-08-11 at
  the user's request (it was drawing over their map). Grep clean. **This is why the file count is 5971 and
  not 5972.**
- 🟡 **Phase 6 — the CODE half is done, the MEASUREMENT is not.** Three levers taken (see D8 below):
  a coarser march step, LEVER B's threshold-sorted rival scan, and LEVER C's **closed-form rival
  boundary**, which removes the per-march-step rival test outright. The instrumentation the measurement
  needs is in and gated. Gates: compile **exit 0 / 5971 files**, Fast **66**, All **101** — the +1 is
  case 12 and is the only expected count change. ⚠️ **No number in the table below has been measured**,
  and nothing here ticks Q-1.

- ✅ **Phase 7 — the AUDIT half only.** All four collector sources verified replicated to clients, the
  `HashSites` split verified correct against K9, the camp exclusion re-verified, and I-4's boundary greps
  re-run clean. Findings **P7-A** … **P7-D** below. ⚠️ **This was a code reading. Not one byte crossed a
  wire** — the two-client gate (I-5/I-6/I-7) is untouched and is still owed.
- ✅ **Phase 8 — docs and contract records.** Probe removal re-verified by grep; the `OVT_MapCanvasLayer`
  contract rows written into `docs/features/map/core/context.md`; `map-layers/requirements.md`'s open scope
  question replaced with K1's generic-registration answer plus both caveats; the epic overview's feature-6
  row, rollup and Tech Debt (D3's threat grid, accepted 2026-08-11) updated. `help-docs-sync` **not run** —
  it is a separate agent and a separate pass.
- Gates at the end of Phase 8: compile **exit 0 / 5971 files**, Fast **66**, All **101** — all three
  unchanged from the end of Phase 6, which is the expected result for a docs-and-audit phase.
- ✅ **D10 applied** (user decision, the reframing — see below) — the overlay now draws **occupier-held
  ground only** (`m_bOnlyShowOccupying` 1), **contested** means occupier-held **and** support ≥ 50 %
  (`m_fContestedSupportThreshold` 0.5, `m_iContestedAlpha` 35 against a flat 70), and **coast-stopped rays
  are no longer smoothed**. D9's stability lerp is **deleted**, not hidden — stability no longer reaches any
  visual. **This overrides §6 K8**, with the reasoning recorded in D10 below; it is the whole justification
  and must not be lost. Gates: compile **exit 0 / 5971 files**, Fast **70**, All **105** — the +3 are cases
  14, 15 and 16 and are the only expected count change.
- ✅ **D11 applied** (user decision, and the **first change made with the art in the tree** — see below) —
  the diagonal hatch is wired to **contested regions only** (held stays solid), `m_iContestedAlpha`
  **35 → 95** because a hatch lays down less ink than a solid fill, `m_bUseTextures` defaults **on**,
  restricted rings take the **owning faction's** hue through the shared K6 helper with both alpha tiers
  kept, and the QRF is hatched at a different scale rather than recoloured — because **no fixed alarm hue
  is reliable against a configurable faction palette**. Ring **geometry is byte-identical**, proven by
  diff. Gates: compile **exit 0 / 5971 files**, Fast **70**, All **105** — unchanged, and expected to be:
  colour and texture are not assertable in the Logic tier. ⚠️ **Unrendered.**
- ✅ **D9 applied** (user finding after the third look, with a screenshot — see below) — fill alpha is now
  **flat per faction** (`m_iFillAlpha` 70, with K8's per-site lerp preserved behind `m_bContestedShading 0`),
  and the shrink-only smoothing clamp is **exempted on rays that ended against a same-faction neighbour**, so
  two cells of one colour tessellate on their shared boundary instead of both retreating from it. **This waives
  DoD F-6** — recorded as a waiver in D9, not dropped. Gates: compile **exit 0 / 5971 files**, Fast **67**, All
  **102** — the +1 is the new case 13 and is the only expected count change. **Case 7's body changed** and was
  re-proven with two inversions.

- ✅ **D12 applied** (user decision after the third failed render — see below) — the **core geometric
  representation was replaced**. Territory is an ownership grid; the fill is merged rectangles and the
  border is a traced, Chaikin-smoothed contour. Gaps and overlaps are now **structurally impossible**, and
  bays, inlets, holes and offshore islands need no special case. The ray-march, D8's closed form and the
  D9/D10 smoothing exemptions are **deleted with their seven test cases**. Gates: compile **exit 0 / 5971
  files**, Fast **71**, All **106** — the +1 is the net of seven cases deleted and eight added.
- ✅ **D13 applied** (user decision after the **fourth** render — the first one they liked, _"a massive
  improvement overall"_ — see below) — **the fill is now cut out of the same smoothed contour the band is
  offset from**, as a trapezoid scanline decomposition, so one region has ONE boundary instead of two that
  disagreed. Holes and disjoint components fall out of even-odd pairing. Regions are traced **per
  appearance** rather than per faction, with the faction folded into the appearance key to protect D7. The
  ownership grid's origin is **snapped to a multiple of the square size from world zero**, so it lands on
  the game's own map grid lines. D12's merged-rectangle fill survives as the `m_bTraceContours 0`
  fallback. Gates: compile **exit 0 / 5971 files**, Fast **74**, All **109** — the +3 are cases 18, 19 and
  20 and are the only expected count change. ⚠️ **Unrendered.**

**What's Next — all three need a human, and nothing else is blocking:**

1. **Q-1, by the user** — one map open on a populated save with `m_bDebugTiming 1`. The paste-back
   procedure is in **Measured numbers (Phase 6)** below. **This is the only thing standing between the
   feature and a verdict on whether it is shippable.**
2. **The two-client / JIP gate (I-5, I-6, I-7)** — `tools/launch-server.sh` plus two
   `tools/launch-game.sh --timeout 3600 --profile <name> --allow-concurrent -- -client 127.0.0.1:2001`
   clients. ⚠️ **Warn before launching** — client windows open on the user's desktop and can orphan.
3. **Phase 9** — the full verification method, and the hatch art before F-7/V-6 can be judged.

**Blockers:** none. ⚠️ **The overlay has been seen three times**, every time by the user on 2026-08-11, and
**every look produced a design change**: "very splotchy, parts of the map not filled" → **D6**, then "neutral
zones between areas controlled by the same faction" → **D7**, then a screenshot of a one-faction island reading
as mottled noise → **D9**, and then a reframing of what the overlay is for → **D10**. Nothing since D8 has been
rendered, so **D9 and D10 are both unrendered** — five fixes and one reframing, all gate-green and all unseen.
D10's own shortlist of what may still read as splotchy is at the end of that section; start there.

🟡 **The map-open hitch is now an open question rather than an expectation.** D6's estimate was a
**0.5–2.5 s** solve against Q-1's 250 ms budget; D8's three levers are expected to cut the solve by a
large multiple, but **the arithmetic below is a prediction and nothing has been timed**. Assume nothing
until the log line is read.

---

## Baselines (Phase 0)

| Gate                                             | Plan's recorded value               | Re-measured                                                          |
| ------------------------------------------------ | ----------------------------------- | -------------------------------------------------------------------- |
| `tools/compile-check.sh`                         | exit 0, **5964 files**, Game module | ✅ identical                                                         |
| `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) | OK, **54 tests**                    | ✅ identical                                                         |
| `tools/run-tests.sh "{6A6E2A002F53A581}"` (All)  | OK, **89 tests**                    | ✅ identical                                                         |
| Highest allocated bug id                         | **BUG-144**                         | ✅ identical                                                         |
| Free GUID series                                 | **`{6A84…}`**                       | ✅ free; `{6A84B1C0D2E3F405}` allocated to the probe layer (Phase 2) |

**Test-case inversions (Phase 3, DoD Q-5).** Each mutation was run alone against a full Fast group and
reverted from a pristine copy; the final solver diffs identical to pre-mutation and the clean gate was
re-run afterwards.

> 🔴 **THE TABLE BELOW IS HISTORY. D12 replaced the geometry**, and seven of these cases were deleted
> with the code they pinned (rows 6, 7, 8, 10, 12, 13, 16) while five more were reworked (2, 3, 4, 5, 11
> — 11 became `_FrontierSide`, and 9 became `_ChaikinPassesZero`). **The live inversion table is in D12
> §7.** This one is kept because the _reasoning_ in several rows — why an equal-weight case cannot
> detect a lost weight division, why a friendly boundary is unobservable in an unobstructed world, why
> case 15's sentinel needed a nonsense threshold — is still the reason those assertions are written the
> way they are.

| Case | Class suffix               | Inversion that turned it red                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| ---- | -------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 1    | `_LandPredicate`           | `>` → `<` in `IsLand`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| 2    | `_OwnsEqualWeights`        | `<` → `>` in `OwnsPoint`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| 3    | `_OwnsWeighted`            | dropped the `/ weight` division — **and case 2 stayed green under it**, which is exactly why §9 says case 2 alone is insufficient and case 3 exists                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| 4    | `_IsolatedSite`            | ~~`<` → `>` in `OwnsPoint`~~ — **body changed by D6, re-proven 2026-08-11:** restore `Solve`'s `\|\| site.m_fMaxRadius <= 0` skip, so the uncapped site produces **no cell at all** (FAILED 1 of 64)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| 5    | `_CoastStop`               | `>` → `<` in `IsLand`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| 6    | `_RivalStop`               | candidate reach `2×` → `0.1×` maxRadius                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| 7    | `_SmoothShrinkOnly`        | ~~removed the `Min(smoothed, raw)` clamp~~ — **body extended by D9, re-proven 2026-08-11 with TWO inversions**, because the invariant is now conditional and the case has to pin the clamp _where the clamp still applies_: (a) remove the `Min(smoothed, raw)` clamp — still red, on the original half, which passes **no** exemption array and is therefore a statement about all eight rays exactly as before (_"Smoothing GREW ray 1 from 100 m to 166.667 m"_); (b) apply the clamp only when no exemption array was supplied (`if (rawRadii && …)` → `if (!exempt && rawRadii && …)`), i.e. let one exemption switch the clamp off for the whole array — red on the **new** half only (_"With one ray exempted, smoothing GREW unexempted ray 1 from 100 m to 166.667 m; the exemption switched the clamp off for the whole array instead of for one ray"_). Both reverted from a pristine copy and the clean gate re-run                                                                                                                                                                                    |
| 8    | `_SmoothWrap`              | circular index wrap → clamped index                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| 9    | `_SmoothPassesZero`        | smooth anyway despite `passes = 0` (**not** the plan's stated inversion — see P3-B)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| 10   | `_CandidateNeutral`        | ~~candidate reach `2×` → `0.1×` maxRadius~~ — **body changed by D6, re-proven 2026-08-11:** drop the weight ratio from the candidate bound (`reach × (1 + w_R/w_S)` → `reach × 2`, i.e. the _old shipped_ rule), which excludes the distant heavy rival and makes the filtered march disagree with the unfiltered one (FAILED 1 of 64)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| 11   | `_FriendlyBoundary`        | **added by D7, proven 2026-08-11 with TWO inversions**, because it pins two separable things: (a) drop the faction comparison in `IsFrontierStop` (`return winner.m_iFactionIndex != ownFactionIndex` → `return true`), so a same-faction boundary is banded again — FAILED; (b) stop recording the winner (`stopSite = winner` → `stopSite = -1` in `SolveRayOwner`), so nothing can be classified at all — FAILED. Both reverted from a pristine copy and the clean gate re-run                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| 12   | `_AnalyticMatchesMarch`    | **added by D8, proven 2026-08-11 with TWO inversions**, one per lever: (a) drop the factor of two from the takeover quadratic's linear term (`b = 2*wsSq*(u.D)` → `b = wsSq*(u.D)`), which doubles every equal-weight boundary — FAILED 6 of 66, reporting _"the closed form and the reference march DISAGREE: 1000 m … solved, against 500 m … marched"_, i.e. the independent oracle caught it and named both answers; (b) replace LEVER B's proven early exit with an unconditional one (`thresholds[c] >= best` → `c > 0`), which drops every rival after the first — FAILED 2 of 66, reporting _"the SORTED early exit changed the answer … 2000 m / winner -1 sorted, against 700 m / winner 2 unsorted"_, on exactly the ray predicted (the one whose nearest-threshold rival contributes no takeover). Both reverted from a pristine copy and the clean gate re-run                                                                                                                                                                                                                                        |
| 13   | `_FriendlySmoothingExempt` | **added by D9, proven 2026-08-11 with TWO inversions**, one per separable half: (a) stop exempting — delete the `if (IsExemptRay(exempt, i)) continue;` guard in `SmoothRadii`, so a friendly boundary is smoothed like any other — FAILED, reporting _"The ray ending on a SAME-FACTION neighbour was smoothed from 500 m to 260.938 m"_, i.e. the shared boundary retreating by half its length, which is the sliver in the user's screenshot; (b) let an exempt ray still **contribute** to its neighbours' averages (drop the two `source[i]` substitutions) — FAILED on part 1 with _"the ray beside the exempt one was pulled from 500 m to 322.222 m"_, which is the gap re-opening at the ends of the seam. ⚠️ Inversion (a) is **not** caught by part 1: a friendly boundary is normally a local _minimum_, so the clamp pins it to raw whether it is exempt or not — **part 2 exists precisely because the exemption is unobservable in an unobstructed world**, and it uses a channel-coast stub to make the boundary ray a local maximum. Both reverted from a pristine copy and the clean gate re-run |

| 14 | `_OccupierOnlyEmit` | **added by D10, proven 2026-08-11 with THREE inversions**, because it pins two separable things and the second one twice: (a) flip the emit comparison (`cellFactionIndex == occupyingFactionIndex` → `!=`) — FAILED, _"An OCCUPIER-held region was not drawn…"_; (b) 🔴 **the one that matters** — make the geometry stop competing, by having `BuildCandidates` skip sites of another faction — FAILED, reporting _"The occupier's boundary toward a LIBERATED neighbour ran to 2000 m instead of the 500 m midpoint; a site that is not drawn must still compete, or occupier colour floods the ground the player has liberated"_, which is verbatim the defect the case exists for; (c) filter at solve time instead, by adding `if (site.m_iFactionIndex != sites[0].m_iFactionIndex) continue;` to `Solve`'s site loop — FAILED on the count, _"3 sites produced 2 cells; a site that is not DRAWN must still be SOLVED"_. All reverted from a pristine copy and the clean gate re-run |
| 15 | `_ContestedSupport` | **added by D10, proven 2026-08-11 with TWO inversions**: (a) drop the `supportFraction < 0` sentinel guard — FAILED, _"A military site with NO support figure was marked contested…"_; (b) make the threshold exclusive (`>=` → `>`) — FAILED, _"A town sitting EXACTLY on the support threshold was not contested"_. ⚠️ **The first attempt at (a) did NOT fail**, and the case was rewritten because of it: the sentinel was being probed at a threshold of 0, where `-1 >= 0` rejects it arithmetically and the guard is redundant. The guard is only load-bearing at a threshold _below_ the sentinel, so the case now probes at −2 and its comment says plainly what that buys — the sentinel is a **state**, and no configured threshold, including a mistyped negative one, may turn "no support to measure" into "fully supported" |
| 16 | `_CoastSmoothingExempt` | **added by D10, proven 2026-08-11 with TWO inversions**, one per separable half: (a) `IsCoastStop` returns false — FAILED on the classification, _"a ray that ran out of land was not classified as a coast stop; every shoreline on the map would be filtered like a bisector"_; (b) keep the predicate but stop wiring it in (`pinned.Insert(IsCoastStop(reason))` → `pinned.Insert(false)`) — FAILED end to end, _"The ray that ended at the SHORE was smoothed from 783.594 m to 629.34 m"_, i.e. the fill retreating 154 m from a coastline it had already found, which is the fringe. ⚠️ **Only the SHALLOW coast ray is observable** — a ray aimed straight at the nearest shore is a local minimum and the clamp pins it exempt or not, so the case asserts on the grazing ray and says so |

⚠️ `CLAUDE.md` says Fast 38 / All 66 and is **stale** — never quote it. A _changed_ count at a phase
boundary is a finding to investigate, never a number to update.

**Case 13 was re-verified rather than changed by D10.** Its body is untouched, and D9's inversion (a) —
deleting the friendly exemption guard — **still turns it red** (_"…smoothed from 500 m to 287.5 m"_, against
260.9 m under D9; coast neighbours are now pinned, so the arithmetic moved and the conclusion did not). That
check was run deliberately: had D10 made coast rays _isolating_ rather than _pinned_, case 13's assertion
would have started passing for the wrong reason and D9's inversion would have stopped failing it. **A
silently vacuous case is worse than a red one**, and this is the check that ruled it out.

**One extra inversion, run to de-risk D6's sqrt-free rewrite** (not a case body change — evidence that the
squared form still _carries the weighting_): replacing the cross-multiplied comparison
`distSq * bestWeightSq < bestDistSq * weightSq` with a plain `distSq < bestDistSq` turns **cases 3 and 10
red and leaves case 2 green** — exactly the signature the original `/ weight` division produced, which is
why case 2 alone was never sufficient.

---

## Key Decisions Made

### D14 — The threat grid was a **debug layer**, and D3's two recorded reasons are wrong (user, 2026-08-11)

The user, who wrote it: _"the threat layer was actually just written as a debug layer during the development
of the deployment systems. it wasn't really intended to be shipped and that's why it was disabled."_

**`implementation.md` §4 D3 speculated two reasons** — ~2,300 draw commands a frame, and mutual exclusivity
with the restriction rings under the shared-canvas defect. **Both are wrong.** They were labelled unproven at
the time and were _still_ reasoned from twice during this feature, which is the lesson: a recorded guess
outlives the doubt attached to it. They are struck through in the plan rather than deleted so the correction
is visible to anyone who read the original.

**It is also not tech debt.** A debug tool behind a disable flag is an ordinary thing to have in a tree; it is
not the "written-but-disabled code left indefinitely" that `map-layers/requirements.md` warns against. The
epic's Tech Debt entry is closed rather than carried.

**Forward view, recorded but not scheduled:** _"if we updated it and made it performant it could be added as a
switchable layer turned off by default… just leave it as is for the moment until this is finished."_ That is a
**feature**, not repayment. It is also the natural second consumer for this feature's machinery — see D15.

### D15 — Extract the field→region pipeline, but **only when a second consumer exists** (user, 2026-08-11)

The user: _"it might be advantageous (after we have tuned and reached full acceptance) to extract any reusable
code from this layer so that other future layers (ie the threat layer) can apply the same techniques."_

**Agreed, including the timing.** What has emerged is generic and nothing in it is specific to territory:

> sample a field over a snapped world grid → threshold into regions → marching-squares contours → Chaikin
> smoothing → trapezoid scanline fill → batch by draw state

Territory's sampler answers _"which site owns this point"_. A threat layer's would answer
_"how much threat is here"_ and threshold at a few levels. The natural seam is a field-grid class, the contour

- smoothing pair (already pure and already unit-tested), the scanline decomposition, and the batching — with
  territory reduced to a sampler plus its colour and alpha rules.

⚠️ **Do not extract speculatively.** Three of the last four rounds on this feature changed the representation
outright; an API frozen at any of those points would have been wrong. Two real consumers is the minimum
evidence that an abstraction is real rather than imagined, so the threat layer being _built_ is the forcing
function, not the anticipation of it.

_(Planning decisions live in `implementation.md` §4 and §6 and are not duplicated here. This section
records decisions made **during implementation** — the ones a future reader cannot recover from the plan.)_

### D13 — The fill is **cut out of the smoothed contour**, and the grid is **snapped to the map's own grid** (user decision, 2026-08-11)

**The first decision taken after D12 was RENDERED**, and the user called D12 _"a massive improvement
overall"_. Two faults remained, from one screenshot.

---

#### 1. 🔴 One region had TWO boundaries, and they did not coincide

> _"the 'filled' areas are sticking to the grid while the bands are smoothed… it would be good if the
> filled areas were smoothed like the bands are so they match"_

D12 filled from **grid runs** and banded from the **Chaikin-smoothed contour** traced through those same
runs. That is two independently derived curves describing one region: the hatched band was a smooth
organic line, the solid fill beneath it an axis-aligned staircase, and blocky fill jutted past the band
in places and fell short in others. **No parameter could have fixed it** — a finer grid makes the steps
smaller and costs four times the classify, and the two curves are still different curves. (This is D12's
own lesson recurring one level down: the defect was in the representation, not the parameters.)

**The fix: one boundary per drawn region, used twice.** The contour is now the fill's boundary as well
as the band's.

- **`OVT_TerritorySolver.BuildTrapezoids`** — a **trapezoid scanline decomposition**. Horizontal scan
  rows sweep the region; at each row's **top and bottom** edge every contour segment is intersected with
  the line, the crossings are sorted, and consecutive pairs are the inside under the **even-odd rule**.
  Each pair becomes a **trapezoid** whose left and right edges run between the _actual_ crossings at top
  and bottom — so a slanted stretch of border is filled slanted, not stepped. A triangulator was not
  needed and non-convex `PolygonDrawCommand` fill was not relied on.
- **`[Attribute] m_fScanRowHeight`, default 25 m.** 🔴 **This knob and `m_fGridCellSize` look alike and
  are not.** The grid costs `(span / cellSize)²` — halving it is four times the work. The scanline costs
  `span / rowHeight` — **linear**. That is written at both attributes and at `MAX_SCAN_ROWS`, because the
  wrong mental model here leads someone to refuse the cheap lever.
- **Holes and disjoint components need no case at all.** Four crossings is two spans whether the gap is a
  liberated pocket inside occupier ground or a strait between two islands. `_ScanlineSpans` asserts both
  side by side to say so.
- **Segments are bucketed by the rows they can reach** (a two-pass compressed row table). Without it the
  pass is rows × segments; a smoothed island border is thousands of segments against hundreds of rows.
- **A row whose two ends describe different structure is HALVED**, up to `MAX_ROW_SUBDIVISIONS` (4), and
  only the leftover sliver — ≤ `rowHeight / 16`, about **1.5 m** at defaults — is squared off as a
  rectangle taken from whichever end has _more_ spans. That covers the tip of a peninsula, the first row
  of a hole, a region splitting, **and a near-horizontal stretch of border**, which is the case a naive
  scanline steps on. Equal span counts alone are not trusted: the paired spans must also **overlap in X**,
  or a region ending while another begins draws a diagonal of fill across ground belonging to neither.
- **The tiling property survives.** One row's bottom crossings ARE the next row's top crossings, carried
  forward rather than recomputed, so adjacent rows share their edge exactly and there is no seam. Spans
  within a row are disjoint by even-odd. `_ScanlineTrapezoids` pins the carry-forward directly.

**The one structural change this forced: a region is now an APPEARANCE, not a faction.** A fill has to
know its own colour, and a faction's held ground and its contested ground are two different washes, so
one loop around both could not bound either. Contours are therefore traced per appearance key.
⚠️ **D7 was checked rather than assumed**: the seam between a faction's held and contested ground is now a
real contour where it used to be nothing at all — but it is a **same-faction** boundary, so
`IsFrontierSide` answers no on both sides and no band is drawn. **The faction index was added to the
appearance key** (`FindOrAddAppearance(faction, colour, contested)`) precisely to protect D7: two
factions resolving to the same colour — reachable through the white unresolved-faction fallback — would
otherwise have been traced as ONE region and the frontier between them would simply have ceased to exist.

**`m_bTraceContours 0` now means more than it did, and the old fill is kept for it.** With contours off
there are no borders, no bands, no Chaikin **and no contour to cut a fill out of**, so the fill falls back
to D12's merged grid squares — `BuildRects`, `EmitRectFills` and `m_bMergeRectsVertically`, all retained
and all still pinned by cases 7, 8 and 16. It is the blocky, axis-aligned silhouette again, which is
exactly right for a fallback: it needs no traced border, so it is the thing to reach for if the tracer
ever misbehaves. It is a clean configuration, not a degraded one.

**Cost, measured in geometry rather than milliseconds** (nothing here has been timed either):

|                          | D12 (grid runs)   | D13 (contour trapezoids)                                  |
| ------------------------ | ----------------- | --------------------------------------------------------- |
| Fill quads               | ~200–900 rects    | **~1,000–1,500 trapezoids** at 25 m rows on a 12 km world |
| Fill vertices/frame      | ~800–3,600        | **~4,000–6,000**                                          |
| Fill commands/frame      | ~2–3              | **~6–8** (chunked at 256 quads)                           |
| Band commands/frame      | 1 **per contour** | **1 per appearance** — batched, which is new              |
| **Total commands/frame** | ~5–15             | **~10–20**, against Q-1's ≤ 250                           |

The trapezoid count roughly triples the fill geometry; batching the bands per appearance instead of per
loop pays part of it back, and it had to, because a region with pockets in it has many loops and one
command each would have put the count into the hundreds. **Against the 250/frame budget this is still an
order of magnitude of headroom.** The first lever if the per-frame emit ever matters is
`m_fScanRowHeight`, and it is linear.

---

#### 2. The ownership grid is snapped to the game's grid

> _"our current grid size seems to match the game's grid in size but is offset slightly to the left. I
> think we can keep the current grid size but if possible try to match it to the game's"_

The grid started at the **world bound box minimum**, which is not generally a multiple of anything, so
squares the same size as the map's grid squares still sat beside its lines. `BuildGrid` now floors the
origin to a multiple of the square size **measured from world zero** and grows the grid back out to cover
the whole box (at most one extra row and column). ⚠️ **The snap is inside the square-count valve loop**:
it has to use the size the grid was _actually built at_, not the one configured, or growing the squares
puts the origin back off the grid — a separate defect with the same symptom, and `_GridOriginSnap` pins
it separately. The origin is now in the solve log for the same reason.

**The default cell size was NOT changed**, per the user.

✅ **The vanilla grid spacing IS discoverable, and it is not a single number.** It is
`SCR_LayerConfiguration.m_fGridSquareSize`, one value **per zoom layer**, in the base game's
`Configs/Map/MapLayersDefault.conf` — **100 m** on the closest layer, **1000 m** on the three middle ones
and **10000 m** on the furthest. Overthrow does not override that file. At runtime it reaches the engine
through `MapLayer.GetGridProps().SetGridStepSize`; there is a `GetSquareSize()` getter but it is marked
_"TODO Remove"_ in the engine proto and it is ambiguous whether it reports the step size or the main grid
size (100 km). 🔴 **So a future change could read it, but it could not simply adopt it**: the spacing
changes as the player zooms and the ownership grid is solved once. The useful property is that snapping
to multiples of **100** lands on the 100 m grid _and_ on the 1000 m and 10000 m ones, since all three are
drawn from world zero — which is what the shipped default already does.

---

#### 3. New cases — 17 → 20, every one proven able to fail

| Case                     | What it pins                                                                                                                                                                                                                                                                                                                         | Inversion that turned it red                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| ------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 18 `_ScanlineSpans`      | **NEW** — even-odd pairing: 2 crossings → 1 span on a convex region; **4 → 2 spans with a HOLE**, and the spans do not overlap; the **same 4 → 2** on two DISJOINT regions; and a contour of another appearance is not part of this region                                                                                           | (a) pair the first crossing with the last instead of consecutively — _"The two spans either side of the hole ended at 400 and began at 0 instead of 150 and 250"_, i.e. the liberated pocket painted over; (b) drop the appearance-key filter — _"Cutting appearance 0 produced 2 spans instead of 1"_                                                                                                                                                                                                                                                                                                                                                                                           |
| 19 `_ScanlineTrapezoids` | **NEW, and the case this change exists for** — a slanted edge is genuinely slanted: the row's right corner is at 125 at its top and 150 at its bottom, the two are **not equal**, the vertical edge stays vertical, one row's bottom edge **is** the next row's top edge to 0.001 m, and the total area is the shape's own 60,000 m² | (a) take the bottom edge's X from the top crossings — _"The bottom of the row met the slanted border at x=125 instead of 150"_, which is a stack of rectangles, i.e. the staircase again; (b) disable the row subdivision — the tip of the region squares off and the **area** assertion fires: _"The fill covered 58750 square metres against the shape's own 60000; a fill that stepped each row to one of its edges would come out at 55000"_. ⚠️ **(b) exists because (a) never reaches the area assertion.** The area is the only thing that can tell a genuinely slanted fill from a stepped one that tiles just as cleanly and has just as many quads, so it needed its own proof of life |
| 20 `_GridOriginSnap`     | **NEW** — the origin is a whole multiple of the square size, it rounds **down** (from a negative min and from a positive one, which are the same answer only if one is wrong), the grid still covers the whole requested box, **and the snap uses the size the ceiling settled on**                                                  | (a) drop the snap — _"The grid started at -1234, -567 with 100 m squares; neither is a whole multiple"_; (b) snap once before the valve loop instead of inside it — _"After the ceiling grew the squares to 2 m the grid still started at 3, which is not a multiple of them"_                                                                                                                                                                                                                                                                                                                                                                                                                   |

**No existing case changed.** The rect merger, the tracer, Chaikin, the frontier rules and the emit
filter are all untouched by D13, and cases 1–17 are byte-identical.

**Gates:** compile **exit 0 / 5971 files** (unchanged — the new classes live in existing files), Fast
**74**, All **109** — the +3 are cases 18, 19 and 20 and are the only expected count change.
⚠️ **Unrendered.**

---

#### 4. What may still read as imperfect

1. **A near-horizontal stretch of border is where the silhouette is weakest.** It is the axis the scan
   rows quantise, and it is handled by the subdivision rather than by the trapezoid, so the residual is a
   ≤ 1.5 m step rather than a slant. That is a quarter of what D12's 100 m squares did on _both_ axes, and
   the lever is `m_fScanRowHeight` — linearly.
2. **The trapezoid's side is a CHORD, not the contour.** Between one row boundary and the next the border
   may bow by up to about half a smoothed segment — a few metres at 100 m squares with two Chaikin
   passes. Lower the row height or raise `m_iSmoothPasses` if it shows.
3. **Where a held/contested seam meets the outer boundary**, the two appearances' loops diverge within
   about two nodes because Chaikin's neighbourhood differs there. Along the interior of a shared seam the
   smoothed points are **identical** (corner cutting is direction-symmetric), so the two fills tile
   exactly; only within a couple of nodes of a T-junction can a sub-cell sliver appear, between two washes
   of the same hue.
4. **D12 §9's items 2–5 are unchanged** — the band folding at tight corners, the band being the loudest
   thing on the map, blank holes around FOBs, the two contested tones. Item 1 (_"the fill edge is now
   BLOCKY by construction"_) is **retired**: that is what D13 removed, and it now describes only the
   `m_bTraceContours 0` fallback.

### D12 — Territory is an **ownership grid**, not a star polygon per site (user decision, 2026-08-11)

**The ray-march failed visual inspection three times and the user directed that the representation be
replaced.** This is not a tuning change and it is not a bug fix — it is a different way of describing
where territory is, and it deleted about half the solver.

---

#### 1. Why the ray-march could not be fixed

The march rested on one property, stated in §6 K3 as the structural justification for the whole
approach: _"the cell is star-shaped about its own site by construction"_. That holds for **rival**
boundaries. It is **false the moment a cell is clipped to an indented coastline**, and both failure
modes the user reported follow from it directly:

- **A ray stops at the FIRST water it meets.** Land on the far side of a bay that legitimately belongs
  to a town can therefore never be filled, because the ray that would reach it died at the near shore.
  No ray count fixes that; the region simply is not star-shaped about its site.
- **Two neighbouring 48-gons inscribe the same curved boundary with different vertices**, so they
  alternate gap and overlap along it. D9 measured that at ≈ 2 m per 1000 m of boundary and called it a
  hairline; more rays shrink it quadratically and never remove it.

The user's screenshot showed the visible consequence: **straight radial lines and pale wedges**.

🔴 **The lesson worth keeping: the defect was in the REPRESENTATION, not in the parameters.** Three
rounds of tuning (D6's unlimited reach, D7's faction-aware bands, D9's smoothing exemptions, D10's
coast pinning) each removed a real artefact and each left the underlying one untouched, because a
radius-per-angle cannot express a bay, a hole, or a second component. That is why the fourth pass
changed the representation instead.

---

#### 2. What replaced it

**An ownership grid, and everything else falls out of it.**

- **`OVT_TerritoryGrid`** — a world-space square grid over `BaseWorld.GetBoundBox`, one **owner index**
  per square, `-1` for water or unowned. Built by `OVT_TerritorySolver.BuildGrid`.
- 🔴 **NO NEW MATHS.** Ownership is still `OwnsPointXZ` — the same sqrt-free cross-multiplied weighted
  comparison — and land is still the same virtual `IsLandAt`. **This is a change of sampling strategy,
  not of what territory means**, and both predicates are still pinned directly by their own cases.
- **`[Attribute] m_fGridCellSize`, default 100 m.** Cost is **quadratic** in it, and so is the blockiness
  of the fill edge. A hard `MAX_GRID_CELLS = 250000` ceiling **grows** the square size until the grid
  fits and records what it settled on, so a mistyped value is a coarse overlay that says so in the log
  rather than a hung client. That valve has its own assertion.
- **Fill = merged rectangles.** Maximal horizontal runs of squares that would draw _identically_, then
  greedily grown downward while a whole run repeats in the row below.
- **Border = traced, smoothed contours.** Marching squares over "does this square belong to the faction
  being outlined", then Chaikin corner cutting. `m_iSmoothPasses` **finally means what `requirements.md`
  meant by smoothing** — it rounds the border rather than eroding a radius.

**🔴 Gaps and overlaps are now STRUCTURALLY IMPOSSIBLE, and that is the whole point.** A square has
exactly one owner, so two regions cannot overlap; the squares tile, so they cannot leave a gap. The
pale seam and the dark hairline are not fixed — they are inexpressible. `OVT_TEST_Logic_Territory_
FillTiling` asserts it directly: every drawn square covered by exactly one rectangle, every undrawn
square by none, on a deliberately ragged field.

**And bays, inlets and offshore islands need no special case whatsoever.** Every square is classified
independently of every other, so topology stops mattering. The tracer handles a region with a **hole**
and two **disjoint** components, both of which the old representation could not express at all.

---

#### 3. Why merging is keyed on APPEARANCE, and what that cost

Runs merge across a change of **owning site** as long as the two squares draw identically. Two
neighbouring towns of one faction are one colour, so a row crossing six of them is one quad rather than
six — that is the difference between an affordable fill and a hopeless one.

⚠️ **This moved work from the solve onto the refresh tick, and §6 K9's wording needs reading with that
in mind.** The split K9 exists to protect is preserved _in substance_: classifying the grid is
`O(cells × sites)` and still happens only at map open or when `HashSites` changes. But which squares are
drawn, in what colour, and where the borders between factions fall are all functions of **who holds
what**, so the rectangles and the contours are rebuilt on the slow tick — `O(cells)`, roughly 14,000
trivial iterations every 5 s, against the ~576,000 weighted comparisons a re-solve costs. **The recolour
is no longer strictly O(sites); it is O(sites) + O(cells) and does not touch the world.** That is stated
rather than hidden, and `RefreshTick` prints its own millisecond count under `m_bDebugTiming`.

---

#### 4. Everything preserved, and how each one is now expressed

| Decision                                                          | How it survives D12                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| ----------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **D6** — no maximum influence radius                              | **Falls out.** The grid has no radius concept at all, so there is nowhere left to put one. `m_fMaxRadius` is deleted from both `OVT_TerritorySite` and `OVT_TerritorySiteConfig`, and weights are now purely _relative_. `_IsolatedSite` pins it: one site owns every square out to the far corner                                                                                                                                                                                                                  |
| **D7** — bands only on inter-faction frontiers                    | **Preserved exactly; only the mechanism moved.** `IsFrontierStop(stopReason, stopSite, …)` became `IsFrontierSide(otherOwnerIndex, ownFaction, sites)` — the same faction comparison with the ray stripped out. Water is answered by the **first** branch (`owner < 0`), because nobody owns the sea. And the same-faction case now cannot even arise: the region traced is **a whole faction's ground**, so two towns of one faction produce no border between them rather than a border that has to be suppressed |
| **D9** — flat fill alpha, no per-site gradient                    | **Unchanged.** `m_iFillAlpha` 70 flat, `m_bContestedShading` the one switch. D9's _geometric_ half — the smoothing exemption that stopped two same-faction cells tearing apart — is **deleted along with the defect it worked around**: same-faction squares are now one region and there is no seam to open                                                                                                                                                                                                        |
| **D10** — occupier-only, contested = support ≥ threshold          | **Unchanged, including the trap.** `IsEmittedCell` and `IsContestedCell` are byte-identical, support is still `(support * 100.0) / population` and **never** `SupportPercentage()`. 🔴 Resistance sites still **own grid squares** — the filter is on appearance resolution, never on collect or on the grid build. `_OccupierOnlyEmit` pins it by asserting the liberated site owns its half of the world _and_ that no drawn rectangle crosses the boundary                                                       |
| **D11** — contested hatched, faction-coloured rings, QRF by scale | **Unchanged.** Contested fill carries `{B7E8255E75EE66ED}…overthrow_map_diagonal.edds` at `m_fUVScaleContested`. `OVT_MapRestrictedAreas` was **not touched**                                                                                                                                                                                                                                                                                                                                                       |
| **§6 K9** — solve once, recolour often                            | **Preserved in substance, amended in cost.** See §3 above                                                                                                                                                                                                                                                                                                                                                                                                                                                           |

---

#### 5. What was deleted, and why leaving it would have been worse

**All of it went with its tests.** A tested-but-unreachable function is the outcome D3's deferred threat
grid is criticised for, and this would have been six times the size of that.

- `OVT_TerritoryCell.c` — **file deleted**, taking `OVT_TerritoryCell` and the `OVT_TerritoryStop` enum.
- `SolveRay`, `SolveRayOwner`, `SolveRayMarched`, `SampleReason`, `IsRayPointUsable`, `Solve`.
- **D8's entire closed-form takeover machinery**: `RivalTakeoverRadius`, `SolveRivalRadius`,
  `BuildSortedRivals`, `BuildCandidates`, `ResolveMaxRadius`, `EQUAL_WEIGHT_TOLERANCE`.
- `SmoothRadii`, `IsExemptRay`, and **D9's and D10's exemption arrays** — the grid has no rays to exempt.
- `IsFrontierStop` / `IsFriendlyStop` / `IsCoastStop`, replaced by the single `IsFrontierSide`.
- `m_iRayCount`, `m_fMarchStep`, `m_iRefineSteps`, `m_fMaxRayLength`, `m_fBandFraction`,
  `IsInsideWorldBounds`, the `OVT_TerritoryPrimitive` enum and the whole three-rung `m_ePrimitive`
  ladder.
- **Seven Logic cases**: `_RivalStop`, `_SmoothShrinkOnly`, `_SmoothWrap`, `_CandidateNeutral`,
  `_AnalyticMatchesMarch`, `_FriendlySmoothingExempt`, `_CoastSmoothingExempt`.

⚠️ **`SolveRayMarched` is gone, and with it case 12's independent oracle.** That was a real asset — it
was the only evidence D8's algebra was right, and it caught a seeded defect during its own inversion
run. It is deleted anyway because **the algebra it verified is deleted**: there is no closed form left to
check. `OwnsPointXZ` is now called directly rather than solved around, so the thing case 12 bridged no
longer has two sides.

**Kept and still green:** `OwnsPoint` / `OwnsPointXZ` (the `candidates` parameter dropped — nothing
filters candidates any more) and `IsLand` / `IsLandAt`.

---

#### 6. The primitive, and a correction

⚠️ **A false premise nearly went into this file and is recorded here so nobody re-derives it.** The task
that produced D12 asserted that `TriMeshDrawCommand` probably ignores `m_pTexture`, citing the user's
report that contested territory rendered solid, and instructed fills onto `PolygonDrawCommand` for that
reason. **That was wrong on both halves.** The user was describing the **neutral bands**, which were
solid because D11 deliberately shipped `m_sBandTexture` **empty**; and the live conf at the time carried
`m_ePrimitive TRIANGLE_LIST`, which already draws through `PolygonDrawCommand`, so the observation could
not have been evidence about TriMesh either way.

✅ **The correct finding, observed by the user on 2026-08-11:** with the band texture configured and
`TRIMESH_FAN` selected, _"it works fine with TRIMESH_FAN and is looking good"_. **`TriMeshDrawCommand`
honours `m_pTexture` and `m_fUVScale` in this build.** There was never a texture bug.

**So the primitive was chosen on command count instead, which is the real constraint.** Quads are
**batched by draw state** into `TriMeshDrawCommand`s with explicit indices — one command per appearance
for the fill, one per contour for the band, chunked at `MAX_QUADS_PER_COMMAND = 256` as a valve against
an unmeasured engine vertex limit. That is single-figure commands a frame where one-command-per-quad
would be several hundred. **`m_bBatchDrawCommands 0`** falls back to one convex `PolygonDrawCommand` per
quad — identical geometry, no engine feature needed, hundreds of commands — and is the escape hatch if
batching ever misbehaves.

**The band is textured.** `m_sBandTexture` now **defaults** to the diagonal hatch (so it survives a
config reset) and is separated from the contested fill by **UV scale** — `m_fUVScaleBand` 0.02 against
`m_fUVScaleContested` 0.01 — because hue is spent entirely on faction identity. The user's
`MapOverthrow.conf` line setting the same resource explicitly is preserved and is now redundant rather
than load-bearing. `m_ePrimitive TRIANGLE_LIST` was removed from that conf **by the user** before D12, so
deleting the enum orphaned no config line.

---

#### 7. New and reworked test cases — 16 → 17, every one proven able to fail

Every inversion below was applied alone to a pristine copy of the solver, compiled, run against **that
one case**, and reverted; the clean gate was re-run afterwards.

| Case                       | What it pins                                                                                                                                                                                                                                  | Inversion that turned it red                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| -------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1 `_LandPredicate`         | unchanged                                                                                                                                                                                                                                     | `>` → `<` in `IsLand`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| 2 `_OwnsEqualWeights`      | **reworked** — the predicate _and_ the grid column where ownership flips                                                                                                                                                                      | sample at `EdgeX`/`EdgeZ` instead of `CentreX`/`CentreZ` — the half-square offset puts the boundary column on an exact tie and it stops changing hands                                                                                                                                                                                                                                                                                                                                                                           |
| 3 `_OwnsWeighted`          | **reworked** — the only assertion that can tell a weighted diagram from a plain one                                                                                                                                                           | drop the cross-multiplication (`distSq * bestWeightSq < bestDistSq * weightSq` → `distSq < bestDistSq`); the boundary retreats to the midpoint                                                                                                                                                                                                                                                                                                                                                                                   |
| 4 `_IsolatedSite`          | **reworked** — D6: one site owns every square out to the far corner                                                                                                                                                                           | reintroduce a 500 m reach in `BuildGrid`; the outer squares go unowned, which is the first render the user rejected                                                                                                                                                                                                                                                                                                                                                                                                              |
| 5 `_CoastStop`             | **reworked** — the virtual land hook decides which squares are ownable, and `m_bClipToCoast` is the only thing that turns it off                                                                                                              | replace `IsLandAt(x, z)` with `true`; the sea is owned                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| 6 `_GridIndexing`          | **NEW** — `Ceil` keeps the partial column, centre/edge/index agree, outside the grid is unowned, and the square-count ceiling grows the cell size                                                                                             | (a) `Math.Ceil` → truncation: 11 columns become 10 and a 50 m strip of the world is never classified; (b) raise `MAX_GRID_CELLS` to 250,000,000: the valve stops firing and a 1 m cell size is allowed to stand                                                                                                                                                                                                                                                                                                                  |
| 7 `_RunMerging`            | **NEW** — maximal runs, merged across an owner change, broken by water and by an appearance change; a site that is not drawn contributes no quad                                                                                              | extend a run across any drawn square (`== key` → `>= 0`); the run swallows the second appearance                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 8 `_FillTiling`            | **NEW, and the case D12 exists for** — every drawn square covered exactly once, every undrawn square not at all, with and without the vertical merge; and the merge must actually fire                                                        | drop `below.m_iCol1 == rect.m_iCol1` from the vertical-merge match; a run merges with a shorter one below it and the rectangle paints two squares of open water                                                                                                                                                                                                                                                                                                                                                                  |
| 9 `_ContourTrace`          | **NEW** — marching squares on a hand-built field: one closed loop, 8 points, the right enclosed area, no segment longer than a square diagonal, and **positive signed area** (region on the left, which is what makes the band offset inward) | reverse marching-squares case 12 (`L→R` → `R→L`); the walk can no longer be followed and the loop falls apart                                                                                                                                                                                                                                                                                                                                                                                                                    |
| 10 `_ContourTopology`      | **NEW** — a region with a HOLE gives two loops wound in **opposite** directions; two disjoint regions give two wound the **same** way                                                                                                         | drop the one-square overscan (`for r = -1` → `for r = 0`); a region filling its grid loses its left and bottom edges. ⚠️ **The first version of this case did NOT fail under that mutation** — a partial chain still counts as a loop, still encloses a positive area, and leaves the hole looking perfect. The case was rewritten to assert the outer loop's exact **point count and area**, and it then failed with _"9 points instead of 20"_. This is the one place a case had to be strengthened before it earned its place |
| 11 `_ChaikinSmoothing`     | **NEW** — points double, land on the quarter points, stay inside the original outline, and the frontier flags propagate                                                                                                                       | (a) cut at midpoints (0.75/0.25 → 0.5/0.5): corners collapse instead of rounding; (b) corner segment takes one side only (`here && after` → `here`): the band turns the corner from a frontier onto a coastline                                                                                                                                                                                                                                                                                                                  |
| 12 `_ChaikinPassesZero`    | **reworked** — zero passes is the identity, a negative count is the same, **and one pass is not**                                                                                                                                             | `pass < passes` → `pass <= passes`. ⚠️ The production code originally ALSO carried an `if (passes <= 0) return;` early return, which made this contract **impossible to turn red** — either mechanism covered a mutation of the other. The redundant guard was **deleted** so the loop bound is the single mechanism, with the reason written at the function. _An assertion nothing can break is an assertion nobody is checking_                                                                                               |
| 13 `_FrontierSide`         | **reworked from `_FriendlyBoundary`** — D7's rule as a pure predicate: same faction no, different faction yes, **water no**, out-of-range no, and symmetric from both sides                                                                   | (a) `return true` instead of the faction compare: a same-faction boundary is banded again; (b) `if (otherOwnerIndex < 0) return true`: every coastline on the map is banded                                                                                                                                                                                                                                                                                                                                                      |
| 14 `_ContourFrontierSides` | **NEW, end to end** — a region with open water on three sides and another faction on the fourth is banded on **exactly one** of its eight segments, and it is the right one                                                                   | `&&` → `\|\|` in the segment rule; three segments are banded and two of them turn the corner onto a beach                                                                                                                                                                                                                                                                                                                                                                                                                        |
| 15 `_FrontierSpans`        | **NEW** — circular runs, including one that **wraps past index 0**, the all-frontier ring, and the no-frontier ring                                                                                                                           | anchor the scan on the first FRONTIER segment instead of the first non-frontier one; the wrapping run splits into two strips with a notch where they meet. ⚠️ The input had to be chosen for this: on `T F T T` the wrong anchor produces the RIGHT answer by luck, so the case uses `T T F T`, and says so                                                                                                                                                                                                                      |
| 16 `_OccupierOnlyEmit`     | **reworked** — D10's classification, plus the trap: the liberated site owns its half of the grid and no drawn rectangle crosses the boundary                                                                                                  | make only `sites[0]`'s faction compete in `OwnsPointXZ`; the occupier owns everything and _"occupier colour floods the ground the player has liberated"_                                                                                                                                                                                                                                                                                                                                                                         |
| 17 `_ContestedSupport`     | unchanged (`IsContestedCell` is byte-identical)                                                                                                                                                                                               | `>=` → `>`: a town exactly on the threshold stops being contested                                                                                                                                                                                                                                                                                                                                                                                                                                                                |

---

#### 8. Cost, honestly

**Instrumented behind `m_bDebugTiming`**, and two lines print regardless: the grid line (sites, grid
dimensions, effective square size, owned/water counts, collect ms, classify ms) and the draw-set line
(appearances, runs → rects, contours, contour points, frontier segments, build ms).

**The arithmetic, on a ~12 km world at 100 m — and it is an ESTIMATE, because none of it has run in a
game:**

|                         | Old (ray-march)         | New (grid)                                 |
| ----------------------- | ----------------------- | ------------------------------------------ |
| Terrain samples         | ~19 k                   | **~29 k** (2 proto calls × 14,400 squares) |
| Ownership comparisons   | ~3 M before D8's levers | **~576 k** (14,400 × ~40 sites)            |
| Per-frame vertices      | ~1,900                  | ~800–3,600 (rect corners + frontier band)  |
| **Draw commands/frame** | ~80 at rung 1           | **~5–15**                                  |

🟢 **The command count — the metric the task flagged as most at risk — is the one that improved most,
and strip merging turned out to be far more than enough.** Batching by draw state is what does it: the
fill is one command per _appearance_ (typically two: occupier-held and occupier-contested) rather than
one per quad, and the band is one per contour. Against Q-1's ≤ 250/frame budget that is an order of
magnitude of headroom. **Without batching** (`m_bBatchDrawCommands 0`) the same geometry costs roughly
**500–900 commands** and would blow the budget — which is exactly why batching is the default and why
the fallback is labelled as a diagnostic rather than a configuration.

⚠️ **Still unmeasured, and all of it needs the game:** the grid classify time against the 250 ms budget,
the per-frame emit against 1.5 ms, and whether 100 m squares read as acceptably blocky at map zoom.

---

#### 9. What may still read badly — the shortlist for the fourth look

1. ~~**🔴 The fill edge is now BLOCKY by construction.** A rectangle is cut on square boundaries, so at
   100 m squares and a typical map zoom the fill steps in ~11 px increments while the traced border over
   it is smooth. **That mismatch is the design, not a defect** — but if the steps read badly the lever is
   `m_fGridCellSize`, and it costs **quadratically**: 50 m is four times the classify time.~~
   🔴 **RETIRED BY D13, and the prediction was right but the verdict was wrong.** The steps did read
   badly — the user's words were _"the 'filled' areas are sticking to the grid while the bands are
   smoothed"_ — and calling the mismatch "the design" is what D13 had to undo. The fill is now cut out of
   the contour itself. **This paragraph now describes only the `m_bTraceContours 0` fallback.**
2. **The neutral band can fold at tight corners.** It is offset inward from the border, and a width
   larger than the border's local radius of curvature — roughly half a square after smoothing — crosses
   its own inner edge and composites darker. `m_fBandWidth` ships at **80 m**, below the square size, for
   exactly this reason. Widen it only alongside a larger square size or more smoothing passes.
3. **The band is still the loudest thing on the map** (D10's item 1, unchanged): alpha 120 over a fill of
   70, on every edge against liberated ground. First lever is still `m_iBandAlpha` toward 90.
4. **Blank holes around FOBs and captured towers** (D10's item 2, unchanged): that ground _is_ liberated.
   If it reads badly, drop the type's weight or set `m_bEnabled 0` — **never** filter at collect.
5. **The two contested tones** (D10's item 3, unchanged).
6. **Gone, and should stay gone:** the pale seams between same-faction regions, the ragged coastal
   fringe, and unfilled land across a bay. If any of those reappears it is a _new_ defect, not a
   recurrence — the representation cannot produce them.

### D11 — Contested is **hatched**, and restricted rings take the **faction's** colour (user decision, 2026-08-11)

**The first decision in this feature taken with real art in the tree.** The user authored and imported a
tiling diagonal hatch — `{B7E8255E75EE66ED}UI/Textures/Map/overthrow_map_diagonal.edds`, tiling confirmed
by them — and redirected it. Their words:

> _"I want to use this in contested areas"_
>
> _"leave restricted solid for now, as they look fine atm and are readable, they just increase the
> opacity — however for it to work properly we might need to change the color of the restricted areas to
> match the faction"_

---

#### 1. Contested regions are hatched, not merely fainter

**This is a better idiom than the plan had.** §4 D4 earmarked textures for the neutral bands and the
restricted rings; the first one goes to **contested fill** instead. Hatching is the conventional visual
language for disputed ground, and it fixes a real weakness in D10: expressing contested purely as _lower
alpha_ made a contested region **quieter** than a held one, when a region slipping out of the occupier's
hands is the more interesting thing on the map.

**As applied.**

- `OVT_TerritoryCell.m_bContested` (defaults **false**) is resolved on the **same recolour tick** as the
  colour and the alpha, from the unchanged `OVT_TerritorySolver.IsContestedCell`. **The predicate did not
  change shape** — case 15 still pins it exactly as written.
- Resolved **once** in `RecolourCells` and carried, rather than asked again at emit: the alpha and the
  hatch must not be able to disagree, because a region at contested alpha _without_ the hatch reads as a
  rendering fault rather than as a state.
- `UseContestedTexture(cell)` gates the texture at **one** place feeding all three emit rungs. Held cells
  are solid; only contested cells carry `m_pTexture`.
- **`m_iContestedAlpha` 35 → 95.** The reasoning is the reason D10's 35 no longer applies: a hatch covers
  only part of the area it fills, so the _same_ alpha through a hatch lays down less ink than a solid
  fill. 95 sits **above** the flat 70 so a disputed region reads at least as present as a held one, and
  **below** the band's 120 so the frontier still reads as the edge. It is 95 rather than an arbitrary
  number because that is P5-E's measured legibility ceiling _for a solid fill_ — through a partial-coverage
  hatch it stays under that ceiling in practice. **It is an attribute; tune it by eye.** If the hatch fails
  to load, contested is still distinguishable at 95 against 70 — louder rather than fainter, which inverts
  D10's metaphor but keeps two states rather than none.
- **`m_bUseTextures` default flipped 0 → 1**, and the resource name is now the attribute default, so the
  art is reached with no `.conf` edit. `m_fUVScaleContested` ships at **0.01** — ⚠️ **a starting point, not
  a measurement.** The units are still undocumented and unmeasured; nothing in this feature has established
  how many pixels or metres one tile covers. If the hatch **slides as the map is panned**, that is UVs
  derived from screen-space vertices and no value here fixes it.
- **The load-once / `IsValid()` / degrade-to-flat-with-an-ERROR behaviour is untouched.** It is now
  attempted in `OnMapOpen` as well as `Draw` (idempotent behind its flag) so the solve report can say
  truthfully whether the hatch is available.
- **The neutral band keeps its solid treatment.** An **empty** band texture is now silent rather than an
  ERROR — with the master switch on, empty is the shipped configuration, not a missing asset. A
  _configured_ band texture that fails to load still logs.

#### 2. Restricted rings take the owning faction's colour

**The user's reasoning is subtle and it is right.** They are happy with a restricted zone reading as _"the
same territory, denser"_ — that is the "increase the opacity" effect they describe. But that effect only
reads cleanly **if the ring and the territory beneath it share a hue.** The hardcoded `ARGB(50,255,0,0)`
and `ARGB(40,0,120,255)` produced a **muddy third colour** over faction-coloured territory instead of a
denser version of the one underneath.

**As applied.**

- Each ring's hue comes from `OVT_MapLocationType.GetFactionColorByIndex` — **the same helper** the markers
  and the territory fill use. That is **K6's one-colour-source principle extended to its third consumer**,
  and there is still exactly one implementation of "what colour is faction N". Unresolvable faction falls
  back to **white**, matching the territory layer rather than reintroducing a hardcoded red.
- **Both alpha tiers survive** and are now attributes: `m_iRestrictedAlpha` **50** (occupier-held),
  `m_iFriendlyRestrictedAlpha` **40** (resistance-held). The alpha is what makes a ring read as denser than
  the ground under it, so it is the one thing that must not collapse to a single value.
- **The faction index reaches `Draw()` without a per-frame manager lookup.** `OnMapOpen` collects
  `m_FactionIndices` / `m_FriendlyFactionIndices` alongside the centres and radii, and `ResolveRingColours()`
  — run **once, after both collect loops** — turns them into `m_Colors` / `m_FriendlyColors`. `Draw()` does a
  plain array read. Resolving in `Draw` would have put a faction-manager query on the only path in this layer
  that runs every frame. Building the colour arrays after _both_ loops (rather than inside them) is what
  keeps them index-parallel across the early-`continue`s, and `Draw` bails on a count mismatch rather than
  painting rings from the wrong records.
- 🔴 **GEOMETRY IS UNTOUCHED — DoD I-3 / BUG-070.** Same centres, same radii, same
  `baseCloseRange + FOB_DEPLOY_BASE_BUFFER` and `FOB_DEPLOY_TOWER_RANGE` sources. **Proven by diff:** zero
  changed `Centers.Insert` / `Ranges.Insert` lines, and all eight geometry lines byte-identical to `HEAD`.

#### 3. The QRF circles — distinguished by **structure**, not by hue

**The QRF is the one thing on this layer that does NOT take the faction's colour, and the interesting part
is why the obvious alternative was also rejected.**

- It must not be faction-coloured: a QRF fires when the occupier counter-attacks, so it appears **inside
  occupier territory almost by definition**, and the occupier's own hue would dissolve it into its
  background at exactly the moment it matters most. It is also a different **category** of information — a
  base ring is reference state true for the whole campaign; a QRF is an alert that appears and vanishes on a
  minute timescale.
- 🔴 **But it cannot be a fixed "alarm colour" either, and this is the reasoning that is unrecoverable from
  the code.** An alarm hue assumes a fixed palette to contrast _against_, and there is none: the occupying
  faction is **configurable** (`OVT_OverthrowConfigComponent.m_sOccupyingFaction`) and faction colours run
  from red to blue — the US faction is blue. Orange reads well against a blue occupier and disappears
  against a red one. **No fixed hue is reliably contrasting**, so hue was rejected as the QRF's
  distinguishing channel. _(An earlier revision of this decision did ship an orange alarm colour; the user
  caught the flaw.)_ **Do not "simplify" this back to a hardcoded colour.**
- **Structure carries it instead: the QRF circles are hatched**, at a UV scale deliberately set apart from
  the contested fill's. Structure is hue-independent, so it reads whatever colour the occupier turns out to
  be. The semantic overlap with contested is a **generalisation, not a collision** — hatch means _"this
  ground is being fought over"_, and a QRF is the most literal case of that. The two are kept apart by
  **scale**: `m_fQRFUVScale` **0.003** against the fill's **0.01**, both attributes. ⚠️ Which direction is
  _coarser_ is **unverified** — if the QRF reads finer than a contested town instead, the mapping is
  inverted and **0.03** is the value to try.
- The QRF colour stays the **current red**, now as four attributes (`m_iQRFAlpha` 50 / 255 / 0 / 0). It is
  the secondary signal now, so it no longer has to carry the distinction alone.
- The QRF hatch has its **own** texture ref, flags and switch (`m_bUseQRFTexture`, default 1), independent
  of the ring texture (`m_bUseTexture`, still **0** — rings ship solid). One switch could not express both.
  It is attempted from `Draw` because a QRF can start while the map is already open, but still at most once
  per map session.

**Gates:** compile **exit 0 / 5971 files**, Fast **70**, All **105** — all three identical to the pre-D11
baseline, which is the expected result: this decision changes colours and textures, and **neither is
assertable in the Logic tier.** No test case was added or changed; inventing one that restates a constant
would be worse than none. ⚠️ **Nothing in D11 has been rendered.**

---

### D10 — The overlay shows **occupier territory only**, contested is **support ≥ 50 %**, and the coastline is not smoothed (user decision, 2026-08-11)

**Three changes from one user session, and the first is a reframing of what the overlay is FOR.** Their words:

> _"we may also not render the zones controlled by the resistance. the aim of the game is to clear the
> occupying forces out so it might make sense that nothing renders in the areas you've 'liberated'."_
>
> _"if support is over 50% and its occupier controlled that region should show as contested"_

---

#### 1. Occupier territory only — the reframing

The overlay stops answering **"who holds what"** and starts answering **"how much does the occupier still
hold"**. Liberated ground reads as _clean map_. On an island that starts ~100 % occupied that turns the fill
into a **progress bar for the entire campaign**, and clearing a region shows as the region _disappearing_
rather than changing colour. A **fully liberated island therefore draws nothing at all** — that is the
intended end state, not a failure, and the solve log is what distinguishes it from a broken overlay.

🔴 **The trap, and the whole reason this needed care: resistance sites still COMPETE in the solve. They are
simply not EMITTED.** Every FOB, every liberated town and every captured base still pushes the occupier's
boundary back — that is what _makes_ liberated ground go blank. Filter them out at **collect** time instead
and occupier territory flows straight over ground the player has already taken, with no error, no crash and a
perfectly healthy-looking site count in the log. **Filter at emit, never at collect.**

**As applied.**

- `[Attribute] bool m_bOnlyShowOccupying`, default **1**. Flipping the whole reframing back off is one config
  value.
- The rule itself is the pure static **`OVT_TerritorySolver.IsEmittedCell(cellFaction, occupyingFaction,
onlyOccupying)`** — on the solver for the same reason D7 put the band rule there: the layer is invisible to
  every automated tier, and every defect this feature has shipped so far has been exactly this kind of
  one-line comparison about faction.
- It is evaluated in **`RecolourCells`**, which is the O(sites) tick, so a capture blanks or fills a region
  **within one refresh interval** and never triggers a geometry re-solve (K9's split is preserved exactly —
  faction is still out of `HashSites`). The answer is carried on the cell as `OVT_TerritoryCell.m_bEmit`,
  which **defaults to `true`** so a cell that has not been through a recolour pass is drawn rather than
  silently missing.
- The occupying faction index is read **per call** through `OVT_Global.GetConfig().GetOccupyingFactionIndex()`
  (K10's idiom) and **never cached on the layer**. An **unresolvable occupier degrades to drawing everything**,
  never to drawing nothing — an empty overlay is this feature's least visible failure, and that fallback is
  pinned by a case.
- **Bands follow the fill, structurally.** `EmitCell` returns before it emits any band, so a band belonging to
  a cell that is not drawn cannot exist. A frontier between occupier and resistance is the _edge of occupier
  control_ and is drawn on the occupier's side only.
- **The solve log now reports the drawn count**: `N sites -> N cells (N drawn)`. That single number separates
  the two ways this feature can go blank — sites far above drawn is the overlay working as designed on a
  mostly-liberated island; the _site count itself_ collapsing is somebody filtering at collect time.

---

#### 2. Contested = occupier-held **AND** support ≥ 50 % — and why this overrides K8

> ⚠️ **§6 K8 explicitly rejected support as a driver, and this overrides it.** K8's objection was that _"a
> resistance-supporting town still held by the occupier would read as resistance territory"_, making the
> overlay a **second opinion about control** — the exact boundary §3.4 forbids. **The user's framing escapes
> that objection and the plan never considered it.** The region is still drawn in the **occupier's** colour,
> because the occupier _does_ control it. Support does not recolour it — it **marks** it. That is a second
> axis layered over an accurate first one, not a competing answer to the same question. K8's rejection was
> correct on its own terms and remains correct _as a rejection of colouring by support_; it simply never
> examined marking.

**As applied.**

- `[Attribute] float m_fContestedSupportThreshold`, default **0.5**, and `[Attribute] int m_iContestedAlpha`,
  default **35** against the flat `m_iFillAlpha` of 70.
- **A threshold, not a gradient — two states, not forty.** A continuous per-site shade is exactly what
  produced the patchwork D9 had to fix; two states cannot do that. The signal is carried by alpha because
  under D10 only one faction is ever drawn, so alpha is the only channel that exists — but it now takes
  **two discrete values**, which is a different thing from the lerp D9 removed.
- **Fainter, not louder** (35 against 70). The occupier's grip is weakening, so its ink _thins before it
  disappears_ — which is the same metaphor the reframing runs on. Raise it before lowering it if the two
  states do not read apart, and re-check terrain legibility after any raise (P5-E's rule).
- **Towns only.** Bases, radio towers and FOBs have no popular support and carry
  `OVT_TerritorySite.SUPPORT_NONE` (= −1), rejected explicitly rather than by arithmetic.
- The rule is the pure static **`OVT_TerritorySolver.IsContestedCell(...)`**, alongside the emit rule and for
  the same reason.

🔴 **`OVT_TownData.SupportPercentage()` is deliberately NOT called.** The support fraction is computed inline
in `ResolveTownSupportFraction` as **`(support * 100.0) / population`**, float-promoted first — the _same
expression the town info panel already uses_ to print the percentage the player sees when they click the
town — then normalised to a 0..1 fraction. So the threshold and the number on screen cannot disagree about
what "half the town" means. `population <= 0` returns `SUPPORT_NONE`, which doubles as the divide-by-zero
guard. **No bug was filed and nothing was "fixed"**: `OVT_TEST_Logic_Town_SupportPercentage_Boundaries`
already asserts that helper returns 50 for 25-of-50, so the suspicion that its `(support / population) * 100`
truncates is **not** supported by the tree's own evidence — this is an _agreement_ decision, not a
workaround.

**And stability drops out of the visuals entirely.** D9's `lerp(m_iFillAlphaMin, m_iFillAlphaMax,
stability/100)` is **deleted, not hidden behind a second flag** — `m_iFillAlphaMin`/`m_iFillAlphaMax` are
gone, `OVT_TerritorySite.m_fHoldStrength` is replaced by `m_fSupportFraction`, and `m_bContestedShading`
(now default **1**) is the **one** switch for the contested signal. D9 kept the lerp reachable on the grounds
that _"if the overlay ever earns a way to show [contested] that does not fragment the map, the lerp is where
it starts"_ — D10 is that way, and it does not start at the lerp, so keeping a second dormant mechanism would
only be a second thing to be wrong about.

---

#### 3. The coastline fringe — D9's own residual, and the third face of "splotchy"

D9's report named it: **the K5 clamp still applied at `COAST`, so every cell retreated from the shore**, and
coast radii vary sharply ray-to-ray, which is where a shrink-only filter erodes hardest. On a coast-heavy
island that is a **ragged pale fringe between the fill and the sea** — the original D6 complaint ("parts of
the map not filled") in a different place.

**Fix: coast-stopped rays keep their raw marched radius**, the same treatment D9 gave friendly-stopped rays.

**The justification is not "the same trick worked before".** The requirement smoothing exists to serve is
_"borders read as organic frontiers rather than straight bisectors"_ — and **a bisector is the only straight
thing in this solve**. A coastline is **already** organic: the raw marched radius, sharpened by the bisection
refine to ~2.34 m, _is_ the shoreline. Averaging it can only pull the fill **back from a boundary that was
already correct**.

**The averaging window: a DIFFERENT call from D9's, deliberately.** D9 additionally removed friendly rays
from their neighbours' windows. **Coast rays are not removed — they still contribute.** Both halves of D9's
argument fail to transfer:

- D9's reason was that _a friendly boundary is a local **minimum** of the radius field almost everywhere_, so
  letting it into a window reliably drags that neighbour inward. **A coast radius is not reliably a local
  minimum** — it is a minimum where the ray points at the nearest shore and a **maximum** where it grazes the
  coast at a shallow angle, so there is no systematic pull to protect against.
- The exclusion exists to stop a gap reopening at the ends of a **shared** edge. **A coastline has no second
  cell on the far side of it**, so there is no shared edge and no gap. What contributing buys instead is the
  one job smoothing has left on a coastal cell: **rounding the corner where a frontier runs into the sea**,
  which is the junction where a straight bisector against a curved coast looks most artificial.

Mechanically this is a **second, separate array**: `SmoothRadii(radii, passes, raw, exempt, pinned)`.
`exempt` (friendly) means _keeps its raw radius **and** takes no part in any neighbour's average_; `pinned`
(coast) means _keeps its raw radius **but still counts** toward its neighbours' averages_. One array could
not express both, and collapsing them would have changed the friendly rule.

⚠️ **This choice is also what kept case 13 honest, and that was checked rather than assumed.** Had coast rays
been _isolating_, case 13's channel world would have had every neighbour of its friendly boundary exempt,
its assertion would have passed **for the wrong reason**, and D9's inversion (a) would no longer have turned
it red — a silently vacuous case, which is worse than a red one. Re-run under D10, **inversion (a) still
fails it** (_"The ray ending on a SAME-FACTION neighbour was smoothed from 500 m to 287.5 m"_ — 260.9 m under
D9; the arithmetic moved because coast neighbours are now pinned, the conclusion did not).

**🔴 The K5 clamp is NOT disabled and must not be.** It stays in full force wherever smoothing still applies.
No vertex may be pushed into a rival's cell.

---

#### What smoothing still does — stated plainly, because the reduction is large

After D10, smoothing applies only to **hostile-`RIVAL`** rays and **`MAX_RADIUS`** rays. It is **not inert,
and it is not useless — it is targeted**, but the honest shape of it is worth writing down:

- **It never did much on a pure rival arc anyway.** For equal weights the boundary radius is `d / (2 cos θ)`,
  which is **convex** in θ, so the 3-point average always exceeds the centre value and the shrink-only clamp
  pins it. That was true before D10. Smoothing's real job has always been **rounding corners** — the vertex
  where two bisectors meet, which is a local _maximum_.
- **What it now rounds is the corner of a hostile frontier**, which under D10 is _the visible edge of occupier
  control_ — the single line on the map the player is actually watching. So the reduction moved smoothing
  onto the boundary that matters and off the two that were never boundaries at all.
- **Early campaign it will do almost nothing**, because almost every boundary is occupier-vs-occupier
  (friendly, exempt) or occupier-vs-sea (coast, pinned). **Its scope grows with the front line.** That is a
  property to expect in the log, not a defect: a solve reporting a large friendly + coast count and a small
  frontier count is a map that is mostly one faction's, and there is genuinely almost nothing to smooth.
- `MAX_RADIUS` rays are the safety bound and _should not exist on a real world_. If smoothing ever appears to
  be doing visible work on them, the diagnosis is the bound, not the filter.

---

#### What is still likely to read as "splotchy" — the third pass on that complaint

Named in advance so the fourth report, if there is one, starts from a shortlist rather than from scratch.
**None of these is fixed here** and none is a defect in what D10 changed.

1. **🔴 The neutral band is now the loudest thing on the map, and it is the top suspect.** Under D10 _every_
   drawn boundary against liberated ground is a hostile frontier, so it is banded — the outer
   `m_fBandFraction` (**0.15**) of that arc, at `m_iBandAlpha` **120** over a fill of **70**. A small occupier
   pocket surrounded by liberated ground is banded around most of its ring and will read as a **dark blob**
   rather than a region. **First lever: drop `m_iBandAlpha` toward 90, then `m_fBandFraction` toward 0.08.**
2. **🔴 Blank holes inside occupier territory, around every FOB and every captured radio tower.** This is the
   reframing working exactly as specified — that ground _is_ liberated — but visually it is a hole punched in
   a region, and a FOB (weight 0.55) against a town (1.0) blanks a disc reaching ~35 % of the way to the town.
   Expect several of them mid-campaign. If it reads badly the honest options are to drop the FOB weight, or
   to exclude FOBs from the site list entirely (a config `m_bEnabled 0`), **not** to filter them at collect
   time — they must keep competing.
3. **The two contested tones are a deliberate patchwork.** Towns hovering near 50 % support will differ from
   their neighbours by one alpha step. That is the signal working; it is two tones rather than forty, and the
   lever is `m_fContestedSupportThreshold` (raise it so fewer towns qualify) or `m_iContestedAlpha` (move it
   closer to 70 so the step is quieter).
4. **D9's hairline overlap at curved boundaries is unchanged** (~2 m at a 1000 m boundary, sub-pixel zoomed
   out) — and under D10 _every_ internal boundary in the drawn set is same-faction, so this is now the only
   internal artefact left. Lever is `m_iRayCount`, quadratically.
5. **The coast fringe should be gone.** If a pale gap along the shore survives, it is **not** smoothing any
   more: look at `m_fShorelineMargin`, at the 150 m march step straddling a bay, or at the bisection returning
   `lo` (the last known-land sample, always ≤ the true shore).

---

### D9 — **Flat alpha per faction**, and same-faction cells tessellate (user finding, 2026-08-11)

**The user reported "splotchy" for the second time, with a screenshot.** The shot shows an island held almost
entirely by one faction. Because everything is one colour, **alpha was the only variable left**, and it read as
mottled noise — darker patches, lighter patches, pale gaps between blobs, no legible structure. It did not read
as territory; it read as a stain.

**Two independent causes were found, and both are fixed. Neither is a coding slip; both are the plan working
exactly as written and the result being wrong.**

---

#### Cause 1 — per-site alpha fragments a single-faction island

§6 **K8** made fill alpha `lerp(m_iFillAlphaMin, m_iFillAlphaMax, holdStrength)` _per site_, with towns feeding
`stability/100`. On a settled map, hue carries no information at all, so two neighbouring cells that are one
territory in every sense a player cares about differ **only** in alpha — and the map fragments into dozens of
differently-shaded blobs. The signal K8 wanted (contested vs firmly held) is real, and it was being drowned by
the noise it created.

**As applied.** The fill ships **one flat alpha per faction**: a new `[Attribute] int m_iFillAlpha` (**70**) and
`[Attribute] bool m_bContestedShading` (**0 — off**). `m_iFillAlphaMin` / `m_iFillAlphaMax` and the lerp are
**kept intact and reachable** behind the flag, in one method (`ResolveFillAlpha`), so turning contested shading
back on is one config value rather than a rewrite. The layer's `tunables:` log line now reports the flat alpha
and the flag, so a pasted-back measurement says which mode produced it.

**Why 70 and not 95.** P5-E's 30→95 range is a _legibility_ range, and 95 is its **ceiling** — about 37 %
coverage, where roads, contours and place names stop reading through. Under K8 that ceiling was paid only by
firmly-held sites; a flat value pays it **everywhere on the map at once**, which is the loudest possible
setting and part of what the screenshot is complaining about. 70 (≈ 27 %) is comfortably above the 30 wash so a
region reads as a deliberate tint rather than a smudge, and leaves 25 of headroom under the ceiling for the
band (alpha 120) and the markers to still read on top. **Raise it before lowering it** if the overlay reads too
faint, and re-check terrain legibility after any raise — the same rule P5-E set.

> ⚠️ **THIS WAIVES DoD F-6** — _"a town with low stability has a visibly more transparent region"_. **Waived by
> the user on 2026-08-11**, deliberately and with the reason above: the criterion is satisfiable and its
> satisfaction is what made the map unreadable. It is recorded here rather than dropped, and the mechanism that
> satisfied it is still in the code behind `m_bContestedShading`. If a future pass wants the contested signal
> back, the lesson is that it must not be carried by **alpha on the fill** — that channel is spent making one
> faction's ground read as one region.

---

#### Cause 2 — shrink-only smoothing tore same-faction cells apart

§6 **K5** clamps smoothing to `Min(smoothed, raw)` so a vertex can never be pushed past the coastline or into a
rival's cell. That is correct at a **real** edge. At a boundary with a **same-faction** neighbour it is wrong in
a specific way: both cells shrink away from the boundary they _share_, and the ground between them is filled by
neither. Drawn, that is the pale gap running between blobs in the screenshot — and it is the same defect D7
fixed for _bands_, showing up again in _geometry_.

**As applied.** Smoothing is applied only where the boundary is real. D7 already records which site won each ray
(`m_aStopSite`), so the solver now also carries the positive predicate
`OVT_TerritorySolver.IsFriendlyStop(stopReason, stopSite, ownFactionIndex, sites)` — deliberately **not** the
negation of `IsFrontierStop`, because a coast ray and a safety-bound ray are neither friendly nor a frontier.
`Solve` builds a per-ray exemption array from it and hands it to `SmoothRadii`, which gained an optional fourth
parameter. An exempt ray **keeps its raw marched radius**; every other ray is smoothed and clamped exactly as
before.

🔴 **The K5 clamp was NOT disabled, and must not be.** It stays in force for `COAST` and hostile-`RIVAL` rays,
which is where the two errors it exists to prevent actually live. The friendly case is exempt for one reason
only: **there is nothing on the far side to protect against — the neighbour is the same colour.** Case 7 now
pins both halves of that (see the inversion table).

**The averaging window, decided and justified.** An exempt ray is **also excluded from contributing** to its
neighbours' averages; it is replaced in their window by the ray's own value, so the window stays width 3 and
uniformly weighted and a run of smoothable rays is averaged only against itself. The reason is the _shape of the
data_, not symmetry: a rival boundary is a **local minimum** of the radius field almost everywhere (the boundary
between two sites is nearest along the line joining them), so an exempt ray is typically **shorter** than its
neighbours. Letting it into their windows drags them inward precisely where the seam meets the rest of the
cell — **tapering the gap open again at each end of the boundary the exemption was added to close**. The reverse
configuration (an exempt ray _longer_ than its neighbours, e.g. a friendly boundary flanked by coastline) is
real and harmless: excluding it costs those neighbours a little extra erosion on ground interior to their own
cell, never on a shared edge.

**Do same-faction cells now tessellate exactly? Where the boundary is straight, yes. Where it is curved, to
within a few metres — and it is a _hairline_, not a seam.** Both cells' rays land on the _same_ Apollonius
boundary, but each cell polygonalises it with **its own** vertex set, so the two chord-polylines coincide only
where the boundary is a straight line. That is the **common** case and is exact: equal weights — two towns of
one size, two bases, a base and its own tower after the size factor — give a linear bisector, and both
polylines lie on it. Unequal weights curve it, and the two inscribed polygons then differ by one segment's
sagitta, `d × (1 − cos(π/rays))` ≈ **d × 0.00214** at the shipped 48 rays — about **2 m at a 1000 m boundary**,
alternating gap and overlap along the edge. An overlap of two 70-alpha fills composites to ≈ 47 % rather than
27 %, so it darkens; at ~2 m on a zoomed-out map that is sub-pixel. **This is a property of D6/K3's per-site ray
polygonalisation, not of D9**, and it is orders of magnitude smaller than the sliver K5's shrink was opening.
The lever if it ever shows is `m_iRayCount`, which shrinks it quadratically.

⚠️ **Residual, stated rather than hidden: this is the one place faction reaches the geometry, and it is
evaluated at SOLVE time.** P4-E deliberately keeps faction out of `HashSites`, so a capture recolours without
re-solving — which means a boundary that flips from friendly to hostile (or back) **keeps the smoothing
treatment it was solved with** until the next real re-solve. The size of that staleness is one filter pass's
erosion on the handful of rays facing the site that changed hands, i.e. metres, against the sliver the exemption
removes. **Adding faction to the hash was considered and rejected**: it would make every capture trigger a full
geometry re-solve, which is exactly the split K9 exists to prevent.

### Phase 7 findings — the multiplayer audit (2026-08-11)

⚠️ **Every finding in this section comes from reading code, not from running two clients.** They say the
data the overlay reads _is_ present on a client; they say nothing about what the overlay looks like there.
I-5, I-6 and I-7 remain entirely unproven.

**P7-A — 🟢 `OVT_TownData.size` IS readable on a remote client, and P4-D was over-pessimistic.** This was
the highest-value question in the phase, because `size` feeds the town's site weight and therefore cell
**geometry** — if it were 0 on one machine and 3 on another, two clients would compute _different_
territory from identical campaign state and silently fail I-5. It is not. The chain:

- `OVT_OverthrowGameMode.EOnInit` calls `m_TownManager.Init(this)` with **no server guard** — it runs on
  every machine.
- `Init` calls `InitializeTowns()` **unconditionally** (only the legacy `SetupTowns()` behind it is
  server-gated), which world-queries **static** entities carrying `OVT_TownControllerComponent`.
- `FilterTownControllerEntities` builds each `OVT_TownData` locally and sets `town.size =
townController.m_Size`. `m_Size` is an `[Attribute]` on the controller component, authored in the world
  — so it is byte-identical on every machine by construction, not by replication.
- The legacy auto-detect path is the same story: `size` comes from the map descriptor's base type
  (village/town/city), also world-static, also computed locally on the client.

So `[NonSerialized()]` is **correct and deliberate**: `size` is _rediscovered_, not replicated, and
`RplSave`/`RplLoad` carry only the fields that actually change (population, target population, stability,
support, faction, and the two modifier lists). **P4-D's "legitimately 0 on a client" is wrong as a
statement about the shipped game**, and the correction is recorded there rather than deleted.

**The defensive code stays exactly as it is.** `ResolveTownSizeFactor` still returns
`SIZE_FACTOR_UNKNOWN = 1.0` for a null town, a 0, or an out-of-range value, and weight is still folded into
`HashSites`. Both are cheap, both are correct, and both cover the one window this audit cannot rule out:
a client whose world query has not yet found the controllers when the map is first opened. The overlay
heals on the next refresh tick because the weight changes, the hash changes, and the geometry re-solves.

**P7-B — the `HashSites` split is correct, and here is which event does what.** Four events, two outcomes:

| Event                    | Changes the hash? | What happens                                                                                                                                                                                           | Correct per K9?                               |
| ------------------------ | ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------- |
| **Base captured**        | **No**            | `RpcDo_SetBaseFaction` broadcasts; the next refresh tick re-collects, the hash matches, `RecolourCells()` repaints in **O(sites)**. Geometry is untouched — a base does not move when it changes hands | ✅ yes, this is exactly the split K9 asks for |
| **Radio tower captured** | **No**            | `RpcDo_SetRadioTowerFaction` broadcasts; same O(sites) recolour                                                                                                                                        | ✅ yes                                        |
| **FOB built**            | **Yes**           | `RpcDo_RegisterFOB` broadcasts and `m_FOBs` gains a record ⇒ site count **and** the per-site position/type terms change ⇒ full re-solve                                                                | ✅ yes — a site appearing must re-solve       |
| **FOB destroyed**        | **Yes**           | `RpcDo_RemoveFOB` broadcasts and the record leaves `m_FOBs` ⇒ same                                                                                                                                     | ✅ yes                                        |

Two more that fall out of the same rule and are also right: a **town changing hands**
(`RpcDo_SetTownFaction`) recolours only, and a **town's stability moving** (`RpcDo_SetStability`) changes
only the fill alpha, because hold strength is a _separate_ value from weight and is deliberately not in the
hash. The one thing that legitimately re-solves without any site appearing is a town whose **size** becomes
readable later (P7-A's JIP window), which is the belt-and-braces P4-D described.

⚠️ **The timing is a refresh tick, not an event.** Nothing subscribes to these RPCs; `RefreshTick` polls
every `m_fRefreshInterval` (5 s) while the map is open. So "within one refresh interval" in I-6/I-7 is
literal, and a capture watched with a stopwatch can take up to 5 s to show. That is the design, not a lag.

**P7-C — the camp exclusion is still in place and still carries its reason.** `CollectSites` calls exactly
four collectors and no `CollectCamps`, and the block comment explaining _why_ — a camp belongs to one
player and can be marked private, so a territory region around one would broadcast that player's position
to every client — is intact, along with the sentence telling a future reader it is not a to-do. The four
types that _are_ collected are globally-visible campaign state already drawn as markers for every player,
so the overlay reveals nothing a player cannot already see on the same map.

**P7-D — I-4's boundary greps, run over the diff AND the untracked files, all clean.** No `[RplProp]`, no
`[RplRpc]`, no `RpcAsk_`/`RpcDo_`, no `EPF_` class added anywhere; no write to any field of `OVT_TownData`,
`OVT_BaseData`, `OVT_RadioTowerData` or `OVT_FOBData`; `OVT_PlayerCommsComponent` untouched; and all seven
new `.c` files live under `Scripts/Game/UI/Map/` or `Scripts/Game/Tests/`. ⚠️ Running these on `git diff`
alone would have proved nothing — **every new file in this feature is untracked**, so the diff cannot see
them. Whoever re-runs V-9 must grep the untracked set too.

**P7-E — one observation that belongs to `towns/core`, reported not fixed.** Each machine builds its own
`m_Towns` from its own world query, and `RplLoad` then matches the server's records to it **by index**. If
the query ever returned entities in a different order on two machines, every town's replicated faction and
stability would land on the wrong town — silently. The manager already warns on a _count_ mismatch, which
suggests the risk is known, but nothing checks the _order_. This is not territory's defect and territory
does not make it worse (site geometry is order-independent, and the markers read the same records), but it
is the kind of thing the two-client gate could surface as "the wrong town is shaded". **Not filed as a bug**
— no evidence exists that engine query order actually varies.

### D8 — The rival boundary is **solved**, not marched (Phase 6, 2026-08-11)

**Why this was forced.** D6 removed the maximum influence radius, and that invalidated §6 K4's entire cost
model. K4 priced the solve at ~150 k comparisons on two assumptions that D6 made false: that rays stop
early (they now run to the coastline) and that a `2 × maxRadius` filter cuts ~40 rivals to ~5–8 (with an
unlimited reach the filter is _exhaustive_ and excludes nobody — the code already said so). The realistic
shape afterwards was ~40 sites × 48 rays × ~40 steps × ~40 rivals ≈ **3 M inner iterations**, against a
250 ms budget, in an **interpreted** language.

**Three levers, in ascending order of how much they change.**

**LEVER A — march step 50 → 150 m, refine 4 → 6.** A linear cut on terrain samples. It is not a loss of
accuracy: final boundary error is `step / 2^refine`, so 150/2⁶ = **2.34 m** is _sharper_ than the 50/2⁴ =
3.13 m it replaces. The reason that is free is structural — **the refine costs per ray, the march costs
per step** — so two extra bisections are ~2 samples on the rays that actually hit a coast, against ~2/3 of
every land sample saved. The residual risk is real and stated: a 150 m step can straddle a water channel
narrower than 150 m, which reads as a cell crossing a strait. Both knobs are config attributes.

**LEVER B — prune the rival scan at the ray's CURRENT radius.** The bound `BuildCandidates` proves is
evaluated at `maxRadius`; evaluated at `r` it is `r > d(S,R) / (1 + w_R/w_S)`. That right-hand side depends
only on the two sites, so it is computed **once per site** (`BuildSortedRivals`), sorted ascending, and the
scan stops at the first candidate whose threshold already exceeds the best takeover found. **It is a proof,
not a heuristic** — a skipped rival provably could not have won. ⚠️ **What it does not prune:** a ray
pointing _away_ from every nearby rival, because the bound is direction-blind and the running minimum stays
empty. Those rays still scan the whole list, so the honest expectation is a **2–4×** cut on the rival work,
not the clean 5× K4 imagined.

**LEVER C — the closed-form takeover radius, which is the structural fix.** Along a ray `q(r) = C + r·u`,
rival `R` beats site `S` exactly when

```
(w_S² − w_R²) r² + 2·w_S²·(u · (C − R))·r + w_S²·|C − R|² < 0
```

so the takeover is the **smallest strictly positive root**, and the ray's rival boundary is the minimum of
that over the candidates. This removes the `× steps` factor from the rival test **entirely**: the march now
samples nothing but land. Two further wins fall out — the rival boundary becomes **exact** rather than
bisection-approximated, and a thin sliver of rival territory can no longer be stepped over.

**Three branches, and all three are real.** `A = 0` (equal weights) degenerates to a **line**, and is the
_common_ case, not a rounding accident — two towns of one size, or two bases, carry byte-identical weights.
`A < 0` (heavier rival) has roots straddling zero and takes the ray **in every direction eventually**,
because a lighter site's region is a bounded Apollonius disk. `A > 0` (heavier site) has both roots the same
sign; two positive ones mean the ray crosses the rival's disk and comes out the far side, and taking the
smaller is exactly what the march would have done.

**Why `OwnsPoint` survives as the definition of ownership.** The closed form is a solution _of_ that
predicate, so the predicate is what says whether the solution is right. Cases 2 and 3 still pin it directly,
and **case 12 bridges the two**: a metre inside each solved boundary the site must still own the ground and
a metre outside the named winner must. The brute-force marcher was **kept**, as
`OVT_TerritorySolver.SolveRayMarched`, and nothing ships through it — it is the independent oracle case 12
compares against, and deleting it as dead code would delete the only evidence the algebra is right. That is
not theoretical: the first inversion run against case 12 was caught _by the oracle_, which reported both
answers side by side.

**What was NOT done, and why.** No incremental across-frames solve (§5's second fallback) — it changes the
feature's shape and there is no measurement yet saying it is needed. No ray-count cut. No every-other-step
land sampling. No draw-command pooling: the compositor holds a pointer to the command list, so recycling
command objects changes lifetime semantics that cannot be checked without running the game.

**Emit side, checked rather than assumed.** `CacheProjection()` is called once per frame before the cell
loop and no `WorldToScreen` survives in any per-vertex path (P4's affine basis is intact). Two per-frame
costs that could be reused now are: the **ray direction table** (a sine and a cosine per vertex per frame —
~3,800 of each at 40 cells × 48 rays — now a table keyed on the ray count) and the **centre-fan index
list**, which depends only on the ray count and so is built once and shared by every cell's command instead
of ~5,700 array inserts a frame. The index array is _replaced_ rather than refilled when the ray count
changes, so a command still holding the previous one is never mutated underneath the renderer.

### D7 — Neutral bands only on **inter-faction** frontiers (user finding, 2026-08-11)

**Found by the user on the second look, after D6 landed.** Their words: _"Now the territories are butting up
against each other even if they are the same faction. Neutral zones are being drawn between areas controlled
by the same faction."_

**The defect.** `OVT_TerritoryStop.RIVAL` means _another **site** won this point_ — it says nothing about
**faction**. K7's band rule reads `RIVAL` as "a real frontier with another faction" and then never checks
the faction, so two same-faction neighbours (two towns, or a base and its own radio tower) get a neutral
band drawn down the middle of uncontested territory.

**This is a hole in the plan, not a coding slip.** §6 K7's own wording — _"the band is drawn only along
contiguous spans that stopped on `RIVAL` — a real frontier with another faction"_ — asserts a property the
data cannot express. It stayed invisible until D6 made cells large enough for same-faction sites to actually
meet.

**The fix, as applied.** The solver records **which site won** each ray in `OVT_TerritoryCell.m_aStopSite`,
parallel to `m_aStopReason`, `-1` where no rival won. The band rule itself moved into the solver as the pure
static `IsFrontierStop(stopReason, stopSite, ownFactionIndex, sites)` — deliberately, because the layer is
invisible to every automated tier and this is the rule that was wrong; the layer's `IsHostileStop` is now
only the array lookup that feeds it. `CollectRivalSpans` swaps its predicate for that call, so a
same-faction stop **breaks** a span exactly as a coast stop does and two enemy spans either side of a
friendly neighbour stay two frontiers. Everything else about K7 is untouched: no band on `COAST` or
`MAX_RADIUS`, still circular spans via P5-D's origin walk, still a convex quad per segment through the same
`m_ePrimitive` switch.

**Why the site INDEX rather than the winner's faction.** Faction is live state the layer re-reads every
refresh tick; geometry is only re-solved when `HashSites` changes, and P4-E deliberately excludes faction
from that hash so a capture never triggers a re-solve. Baking the winner's faction into the solved cell
would therefore freeze each frontier as it stood at solve time and leave it wrong until a site moved. The
index is stable under capture; the faction is read through it at draw time. For the same reason **no new
stop-reason enum value was added** — a friendly/hostile split in `OVT_TerritoryStop` would be stale by
construction, and would also have forced every existing `MAX_RADIUS`-frequency consumer to test two values.

**The solve log now splits it.** `ReportSolve`'s stop line reads `N enemy frontier (banded), N friendly
neighbour, N coast, N safety bound`. The old line's single "rival" count is what hid this defect, so the
split is the diagnostic that would have shown it.

**~~Known residual, called out before it is reported as a bug:~~ — ✅ REPORTED, AND FIXED BY D9.** The residual
predicted here (_"same-faction cells can still show a faint seam where their alpha differs… if it reads as
splotchy, the answer is to make alpha a property of the merged region rather than the individual site"_) is
exactly what the user reported next, and **D9 above is the answer**: fill alpha is now flat per faction. D9 also
found the _second_ half of the same complaint, which this note did not predict — the shrink-only smoothing was
pulling same-faction cells apart geometrically as well as tonally.

### Phase 5 findings

**P5-A — the shared colour helper returns `null`, and that is what preserves three different fallbacks.**
`OVT_MapLocationType.GetFactionColorByIndex(int)` returns the faction `Color`, or **null** when the index
is negative, the faction manager is unresolvable, or the faction is not found. Each `GetIconColor` override
then does `if (factionColor) return factionColor;` and falls through to its **own historical constant** —
Town → `Color.Black`, Base and RadioTower → `Color.White`. Those differ, so a single shared fallback would
have silently restyled markers. ⚠️ **The regression to look for is a town marker turning white** where it
used to be black.

**P5-B — there is a second `GetFactionColor()` call site and it is not a fourth copy.** The pre-existing
config-`OVT_FactionType` path (used by FOB, Camp and GunDealer) takes a faction **type**, not an index. K6's
"exactly one implementation" grep should expect **two** hits in `Scripts/Game/UI/`: the new helper and that
type-keyed function.

**P5-C — the same triplication exists for faction _icons_ and was left alone.** All three location types
still carry their own `GetUIInfo().GetIconPath()` lookup. It is not colour and not K6's scope, but it is the
obvious candidate for the same treatment later.

**P5-D — circular `RIVAL` span detection needs no wrap special-case, by construction.** `CollectRivalSpans`
first finds an origin ray whose stop reason is **not** `RIVAL`, then walks `(origin + step) % rays`. Because
the walk starts on a boundary, no run can straddle the start of the scan, so an ordinary run loop is already
circular-correct and a span covering `[rays-1, 0, 1]` comes out as one span of 3. The all-`RIVAL` case (no
such origin exists) is emitted up front as a single whole-ring span. A span of length 1 emits nothing — one
ray has no adjacent partner to form a quad.

**P5-E — shipped alphas are 30 (contested) → 95 (held), a 3.2× step.** The ceiling is a _legibility_
constraint, not a preference: 95/255 ≈ 37 % coverage is about where roads, contours and place names stop
reading through a flat fill (Q-9). If F-6's contested/held difference is still too subtle, **raise the max
before lowering the min** — and re-check terrain readability after any raise. The band uses the cell's own
faction RGB at alpha 120, not a third hue: a neutral-grey edge would stop saying _whose_ frontier it is.

**P5-F — `m_fUVScale`'s units are still unknown.** The probe rendered textures but never established
whether the three UV scales tiled _differently_. All three scale attributes (`m_fUVScaleFill`,
`m_fUVScaleBand`, and the restricted layer's) default to `0.01` **as a guess** and must be dialled in by eye
at V-6 when the art lands. The band's is expected to want a finer scale than the fill.

**P5-G — the texture guards add a null check vanilla omits.** `SCR_MapSelectionModule.RenderSelectionCircle`
would null-deref if `LoadTexture` returned null; the three `Ensure*Texture` methods here check for it, log one
`LogLevel.ERROR` naming the resource, and **null the ref again** on the invalid path so nothing downstream can
see a non-null-but-invalid ref. ⚠️ The broken-`ResourceName` path (Q-4) is **correct by inspection only** —
it has never been executed.

### D6 — Territory has **no maximum influence radius** (user decision, 2026-08-11)

**Decided by the user after seeing the first render.** Their words: _"there are parts of the map not
filled, likely because the sites have a maximum distance. But they shouldn't have a max distance, the
territory should extend until it reaches coast or a competing site."_

**Overrides:**

- `requirements.md`: _"each cell clipped to a **maximum influence radius**"_ and _"Cell extent is
  distance-based with a uniform influence radius"_.
- `implementation.md` §4 D1's `m_fMaxRadius` / `m_fRadiusPerWeight` reach derivation, and the
  `MAX_RADIUS` stop reason as a _routine_ outcome.

**What it means.** A ray stops on exactly two conditions: a rival wins the point under the weighted test,
or the ray leaves land. The result is a genuine partition of the island rather than islands of colour with
dead ground between them — which is what the first render actually looked like, and it read as broken.
Weighting still does its job: the boundary between a base and a town sits closer to the town, it just no
longer stops in empty space.

**`MAX_RADIUS` survives as a safety bound only** — rays must terminate on a world with no ocean and no
rival, so the bound becomes the world diagonal rather than a per-site derived reach. The stop reason stays
in the enum and stays untested-for in the band logic; it should now be **rare in practice and is a
diagnostic signal if it is common**.

**Consequences, both real:**

1. **Neutral bands get strictly better.** K7 draws a band only on spans that stopped on `RIVAL`. With the
   reach limit gone, every remaining non-coast boundary _is_ a frontier, so the band means what it says.
2. **It costs solve time, and K4's cost model no longer holds.** K4 assumed "average marched distance is
   far below `maxRadius`" and priced the solve at ~150 k comparisons on that basis. Rays pointing out to
   sea now march to the coastline instead of stopping at the reach limit. The candidate-rival filter, which
   K4 bounded at `2 × maxRadius`, no longer bounds anything. **Phase 6 must re-measure rather than reuse
   the plan's arithmetic.**

### Phase 4 findings

**P4-A — 🔴 "rung 1" is enum value `0`.** K3's ladder counts from 1 but `OVT_TerritoryPrimitive` counts
from 0: `TRIMESH_FAN = 0` (the measured, shipped default), `POLYGON = 1`, `TRIANGLE_LIST = 2`. **Anyone
reading "default rung 1" and writing `m_ePrimitive 1` silently ships the polygon rung** — the one whose
correctness the probe never established. The attribute default is `"0"` and it is correct.

**P4-B — the O(sites) recolour requires cells index-aligned with sites, and `Solve` does not guarantee
it.** The solver skips any site with non-positive weight or radius, so one bad site would shift every
later cell's colour onto the wrong site — a silent, plausible-looking wrongness. Fixed at the source:
`AddSite` refuses to create a site the solver would reject, so the counts always match, and
`RecolourCells` additionally bails on a count mismatch rather than painting from mismatched indices.

**P4-C — `Color.PackToInt()` is the wrong way to get the faction colour here.** It bakes the faction's own
alpha, which would defeat the contested-vs-held fade entirely. The single colour site unpacks `R()/G()/B()`
and composes `ARGB(ourAlpha, r, g, b)`. That is the one line Phase 5's shared helper (K6) replaces.

**P4-D — ~~`town.size` is legitimately 0 on a client~~ — ⚠️ CORRECTED by Phase 7's audit, see P7-A above. `size` is `[NonSerialized()]` but is _rediscovered locally on every machine_ from world-static town controllers, so it reads correctly on a client and both clients weight towns identically. The defensive handling described below is kept anyway, and is what covers the JIP window before the client's world query has found the controllers. Original text:** It is `[NonSerialized()]` and populated locally from
the town controller, so `ResolveTownSizeFactor` returns `SIZE_FACTOR_UNKNOWN = 1.0` for a null town, 0, or
anything out of range. A zero factor would zero the site weight, collapse the cell **and** divide by zero
in the ownership test. Weight is folded into `HashSites` partly for this reason: a town whose size becomes
readable later changes its weight, which changes the hash, which re-solves the geometry the size affects.

**P4-E — `HashSites` deliberately excludes faction and hold strength.** It is `17`, then `×31 +` the site
count, then per site `×31 +` floor(x), floor(z), floor(weight × 100) and `m_sTypeId.Hash()`. A capture
therefore recolours in O(sites) and never triggers a full re-solve — which is the whole point of K9's split.

### Phase 3 findings

**P3-A — `SolveRay` cannot be purely static as the plan specifies.** The coast test _is_ the virtual
`IsLandAt` hook, and a static function has no way to reach it. The signature takes the solver as its first
parameter — still static, with the receiver handed in — and a `null` solver means "everything is land",
the same contract as `SetWorld(null)`. This is what keeps the ray-march reachable from the Logic tier.

**P3-B — §9's case-9 inversion is stated backwards in the plan.** "Return early from `SmoothRadii`
regardless of `passes`" makes case 9 _green_, because case 9 asserts the array is unchanged at
`passes = 0`. The mutation that actually turns it red is the opposite: **smooth anyway despite
`passes = 0`.** That is the one that was run.

**P3-C — `SCR_AutotestCaseBase.SetResultFailure` takes `string` parameters only, at most three.** No
numeric overloads, no implicit conversion — every value must be `.ToString()`d, and a four-value failure
message has to be composed down to two strings. Worth knowing before writing any new case file in any tier.

**P3-D — the smoothing clamp is applied per pass, not once at the end**, so K5's invariant
`smoothed[i] ≤ raw[i]` holds after _every_ pass rather than only the last. An end-only clamp would let an
intermediate pass push a vertex past the coast and then clamp the wrong value back.

**P3-E — a pre-existing `maxAttempts` mention survives the DoD Q-5 grep.**
`Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GroupRecruits.c` contains the word in **prose**
("No retries, no maxAttempts anywhere"). It is not an attribute and not this feature's file, so it was
reported rather than edited — but Q-5's grep will hit it. Whoever runs that gate should expect one
known-benign hit.

### Phase 1 findings

**P1-A — `SetDrawCommands` takes a pointer, not a copy.** The engine header states the caller must keep
the array alive; the callee holds a pointer. Two consequences: the compositor's shared list is owned by
the static singleton and is **cleared and refilled in place, never reassigned or nulled**; and **K2's
second-defect claim is partially contradicted** — if the engine renders through a live pointer to the
layer's own array, a layer that cleared its bucket would already have rendered nothing without any
`SetDrawCommands` call, so the `Count() > 0` guard was probably wrong-but-inert rather than actively
freezing frames. The guard is deleted and the flush is unconditional (correct either way). **Settle it by
observation in Phase 2 / I-2, not by reasoning.**

**P1-B — `array.Remove(int)` is a swap-remove** ("the empty position is replaced by the last element…
does not retain order"). Using it in `Unregister` would silently scramble the sorted draw order the first
time a layer left mid-session. `RemoveOrdered` is used instead — this is load-bearing, not style.

**P1-C — `array<T>.InsertAll` will not accept an array of strong refs** (`array<@CanvasWidgetCommand>` vs
`array<CanvasWidgetCommand>` is a hard type error). The bucket concatenation is a manual `foreach`/`Insert`
loop and is commented so nobody "tidies" it back.

**P1-D — `WorldToScreen` is provably affine, and world-Z → screen-Y is _negatively_ signed**
(`worldY = m_iMapSizeY - worldY` before scaling). The basis is still derived **empirically** and the sign
is **not** encoded anywhere. Prediction for probe P4: max error ≤ 1 px, sourced purely from
`WorldToScreen`'s integer truncation. ⚠️ That is a prediction to falsify, **not** a reason to skip P4 —
a wrong sign produces a plausible-looking mirrored cell, which is the failure R4 exists for.

**P1-E — "client-only" is by construction, not by a guard.** No honest server check exists:
`Replication.IsServer()` is true on a listen host that _does_ have a map, and `BaseWorld` exists on a
headless server. The compositor's only callers are canvas layers, which only `Update` where a map exists.
What is guarded instead is every way it can actually fail — null layer, null canvas, null bucket, and a
null world in `GetFrameToken` (which degrades to "composite everything", never "composite nothing").

**P1-F — `SubmitAndFlush` adopts an unregistered layer.** A subclass that overrides `OnMapOpen` without
calling `super` would otherwise have its commands dropped every frame with no symptom but "my layer
doesn't draw".

**P1-G — `SCR_MapModuleBase.SetActive(bool active, bool isCleanup = false)`.** Unregister happens in the
`!active` branch **before** `super.SetActive(...)`, because super's `DeactivateModule` is what stops the
layer ever submitting again. There is no other `SetActive` override in the vanilla tree, so
`compile-check` is the only confirmation the redeclared default arg is legal — it is.

---

## Probe results (Phase 2)

### How to run the probe

**The probe is built and registered but has never been executed.** Everything below the instructions is
blank on purpose — nothing about the engine's behaviour is known until a human runs this in Workbench.

**What exists:**

- `Scripts/Game/UI/Map/Visualization/OVT_MapProbeLayer.c` — throwaway, **deleted in Phase 8**.
- `Configs/Map/MapOverthrow.conf` — an `OVT_MapProbeLayer` entry, GUID `{6A84B1C0D2E3F405}`,
  `m_iDrawOrder 900` (above everything), `m_sLayerId "probe"`, `m_iProbe 0`. **This entry and the class are
  both deleted in Phase 8.** The `OVT_MapThreatGrid` entry is untouched and stays `m_bDisableModule 1` (D3).

**Knobs, all on the conf entry — no code edit needed to isolate a result:**

| Attribute           | Default                         | What it does                                                                                                                                                                                          |
| ------------------- | ------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `m_iProbe`          | `0`                             | `0` = draw all four. `1`/`2`/`3`/`4` = draw only that probe. Set this the moment anything is ambiguous.                                                                                               |
| `m_sProbeTexture`   | `overthrow_mapicons_atlas.edds` | P1's texture. A shipped sprite atlas, chosen because a _recognisable_ image makes "the texture rendered" unmistakable where a subtle hatch could be missed. Repoint at the real hatch when it exists. |
| `m_fUVScale`        | `0.01`                          | P1's base UV scale. P1 draws hexagons at `0.1×`, `1×` and `10×` this, because `m_fUVScale`'s units are undocumented and unused anywhere in vanilla.                                                   |
| `m_bReverseWinding` | `0`                             | P3 only. Flip the triangle index winding. Use **once** if P3 renders partially/garbled, then abandon.                                                                                                 |
| `m_fPrintInterval`  | `1.0`                           | Seconds between P4 log lines.                                                                                                                                                                         |

**Steps, in order:**

1. Start (or continue) a campaign and **spawn in** — the probe hangs its shapes off the local player's
   position. With no controlled entity it falls back to the map centre and says so in the log.
2. Open the **fullscreen map**. At map open the probe prints one block of world coordinates, one line per
   probe, all prefixed `[OVT_Probe P1]` … `[OVT_Probe P4]`. **Grep the log for `[OVT_Probe`** — that block
   is the map for where to pan.
3. The shapes sit **2600 m** out from the player in three directions, so zoom out first:
   - **Up-left** — P1's row of four hexagons plus a star below them.
   - **Up-right** — P2's green star.
   - **Down-left** — P3's orange star.
   - **Scattered around the player** — P4's yellow crosses and magenta circles.
4. Record each result in the table below, **verbatim**, including the observed signature. If a probe is
   confusing, set `m_iProbe` to that probe's number and look at it alone before writing anything down.

**What each result looks like:**

**P1 — textured polygon** (up-left). Four hexagons in a row, then a star below.

- **PASS:** the three left hexagons show the atlas image, and the tiling **visibly differs** between them —
  the left one (0.1×) coarser or finer than the right one (10×). The rightmost hexagon is a **flat blue
  control** with no image; that is the comparison, so you are not being asked to remember what a flat fill
  looks like.
- **FAIL F1a — texture ignored:** all three textured hexagons render as flat **white**, identical to each
  other, and only the control differs (by colour). ⇒ hatching is off the table; D4 falls back to flat-fill
  differentiation.
- **FAIL F1b — command dropped:** the three textured hexagons render **nothing at all** and only the blue
  control appears. ⇒ same fallback, and `m_pTexture` must never be set.
- ⚠️ **Before recording F1a, check the log.** If it says `LoadTexture returned null` / `INVALID
SharedItemRef` / `No CanvasWidget`, the probe drew untextured on purpose and **the P1 result is void** —
  fix the `ResourceName` and re-run.
- ⚠️ If all three textured hexagons are one flat colour that is _not_ white and _not_ the control's blue,
  the texture is probably being stretched so one texel covers the shape. Try `m_fUVScale` `1.0`, then
  `0.0001`, before concluding F1a.
- The textured **star** below the row is P1 and P2 interacting. Read P2 first; if P2 fails, the star's
  shape is expected to be wrong and only its _texture_ is evidence.

**P2 — non-convex fill** (up-right). One green star, 12 vertices, radii alternating 500 m / 200 m, as a
**single** `PolygonDrawCommand`.

- **PASS:** a clean star with six sharp notches. ⇒ rung 2 available.
- **FAIL — the signature to recognise, not debug:** the notches are filled in and the shape reads as a
  **lopsided pinwheel hinging on one vertex**, with the convex hull correct. That is the engine fanning
  naively from vertex 0. **It is not a bug in the vertex order** — do not go looking for one. ⇒ rung 2
  unavailable.

**P3 — `TriMeshDrawCommand`** (down-left). The same star as an explicit centre fan.

- **PASS:** a clean orange star, identical in shape to P2's PASS. ⇒ rung 1 available, and it is the rung to
  choose (1 command per cell, correctness guaranteed by construction).
- **FAIL — nothing renders:** the type is declared but unimplemented. ⇒ rung 1 unavailable.
- **Partial / garbled:** the index convention differs. Set `m_bReverseWinding 1`, re-run **once**. If it is
  still wrong, abandon `TriMeshDrawCommand` and record it.

**P4 — affine projection** (around the player). Eight sample points, each with a **yellow cross** at
`ProjectWorld` and a **magenta circle** at the direct `WorldToScreen`.

- Run this at **3 zoom levels × 2 pan positions** — six readings. The log line prints the current zoom, so
  each reading is self-labelling. It prints once per second, not per frame.
- **PASS:** every cross sits centred inside its circle, and `maxError` ≤ **2 px** at every reading. Phase 1
  finding P1-D predicts ≤ 1 px from `WorldToScreen`'s integer truncation alone — **that is a prediction to
  falsify, so record the number, not the prediction.**
- **FAIL — mirrored:** crosses appear on the **far side of the map** from their circles, roughly reflected.
  This is the world-Z → screen-Y sign, and it is the failure this probe exists for (R4) — a mirrored cell
  is a plausible-looking wrong answer, not an obvious bug.
- **FAIL — drift:** crosses sit just outside their circles, worse the further from the basis origin, and
  `maxError` grows with zoom. ⇒ `WorldToScreen` is not affine within the precision needed; fall back to a
  per-vertex `WorldToScreen` and re-measure Phase 6's frame cost with ~1,900 calls instead of 3.
- Log line format: `[OVT_Probe P4] zoom=<z> maxError=<n> px PASS/FAIL | p0=..px p1=..px ...`

**One more observation to write down while the map is open (it settles Phase 1's I-2 and finding P1-A):**
the probe is a **third** live canvas layer at `m_iDrawOrder 900`. Confirm the FOB **restriction rings still
render** at the same time as the probe shapes, with the probe on top. If the rings vanish, the compositor is
not composing and that is a Phase 1 defect, not a probe result.

### Results

**Run by the user in Workbench on 2026-08-11.** P4's numbers are quoted verbatim from the log; P1–P3 are
the user's visual report ("Seems to be working… I saw all of them including the textured ones"), which is
recorded at exactly that strength and no stronger.

| Probe                       | Result                                                | Observed signature                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| --------------------------- | ----------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **P1** textured polygon     | ✅ **PASS**                                           | `[OVT_Probe P1] Texture loaded and valid: '{B2EDB8AD2EA10AD3}UI/Textures/Map/overthrow_mapicons_atlas.edds'. Base m_fUVScale=0.01`. The textured hexagons rendered — the user confirmed seeing them, _including the textured ones_. **`PolygonDrawCommand.m_pTexture` + `m_fUVScale` work in this engine build**, so D4's hatch plan is viable and the flat-fill fallback is not needed. ⚠️ **Not measured:** whether the three UV scales (0.1× / 1× / 10×) tiled _differently_ — so the units of `m_fUVScale` are still unknown and must be dialled in by eye when the art lands (V-6) |
| **P2** non-convex fill      | ⚠️ **RENDERED — clean-vs-pinwheel not distinguished** | The green star drew. The user did not report on the decisive detail (sharp notches vs a lopsided pinwheel with the notches filled in), and **it no longer matters**: rung 1 is available, so nothing in the shipped feature depends on the engine triangulating a non-convex polygon. Recorded as unknown rather than assumed PASS                                                                                                                                                                                                                                                      |
| **P3** `TriMeshDrawCommand` | ✅ **PASS**                                           | The orange star rendered from an explicit centre fan with `m_bReverseWinding 0` — no winding retry was needed. **The type is implemented, and this is the rung.** `TriMeshDrawCommand` also carries `m_fUVScale`/`m_pTexture`, so rung 1 supports textured fill too                                                                                                                                                                                                                                                                                                                     |
| **P4** affine projection    | ✅ **PASS, decisively**                               | `maxError` **0 – 1.41421 px** across zoom `0.1125`, `0.11858`, `0.180339` and `0.221632`, at both pan positions. Every per-point error was 0, 1 or 1.41421 px — i.e. **exactly 0, 1 or √2 px**, which is a one-pixel diagonal and nothing else. This confirms finding P1-D's prediction: the error is `WorldToScreen`'s integer truncation and **contains no basis error at all**. No mirroring, no drift with zoom                                                                                                                                                                     |

**What P4 buys.** The 3-call affine basis is exact, so Phase 4 projects ~1,900 cell vertices per frame with
**three** `WorldToScreen` calls instead of ~1,900. R3's per-frame projection cost is retired as a risk, and
R4 (the mirrored overlay) is retired outright — the sign was derived empirically and measured correct.

**P4 readings — 3 zoom levels × 2 pan positions:**

| Reading | Zoom (from the log line) | Pan position   | `maxError` px | Cross inside circle? |
| ------- | ------------------------ | -------------- | ------------- | -------------------- |
| 1       | _pending_                | player centred | _pending_     | _pending_            |
| 2       | _pending_                | panned far off | _pending_     | _pending_            |
| 3       | _pending_                | player centred | _pending_     | _pending_            |
| 4       | _pending_                | panned far off | _pending_     | _pending_            |
| 5       | _pending_                | player centred | _pending_     | _pending_            |
| 6       | _pending_                | panned far off | _pending_     | _pending_            |

**Restriction rings still render alongside the probe (Phase 1 I-2):** _pending_ — ⚠️ **and now unobtainable
as stated.** The probe layer was deleted on 2026-08-11, so this exact observation (three live layers, probe
on top) can never be made again. Its purpose — proving the compositor really composes rather than letting
the last layer win — is now carried by **I-1b/I-2** with territory itself as the second layer: territory and
the rings must render **at the same time**, rings on top. That is the check to run instead.

⚠️ **The six-row table above was never filled in.** The user reported the aggregate — `maxError` 0 – 1.41421
px across zooms `0.1125`, `0.11858`, `0.180339` and `0.221632` at both pan positions — and that aggregate is
recorded verbatim in the Results table. The per-reading breakdown is simply not written down anywhere, so
the rows stay `_pending_` rather than being back-filled from the summary.

**Chosen rung: 1 — `TriMeshDrawCommand` with an explicit centre fan.** P3 passed, and rung 1 is the top of
K3's ladder: **one draw command per cell**, and correctness **guaranteed by construction** because the
indices are supplied rather than inferred — the engine never triangulates anything, so P2's unresolved
clean-vs-pinwheel question cannot affect the shipped feature. At ~40 sites this is ~40 fill commands plus
the band strips, comfortably inside Q-1's ≤ 250/frame budget, and it removes the `m_iRayCount` 48 → 16 cut
rung 3 would have forced (§6 K3's "visibly polygonal cells" cost is not paid).

Rungs 2 and 3 are still implemented behind `m_ePrimitive` — the cell geometry is identical on all three, so
switching is a config edit, not a code change. Rung 3 remains the escape hatch that needs no engine feature
at all.

---

## Measured numbers (Phase 6)

⚠️ **Every cell in this table is still empty and must stay empty until a human reads the log.** The plan's
own bar: _"an unrecorded number is a failure of this criterion even if the overlay feels fine."_ The code
that produces these numbers is written; none of it has ever run.

| Metric                                    | Budget   | Measured  |
| ----------------------------------------- | -------- | --------- |
| Site count on the populated save          | —        | _pending_ |
| `CollectSites` at map open                | —        | _pending_ |
| **Grid classify** at map open             | ≤ 250 ms | _pending_ |
| **Presentation build** (per refresh tick) | —        | _pending_ |
| Rolling 60-frame `Draw()` average         | ≤ 1.5 ms | _pending_ |
| Total composited commands/frame           | ≤ 250    | _pending_ |

> 🔴 **D12 CHANGED THE LOG LINES.** The block below is the CURRENT format. The old five-line block —
> `solve:` / `stops:` — described the ray-march and will never print again.

### How to produce them — one map open, five log lines

1. On the layer entry in `Configs/Map/MapOverthrow.conf`, set **`m_bDebugTiming 1`**. (It is not in the
   conf today, so add the line; every other tunable is at its class default.)
2. Load the **fully-populated** save — all towns, all bases, all radio towers, **≥ 3 FOBs**. An early-game
   world makes the measurement meaningless.
3. **Open the fullscreen map and leave it open for ~10 seconds**, then close it. The draw line only prints
   once every 60 frames and the recolour line only once per refresh interval, so a map that is opened and
   shut immediately produces the two grid lines and nothing else.
4. `grep` the log for **`[OVT_Territory]`** and paste back the block. It looks like this:

```
[OVT_Territory] grid: <N> sites, <N> x <N> squares at <N> m (<N> owned, <N> water/unowned), collect <N> ms, classify <N> ms
[OVT_Territory] draw set: <N> appearances, <N> runs -> <N> rects, <N> contours (<N> points, <N> frontier segments), build <N> ms
[OVT_Territory] tunables: <N> m squares, <N> smooth passes, contours <0/1>, vertical merge <0/1>, batching <0/1>, band <N> m / alpha <N>, fill alpha <N>, occupier only <0/1>
[OVT_Territory] textures: contested <0/1> at uv <N>, band <0/1> at uv <N>, shading <0/1>, contested alpha <N>, support threshold <N>
[OVT_Territory] recolour: <N> ms, <N> runs -> <N> rects, <N> contours
[OVT_Territory] draw: <N> ms average over 60 frames (<N> ms total), <N> rects, <N> own commands, <N> composited last frame (rings included)
```

5. Then set **`m_bDebugTiming 0`** again. The first two lines keep printing either way — deliberately: two
   lines at map open are how anyone tells an overlay that found no sites from one that found them and drew
   nothing.

**What each line settles.** The **grid** line is the ≤ 250 ms budget and splits collect from classify, so a
slow manager walk cannot masquerade as slow geometry. Its `<N> m` term is the D12 valve: **a square size
larger than the configured one means the square-count ceiling grew it** and the overlay is coarser than
asked for. Its owned/water split is the D6 and D2 diagnostic at once — nearly everything unowned is a land
test rejecting land, nearly nothing unowned is a land test that never fired. The **draw set** line is D10's
diagnostic (appearances far below sites is a mostly-liberated island; the **site count itself** collapsing is
a collect-time filter, which must never happen) and it is also where the vertical merge shows its worth,
`runs -> rects`. The **recolour** line is D12's own cost — the per-tick `O(cells)` rebuild — and is the number
that says whether K9's split is still paying for itself. Lines 3 and 4 say what the others were measured at.
The **draw** line is the ≤ 1.5 ms emit budget and the ≤ 250 command
budget, and the command count is the **compositor's**, so it includes the FOB restriction rings, which is
what the budget is written against. ⚠️ **The emit numbers are now a function of how much of the island is still
occupied** — a late-game save draws far fewer cells than an early one, so record the drawn count alongside
them or the measurement cannot be compared with anybody else's.

⚠️ **The draw average is quantised.** `System.GetTickCount` is whole milliseconds, so a sub-millisecond
`Draw()` reads 0 or 1 on any single frame; the 60-frame sum is what makes it meaningful, and the average is
that sum divided by 60. A reported `0 ms` means "well under one millisecond per frame", not "free".

> 🔴 **THE D8 TUNABLE TABLE IS DELETED, NOT SUPERSEDED.** `m_fMarchStep`, `m_iRefineSteps` and
> `m_iRayCount` no longer exist — there is no march and there are no rays. The knobs that decide geometry
> now are `m_fGridCellSize` and `m_iSmoothPasses`.

**Tunables as they ship after D12. Only `m_sBandTexture` is in `MapOverthrow.conf` (set by the user, and
now redundant because it is also the attribute default); everything else is a class default, so changing
one is a conf line rather than a code edit.**

| Knob                                          | Ships at                      | What it does                                                                                                                                                                                                     |
| --------------------------------------------- | ----------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `m_fGridCellSize`                             | **100 m**                     | Square size. Decides both resolution and cost, and **cost is QUADRATIC**. Also decides how blocky the fill edge is. A `MAX_GRID_CELLS` ceiling grows it rather than allocate an unbounded grid.                  |
| `m_iSmoothPasses`                             | **2**                         | Chaikin corner-cutting passes on the traced border. **Each pass DOUBLES the border segment count.** 0 leaves the raw marching-squares staircase.                                                                 |
| `m_bTraceContours`                            | **1**                         | 🔴 **The Option-A fallback.** 0 draws fills only — no contours, no bands, no Chaikin. A clean configuration, not a degraded one.                                                                                 |
| `m_bMergeRectsVertically`                     | **1**                         | Pure optimisation on the fill quad count. Changes no pixel; asserted not to.                                                                                                                                     |
| `m_bBatchDrawCommands`                        | **1**                         | Pack quads into one `TriMeshDrawCommand` per colour. **This is what keeps the frame inside the command budget.** 0 falls back to one convex `PolygonDrawCommand` per quad — same geometry, hundreds of commands. |
| `m_bOnlyShowOccupying`                        | **1**                         | Draw occupier-held ground only. 0 draws every faction and is the one-value escape hatch if the reframing does not read.                                                                                          |
| `m_iFillAlpha`                                | **70**                        | Flat fill for a held region. Raise before lowering (P5-E); 95 is the legibility ceiling.                                                                                                                         |
| `m_bContestedShading`                         | **1**                         | The **one** switch for the contested mark.                                                                                                                                                                       |
| `m_iContestedAlpha`                           | **95**                        | Fill for a contested region. Above the held value on purpose — a hatch lays down less ink than a solid fill (D11).                                                                                               |
| `m_fContestedSupportThreshold`                | **0.5**                       | Half the town. Compared against the same figure the info panel prints.                                                                                                                                           |
| `m_fBandWidth`                                | **80 m**                      | Neutral band width in **metres**, replacing D7/K7's `m_fBandFraction`. Kept below the square size because the band offsets inward and folds through itself at corners tighter than its own width.                |
| `m_iBandAlpha`                                | **120**                       | On every visible edge, which makes it **the first thing to turn down** if the map reads loud.                                                                                                                    |
| `m_sBandTexture` / `m_fUVScaleBand`           | **the diagonal hatch / 0.02** | The band is hatched too, separated from the contested fill by **scale** rather than hue.                                                                                                                         |
| `m_sContestedTexture` / `m_fUVScaleContested` | **the diagonal hatch / 0.01** | D11, unchanged.                                                                                                                                                                                                  |

---

## Still unverified

_The running list of everything that needs a human. Nothing is ticked here on reasoning alone._

- **Q-1's numbers.** Nothing has ever been timed. D12's arithmetic says the grid is CHEAPER than the
  march it replaced, which says the answers are _right-shaped_, not that they are _fast enough_.
- **Whether the grid classify lands under 250 ms** at 100 m squares on a real world. The estimate is
  ~29 k terrain samples and ~576 k weighted comparisons — but an estimate is what D6's 0.5–2.5 s was too.
- **🔴 Whether 100 m squares read as acceptably blocky at map zoom.** The fill edge steps on square
  boundaries by construction now, roughly 11 px at typical zoom, under a smooth traced border. The lever
  is `m_fGridCellSize` and it costs **quadratically**.
- **Whether the neutral band folds at tight corners.** It offsets inward, so a width larger than the
  border's local radius of curvature crosses itself and composites darker. `m_fBandWidth` ships at 80 m
  below the 100 m square size for that reason; it has never been looked at.
- **Whether a batched `TriMeshDrawCommand` renders many disjoint quads correctly**, and whether 256 quads
  per command is inside whatever the engine's real vertex limit is. Nothing in this build has measured
  either. A failure would be _all_ of one colour drawing wrong together, not one region — and
  `m_bBatchDrawCommands 0` is the one-value diagnostic.
- **The `[OVT_Territory] draw:` line has never printed.** It is gated behind `m_bDebugTiming`, which has
  never been switched on.
- **Everything multiplayer (I-5, I-6, I-7).** Phase 7's audit establishes that the _data_ is on the client
  and that the re-solve trigger is correct; **no second client has ever opened this map.** Two clients
  agreeing on geometry, a JIP client healing within one refresh interval, and a capture recolouring on
  both clients are all unobserved.
- **The whole current build, visually.** The overlay has been rendered three times and **every render predates
  D8, D9 and D10.** No human has seen a D8-solved boundary, a flat-alpha fill, two same-faction cells
  tessellating, an unsmoothed coastline, or **a map with only the occupier's ground on it**. **Whether the
  splotchiness is gone is still the open question**, and this is the third attempt at it — see the shortlist
  at the end of D10 for what is expected to survive, the neutral band and the FOB holes especially.
- **Whether the occupier-only map reads as PROGRESS or as ABSENCE.** The reframing is a bet: that a shrinking
  stain reads better than a two-colour partition. Nobody has looked at it. The escape hatch is
  `m_bOnlyShowOccupying 0`, one config value, and it restores the D9 behaviour exactly.
- **Whether contested at alpha 35 against a held 70 reads apart at map zoom.** Reasoned from P5-E's
  legibility range, not measured. Raise `m_iContestedAlpha` toward 70 to quieten it or drop it toward 20 to
  sharpen it, and re-check terrain readability either way.
- **Whether the flat alpha of 70 is the right value.** It is reasoned from P5-E's legibility ceiling, not
  measured against a screenshot. Raise it before lowering it, and re-check terrain readability after any raise.
- **The D3 threat-grid experiment** (Phase 1's three user-driven rows): whether the grid and the rings
  really composite together with the rings on top, and whether ~2,300 quads visibly cost frame time.
  That single observation is what decides revive-or-delete on the epic's threat-grid debt item.
- **`SetLayerVisible(false)` has never been called.** The toggle primitive `map/map-layers` is being told
  to build on is correct by inspection and unexecuted.

---

## Where to look when it doesn't work

| Symptom                                                                                    | Most likely cause                                                                                                                                  | First check                                                                                                                                                                                                                                                                                                                              |
| ------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Territory renders, rings vanish** (or the reverse)                                       | The compositor is not composing — a layer is calling `SetDrawCommands` with its own list, or a bucket's frame stamp is never current               | `Print` the composited command count per frame; if it equals one layer's count, the flush path is wrong                                                                                                                                                                                                                                  |
| **Regions the wrong shape, no error**                                                      | The grid is being sampled somewhere other than square centres, or the ownership predicate lost its weighting                                       | Run `OVT_TEST_Logic_Territory_OwnsWeighted` and `_OwnsEqualWeights` alone — between them they pin the predicate AND the column where the grid's owner changes hands                                                                                                                                                                      |
| **A region crosses a strait or a bay**                                                     | A 100 m square straddled the water, so its CENTRE sampled as land                                                                                  | Lower `m_fGridCellSize` — and note the cost is **quadratic**. This is D12's stated residual and the direct replacement for D8's march-step risk                                                                                                                                                                                          |
| **Overlay is empty, no error**                                                             | `CollectSites` found nothing — a manager was null at map open (JIP window), or a site-type config entry is `m_bEnabled 0`                          | `Print` the site count per source. Zero on a client but non-zero on the host is a **replication** finding against the owning feature, not this one                                                                                                                                                                                       |
| **Everything is mirrored or inside out**                                                   | The world-Z → screen-Y sign in `CacheProjection` (probe P4)                                                                                        | P4 measured 0–1.41 px error with no mirroring, so a mirror now means the basis derivation changed. There is no longer a second candidate: the fan-from-vertex-0 artefact cannot happen, because every quad is convex and its triangles are supplied                                                                                      |
| **Two clients see DIFFERENT territory** (different boundaries, not just different colours) | A site's **weight** differs between the machines — and there is only one weight input that is not replicated: a town's `size`                      | Compare the `[OVT_Territory] grid:` site counts and square sizes first (a different _count_ is a missing site, not a weight problem). If the counts match, print each town's `size` on both machines. P7-A says it should be identical because `size` is world-static, so a difference is a **`towns/core`** finding, not this feature's |
| **Two clients see the same shapes in different colours**                                   | Purely a faction-state disagreement — geometry is fine                                                                                             | The colour is re-read from the managers every refresh tick, so this is a replication finding against whichever manager owns that site type, **not** a territory bug                                                                                                                                                                      |
| **The overlay reads as mottled / splotchy**                                                | Work D12 §9's shortlist in order: the blocky fill edge, the band folding at corners, the band's loudness, FOB/tower holes, the two contested tones | Check the `draw set:` line's frontier-segment count. A large one is the band suspect — drop `m_iBandAlpha` to ~90 before touching anything else. **The seams and the coastal fringe are gone by construction**, so do not start there                                                                                                    |
| **The whole map is blank**                                                                 | Either the island is genuinely liberated (working as designed), or something filtered at collect time, or the grid has no extent                   | The `grid:` line first — 0 x 0 squares means `GetBoundBox` gave nothing. Then the `draw set:` line's appearance count: sites high and appearances 0 is a liberated island; the **site count itself** collapsing is a collect-time filter, which is the one thing that must never happen                                                  |
| **Occupier colour covers ground the player has liberated**                                 | 🔴 A site was dropped before the GRID was classified rather than before the draw                                                                   | Run `OVT_TEST_Logic_Territory_OccupierOnlyEmit` alone — it asserts the liberated site owns its half of the grid AND that no drawn rectangle crosses the boundary                                                                                                                                                                         |
| **Blank holes inside occupier territory**                                                  | **Working as designed.** A FOB or a captured radio tower is liberated ground and draws nothing                                                     | Only act if it reads badly. Lower that type's `m_fWeight`, or set its `m_bEnabled 0` so it is not a site at all — **never** filter it out of `CollectSites`                                                                                                                                                                              |
| **A ragged pale fringe along the coast**                                                   | 🔴 **Should be IMPOSSIBLE after D12** — nothing erodes a boundary any more.                                                                        | If it appears it is a NEW defect. Look at `m_fShorelineMargin`, then at the grid line's owned/water split: a fringe means square centres near the shore are testing as water                                                                                                                                                             |
| **A hairline dark seam between two same-colour regions**                                   | 🔴 **Should be IMPOSSIBLE after D12** — squares tile and each has one owner.                                                                       | Run `OVT_TEST_Logic_Territory_FillTiling` alone; it asserts every drawn square is covered exactly once. If it is green, look for TWO LAYERS drawing (the compositor), not for a geometry gap                                                                                                                                             |
| **The overlay is much coarser than configured**                                            | The `MAX_GRID_CELLS` ceiling grew the square size                                                                                                  | The `grid:` line prints the size it ACTUALLY built at. A value above `m_fGridCellSize` means the ceiling fired, which means the configured size was too small for this world                                                                                                                                                             |
| **One whole colour of region draws wrong, or not at all**                                  | A batched `TriMeshDrawCommand` exceeded whatever the engine's real vertex limit is                                                                 | Set `m_bBatchDrawCommands 0`. If it comes right, lower `MAX_QUADS_PER_COMMAND`; the failure is per-BATCH, so it takes out one colour at a time, never one region                                                                                                                                                                         |
| **A capture takes several seconds to recolour**                                            | **Working as designed.** Nothing subscribes to the capture RPCs; `RefreshTick` polls every `m_fRefreshInterval` (5 s)                              | Only investigate if it exceeds the interval. If it never updates at all, check the map is actually open — the tick only runs while it is                                                                                                                                                                                                 |
| **A JIP client's overlay is empty or partial on first open**                               | The managers had not replicated when `CollectSites` ran                                                                                            | **Leave the map open.** Within one refresh interval the hash changes and the grid is re-classified (K9's JIP fix). If it stays empty for several intervals, print the per-source site count — zero on the client and non-zero on the host is a replication finding against the owning feature                                            |

---

## Session Notes

### 2026-08-11 — D12 (the representation replaced)

**The core geometry was replaced at the user's direction after the ray-march failed visual inspection a
third time** — straight radial lines and pale wedges. Territory is now an **ownership grid**: one owner
per world square, fill merged into rectangles, border traced by marching squares and rounded with
Chaikin. `OwnsPointXZ` and `IsLandAt` are unchanged and still pinned; **everything about rays, march
steps, the D8 closed form and the D9/D10 smoothing exemptions is deleted, together with seven test
cases**, because a tested-but-unreachable function is worse than none. D6, D7, D9's intent, D10 and D11
all survive — D12 §4 says how each one is now expressed. Test count 16 → 17 for the feature, **every new
or reworked case proven able to fail** with the inversion named in D12 §7; two cases had to be
strengthened before they earned their place (`_ContourTopology` accepted a broken outer chain,
`_FrontierSpans` was lucky on its first input) and one redundant production guard was **deleted** so the
"smoothing tunable to off" contract could be turned red at all. ⚠️ **A false premise about
`TriMeshDrawCommand` ignoring textures was corrected mid-task and is recorded in D12 §6 rather than
silently dropped — TriMesh textures are confirmed WORKING, observed by the user.** Gates: compile
**exit 0 / 5971 files** (one file deleted, one added), Fast **71**, All **106**. **Nothing rendered.**

### 2026-08-11 — Phases 7 & 8 (audit + docs)

Phase 7's code half run as a **code reading** — the two-client gate is user-driven and was deliberately not
launched. All four collector sources verified client-side, `HashSites` verified against K9's split, the camp
exclusion re-verified, I-4's greps re-run over the diff **and** the untracked files. Findings P7-A…P7-E
recorded above; **P7-A corrects P4-D**. Phase 8 wrote the canvas-layer contract into `map/core`'s context,
answered `map-layers`' open scope question, and updated the epic overview (feature-6 row, rollup, and D3's
threat grid accepted as Tech Debt dated 2026-08-11). Gates unchanged: compile **0 / 5971 files**, Fast **66**,
All **101**. **I-4 and I-9 (V-9's content) already pass at this tree state** — re-run them at Phase 9 rather
than trusting this sentence, because the tree will have moved.

### 2026-08-11 — `/start-feature`

Docs scaffolded from `implementation.md` §5: 96 tasks across phases 0–9 plus the external/art rows.
Phases 1 and 6 flagged ADVANCED. Tree state at scaffold time: branch `new-map`, five modified files
from the preceding `map/location-types` work, highest bug id **BUG-144**, `{6A84…}` GUID series
confirmed unused.

### 2026-08-11 — D10 (the reframing)

User session producing three changes, recorded as **D10** above: **occupier-only rendering** (the overlay now
answers _how much does the occupier still hold_), **contested = occupier-held and support ≥ 50 %** (which
**overrides §6 K8**, and the reason it escapes K8's objection is the load-bearing part of that section), and
**coast rays exempted from smoothing** (D9's own item-1 residual, and the third face of "splotchy"). Stability
no longer reaches any visual — D9's lerp is deleted rather than parked behind a flag, so there is exactly one
contested switch. Three new Logic cases (14, 15, 16), each proven able to fail; **case 13 re-verified rather
than changed**, because the coast window decision could have made it silently vacuous and did not. Gates:
compile **exit 0 / 5971 files** (no new `.c`), Fast **70**, All **105**. Nothing rendered — D9 and D10 are
both unseen, and D10's shortlist of what may still read as splotchy is where the fourth look should start.
