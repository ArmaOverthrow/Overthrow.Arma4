# Map Territory Overlay - Task Checklist

**Last Updated:** 2026-08-11
**Progress:** 0/96 tasks complete (0%)

> Generated from `implementation.md` §5 by `/start-feature map/territory-overlay`.
> **Phases 1 and 6 are ADVANCED** (`component-developer-advanced`) — Phase 1 edits the base class every
> canvas layer inherits and answers `map/map-layers`' blocking question; Phase 6's fallback restructures
> the solve across frames.
>
> ⚠️ **Most of this feature's value is invisible to every automated gate.** Rendering, colour, alpha,
> texture tiling, marker occlusion, frame cost, the compositor's real output, the `.conf` module entry
> and **all** multiplayer/JIP behaviour can only be seen in Workbench or in a running session. Rows that
> need a human are marked **user-driven** and are left unticked rather than assumed.

---

## Phase 0 — Baseline — **S — no agent**

- [ ] `tools/compile-check.sh` → exit 0, **5964 files**, Game module
- [ ] `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) → OK, **54 tests**
- [ ] `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) → OK, **89 tests**
- [ ] Re-check `git status` and highest `docs/bugs/` id (**BUG-144**) — parallel sessions commit to this tree
- [ ] Confirm the `{6A84…}` GUID series is still free (`grep -rn "{6A84" .`)
- [ ] ⚠️ `CLAUDE.md`'s "Fast 38 / All 66" is stale — never quote it; a *changed* count is a finding

---

## Phase 1 — 🔴 The shared-canvas compositor — `component-developer-advanced` (**ADVANCED**)

- [ ] `Scripts/Game/UI/Map/Core/OVT_MapCanvasCompositor.c` — client-only scripted singleton
- [ ] `Register(layer)` / `Unregister(layer)` keeping the list sorted by `m_iDrawOrder` ascending
- [ ] `SubmitAndFlush(layer, canvas)` — stamp bucket, rebuild shared list in draw order, **one** `SetDrawCommands`
- [ ] Frame token from `GetGame().GetWorld().GetWorldTime()` (identical for every module in one frame)
- [ ] `GetLayers()` — the enumeration `map/map-layers` builds its rows from (K1)
- [ ] `OVT_MapCanvasLayer` — `[Attribute] m_iDrawOrder` (100), `m_sLayerId`, `m_sDisplayName`
- [ ] `OVT_MapCanvasLayer` — `m_bVisible` + `SetLayerVisible(bool)` / `IsLayerVisible()`
- [ ] `Update` → `if (m_bVisible) Draw(); compositor.SubmitAndFlush(...)` — **submit is unconditional**
- [ ] 🔴 Delete the `if(m_Commands.Count() > 0)` guard — the second latent defect (K2)
- [ ] `OnMapOpen` registers, `OnMapClose` unregisters, `override SetActive` unregisters on deactivation
- [ ] `CacheProjection()` — affine basis from **three** `WorldToScreen` calls
- [ ] `ProjectWorld(wx, wz, out sx, out sy)` — 2 mul-adds per vertex
- [ ] `DrawCircle` gains optional `SharedItemRef tex = null, float uvScale = 0` (null defaults, byte-identical behaviour)
- [ ] `DrawCircle`/`DrawRectangle`/`DrawImage` geometry otherwise **untouched** (BUG-070 / I-3)
- [ ] `Configs/Map/MapFullscreen.conf` — `m_iDrawOrder 200` + `m_sLayerId "restricted"` on `OVT_MapRestrictedAreas`
- [ ] Gate: `tools/compile-check.sh` exit 0 — **5965 files** (5964 + 1)
- [ ] Gate: Fast **54**, All **89** — unchanged
- [ ] **user-driven** Set `m_bDisableModule 0` on `OVT_MapThreatGrid`, confirm grid **and** rings render at once, rings on top; then set back to `1` (D3)
- [ ] **user-driven** Record whether the ~2,300-quad threat grid visibly costs frame time
- [ ] **user-driven** `SetLayerVisible(false)` on restricted-areas → rings vanish **and the canvas keeps updating**

---

## Phase 2 — Render probe: settle the primitive ladder and the projection — `ui-developer`, then user-run

- [ ] Temporary `OVT_MapProbeLayer : OVT_MapCanvasLayer` drawing fixed shapes at fixed world coords
- [ ] Register it in `MapFullscreen.conf` with `m_bDisableModule 0` (deleted in Phase 8)
- [ ] **P1** textured polygon — hexagon + star with `m_pTexture` + `m_fUVScale` from a loaded `SharedItemRef`
- [ ] **P2** non-convex fill — 12-vertex star, radii alternating 200 m / 500 m, one `PolygonDrawCommand`
- [ ] **P3** `TriMeshDrawCommand` — same star as an explicit centre fan with `m_Indices`
- [ ] **P4** affine projection — 8 points × 3 zooms × 2 pans, `Print` the max pixel error vs `WorldToScreen`
- [ ] **user-driven** Run all four probes in Workbench; record each result **verbatim** in `context.md`
- [ ] **user-driven** Choose and record the rung (K3), with the observed signature that chose it
- [ ] Gate: compile exit 0; Fast **54**, All **89**

---

## Phase 3 — `OVT_TerritorySolver` + the Logic tests — `component-developer`

- [ ] `Scripts/Game/UI/Map/Territory/OVT_TerritorySite.c` — `Managed` record (pos, weight, maxRadius, factionIndex, holdStrength, typeId)
- [ ] `OVT_TerritoryCell.c` — `Managed` record (centre, radii, stopReason, colour, alpha)
- [ ] `enum OVT_TerritoryStop { RIVAL, COAST, MAX_RADIUS }` — the neutral band's source (K7)
- [ ] `OVT_TerritorySolver.c` — plain `Managed`, **no manager lookup, no `GetGame()` in the constructor**
- [ ] `SetWorld(BaseWorld)`; null world ⇒ `IsLandAt` returns true (what makes the Logic tier possible)
- [ ] `bool IsLandAt(x, z)` — **virtual**, so a test can stub a synthetic coast
- [ ] `static bool IsLand(surfaceY, oceanY, margin)` — the pure comparison
- [ ] `static int OwnsPoint(q, sites, candidates)` — minimises `dist / weight` (Apollonius)
- [ ] `static float SolveRay(...)` — march + bisection refine, returning radius **and** stop reason
- [ ] `static void SmoothRadii(radii, passes, rawRadii)` — circular window-3, **clamped to never exceed raw** (K5)
- [ ] `void Solve(sites, out cells)` — driver + per-site candidate rival list (sites within `2 × maxRadius`)
- [ ] `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Territory.c` — the 10 cases in §9
- [ ] Register the new case class in `OVT_TEST_LogicSuite.c`
- [ ] ⚠️ Neither `OVT_Global` nor `GetGame()` may appear in the Logic file — **including in comments**
- [ ] Gate: compile exit 0, file count 5964 + new `.c` files
- [ ] Gate: Fast **54 + N**, All **89 + N**
- [ ] **Every new case proven able to fail**, with the inversion recorded in `context.md`
- [ ] Gate: `grep -rn` finds no `maxAttempts` under `TestSuites/Logic/`

---

## Phase 4 — `OVT_MapTerritoryLayer`: collect, solve, cache, emit — `component-developer`

- [ ] `Scripts/Game/UI/Map/Territory/OVT_MapTerritoryLayer.c : OVT_MapCanvasLayer`, `m_iDrawOrder 100`, `m_sLayerId "territory"`
- [ ] `OVT_TerritorySiteConfig.c` — `[BaseContainerProps()]` `m_sId` / `m_fWeight` / `m_fMaxRadius` / `m_bEnabled`
- [ ] Geometry attributes: `m_iRayCount` 48, `m_fMarchStep` 50, `m_iRefineSteps` 4, `m_iSmoothPasses` 2, `m_fRadiusPerWeight`, `m_bClipToCoast` 1, `m_fShorelineMargin` 0.5
- [ ] Appearance attributes: `m_iFillAlphaMin`/`Max`, `m_fBandFraction` 0.15, `m_iBandAlpha`, `m_bUseTextures` **0**, two `ResourceName`s, `m_fUVScaleFill`/`Band`
- [ ] Behaviour attributes: `m_ePrimitive`, `m_fRefreshInterval` 5 s, `m_bDebugTiming` 0
- [ ] `CollectSites()` towns — `m_Towns`, `faction`, `stability/100`, `sizeFactor` from `OVT_TownSize`
- [ ] `CollectSites()` bases — `m_Bases`, `faction`, hold 1.0
- [ ] `CollectSites()` radio towers — `m_RadioTowers`, `faction`, hold 1.0
- [ ] `CollectSites()` FOBs — `m_FOBs`, faction from `GetConfig().GetPlayerFactionIndex()` (no faction field on `OVT_FOBData`)
- [ ] **Camps deliberately excluded** — `OVT_CampData.isPrivate` would leak a private position (the `location-types` N1 class). Reason recorded in the code comment
- [ ] One `AddSite(...)` and one `GetSiteFactionIndex(...)` — the intel seam (K12)
- [ ] `OnMapOpen` → collect → `SetWorld` → `Solve` → cache → `m_iSiteSetHash = HashSites()`
- [ ] `OnMapClose` → null every array (the `OVT_MapRestrictedAreas.OnMapClose` discipline)
- [ ] `Draw()` → `CacheProjection()` once, then `EmitCell()` per cell
- [ ] `EmitCell()` — one switch on `m_ePrimitive` over three short bodies (rungs 1/2/3), identical geometry
- [ ] Colour/refresh tick — re-read faction + hold every `m_fRefreshInterval`; re-solve **only** on hash change (K9)
- [ ] `Configs/Map/MapFullscreen.conf` — `OVT_MapTerritoryLayer` entry with a fresh `{6A84…}` GUID
- [ ] Gate: compile exit 0; Fast **54 + N**, All **89 + N** (unchanged from Phase 3)
- [ ] **user-driven** Map in a started campaign shows faction-coloured regions around towns, bases, towers, FOBs
- [ ] **user-driven** Restriction rings still render, **on top**
- [ ] **user-driven** Cells stop at the shoreline; no cell extends into open sea
- [ ] **user-driven** `Print` confirms every site produced a cell and none a zero-area one

---

## Phase 5 — Legibility: contested shading, neutral bands, textures — `ui-developer`

- [ ] **Colour agreement** — extract the identical body from `OVT_MapLocationTown` / `Base` / `RadioTower` `GetIconColor` into **one** shared helper (K6)
- [ ] Territory layer calls that same helper — one implementation, two call sites
- [ ] Contested vs held — fill alpha `lerp(m_iFillAlphaMin, m_iFillAlphaMax, holdStrength)`; towns use `stability/100`, others 1.0 (K8)
- [ ] Neutral bands — per contiguous `RIVAL` angular span, a quad strip between `r × (1 - m_fBandFraction)` and `r`
- [ ] `COAST` and `MAX_RADIUS` spans get **no** band — a coastline is not a frontier (K7)
- [ ] Textures gated on `m_bUseTextures`; load once, guard with `IsValid()`, copy `SCR_MapSelectionModule.RenderSelectionCircle` exactly
- [ ] Texture load failure ⇒ flat fill + ERROR, **never** a crash (Q-4)
- [ ] Restricted-ring restyle — pass the restricted texture into `DrawCircle`; colours and radii **unchanged**
- [ ] Two `.st` ids for the layer display names — `Language/localization_Overthrow.st` **master only**
- [ ] ❌ Never touch `localization_Overthrow.<lang>.conf`
- [ ] Gate: compile exit 0; Fast/All unchanged
- [ ] **user-driven** `m_bUseTextures 0` — legible flat fills, visible contested/held difference, visible bands
- [ ] **user-driven** `m_bUseTextures 1` + art — both textures tile seamlessly and are distinguishable at three zooms
- [ ] **user-driven** Deliberately broken `ResourceName` — flat fill, ERROR in the log, no crash
- [ ] **user-driven** Territory colour matches marker colour on three sites of different factions

---

## Phase 6 — Performance: measure, budget, tune — `component-developer-advanced` **if the fallback is needed** (**ADVANCED**)

- [ ] Instrument behind `m_bDebugTiming` — `System.GetTickCount()` around `CollectSites` and `Solve`
- [ ] Rolling 60-frame average of `Draw()` + total draw-command count
- [ ] **user-driven** Measure on a **fully-populated** campaign (all towns, all bases, all towers, ≥ 3 FOBs)
- [ ] **user-driven** Record site count, solve ms, per-frame emit ms, command count in `context.md`
- [ ] **user-driven** Budget: solve ≤ **250 ms**
- [ ] **user-driven** Budget: emit ≤ **1.5 ms/frame**
- [ ] **user-driven** Budget: ≤ **250** composited commands/frame including the rings
- [ ] **user-driven** Tune `m_iRayCount` / `m_fMarchStep` / `m_iRefineSteps` / `m_iSmoothPasses`; record the measurement that justified each shipped value
- [ ] **user-driven** If a budget is missed, take the fallbacks **in order** and describe the visual cost in one sentence

---

## Phase 7 — Multiplayer, JIP and live control changes — `component-developer` + user-driven gate

- [ ] Confirm `m_Towns` / `m_Bases` / `m_RadioTowers` / `m_FOBs` are genuinely replicated to clients (a disagreement is a finding against the **owning** feature)
- [ ] Verify the `HashSites` re-solve trigger fires on base capture / tower capture / FOB build+destroy while the map is open
- [ ] Confirm no privacy leak — all four site types are already drawn as markers for every player; camps excluded
- [ ] Gate: `git diff` shows no `[RplProp]`, no `[RplRpc]`, no `RpcAsk_`/`RpcDo_`, no `EPF_` class (I-4)
- [ ] Gate: nothing added to `OVT_PlayerCommsComponent`; every new script under `Scripts/Game/UI/Map/` or `Scripts/Game/Tests/`
- [ ] **user-driven** ⚠️ Warn before launching — client windows open on the user's desktop and can orphan
- [ ] **user-driven** Two clients open the map simultaneously and see identical territory (I-5)
- [ ] **user-driven** JIP client matches the established client within one refresh interval, map still open (I-6)
- [ ] **user-driven** Capturing a base recolours that cell on **both** clients within one refresh interval (I-7)

---

## Phase 8 — Docs, contract records, and the `map/map-layers` answer — `component-developer` → `help-docs-sync`

- [ ] **Delete the Phase 2 probe layer** and its config entry; grep-prove it is gone
- [ ] `docs/features/map/territory-overlay/context.md` — probe results verbatim, chosen rung, measured numbers, shipped tunables, triage section
- [ ] `docs/features/map/core/context.md` — the canvas-layer contract rows (`m_iDrawOrder`, `m_sLayerId`, `m_sDisplayName`, `m_bVisible`/`SetLayerVisible`, `CacheProjection`/`ProjectWorld`, registration lifecycle, the removed `Count() > 0` guard)
- [ ] `docs/features/map/map-layers/requirements.md` — replace the open scope question with K1's answer + the two caveats
- [ ] `docs/features/map/epic-overview.md` — feature 6 status, and **D3's deferred threat grid** added to Tech Debt with its date
- [ ] Run `help-docs-sync` — the overlay is **a display of existing control**, never a mechanic
- [ ] ⚠️ Every help/wiki sentence backed by a `file:line` or cut
- [ ] Gate: probe layer gone; compile exit 0 at the expected file count
- [ ] Gate: `grep` proves `map-layers/requirements.md` no longer says the question is open

---

## Phase 9 — Verification gate — **user-driven, no agent**

- [ ] **V-1** compile → exit 0, file count = 5964 + new `.c` files
- [ ] **V-2** Fast **54 + N** / All **89 + N**, both exit 0
- [ ] **user-driven V-3** 🔴 Workbench clean load — the **only** gate that sees the `.conf`, the new GUID, the `.edds` and their `.meta`
- [ ] **user-driven V-3** `grep -rn "{6A84" .` — each new GUID used exactly where intended and nowhere else
- [ ] **user-driven V-4** Single-player visual pass — F-1…F-9, Q-2, Q-3, I-1, I-1b, I-2, I-8
- [ ] **user-driven V-5** Performance pass on the populated save — Q-1, then `m_bDebugTiming 0`
- [ ] **user-driven V-6** Texture pass after the art lands — F-7, Q-4
- [ ] **user-driven V-7** FOB restriction regression — I-3, both ring types
- [ ] **user-driven V-8** Two-client MP + JIP — I-5, I-6, I-7
- [ ] **V-9** Boundary audit — I-4's four greps/diffs and I-9

---

## External — user / Workbench work

- [ ] 🎨 **Art:** `UI/Textures/Map/overthrow_hatch_restricted.edds` + `.meta` — seamlessly tileable, alpha-bearing, quiet, coarse
- [ ] 🎨 **Art:** `UI/Textures/Map/overthrow_hatch_neutral.edds` + `.meta` — visually distinct in *structure*, not just spacing
- [ ] Regenerate the six `localization_Overthrow.<lang>.conf` exports (not blocking — nothing renders the two new ids until feature 7)
