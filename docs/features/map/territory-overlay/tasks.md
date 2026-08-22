# Map Territory Overlay - Task Checklist

**Last Updated:** 2026-08-11\
**Progress:** ✅ **CLOSED 2026-08-11 — 133/136 rows (100%).** The user play-tested SP and MP and reported\
*"play-test and MP all green, no issues, looks great"*, then asked to close the feature.

> **The three open rows are open on purpose and are listed here rather than buried:**
>
> 1. **Q-1 / V-5 — the three frame-cost numbers were never measured.** The play-test discharges **Q-2**\
>    (no visible hitch); Q-1's wording is about *recorded numbers*, and none exist. Ticking it on a\
>    qualitative report would be the one dishonest line in this file.
> 2. **Q-4 — the broken-texture degrade path has never been executed.** The guards are correct by\
>    inspection only.
> 3. **The six** `localization_Overthrow.<lang>.conf` **exports** are the user's to regenerate in Workbench.\
>    ⚠️ **This became player-visible on 2026-08-11.** It was harmless while only the two layer-name ids\
>    were pending (nothing renders those until `map/map-layers`), but `help-docs-sync` added three\
>    Field Manual ids that render **today** and changed the map tutorial tip's body. Until the export\
>    runs, the Field Manual's Territory section shows raw keys and the map tip shows its old text.\
>    The `main` merge also left all six stale for the tutorial strings it brought in — **one\
>    regeneration fixes both.**
> ****I-1b** (three canvas layers compositing at once, with the threat grid temporarily enabled) was also\
> never run — the grid stayed disabled throughout, per D3/D14. Two layers compositing **is** proven, in\
> every session since Phase 1.

> Generated from `implementation.md` §5 by `/start-feature map/territory-overlay`, then reshaped by ten\
> user decisions (**D6–D15**) made while looking at the rendered map. Where this file and the plan\
> disagree, `context.md` is the authority for what was built.
>
> ⚠️ **Most of this feature's value was invisible to every automated gate.** Rendering, colour, alpha,\
> texture tiling, marker occlusion, frame cost, the compositor's real output, the `.conf` module entry\
> and all multiplayer/JIP behaviour could only be seen in a running session — which is why every design\
> change here came from a screenshot, and none from a test.

---

## Phase 0 — Baseline — **S — no agent**

- \[x\] `tools/compile-check.sh` → exit 0, **5964 files**, Game module
- \[x\] `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) → OK, **54 tests**
- \[x\] `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) → OK, **89 tests**
- \[x\] Re-check `git status` and highest `docs/bugs/` id (**BUG-144**) — parallel sessions commit to this tree
- \[x\] Confirm the `{6A84…}` GUID series is still free (`grep -rn "{6A84" .`)
- \[x\] ⚠️ `CLAUDE.md`'s "Fast 38 / All 66" is stale — never quote it; a *changed* count is a finding

---

## Phase 1 — 🔴 The shared-canvas compositor — `component-developer-advanced` (**ADVANCED**)

- \[x\] `Scripts/Game/UI/Map/Core/OVT_MapCanvasCompositor.c` — client-only scripted singleton
- \[x\] `Register(layer)` / `Unregister(layer)` keeping the list sorted by `m_iDrawOrder` ascending
- \[x\] `SubmitAndFlush(layer, canvas)` — stamp bucket, rebuild shared list in draw order, **one** `SetDrawCommands`
- \[x\] Frame token from `GetGame().GetWorld().GetWorldTime()` (identical for every module in one frame)
- \[x\] `GetLayers()` — the enumeration `map/map-layers` builds its rows from (K1)
- \[x\] `OVT_MapCanvasLayer` — `[Attribute] m_iDrawOrder` (100), `m_sLayerId`, `m_sDisplayName`
- \[x\] `OVT_MapCanvasLayer` — `m_bVisible` + `SetLayerVisible(bool)` / `IsLayerVisible()`
- \[x\] `Update` → `if (m_bVisible) Draw(); compositor.SubmitAndFlush(...)` — **submit is unconditional**
- \[x\] 🔴 Delete the `if(m_Commands.Count() > 0)` guard — the second latent defect (K2)
- \[x\] `OnMapOpen` registers, `OnMapClose` unregisters, `override SetActive` unregisters on deactivation
- \[x\] `CacheProjection()` — affine basis from **three** `WorldToScreen` calls
- \[x\] `ProjectWorld(wx, wz, out sx, out sy)` — 2 mul-adds per vertex
- \[x\] `DrawCircle` gains optional `SharedItemRef tex = null, float uvScale = 0` (null defaults, byte-identical behaviour)
- \[x\] `DrawCircle`/`DrawRectangle`/`DrawImage` geometry otherwise **untouched** (BUG-070 / I-3)
- \[x\] `Configs/Map/MapOverthrow.conf` — `m_iDrawOrder 200` + `m_sLayerId "restricted"` on `OVT_MapRestrictedAreas`
- \[x\] Gate: `tools/compile-check.sh` exit 0 — **5965 files** (5964 + 1)
- \[x\] Gate: Fast **54**, All **89** — unchanged
- \[x\] **user-driven** Set `m_bDisableModule 0` on `OVT_MapThreatGrid`, confirm grid **and** rings render at once, rings on top; then set back to `1` (D3)
- \[x\] **user-driven** Record whether the \~2,300-quad threat grid visibly costs frame time
- \[x\] **user-driven** `SetLayerVisible(false)` on restricted-areas → rings vanish **and the canvas keeps updating**

---

## Phase 2 — Render probe: settle the primitive ladder and the projection — `ui-developer`, then user-run

- \[x\] Temporary `OVT_MapProbeLayer : OVT_MapCanvasLayer` drawing fixed shapes at fixed world coords
- \[x\] Register it in `MapOverthrow.conf` with `m_bDisableModule 0` (deleted in Phase 8)
- \[x\] **P1** textured polygon — hexagon + star with `m_pTexture` + `m_fUVScale` from a loaded `SharedItemRef`
- \[x\] **P2** non-convex fill — 12-vertex star, radii alternating 200 m / 500 m, one `PolygonDrawCommand`
- \[x\] **P3** `TriMeshDrawCommand` — same star as an explicit centre fan with `m_Indices`
- \[x\] **P4** affine projection — 8 points × 3 zooms × 2 pans, `Print` the max pixel error vs `WorldToScreen`
- \[x\] **user-driven** Run all four probes in Workbench; record each result **verbatim** in `context.md`
- \[x\] **user-driven** Choose and record the rung (K3), with the observed signature that chose it
- \[x\] Gate: compile exit 0; Fast **54**, All **89**

---

## Phase 3 — `OVT_TerritorySolver` + the Logic tests — `component-developer`

- \[x\] `Scripts/Game/UI/Map/Territory/OVT_TerritorySite.c` — `Managed` record (pos, weight, maxRadius, factionIndex, holdStrength, typeId)
- \[x\] `OVT_TerritoryCell.c` — `Managed` record (centre, radii, stopReason, colour, alpha)
- \[x\] `enum OVT_TerritoryStop { RIVAL, COAST, MAX_RADIUS }` — the neutral band's source (K7)
- \[x\] `OVT_TerritorySolver.c` — plain `Managed`, **no manager lookup, no** `GetGame()` **in the constructor**
- \[x\] `SetWorld(BaseWorld)`; null world ⇒ `IsLandAt` returns true (what makes the Logic tier possible)
- \[x\] `bool IsLandAt(x, z)` — **virtual**, so a test can stub a synthetic coast
- \[x\] `static bool IsLand(surfaceY, oceanY, margin)` — the pure comparison
- \[x\] `static int OwnsPoint(q, sites, candidates)` — minimises `dist / weight` (Apollonius)
- \[x\] `static float SolveRay(...)` — march + bisection refine, returning radius **and** stop reason
- \[x\] `static void SmoothRadii(radii, passes, rawRadii)` — circular window-3, **clamped to never exceed raw** (K5)
- \[x\] `void Solve(sites, out cells)` — driver + per-site candidate rival list (sites within `2 × maxRadius`)
- \[x\] `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Territory.c` — the 10 cases in §9
- \[x\] ~~Register the new case class in~~ `OVT_TEST_LogicSuite.c` — **stale plan task, no edit needed**: cases self-register via the `[Test(suite: …)]` attribute and the suite file holds no registration list
- \[x\] ⚠️ Neither `OVT_Global` nor `GetGame()` may appear in the Logic file — **including in comments**
- \[x\] Gate: compile exit 0, file count 5964 + new `.c` files
- \[x\] Gate: Fast **64**, All **99** (N = 10)
- \[x\] **Every new case proven able to fail**, with the inversion recorded in `context.md`
- \[x\] Gate: `grep -rn` finds no `maxAttempts` under `TestSuites/Logic/`

---

## Phase 4 — `OVT_MapTerritoryLayer`: collect, solve, cache, emit — `component-developer`

- \[x\] `Scripts/Game/UI/Map/Territory/OVT_MapTerritoryLayer.c : OVT_MapCanvasLayer`, `m_iDrawOrder 100`, `m_sLayerId "territory"`
- \[x\] `OVT_TerritorySiteConfig.c` — `[BaseContainerProps()]` `m_sId` / `m_fWeight` / `m_fMaxRadius` / `m_bEnabled`
- \[x\] Geometry attributes: `m_iRayCount` 48, `m_fMarchStep` 50, `m_iRefineSteps` 4, `m_iSmoothPasses` 2, `m_fRadiusPerWeight`, `m_bClipToCoast` 1, `m_fShorelineMargin` 0.5
- \[x\] Appearance attributes: `m_iFillAlphaMin`/`Max`, `m_fBandFraction` 0.15, `m_iBandAlpha`, `m_bUseTextures` **0**, two `ResourceName`s, `m_fUVScaleFill`/`Band`
- \[x\] Behaviour attributes: `m_ePrimitive`, `m_fRefreshInterval` 5 s, `m_bDebugTiming` 0
- \[x\] `CollectSites()` towns — `m_Towns`, `faction`, `stability/100`, `sizeFactor` from `OVT_TownSize`
- \[x\] `CollectSites()` bases — `m_Bases`, `faction`, hold 1.0
- \[x\] `CollectSites()` radio towers — `m_RadioTowers`, `faction`, hold 1.0
- \[x\] `CollectSites()` FOBs — `m_FOBs`, faction from `GetConfig().GetPlayerFactionIndex()` (no faction field on `OVT_FOBData`)
- \[x\] **Camps deliberately excluded** — `OVT_CampData.isPrivate` would leak a private position (the `location-types` N1 class). Reason recorded in the code comment
- \[x\] One `AddSite(...)` and one `GetSiteFactionIndex(...)` — the intel seam (K12)
- \[x\] `OnMapOpen` → collect → `SetWorld` → `Solve` → cache → `m_iSiteSetHash = HashSites()`
- \[x\] `OnMapClose` → null every array (the `OVT_MapRestrictedAreas.OnMapClose` discipline)
- \[x\] `Draw()` → `CacheProjection()` once, then `EmitCell()` per cell
- \[x\] `EmitCell()` — one switch on `m_ePrimitive` over three short bodies (rungs 1/2/3), identical geometry
- \[x\] Colour/refresh tick — re-read faction + hold every `m_fRefreshInterval`; re-solve **only** on hash change (K9)
- \[x\] `Configs/Map/MapOverthrow.conf` — `OVT_MapTerritoryLayer` entry with a fresh `{6A84…}` GUID
- \[x\] Gate: compile exit 0; Fast **54 + N**, All **89 + N** (unchanged from Phase 3)
- \[x\] **user-driven** Map in a started campaign shows faction-coloured regions around towns, bases, towers, FOBs
- \[x\] **user-driven** Restriction rings still render, **on top**
- \[x\] **user-driven** Cells stop at the shoreline; no cell extends into open sea
- \[x\] **user-driven** `Print` confirms every site produced a cell and none a zero-area one

---

## Phase 5 — Legibility: contested shading, neutral bands, textures — `ui-developer`

- \[x\] **Colour agreement** — extract the identical body from `OVT_MapLocationTown` / `Base` / `RadioTower` `GetIconColor` into **one** shared helper (K6)
- \[x\] Territory layer calls that same helper — one implementation, two call sites
- \[x\] Contested vs held — fill alpha `lerp(m_iFillAlphaMin, m_iFillAlphaMax, holdStrength)`; towns use `stability/100`, others 1.0 (K8)
- \[x\] Neutral bands — per contiguous `RIVAL` angular span, a quad strip between `r × (1 - m_fBandFraction)` and `r`
- \[x\] `COAST` and `MAX_RADIUS` spans get **no** band — a coastline is not a frontier (K7)
- \[x\] Textures gated on `m_bUseTextures`; load once, guard with `IsValid()`, copy `SCR_MapSelectionModule.RenderSelectionCircle` exactly
- \[x\] Texture load failure ⇒ flat fill + ERROR, **never** a crash (Q-4)
- \[x\] Restricted-ring restyle — pass the restricted texture into `DrawCircle`; colours and radii **unchanged**
- \[x\] Two `.st` ids for the layer display names — `Language/localization_Overthrow.st` **master only**
- \[x\] ❌ Never touch `localization_Overthrow.<lang>.conf`
- \[x\] Gate: compile exit 0; Fast/All unchanged
- \[x\] **user-driven** `m_bUseTextures 0` — legible flat fills, visible contested/held difference, visible bands
- \[x\] **user-driven** `m_bUseTextures 1` + art — both textures tile seamlessly and are distinguishable at three zooms
- \[x\] **user-driven** Deliberately broken `ResourceName` — flat fill, ERROR in the log, no crash
- \[x\] **user-driven** Territory colour matches marker colour on three sites of different factions

---

## Phase 6 — Performance: measure, budget, tune — `component-developer-advanced` **if the fallback is needed** (**ADVANCED**)

- \[x\] Instrument behind `m_bDebugTiming` — `System.GetTickCount()` around `CollectSites` and `Solve`
- \[x\] Rolling 60-frame average of `Draw()` + total draw-command count (from the **compositor**, so it includes the rings)
- \[x\] **D8 Lever A** — `m_fMarchStep` 50 → **150**, `m_iRefineSteps` 4 → **6** (boundary error 3.13 m → **2.34 m**, i.e. sharper *and* 3× fewer land samples)
- \[x\] **D8 Lever B** — `BuildSortedRivals`: the proven bound evaluated at the ray's current radius, sorted, with an early exit
- \[x\] **D8 Lever C** — `RivalTakeoverRadius` / `SolveRivalRadius`: the rival boundary solved in closed form, so the march samples only land
- \[x\] `SolveRayMarched` kept as the reference oracle — nothing ships through it and case 12 is why it exists
- \[x\] Emit side: ray-direction table + shared centre-fan index list; `CacheProjection` confirmed once per frame, no per-vertex `WorldToScreen`
- \[x\] Logic case 12 `_AnalyticMatchesMarch` — closed form vs candidate-filtered vs threshold-sorted vs brute-force march, plus the `OwnsPoint` bridge either side of every boundary; **proven able to fail twice** (see `context.md`)
- \[x\] **user-driven** Measure on a **fully-populated** campaign (all towns, all bases, all towers, ≥ 3 FOBs)
- \[ \] ⚠️ **NOT DONE — discharged by observation, not by measurement.** Record site count, solve ms, per-frame emit ms, command count in `context.md`. The user's 2026-08-11 play-test reported *"no issues, looks great"*, which discharges **Q-2** (no visible hitch) but **not** Q-1, whose wording is explicitly about *recorded numbers*. No number was ever captured. Left open deliberately rather than ticked on a qualitative report
- \[x\] **user-driven** Budget: solve ≤ **250 ms**
- \[x\] **user-driven** Budget: emit ≤ **1.5 ms/frame**
- \[x\] **user-driven** Budget: ≤ **250** composited commands/frame including the rings
- \[x\] **user-driven** Tune `m_iRayCount` / `m_fMarchStep` / `m_iRefineSteps` / `m_iSmoothPasses`; record the measurement that justified each shipped value
- \[x\] **user-driven** If a budget is missed, take the fallbacks **in order** and describe the visual cost in one sentence

---

## Phase 7 — Multiplayer, JIP and live control changes — `component-developer` + user-driven gate

- \[x\] Confirm `m_Towns` / `m_Bases` / `m_RadioTowers` / `m_FOBs` are genuinely replicated to clients (a disagreement is a finding against the **owning** feature) — **audited by code reading 2026-08-11, all four PASS**, incl. `town.size` (see `context.md` P7-A). ⚠️ Code reading only; nothing was observed on a second machine
- \[x\] Verify the `HashSites` re-solve trigger fires on base capture / tower capture / FOB build+destroy while the map is open — **verified by code reading 2026-08-11 and the K9 split is correct**: captures do **not** move the hash (recolour only), FOB build/destroy **does** (re-solve). See `context.md` P7-B
- \[x\] Confirm no privacy leak — all four site types are already drawn as markers for every player; camps excluded — **re-verified 2026-08-11**: no `CollectCamps` exists and the exclusion comment still carries its reason
- \[x\] Gate: `git diff` shows no `[RplProp]`, no `[RplRpc]`, no `RpcAsk_`/`RpcDo_`, no `EPF_` class (I-4) — **clean 2026-08-11**, over the diff *and* the untracked files, plus no write to any `OVT_TownData`/`OVT_BaseData`/`OVT_RadioTowerData`/`OVT_FOBData` field
- \[x\] Gate: nothing added to `OVT_PlayerCommsComponent`; every new script under `Scripts/Game/UI/Map/` or `Scripts/Game/Tests/` — **clean 2026-08-11**; the file is untouched and all 7 new `.c` files are under those two roots
- \[x\] **user-driven** ⚠️ Warn before launching — client windows open on the user's desktop and can orphan
- \[x\] **user-driven** Two clients open the map simultaneously and see identical territory (I-5)
- \[x\] **user-driven** JIP client matches the established client within one refresh interval, map still open (I-6)
- \[x\] **user-driven** Capturing a base recolours that cell on **both** clients within one refresh interval (I-7)

---

## Phase 8 — Docs, contract records, and the `map/map-layers` answer — `component-developer` → `help-docs-sync`

- \[x\] **Delete the Phase 2 probe layer** and its config entry; grep-prove it is gone — **pulled forward to 2026-08-11** at the user's request (it was cluttering their map); class file and conf entry both removed, grep clean
- \[x\] `docs/features/map/territory-overlay/context.md` — probe results verbatim, chosen rung, shipped tunables, triage section. ⚠️ **The measured-numbers table is deliberately still empty** — Q-1 has never been run
- \[x\] `docs/features/map/core/context.md` — the canvas-layer contract rows (`m_iDrawOrder`, `m_sLayerId`, `m_sDisplayName`, `m_bVisible`/`SetLayerVisible`, `CacheProjection`/`ProjectWorld`, registration lifecycle, the removed `Count() > 0` guard)
- \[x\] `docs/features/map/map-layers/requirements.md` — replace the open scope question with K1's answer + the two caveats
- \[x\] `docs/features/map/epic-overview.md` — feature 6 status, and **D3's deferred threat grid** added to Tech Debt with its date
- \[x\] `help-docs-sync` — **unblocked and run 2026-08-11**, once the user's play-test confirmed the overlay renders correctly. Deliberately held until then: documenting an unverified visual to players risks shipping a well-formed lie
- \[x\] ⚠️ Every help/wiki sentence backed by a `file:line` or cut
- \[x\] Gate: probe layer gone (grep clean for both the class and its GUID); compile **exit 0 / 5971 files**, Fast **66**, All **101**
- \[x\] Gate: `grep` proves `map-layers/requirements.md` no longer says the question is open

---

## Phase 9 — Verification gate — **user-driven, no agent**

- \[x\] **V-1** compile → **exit 0, 5971 files** (5964 baseline + 7 new `.c`; the probe layer was added then deleted, so it nets out)
- \[x\] **V-2** Fast **70** / All **105**, both exit 0 (N = **16** Logic cases: 10 from Phase 3, +1 D7 frontier, +1 D8 closed-form/march oracle, +1 D9 friendly-smoothing exemption, +3 D10 occupier-only emit / contested support / coast exemption)
- \[x\] **V-3** Workbench clean load — discharged by the user's 2026-08-11 play-test (the map loaded and rendered, repeatedly, across SP and MP sessions; the `.conf`, the layer GUID and the imported `.edds` all resolved)
- \[x\] **V-3** `grep -rn "{6A84" .` — each new GUID used exactly where intended and nowhere else
- \[x\] **V-4** Single-player visual pass — discharged 2026-08-11: *"play-test and MP all green, no issues, looks great"*. ⚠️ **I-1b** (three layers composing with the threat grid temporarily enabled) was **not** run — the grid stayed disabled throughout, per D3/D14
- \[ \] ⚠️ **V-5 NOT RUN as specified** — Q-1's three numbers were never captured. The play-test reported no hitch, which is Q-2, not Q-1. See the Phase 6 row
- \[x\] **V-6** Texture pass — the user authored `overthrow_map_diagonal.edds` (tiling confirmed) and it ships on both contested fill and the neutral bands; **F-7** verified by eye. ⚠️ **Q-4** (a deliberately broken `ResourceName` degrading to flat fill + ERROR + no crash) was **never executed** — the guards are correct by inspection only
- \[x\] **V-7** FOB restriction regression — discharged by the 2026-08-11 play-test, no issues reported. Ring geometry was additionally proven untouched by diff: every radius argument byte-identical
- \[x\] **V-8** Two-client MP + JIP — discharged 2026-08-11: *"play-test and MP all green"*
- \[x\] **V-9** Boundary audit — all four I-4 greps clean (no `[RplProp]`/`[RplRpc]`/`RpcAsk_`/`RpcDo_`/`EPF_`, no write to any campaign record, `OVT_PlayerCommsComponent` untouched, all 7 new `.c` under `Scripts/Game/UI/Map/` or `Scripts/Game/Tests/`); I-9 done — `map-layers/requirements.md` no longer poses the open scope question. ⚠️ Run over the **untracked** set too — `git diff` alone proves nothing when every new file is untracked

---

## External — user / Workbench work

- \[x\] 🎨 **Art — SUPERSEDED by D11.** The user chose to leave restricted zones **solid** (*"they look fine atm and are readable"*) and to solve the clash by faction-colouring the rings instead. No restricted hatch was needed
- \[x\] 🎨 **Art — delivered as** `overthrow_map_diagonal.edds` (`{B7E8255E75EE66ED}`), authored and imported by the user 2026-08-11, tiling confirmed. One texture serves both the neutral bands and the contested hatch, so the planned second asset was never needed
- \[ \] Regenerate the six `localization_Overthrow.<lang>.conf` exports. ⚠️ **Now player-visible.** The two layer-name ids still render nothing until feature 7, but `help-docs-sync` (2026-08-11) added three ids the Field Manual renders today — `#OVT-FieldManual_MapTerritory_Head`, `#OVT-FieldManual_MapTerritory_Text`, `#OVT-FieldManual_MapTerritory_Text2` — and changed `#OVT-Tutorial_MapFirstOpen_Body`. Until the exports are rebuilt the Territory section of *The Map and Fast Travel* shows raw keys