# Map Territory Overlay — Implementation Plan

**Status:** ✅ **COMPLETE — built, gate-green, and play-tested green by the user on 2026-08-11** (SP + MP, no issues)
**Epic:** map (feature 6 of 8 — the **first stretch goal**)
**Started:** 2026-08-11
**Target Completion:** TBD
**Last Updated:** 2026-08-11 (Phases 0–8 built by `/autorun-feature map/territory-overlay`)

> ⚠️ **This plan is no longer the whole truth.** Six decisions were made *during* implementation and
> **five of them override it**: **D6** (no maximum influence radius — kills §4 D1's reach derivation and
> §6 K4's entire cost model), **D7** (neutral bands only on inter-faction frontiers — §6 K7 asserted a
> property the data could not express), **D8** (the closed-form takeover radius that replaced the
> per-step rival test), **D9** (flat alpha per faction, and same-faction cells tessellate — waives DoD F-6
> and narrows §6 K5), and **D10** (**the overlay draws occupier-held ground only**; contested is a
> support threshold, which **overrides §6 K8 by name**; coastlines are not smoothed, which narrows §6 K5
> again), and **D11** (the first decision taken with the **art actually in the tree**: contested regions
> are **hatched** rather than merely fainter, and restricted rings take the **owning faction's** colour —
> which **overrides §5 Phase 5 task 5's "colours … unchanged"** and redirects §4 D4's first texture).
> All are recorded in `context.md`, which is the authority for what was **built**.
> §5's phase list is the authority for what was **planned**. Where they disagree, `context.md` wins.

> **`requirements.md` contradicts itself and this plan overrides it.** That file's Requirements bullets were
> edited later than its Out of Scope block, so the two disagree about weighting, about FOBs and radio towers
> as sites, and about coastline clipping. The contradictions were put to the user on **2026-08-11** and
> settled; **§4 Settled Planning Decisions is the authority**, and it also records the two waivers and one
> deferral the user granted. `requirements.md` and `epic-requirements.md` are left unedited as the record of
> earlier intent — do not "fix" this plan back to their text.
>
> All `file:line` citations in this document are load-bearing; keep them when editing. Citations in **code
> comments** follow the epic's **K-9 discipline** instead: keep the rationale, name the symbol, drop the line
> number.

---

## 1. Executive Summary

Today the player can only learn who controls what by reading town and base markers one at a time. This feature
turns that into a single glance: a **coloured territory overlay** drawn on the map canvas beneath the markers,
partitioning the island into faction-coloured regions around towns, military bases, radio towers and resistance
FOBs.

The geometry is a **radial ray-march** — for each site, cast `N` rays outward and stop each one at the first of
three conditions (a rival site wins the point under a multiplicatively-weighted distance test, the ray leaves
land, or it exceeds the site's influence radius). The resulting cell is **star-shaped about its own site by
construction**, which is the structural escape from the "does `PolygonDrawCommand` fill non-convex polygons?"
unknown `requirements.md` flags: a triangle fan from the site centre is always a valid triangulation, so a
correct render exists on every rung of the fallback ladder. Weighting, coastline clipping and radius clipping
all fall out of the same loop rather than needing three geometry passes, and smoothing is a 1-D circular pass
over the radius array rather than 2-D polygon subdivision.

The solve runs **once per map open** and caches world-space vertices; per frame the layer projects with a
**three-call affine basis** (not one `WorldToScreen` per vertex) and emits a handful of draw commands.

**Before any of that can ship, one defect must be fixed.** Every `OVT_MapCanvasLayer` resolves the *same*
`CanvasWidget` (`OVT_MapCanvasLayer.c:89`) and each calls `m_Canvas.SetDrawCommands(m_Commands)` from its own
`Update` with its own private list (`:12-19`). **The last module to run wins and every earlier layer's commands
are discarded.** It is invisible today only because `OVT_MapThreatGrid` ships disabled, leaving
`OVT_MapRestrictedAreas` as the only live layer. Territory is the second live layer, so the defect becomes real
the moment this feature ships — and its symptom ("territory renders and the FOB restriction rings vanish", or
the reverse) reads exactly like a broken territory layer. **Phase 1 is a shared-command-list compositor on
`OVT_MapCanvasLayer` with an explicit draw order**, and it is what unblocks `map/map-layers` (feature 7), whose
recorded planning note is waiting on this feature to decide whether overlay toggles are generic or hardcoded.
**§6 K1 answers that question: generic registration.**

Three code facts found during planning change the shape of the work and are worth stating up front:

1. **Marker colour is already live-faction-driven for the three types that matter.** The task brief warned that
   `GetIconColor` keys off a config-declared `m_FactionType` rather than a record's live controlling faction.
   That is true of the **base class** (`OVT_MapLocationType.c:437-452` → `GetFactionColor`, `:455-481`), but
   Town, Base and RadioTower all **override** it and resolve `GetDataInt("faction")` through
   `GetGame().GetFactionManager().GetFactionByIndex(...).GetFactionColor()`
   (`OVT_MapLocationTown.c:137`, `OVT_MapLocationBase.c:114-129`, `OVT_MapLocationRadioTower.c:83`). Territory
   colour therefore agrees with marker colour by using **the same expression on the same field** — no marker
   colours change, and no second palette is introduced. See §6 K6.
2. **`OVT_FOBData` has no `faction` field at all** (`OVT_ResistanceFactionManager.c:22-36`) — FOBs are
   implicitly resistance-owned, and `OVT_MapLocationFOB` gets its marker colour from the config path
   (`m_FactionType RESISTANCE_FACTION`, `OverthrowMap.conf:48-49`). Territory must use
   `OVT_Global.GetConfig().GetPlayerFactionIndex()` for FOB sites, which resolves to the same `Faction` object
   and therefore the same colour.
3. **`PolygonDrawCommand` really does carry `m_fUVScale` and `ref SharedItemRef m_pTexture`**
   (`ArmaReforger/scripts/Core/proto/EnWidgets.c:101-107`), verified against the engine header. Overthrow's own
   `DrawCircle`/`DrawRectangle` helpers simply never set them. The textured-fill decision is therefore a
   question of *engine support*, not of API existence — which is what the Phase 2 probe measures.

---

## 2. Goals

### Primary

1. **Control reads at a glance.** Opening the map shows faction-coloured regions; the player learns who holds
   what without clicking anything.
2. **Legible without being loud.** Terrain, roads and markers stay readable underneath. Contested/low-stability
   areas are visually distinguishable from firmly-held ones.
3. **Territory and restriction rings both render, simultaneously.** The shared-canvas defect is fixed, with a
   defined draw order, and `BUG-070`'s restriction radii do not regress.
4. **Correct in multiplayer including JIP.** A joining client's overlay matches an established client's and
   updates as towns and bases change hands.
5. **It costs a measured, budgeted amount of frame time** on a fully-populated campaign — not an assumed one.

### Secondary

6. **The overlay is a projection, never a mechanic.** No new territory or influence model enters the campaign
   simulation; no new replicated state; no persistence; no writes to town/base/FOB records.
7. **`map/map-layers` is unblocked** with a generic canvas-layer registration API and a working toggle
   primitive, so feature 7 can be planned without re-opening this ground.
8. **The geometry is genuinely unit-tested** in the Logic tier — the whole point of splitting a pure solver out
   of the layer.
9. **Tunables live in config**, so tuning the overlay is a `.conf` edit and adding a site type does not touch
   the overlay's core.

### Explicit non-goals

- Fog of war, scouting, per-player knowledge of control. A future intel epic owns that; §6 K12 records the one
  seam it will extend, and nothing here assumes the map always shows everything.
- Territory history, time-lapse, or transition animation as regions change hands.
- Merging adjacent same-faction cells into one region. Cells are per-site and stay per-site (see §6 K3).
- Reviving `OVT_MapThreatGrid` (§4 D3 — deferred by the user).
- A player-facing overlay toggle or legend — that is `map/map-layers` (§4 D5).
- Territory on the **respawn** map (§6 K11 — a deliberate no, with the reason).

---

## 3. Architecture Overview

### 3.1 Component hierarchy

```
SCR_MapEntity  (vanilla, drives module Update every frame while the map is open)
│
└── Configs/Map/MapFullscreen.conf  ─ m_aModules  (same-GUID DELTA over vanilla's)
    ├── OVT_MapTerritoryLayer   : OVT_MapCanvasLayer   NEW   m_iDrawOrder 100
    ├── OVT_MapRestrictedAreas  : OVT_MapCanvasLayer    ~    m_iDrawOrder 200 (above territory)
    └── OVT_MapThreatGrid       : OVT_MapCanvasLayer    ~    stays m_bDisableModule 1

OVT_MapCanvasLayer                                      ~    registers with the compositor,
                                                             owns the projection basis
└── OVT_MapCanvasCompositor                            NEW   ONE ordered command list, ONE
                                                             SetDrawCommands per frame

OVT_MapTerritoryLayer
├── collect  → array<ref OVT_TerritorySite>            NEW   pos, weight, factionIndex, holdStrength
├── solve    → OVT_TerritorySolver                     NEW   PURE geometry, world-free, unit-tested
└── cache    → array<ref OVT_TerritoryCell>            NEW   centre + world-space radii + stop reasons
```

`OVT_MapPlayerLocation` is **not** a canvas layer — it is a `SCR_MapUIBaseComponent` that positions widgets
(`OVT_MapPlayerLocation.c:1`, no `SetDrawCommands` anywhere in it). It is untouched by any of this, and
`map/map-layers` must know its toggle is a different mechanism.

### 3.2 Data flow, one map session

```
OnMapOpen
  ├─ compositor.Register(this, m_iDrawOrder)              every layer, base class
  ├─ CollectSites()          reads m_Towns / m_Bases / m_RadioTowers / m_FOBs via OVT_Global
  │                          → OVT_TerritorySite { pos, weight, factionIndex, holdStrength }
  ├─ solver.Solve(sites)     ONCE. For each site, for each of N rays:
  │                            march r by step until  rival wins | not land | r > maxRadius
  │                            bisect-refine the last step, record WHY it stopped
  │                            smooth radii (circular, shrink-only)
  │                          → OVT_TerritoryCell { centre, radii[N], stopReason[N], colour }
  └─ m_iSiteSetHash = HashSites(sites)

Update (every frame)                                     ← MUST stay cheap
  ├─ Draw()
  │    ├─ CacheProjection()   3 × WorldToScreen → affine basis (origin + 2 axis vectors)
  │    └─ for each cell: emit fill (+ neutral band) using the basis, 2 mul-adds per vertex
  └─ compositor.SubmitAndFlush(this, m_Canvas)

Colour tick (every m_fRefreshInterval, default 5 s — matches the marker refresh)
  ├─ re-read faction + stability per site      O(sites), no geometry
  └─ if HashSites(sites) != m_iSiteSetHash → full re-solve   ← this is what makes JIP self-heal

OnMapClose
  ├─ compositor.Unregister(this)
  └─ null the caches (the OVT_MapRestrictedAreas.OnMapClose discipline, :84-96)
```

### 3.3 The compositor, in one paragraph

`OVT_MapCanvasCompositor` is a client-only scripted singleton holding an ordered list of registered layers and
**one** shared `array<ref CanvasWidgetCommand>`. Each layer keeps its existing private `m_Commands` as its own
**bucket**, cleared and refilled by its own `Draw()` exactly as today. The base `Update` then calls
`SubmitAndFlush`, which stamps the bucket with the current frame token, rebuilds the shared list by
concatenating every bucket whose stamp is current **in `m_iDrawOrder` order**, and calls `SetDrawCommands` once.
Flushing on *every* submit rather than trying to detect the last submitter is deliberate: it is correct without
knowing which layer runs last, it costs one concatenation per layer per frame (three layers × a few hundred
commands is nothing next to the draw itself), and a layer that stops submitting — because it was hidden or
deactivated — simply drops out on the next frame instead of freezing the canvas. See §6 K2 for the rejected
alternatives.

### 3.4 Where the boundary is

**The overlay is a read-only projection.** It reads `m_Towns` (`OVT_TownManagerComponent.c:126`), `m_Bases` /
`m_RadioTowers` (`OVT_OccupyingFactionManager.c:149-150`) and `m_FOBs`
(`OVT_ResistanceFactionManager.c:89`) client-side, from state that is already replicated and already drawn as
markers. It adds **no** `[RplProp]`, **no** RPC, **no** EPF save data, and writes nothing back. Everything it
computes is a rendering artefact that dies with the map close. This is the single most important boundary in
the feature and it has its own DoD criterion (I-4).

---

## 4. Settled Planning Decisions

**Settled with the user on 2026-08-11.** Each item below overrides or waives specific text in
`requirements.md` and/or `epic-requirements.md`. Those files are deliberately left unedited; **this section is
the authority.** A future reader must not be able to "correct" the plan back to the stale text.

---

### D1 — Sites are weighted, and include FOBs and radio towers

**Decision.** Territory sites are **towns + military bases + radio towers + resistance FOBs**. Each location
type carries a **config weight** that drives how far its cell projects, ordered **bases > towns > FOBs** per the
`requirements.md` Requirements bullet. The weight feeds a **multiplicatively-weighted (Apollonius) Voronoi**
test — a point belongs to the site minimising `dist(q, S) / w(S)`.

**Overrides — dated 2026-08-11:**

- `requirements.md` Out of Scope: *"**Weighted / multiplicatively-weighted Voronoi.** Cell extent is
  distance-based with a uniform influence radius; making strongly-held cities project further than contested
  villages was considered and deferred as materially harder to compute and smooth."*
- `requirements.md` Out of Scope: *"**FOBs and radio towers as territory sites** — sites are towns and bases
  only."*
- `epic-requirements.md:68` Out of Scope: *"…and weighted influence — `map/territory-overlay` clips cells to a
  uniform influence radius; …or scaling reach by town strength is deferred."*

**Rationale.** The deferral's stated reason — "materially harder to compute and smooth" — was true of the
half-plane-clipped Voronoi approach it was written against, where multiplicative weighting turns straight
bisectors into Apollonius circles and every clip becomes circle-circle geometry. Under the chosen ray-march
(§3.2) weighting is **a different comparison inside the existing loop** and nothing else changes: the cell is
still star-shaped, the smoothing is still 1-D, the clip is still a scalar. It is free, so the reason to defer
is gone.

**Consequence to honour.** Towns already have a natural per-site strength: `OVT_TownSize`
(`OVT_TownManagerComponent.c:11`, VILLAGE/TOWN/CITY/CAPITAL) with configured ranges
`m_iVillageRange 250` / `m_iTownRange 400` / `m_iCityRange 600` (`:100-106`) surfaced by
`GetTownRange(town)` (`:799`). Site weight is therefore `typeWeight × sizeFactor`, with `sizeFactor` = 1.0 for
every non-town type. This is one multiply, and it is what makes a capital out-project a village without any
new config surface.

---

### D2 — Coastline clipping is IN, by sampled land test

**Decision.** Each ray stops when it leaves land. The test is `BaseWorld.GetSurfaceY(x, z)` vs
`BaseWorld.GetOceanHeight(x, z)` — both `proto external` on `BaseWorld`
(`ArmaReforger/scripts/Core/generated/World/BaseWorld.c:24` and `:126`), verified. This is a **sampled
shoreline, not a polygon coastline**: its accuracy equals the march step, sharpened by the bisection refine
(§6 K4).

**Overrides — dated 2026-08-11:** `epic-requirements.md:68` Out of Scope: *"**Coastline-accurate territory
borders** … following the shoreline … is deferred."*

**Rationale.** It costs two proto calls per march step inside a loop that already exists, and the early-out
means most rays stop long before their maximum radius. `requirements.md` asked to "investigate if the base-game
provides any tooling for this" — it does, and this is the answer.

**Why not `ChimeraWorldUtils.TryGetWaterSurfaceSimple`** (`ArmaReforger/scripts/Game/generated/ChimeraWorldUtils.c:14`,
used by `SCR_CampaignFastTravelComponent` to reject water destinations): it returns true for **ponds and
rivers** as well as ocean (`EWaterSurfaceType.WST_OCEAN` / `WST_POND` / `WST_RIVER`,
`ArmaReforger/scripts/Game/generated/EWaterSurfaceType.c:9-13`). This feature only cares about coastline; a
river must not chop a town's territory in half and an inland lake must not punch a hole in it. The
surface-vs-ocean comparison ignores both **by construction** rather than by filtering. Cheap guard:
`BaseWorld.IsOcean()` — on a world with no ocean, skip the land test entirely.

---

### D3 — `OVT_MapThreatGrid` stays disabled: **DEFERRED by the user**

**Decision.** The user was asked to settle the threat grid's fate on **2026-08-11** and **explicitly chose to
defer**. `OVT_MapThreatGrid` is **not deleted and not revived**. It stays registered-but-disabled in
`Configs/Map/MapFullscreen.conf` (`m_bDisableModule 1`, `m_iGridSize 250`).

**This is a deliberate waiver, recorded rather than dropped.** `requirements.md` states: *"**Decide**
`OVT_MapThreatGrid`**'s fate** in this feature: revive it as a toggleable sibling overlay sharing this
machinery, or delete it and its config block."* That requirement is **waived by the user on 2026-08-11**, and
**leaving written-but-disabled code in the tree is now a known, accepted piece of tech debt** — which is
precisely the outcome the requirement was written to avoid. It should be carried in the epic's Tech Debt
section, not silently forgotten.

> 🔴 **SUPERSEDED 2026-08-11 — the two speculated reasons below are BOTH WRONG.** The user, who wrote the
> layer, states plainly: *"the threat layer was actually just written as a debug layer during the development
> of the deployment systems. it wasn't really intended to be shipped and that's why it was disabled."*
>
> That also **reclassifies the item**. It is not "written-but-disabled code left in the tree indefinitely" —
> the outcome `requirements.md` was written to prevent — it is a **debug tool behind a disable flag**, which
> is an ordinary and unobjectionable thing for a codebase to contain. The epic's Tech Debt entry is corrected
> accordingly.
>
> The user's forward view, recorded but **not scheduled**: *"if we updated it and made it performant it could
> be added as a switchable layer turned off by default. but that's something to consider later anyway, just
> leave it as is for the moment until this is finished."* That is a **feature**, not debt repayment.
>
> The original speculation is struck through rather than deleted, because a recorded guess that outlives the
> truth is exactly what misleads the next reader — and this one did, for the length of this feature.

~~**What the deferral costs, and what it buys.** After Phase 1 the grid becomes *cheap to revive*: it is a
one-line `.conf` edit (`m_bDisableModule 0`) plus an `m_iDrawOrder`, because the compositor removes the very
reason it was probably disabled. This plan records the most likely reason so a future decision is not made
blind: at `m_iGridSize 250` over a large world the grid emits **on the order of 2,300 `DrawRectangle`
quads per frame** (`OVT_MapThreatGrid.c:21-24`, one command per non-zero cell, rebuilt every frame), and —
independently — it would have been **mutually exclusive with the restriction rings** under the shared-canvas
defect. Either alone is sufficient to explain "written, then switched off". Neither is proven; both are
testable in ten minutes once the compositor exists.~~

**Also named, and still not this feature's job to fix:** the pre-existing **orphaned `.meta`** for the retained
`OVT_MapThreatGrid`, GUID `{B8F4C6A8C9D3E4F1}`, introduced in commit `96e6da4d` and recorded in
`docs/features/map/epic-overview.md` as a known non-blocker from `legacy-retirement`. This feature does not
touch it; it is named here so the next person to open the file knows it is known.

---

### D4 — Restricted rings and neutral bands both use **textured** fill, with **different** textures

**Decision.** `requirements.md` asks for a way to stop restricted-area drawing conflicting with territory
("different colour, or hatch shading etc") and separately asks for territory border zones "rendered with
hatching or similar to denote neutral territory". Both are solved the same way and the user chose the
mechanism directly:

> *"Ive seen it used in other mods so it should work, using a texture is likely the best option and that allows
> us to use different textures for restricted zones vs neutral areas."*

So: `PolygonDrawCommand.m_pTexture` + `m_fUVScale` (`EnWidgets.c:105-106`), with the texture loaded via
`CanvasWidget.LoadTexture(ResourceName)` → `ref SharedItemRef`
(`ArmaReforger/scripts/Core/generated/UI/CanvasWidget.c:26`). The working precedent for the *loading* half is
`SCR_MapSelectionModule.RenderSelectionCircle`, which loads once, guards with `IsValid()` and only then assigns
to a draw command — **copy that guard pattern verbatim**; it is the difference between a missing texture being
a flat polygon and being a crash.

**Two new art assets are a hard external dependency on the user** (Workbench import): one hatch/bar texture for
restricted zones, and a visually distinct one for neutral border bands. See §10 for the art brief.

**Plan around not having them on day one.** `m_bUseTextures` defaults **0** until the art lands; with it off,
fills are flat-coloured and bands are a flat darker/lighter alpha. Every phase before Phase 5's art step is
testable without a single new texture, and the feature is shippable-but-plainer if the art slips.

**Retained constraint:** `OVT_MapRestrictedAreas`' ring **geometry is untouched** — same centres, same radii,
same `baseCloseRange + FOB_DEPLOY_BASE_BUFFER` / `FOB_DEPLOY_TOWER_RANGE` sources (`:57`, `:76`, `:80`). Only
*how* the circle is filled and *when* it is composited changes. BUG-070 must not regress and has its own DoD
criterion (I-3).

---

### D5 — Always on: no in-feature toggle

**Decision.** The overlay is always on when the fullscreen map is open. This feature ships **no** player-facing
toggle.

**Waives — dated 2026-08-11:** `requirements.md`: *"The overlay must be **toggleable** rather than always-on
(see `map/map-layers`), and must degrade gracefully if toggled off before it has finished computing."* Both
halves are waived: there is no toggle, so there is no mid-compute toggle-off to degrade from. (The solve is
synchronous inside `OnMapOpen`, so there is no window in which a half-computed overlay is observable at all —
unless the Phase 6 incremental fallback is taken, in which case partial cells simply are not emitted.)

**The free escape hatches, in increasing order of effort:**

1. **`m_bDisableModule`** — `SCR_MapModuleBase` already declares it
   (`ArmaReforger/scripts/Game/Map/Modules/SCR_MapModuleBase.c:6`) and `SCR_MapEntity.ActivateModules` honours
   it at load (`SCR_MapEntity.c:1235-1236`). One line in `Configs/Map/MapFullscreen.conf` switches the whole
   layer off with **zero code**. This is exactly how `OVT_MapThreatGrid` is off today.
2. **`SetLayerVisible(false)`** — the runtime primitive Phase 1 adds (§6 K1). No player UI drives it yet.
3. **`map/map-layers` (feature 7)** adds the real player-facing toggle, built on (2). That is the recorded
   build order (`epic-overview.md:52`), and it is why shipping a throwaway toggle here would be waste.

---

## 5. Implementation Phases

Effort is **S / M / L** relative to a single focused session. "Agent" is the routing hint for `/proceed`.
**Phases 1 and 6 need the advanced variants** — Phase 1 because it edits the base class every canvas layer
inherits and answers a downstream feature's blocking question, Phase 6 because the fallback path it may have to
take restructures the solve.

---

### Phase 0 — Baseline — **S — no agent (already measured)**

Measured on `new-map` at `11a73ba3` + working tree, **2026-08-11**. These numbers were **run, not quoted.**

| Gate | Baseline |
|---|---|
| `tools/compile-check.sh` | **exit 0, 5964 files, Game module, 5 s** |
| `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) | **OK, 54 tests, 15 s** |
| `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) | **OK, 89 tests, 19 s** |
| Highest allocated bug id | **BUG-144** (BUG-138…144 exist as untracked files) |
| Free GUID series | **`{6A84…}`** — `{6A83…}` has 12 uses, `{6A84}`/`{6A85}`/`{6A86}` have zero |

⚠️ **`CLAUDE.md` says Fast 38 / All 66 and is stale — do not quote it.** A *changed* test count at any phase
boundary is a finding to investigate, never a number to update. Expected end state: **Fast 54 + N**, **All
89 + N**, where N is the number of new Logic cases (Phase 3).

Re-check `git status` and the highest `docs/bugs/` id at every phase boundary — parallel sessions commit to this
tree mid-feature and have done so throughout this epic.

---

### Phase 1 — 🔴 The shared-canvas compositor — **M — `component-developer-advanced`**

> **Advanced, and first, because it is a real defect on the base class every canvas layer inherits, and because
> `map/map-layers` is blocked on the API it defines.** Nothing else in this feature can be evaluated visually
> until two layers can draw at the same time.

**Tasks**

1. Create `Scripts/Game/UI/Map/Core/OVT_MapCanvasCompositor.c` — a client-only scripted singleton:
   - `Register(OVT_MapCanvasLayer layer)` / `Unregister(layer)`, keeping the list sorted by `m_iDrawOrder`
     ascending (lower = drawn first = underneath).
   - `SubmitAndFlush(OVT_MapCanvasLayer layer, CanvasWidget canvas)` — stamp the submitting layer's bucket with
     the current frame token, rebuild the shared list from every current-stamped bucket in draw order, call
     `SetDrawCommands` **once**.
   - Frame token: `GetGame().GetWorld().GetWorldTime()`, which is identical for every module within one frame.
   - `GetLayers()` — the enumeration `map/map-layers` will build its rows from.
2. Extend `OVT_MapCanvasLayer` (`Scripts/Game/UI/Map/Core/OVT_MapCanvasLayer.c`) **additively**, following
   `map/respawn`'s discipline — safe defaults so `OVT_MapRestrictedAreas` and `OVT_MapThreatGrid` behave
   identically without config changes:
   - `[Attribute] int m_iDrawOrder` (default 100), `[Attribute] string m_sLayerId`,
     `[Attribute] string m_sDisplayName` (localization key, unused until feature 7).
   - `bool m_bVisible` (runtime, default true) + `SetLayerVisible(bool)` / `IsLayerVisible()`. When false,
     `Update` clears the bucket, submits it empty and returns **without** calling `Draw()`.
   - `Update` becomes: `if (m_bVisible) Draw(); compositor.SubmitAndFlush(this, m_Canvas);` — **the submit is
     unconditional and happens even on the early-return paths**, or a layer that bails silently freezes the
     composite.
   - `OnMapOpen` registers; `OnMapClose` unregisters; `override SetActive(bool, bool)` unregisters on
     deactivation (see §6 K1 for why `SetActive` is one-way and therefore *not* the toggle primitive).
3. **Delete the `if(m_Commands.Count() > 0)` guard** (`:15`) and flush unconditionally. That guard is a second,
   latent defect: a layer that clears its list to empty **never repaints**, so its last non-empty frame stays
   on the canvas forever. If an empty array misbehaves at the engine boundary, emit one degenerate zero-alpha
   command instead — decide by observation in Phase 2, not by guessing.
4. Add `[Attribute] int m_iDrawOrder 200` to the `OVT_MapRestrictedAreas` entry in
   `Configs/Map/MapFullscreen.conf` so rings composite **above** territory (100). Give both layers an
   `m_sLayerId` (`"restricted"`, and `"territory"` in Phase 4).
5. Add the projection helpers to `OVT_MapCanvasLayer` (used by Phase 4, probed in Phase 2):
   `CacheProjection()` derives an affine basis from **three** `WorldToScreen` calls (world origin and two
   widely separated axis points), and `ProjectWorld(float wx, float wz, out int sx, out int sy)` applies it.
   **Leave `DrawCircle`/`DrawRectangle`/`DrawImage` alone** — `OVT_MapRestrictedAreas` geometry must not move.
6. Add the optional texture parameters to `DrawCircle` with **null defaults** so today's call sites are
   byte-identical in behaviour: `DrawCircle(center, range, color, n = 36, SharedItemRef tex = null,
   float uvScale = 0)`.

**Acceptance**

- `tools/compile-check.sh` → exit **0**, file count **5964 + 1**.
- Fast **54**, All **89** — unchanged; this phase adds no assertable logic.
- **The visual proof, and the only one that counts:** temporarily set `m_bDisableModule 0` on
  `OVT_MapThreatGrid`, open the map, and confirm **both** the threat cells **and** the FOB restriction rings
  render at once, with rings on top. Then set it back to `1` (D3 — the grid stays disabled) and record the
  observation, including whether the ~2,300-quad grid visibly costs frame time.
- With `SetLayerVisible(false)` called on the restricted-areas layer from a temporary debug hook, the rings
  disappear **and the canvas keeps updating** (i.e. the stale-command defect from task 3 is gone).

---

### Phase 2 — Render probe: settle the primitive ladder and the projection — **M — `ui-developer`, then user-run**

> **A throwaway probe, deliberately built before the feature depends on any of it.** Three of the four things
> this feature wants from the canvas have **zero usages anywhere in vanilla**: `PolygonDrawCommand.m_pTexture` /
> `m_fUVScale`, `TriMeshDrawCommand` in its entirety, and non-convex `PolygonDrawCommand` fill. The user is
> confident textured polygons work; that is a good prior and a bad foundation. One Workbench session removes
> the guesswork.

**Tasks**

1. Create a temporary `OVT_MapProbeLayer : OVT_MapCanvasLayer` (registered in `MapFullscreen.conf` with
   `m_bDisableModule 0` for the probe, **deleted in Phase 8**) that draws fixed shapes at fixed world
   coordinates near the campaign start position.
2. Run the four probes below and record each result verbatim in `context.md`.

**The probes, with unambiguous pass/fail signatures**

| # | Probe | PASS | FAIL signatures |
|---|---|---|---|
| **P1** | **Textured polygon.** One convex hexagon and one star, both with `m_pTexture` + `m_fUVScale` set from a loaded `SharedItemRef`. | The repeating pattern is visible inside the fill and tiles at the UV scale. | **F1a — texture ignored:** renders as a flat `m_iColor` fill. ⇒ drop to flat-fill differentiation (D4's fallback); hatching is off the table. **F1b — command dropped:** nothing renders at all. ⇒ same fallback, and `m_pTexture` must never be set. |
| **P2** | **Non-convex fill.** A 12-vertex star, radii alternating 200 m / 500 m, as a single `PolygonDrawCommand`. | A clean star with sharp notches. | **The signature to recognise, not debug blind: a naive fan from vertex 0.** The notches are filled in and the shape reads as a lopsided pinwheel — wedges spanning across the concave gaps, with the artefact hinging on **one** vertex. If you see a shape that looks "filled from one corner", that is this, not a bug in the vertex order. ⇒ rung 2 unavailable. |
| **P3** | **`TriMeshDrawCommand`.** The same star as an explicit centre fan: `m_Vertices` = centre + ring, `m_Indices` = `[0,1,2, 0,2,3, … 0,N,1]`. | A clean star, identical to P2's PASS. | Nothing renders ⇒ the type is declared but unimplemented; rung 1 unavailable. Partial/garbled ⇒ index convention differs (try reversed winding once, then abandon). |
| **P4** | **Affine projection.** Derive the basis from 3 `WorldToScreen` calls, then compare `ProjectWorld(p)` against direct `WorldToScreen(p)` for 8 scattered world points × 3 zoom levels × 2 pan positions. `Print` the max pixel error. | Max error ≤ **2 px** at every sample. | Any larger error ⇒ `WorldToScreen` is not affine (or carries rotation the 3-point basis misses); fall back to per-vertex `WorldToScreen` and re-measure Phase 6's frame cost with ~1,900 calls instead of 3. |

> **P4 also settles a question the existing code cannot answer.** `DrawCircle` (`:30-40`) offsets by
> `+r·sin(θ)` and `DrawRectangle` (`:67-68`) projects both corners — a circle is symmetric and a rectangle uses
> two real projections, so **neither reveals the sign of the world-Z → screen-Y mapping.** A polygon whose
> vertices are offset from a projected centre will be **mirrored vertically** if that sign is assumed wrong,
> and a mirrored star-shaped cell looks like a plausible-but-wrong territory shape rather than an obvious bug.
> Deriving the basis empirically removes the question instead of answering it.

**Acceptance**

- All four probes have a recorded PASS/FAIL with the observed signature written down.
- **The rung is chosen and recorded** (see §6 K3 for the ladder and the consequences of each rung).
- compile exit **0**; Fast **54**, All **89**.

---

### Phase 3 — `OVT_TerritorySolver` + the Logic tests — **M — `component-developer`**

> The whole reason the solver is a separate class is that this is the **only** part of the feature any
> automated gate can see. Keep it ruthlessly pure.

**Tasks**

1. Create `Scripts/Game/UI/Map/Territory/OVT_TerritorySite.c` — `Managed` record:
   `vector m_vPos`, `float m_fWeight`, `float m_fMaxRadius`, `int m_iFactionIndex`, `float m_fHoldStrength`
   (0..1), `string m_sTypeId`.
2. Create `OVT_TerritoryCell.c` — `Managed` record: `vector m_vCentre`, `ref array<float> m_aRadii`,
   `ref array<int> m_aStopReason`, `int m_iColour`, `int m_iAlpha`.
   `enum OVT_TerritoryStop { RIVAL, COAST, MAX_RADIUS }` — the stop reason is free from the march and is what
   the neutral band is drawn from (§6 K7).
3. Create `OVT_TerritorySolver.c`, a plain `Managed` class with **no manager lookup and no `GetGame()` in its
   constructor**. The world is handed in by `SetWorld(BaseWorld)`; when it is null, `IsLandAt` returns true
   (everything is land), which is what lets the Logic tier instantiate it directly.
   - `bool IsLandAt(float x, float z)` — **virtual**, `GetSurfaceY(x,z) > GetOceanHeight(x,z) + m_fShorelineMargin`.
     Virtual so a test can stub a synthetic coast without a world.
   - `static bool IsLand(float surfaceY, float oceanY, float margin)` — the pure comparison.
   - `static int OwnsPoint(vector q, array<ref OVT_TerritorySite> sites, array<int> candidates)` — returns the
     index minimising `dist / weight`.
   - `static float SolveRay(...)` — march + bisection refine, returning radius and stop reason.
   - `static void SmoothRadii(array<float> radii, int passes, array<float> rawRadii)` — circular window-3
     moving average, **clamped to never exceed the raw marched radius** (§6 K5).
   - `void Solve(array<ref OVT_TerritorySite> sites, out array<ref OVT_TerritoryCell> cells)` — the driver,
     including the per-site **candidate rival list** (sites within `2 × maxRadius`; §6 K4).
4. Create `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Territory.c` and register it in
   `OVT_TEST_LogicSuite.c`. See §9 for the case list.
   ⚠️ **The Logic tier rule is enforced by a reviewer grep over the whole directory that does not distinguish
   code from prose** — the names of Overthrow's static manager accessor and the engine's game-mode getter must
   not appear anywhere in the file, **including comments**. Phase 4 of the test-coverage feature tripped exactly
   this by quoting its own rule.

**Acceptance**

- compile exit **0**, file count 5964 + new `.c` files.
- Fast **54 + N**, All **89 + N**, where N is the new case count.
- **Every new case proven able to fail**, with the inversion that made it fail recorded in `context.md`.
- No `OVT_Global`, no `GetGame()`, no `BaseWorld` reference under `TestSuites/Logic/`.

---

### Phase 4 — `OVT_MapTerritoryLayer`: collect, solve, cache, emit — **M — `component-developer`**

**Tasks**

1. Create `Scripts/Game/UI/Map/Territory/OVT_MapTerritoryLayer.c : OVT_MapCanvasLayer` with
   `[BaseContainerProps()]`, `m_iDrawOrder 100`, `m_sLayerId "territory"`.
2. Config surface — all `[Attribute]`s on the layer, set in `Configs/Map/MapFullscreen.conf`
   (the epic's "config-driven, not code-driven" rule):
   - `ref array<ref OVT_TerritorySiteConfig> m_aSiteTypes` — a small `[BaseContainerProps()]` record with
     `m_sId` (`"town"` / `"base"` / `"radiotower"` / `"fob"`), `m_fWeight`, `m_fMaxRadius` (0 ⇒ derive from
     `m_fWeight × m_fRadiusPerWeight`), `m_bEnabled`. **Adding a site type is a config entry plus one collector
     method — never a change to the solver or the emitter.**
   - Geometry: `m_iRayCount` (48), `m_fMarchStep` (50 m), `m_iRefineSteps` (4), `m_iSmoothPasses` (2),
     `m_fRadiusPerWeight`, `m_bClipToCoast` (1), `m_fShorelineMargin` (0.5 m).
   - Appearance: `m_iFillAlphaMin` / `m_iFillAlphaMax`, `m_fBandFraction` (0.15), `m_iBandAlpha`,
     `m_bUseTextures` (**0** until the art lands), the two `ResourceName`s, `m_fUVScaleFill` / `m_fUVScaleBand`.
   - Behaviour: `m_ePrimitive` (the Phase 2 rung), `m_fRefreshInterval` (5 s), `m_bDebugTiming` (0).
3. `CollectSites()` — one method per source, each reading through `OVT_Global` **at collect time**
   (§6 K10 settles the T1 idiom):
   - Towns: `OVT_Global.GetTowns().m_Towns` → `factionIndex = town.faction`,
     `holdStrength = town.stability / 100`, `sizeFactor` from `OVT_TownSize`.
   - Bases: `OVT_Global.GetOccupyingFaction().m_Bases` → `factionIndex = base.faction`, hold 1.0.
   - Radio towers: `.m_RadioTowers` → `factionIndex = tower.faction`, hold 1.0.
   - FOBs: `OVT_Global.GetResistanceFaction().m_FOBs` → **`OVT_FOBData` has no faction field**, so
     `factionIndex = OVT_Global.GetConfig().GetPlayerFactionIndex()`, hold 1.0.
   - **Camps are deliberately excluded.** They are per-player and can be private
     (`OVT_CampData.isPrivate`); drawing territory around one would leak a private position to every client —
     the exact defect class `map/location-types` fixed as N1. Record the reason in the code comment.
   - Every site funnels through **one** `AddSite(...)` and one `GetSiteFactionIndex(...)`; §6 K12 explains why.
4. `OnMapOpen` → collect → `solver.SetWorld(GetGame().GetWorld())` → `Solve` → cache cells →
   `m_iSiteSetHash = HashSites()`. `OnMapClose` → null every array, following
   `OVT_MapRestrictedAreas.OnMapClose` (`:84-96`).
5. `Draw()` → `CacheProjection()` once, then per cell emit the fill through **one** `EmitCell()` switch on
   `m_ePrimitive` with three ~20-line bodies (§6 K3). Cell geometry is identical for all three rungs; only the
   command packing differs.
6. Colour/refresh tick (§6 K9): re-read faction + hold strength every `m_fRefreshInterval`; re-solve **only**
   when `HashSites()` differs from `m_iSiteSetHash`.

**Acceptance**

- compile exit **0**; Fast **54 + N**, All **89 + N** (unchanged from Phase 3 — this phase adds no assertable
  logic).
- Opening the map in a started campaign shows faction-coloured regions around towns, bases, towers and FOBs.
- Restriction rings still render, **on top**.
- Cells stop at the shoreline; no cell extends into open sea.
- `Print` of the solve confirms every site produced a cell and none produced a zero-area one.

---

### Phase 5 — Legibility: contested shading, neutral bands, textures — **M — `ui-developer`**

**Tasks**

1. **Colour agreement.** Extract the identical four-line body from `OVT_MapLocationTown.GetIconColor:137`,
   `OVT_MapLocationBase.c:114-129` and `OVT_MapLocationRadioTower.c:83` into one shared helper, and have both
   the markers and the territory layer call it. This is a **refactor to a single source, not a behaviour
   change** — verify by diffing rendered marker colours before and after (§6 K6).
2. **Contested vs firmly held.** Fill alpha = `lerp(m_iFillAlphaMin, m_iFillAlphaMax, holdStrength)`. Towns use
   `stability / 100`; bases, towers and FOBs are binary-held and use 1.0. A contested town's territory is
   visibly fainter than a stable one's.
3. **Neutral border bands.** For each contiguous angular span whose rays stopped on `RIVAL`, emit a band
   between `radii[i] × (1 - m_fBandFraction)` and `radii[i]`. Spans that stopped on `COAST` or `MAX_RADIUS`
   get **no** band — a coastline is not a frontier and neither is the edge of a site's reach. Under rungs 1
   and 3 the band is a quad strip; each quad is convex, so the band is safe on **every** rung (§6 K7).
4. **Textures**, gated on `m_bUseTextures` and on the art actually existing. Load once, guard with
   `IsValid()`, copy `SCR_MapSelectionModule.RenderSelectionCircle`'s pattern exactly. Fall back to flat fill
   when the load fails — a missing texture must degrade, never crash.
5. **Restricted-ring restyle.** Pass the restricted-zone texture into `DrawCircle` from
   `OVT_MapRestrictedAreas.Draw`. ~~Colours and radii unchanged (`ARGB(50,255,0,0)` /
   `ARGB(40,0,120,255)`, `:21`/`:26`).~~ — **⚠️ THE COLOUR HALF IS OVERRIDDEN BY D11** (user decision,
   2026-08-11). The two hardcoded ARGBs are gone: each ring now takes its **owning faction's hue**
   through `OVT_MapLocationType.GetFactionColorByIndex`, keeping only the two alpha tiers (50 / 40, now
   attributes). A ring is meant to read as *"the same territory, denser"*, and that only works when the
   ring and the ground beneath it share a hue — over faction-coloured territory the hardcoded red and
   blue produced a muddy third colour instead. **The RADII half of this sentence still stands and is
   absolute** (DoD I-3 / BUG-070): centres and radii are byte-identical to what they were, verified by
   diff. See `context.md` **D11**.
6. Add two `.st` ids for the layer display names (`territory`, `restricted`) to
   `Language/localization_Overthrow.st` — the master only. Nothing renders them until feature 7;
   **never touch `localization_Overthrow.<lang>.conf`.**

**Acceptance**

- With `m_bUseTextures 0`: legible flat fills, visible contested/held difference, visible neutral bands.
- With `m_bUseTextures 1` and the art present: both textures tile without seams and are distinguishable from
  each other at three zoom levels.
- With `m_bUseTextures 1` and a **deliberately broken** `ResourceName`: flat fill, an ERROR in the log, no
  crash.
- Territory colour matches marker colour for the same town/base/tower — checked by eye against three sites of
  different factions, and by the shared helper having exactly one implementation.

---

### Phase 6 — Performance: measure, budget, tune — **M — `component-developer-advanced` if the fallback is needed, otherwise user-driven measurement**

> Advanced routing applies **only if the measurement misses budget**, because the fallback restructures the
> solve across frames. Measure first.

**Tasks**

1. Instrument behind `m_bDebugTiming`: `System.GetTickCount()` around `CollectSites` and `Solve`, printing
   `[OVT_Territory] solve: <sites> sites, <rays> rays, <ms> ms`; and a rolling 60-frame average of `Draw()`
   plus a total draw-command count.
2. **Measure on a fully-populated campaign** — all towns, all bases, all radio towers, and at least three FOBs
   — not an early-game one. Record the site count, the solve time and the per-frame emit cost in `context.md`.
3. **Budget** (these are the numbers the DoD checks against):
   - **Solve ≤ 250 ms**, once, inside the map-open transition.
   - **Emit ≤ 1.5 ms/frame** (~10 % of a 60 fps budget).
   - **Total composited draw commands ≤ 250/frame** including the restriction rings.
4. Tune `m_iRayCount`, `m_fMarchStep`, `m_iRefineSteps`, `m_iSmoothPasses` against the budget and record the
   shipped values with the measurement that justified each.
5. **Fallbacks, in order, only if a budget is missed:**
   - Solve too slow ⇒ raise `m_fMarchStep` (the refine keeps the boundary sharp), then shrink the candidate
     rival radius, then sample the land test every other step.
   - Still too slow ⇒ **budgeted incremental solve**: process K sites per frame from `OnMapOpen`, emitting only
     completed cells. The overlay fills in over ~0.5 s instead of appearing whole. This is the one change that
     alters the feature's shape, hence the advanced routing.
   - Too many commands (rung 3 only) ⇒ drop `m_iRayCount` **48 → 16** and `m_iSmoothPasses` → 0, accepting
     visibly polygonal cells; if that still misses, replace the neutral band with a single `LineDrawCommand`
     outline per cell. §6 K3 does the arithmetic.

**Acceptance**

- All three budget numbers measured and recorded, on a named save with a recorded site count.
- Shipped tunable values recorded with their justification.
- If any fallback was taken, the visual cost is described in one sentence.

---

### Phase 7 — Multiplayer, JIP and live control changes — **M — `component-developer`** (code) + user-driven gate

**Tasks**

1. Confirm every source the collector reads is genuinely replicated to clients and that a JIP client sees the
   same set: `m_Towns`, `m_Bases`, `m_RadioTowers`, `m_FOBs`. The markers already depend on this, so a
   disagreement is a **finding against the owning feature**, not something to work around here.
2. Verify the `HashSites` re-solve trigger fires when a base changes hands, a tower is captured, or a FOB is
   built/destroyed while the map is open — this is what makes a JIP client's initially-empty overlay heal
   rather than stay wrong for the whole map session (§6 K9).
3. Confirm no privacy leak: the four site types are all globally-visible campaign state already drawn as
   markers for every player; camps are excluded (Phase 4 task 3).
4. Run the two-client gate: `tools/launch-server.sh`, then two
   `tools/launch-game.sh --timeout 3600 --profile <name> --allow-concurrent -- -client 127.0.0.1:2001`
   clients. **Always pass the long timeout** — the default 600 s kills the client mid-test.
   ⚠️ **Warn the user before launching**: client launches open a window on their desktop and can orphan.

**Acceptance**

- Two clients open the map simultaneously and see identical territory.
- A JIP client that joins an established campaign sees the same territory as the established client, within
  one refresh interval of opening the map.
- Capturing a base with the map open changes that cell's colour within one refresh interval, on **both**
  clients.
- No new `[RplProp]`, no new RPC, no EPF save data (this is DoD I-4 and is checked by grep).

---

### Phase 8 — Docs, contract records, and the `map/map-layers` answer — **S — `component-developer`**, then **`help-docs-sync`**

**Tasks**

1. **Delete the Phase 2 probe layer** and its config entry. Grep-prove it is gone.
2. Write `docs/features/map/territory-overlay/context.md`: probe results verbatim, chosen rung, measured
   numbers, shipped tunables, and a "where to look when it doesn't work" triage section.
3. Add the canvas-layer contract rows to `docs/features/map/core/context.md` — `m_iDrawOrder`, `m_sLayerId`,
   `m_sDisplayName`, `m_bVisible`/`SetLayerVisible`, `CacheProjection`/`ProjectWorld`, the compositor
   registration lifecycle, and the removal of the `Count() > 0` guard. That file is the epic's record of what
   the base contract is and **it is never changed silently.**
4. **Answer the open question in `docs/features/map/map-layers/requirements.md`** — replace "The scope question
   left open" with §6 K1's answer (generic registration), including the two caveats feature 7 must know:
   `SetActive(false)` is one-way from script, and `OVT_MapPlayerLocation` is not a canvas layer.
5. Update `docs/features/map/epic-overview.md`: feature 6 status, and add **D3's deferred threat grid** to the
   epic Tech Debt section as an accepted item with its date.
6. Run **`help-docs-sync`**. This is player-facing: the in-game help and the wiki must describe the overlay as
   **a display of existing control**, never as a new mechanic. ⚠️ Every sentence must be backed by a
   `file:line` or cut — two tips have shipped inventing mechanics before, and no gate catches a well-formed lie.

**Acceptance**

- Probe layer gone; compile exit **0** at the expected file count.
- `map-layers/requirements.md` no longer says the question is open.
- Help/wiki text contains no claim that territory affects the campaign.

---

### Phase 9 — Verification gate — **M — user-driven, no agent**

Run §8's Verification Method end to end. This is the only evidence that exists for the parts no automated gate
can see: rendering, colour, frame cost, the `.conf` edits, the textures, and multiplayer.

---

## 6. Key Technical Decisions

### K1 — `map/map-layers` gets a **generic registration API**, and the toggle primitive is **not** `SetActive`

**The question feature 7 left open** (`map-layers/requirements.md`, "Recorded planning decisions (2026-08-11)"):
whether overlay toggles are built against a generic registration API or hardcoded to the layers that exist.

**Answer: generic.** `map/map-layers` builds one row per entry in `OVT_MapCanvasCompositor.GetLayers()`, keyed
by `m_sLayerId`, labelled from `m_sDisplayName`, toggled by `SetLayerVisible(bool)`. Adding a canvas layer adds
a toggle with no code change in feature 7 — the same config-driven principle the location-type system already
follows, and the reason there is a registry at all is that the compositor needs one anyway. Hardcoding would
buy nothing and would have to be undone the first time a fourth layer appears.

**And the load-bearing caveat: the toggle must be `SetLayerVisible`, not `SetActive`.**
`SCR_MapModuleBase.SetActive(false)` calls `m_MapEntity.DeactivateModule(this)`
(`ArmaReforger/scripts/Game/Map/Modules/SCR_MapModuleBase.c`), which removes the module from
`m_aActiveModules` (`SCR_MapEntity.c:1338-1340`). There is **no script-reachable way to put it back**:
`ActivateModules` is `protected` (`:1223`) and `m_aActiveModules` is `protected` (`:62`). **`SetActive(false)`
is therefore one-way from script**, which makes it unusable as a toggle. `SetLayerVisible` keeps the module
registered and running, clears its bucket, and is instantly reversible — which is also exactly what feature 7's
"toggling must be cheap and immediate" requirement asks for.

**Second caveat for feature 7:** `OVT_MapPlayerLocation` is a `SCR_MapUIBaseComponent`, not an
`OVT_MapCanvasLayer` (`OVT_MapPlayerLocation.c:1`), so its toggle is a different mechanism and will not appear
in `GetLayers()`.

### K2 — Compositor design: flush on every submit, skip stale buckets

**Rejected — a second `CanvasWidget`.** Give each layer its own canvas and the problem disappears. But
`DrawingWidget` lives in vanilla's `UI/layouts/Map/Map.layout:43-44` and Overthrow does not own that layout;
adding a sibling canvas means overriding a vanilla layout to solve a problem a 60-line script class solves, and
the z-order would then be layout-determined rather than config-determined.

**Rejected — "the last layer to run flushes".** There is no reliable way to know which layer is last.
`m_aActiveModules` order follows config order (`SCR_MapEntity.c:1230-1261`), but the reactivation path matches
loaded modules by `IsInherited(module.Type())` (`:1249`), and any layer that early-returns breaks the
assumption silently.

**Rejected — "all layers submitted, then flush".** Exact, but it wedges: a layer that stops submitting (hidden,
deactivated, or bailing on a null canvas) means the condition is never met again and the canvas freezes on its
last complete frame. That failure mode — a *frozen* overlay — is far harder to diagnose than a slightly
redundant one.

**Chosen — stamp and flush every submit.** Each bucket carries the frame token it was filled at; the flush
concatenates only current-stamped buckets. The final submit of any frame therefore always produces the complete
list, no completion detection is needed, and a layer that drops out simply stops contributing. Cost is one
concatenation per layer per frame: with three layers and a few hundred commands, immaterial next to the draw.

**And it fixes a second latent defect.** `Update`'s `if(m_Commands.Count() > 0)` guard (`:15`) means a layer
that clears its list to empty never issues a `SetDrawCommands`, so **its last non-empty frame stays on the
canvas indefinitely**. That is invisible today because the only live layer never empties; it becomes visible
the moment anything is toggled off. The compositor flushes unconditionally.

### K3 — The render-primitive ladder, and why the geometry is rung-independent

The cell is **star-shaped about its own site by construction** — every vertex is `centre + r(θ)·(cos θ, sin θ)`
with `r ≥ 0`. A triangle fan from the centre is therefore *always* a valid triangulation, with no degenerate or
self-intersecting triangles. That is the structural point of the ray-march: it converts an unverified engine
question ("does `PolygonDrawCommand` fill non-convex polygons?") into a geometry guarantee.

| Rung | Primitive | Commands/cell | Correctness | Needs |
|---|---|---|---|---|
| **1** | `TriMeshDrawCommand` with an explicit centre fan | **1** | **Guaranteed by construction** — we supply the indices | P3 PASS |
| **2** | One star-shaped `PolygonDrawCommand` | **1** | Only if the engine triangulates properly | P2 PASS |
| **3** | N triangle `PolygonDrawCommand`s | **N (=48)** | **Provably correct** — each triangle is convex | Nothing |

**The arithmetic that makes rung 3 a real constraint.** At ~40 sites, rung 3 costs ~1,900 fill commands plus
~1,900 band quads ≈ **3,800 commands/frame** — worse than `OVT_MapThreatGrid`'s ~2,300, which is the most likely
reason that layer was switched off. **If rung 3 is reached, `m_iRayCount` drops 48 → 16 and `m_iSmoothPasses`
→ 0**, giving ~1,280 commands. The visual cost is real and should be stated plainly: cells become visibly
polygonal — sixteen-sided regions with straight edges instead of organic frontiers, which is a partial
retreat from the "organic rather than geometric" requirement. Rungs 1 and 2 cost ~80 commands and have no such
constraint.

**Implementation consequence:** `m_ePrimitive` is a config enum and `EmitCell()` is one switch over three short
bodies. The solve, the cache, the smoothing, the bands and the colours are identical on all three rungs, so the
probe result changes ~60 lines and nothing else.

### K4 — Ray-march parameters, and why the cost is bounded

> 🔴 **SUPERSEDED — do not price anything off this section.** **D6** removed the maximum influence radius,
> which falsified both bounds below: rays now run to the coastline rather than stopping early, and the
> candidate filter is *exhaustive* at an unlimited reach, so it excludes nobody. **D8** (Phase 6) replaced
> the model entirely — the rival boundary is now **solved in closed form** rather than sampled per step, so
> the `× steps × rivals` product this section is built on no longer exists. The bisection-refine argument
> in the last paragraph survives and is now a *shoreline* argument. See `context.md` § D8.

Naively, 40 sites × 48 rays × (2 km / 50 m) steps × 40 rival tests is ~3.4 M comparisons — too slow. Two
bounds fix it, and both are structural rather than tuned:

1. **Candidate rival lists.** Precompute, per site, the sites within `2 × maxRadius`. In a populated campaign
   that is ~5–8 rivals, not 40 — an 5× cut.
2. **Early stop is the common case.** A ray stops at the *first* of three conditions, and in a dense town
   cluster that is usually a few hundred metres. Average marched distance is far below `maxRadius`, so the
   realistic figure is ~40 × 48 × ~10 steps × ~8 rivals ≈ **150 k comparisons** and ~19 k land samples.

**Bisection refine instead of a fine step.** After the coarse march overshoots, 4 bisection steps between the
last two positions sharpen the boundary from 50 m to ~3 m for 4 extra evaluations per ray, rather than the 16×
cost of a 3 m step everywhere. This is what makes D2's "accuracy equals the march step" claim cheap.

`BaseWorld.IsOcean()` short-circuits the land test entirely on worlds with no ocean.

### K5 — Smoothing may only shrink, never grow

The circular window-3 moving average over `radii[]` is applied `m_iSmoothPasses` times, and each result is
**clamped to the raw marched radius**: `radii[i] = Min(smoothed[i], raw[i])`.

This is not a detail. An unclamped average of a radius next to a much longer neighbour can push a vertex
**past the coastline or into a rival's cell** — reintroducing, in the smoothing pass, exactly the two errors the
march exists to prevent. Shrink-only makes that impossible by construction, and yields a property that is
**trivially assertable in the Logic tier**: `∀i: smoothed[i] ≤ raw[i]`.

The cost is slight erosion — cells shrink a little with each pass. At window 3 and 1–2 passes it is a few
percent and invisible; it is also why `m_iSmoothPasses` is a config attribute rather than a constant, per the
requirement that smoothing resolution be tunable because it directly costs frame time.

⚠️ **Narrowed twice since, by D9 and D10** (see `context.md`). "Slight erosion… invisible" is the sentence
that turned out to be wrong: at a boundary two same-faction cells *share*, both retreating from it leaves an
unfilled sliver (**D9**), and at a coastline the marched radius is already the correct organic edge, so
filtering it can only pull the fill back from the sea (**D10**). Both kinds of ray now keep their raw radius.
**The clamp itself is untouched and still governs every ray that is still smoothed** — which after D10 means
hostile-`RIVAL` and `MAX_RADIUS` rays only.

### K6 — Colour agreement is a refactor, not a new palette

The task brief warned that the marker colour API keys off a config-declared `m_FactionType` rather than a
record's live controlling faction, and that reconciling the two would be work. **Measured against the tree, it
is nearly free**, and this correction materially shrinks the feature:

- `OVT_MapLocationType.GetIconColor` (`:437-452`) → `GetFactionColor(m_FactionType)` (`:455-481`) is the
  **base-class** path, used by FOB, Camp and GunDealer — all of which are unconditionally resistance-owned
  (`OverthrowMap.conf:48-49`, `:68-69`, `:129-130`).
- **Town, Base and RadioTower all override it** and resolve the live faction:
  `GetDataInt("faction", -1)` → `GetGame().GetFactionManager().GetFactionByIndex(...).GetFactionColor()`
  (`OVT_MapLocationTown.c:137`, `OVT_MapLocationBase.c:114-129`, `OVT_MapLocationRadioTower.c:83`).
  That `faction` int is written at populate time straight from `town.faction` / `base.faction` / `tower.faction`.

Territory therefore uses **the same field through the same expression**, and agreement is by construction rather
than by coincidence. Phase 5 task 1 extracts the three identical bodies into one helper so there is literally
one implementation — the requirement is "agrees", and one function is the only way to guarantee that as both
sides change.

`OVT_TownData.ControllingFactionData()` (`OVT_TownManagerComponent.c:51-54`) returns the same `Faction` object
and is the nicer API, but the markers do not use it; matching the markers matters more than using the tidier
call, so the shared helper takes a faction **index** — which is what all four site types can supply, including
FOBs via `GetPlayerFactionIndex()`.

### K7 — Neutral bands come free from the stop reason

Recording *why* each ray stopped (`RIVAL` / `COAST` / `MAX_RADIUS`) costs one int per ray and gives the
"neutral territory" band the requirements ask for without any extra geometry: the band is drawn only along
contiguous spans that stopped on `RIVAL` — a real frontier with another faction. A coastline is not a frontier
and neither is the outer edge of a site's reach, so those spans get no band. Without the stop reason this would
need a second pass re-testing every boundary vertex against every rival.

⚠️ **Superseded in part by D7** (see `context.md`). The paragraph above asserts a property the stop reason
cannot express: `RIVAL` means another **site** won the point, never that another **faction** did, so two
same-faction neighbours were banded down the middle of uncontested ground. The solver additionally records
**which site** stopped each ray, and the band draws only where that site's faction differs from the cell's.
Everything else here stands.

Each band segment is a **quad** between `r × (1 - m_fBandFraction)` and `r` at adjacent angles — convex, so the
band renders correctly on every rung of the ladder, including rung 3.

### K8 — Contested vs firmly held is alpha, driven by `stability`

`OVT_TownData.stability` (`OVT_TownManagerComponent.c:24`) is literally the campaign's measure of how firmly a
town is held, and it is already replicated and already shown on the town info panel. Fill alpha is
`lerp(m_iFillAlphaMin, m_iFillAlphaMax, stability/100)`, so a contested town's region is faint and a secure
one's is solid. Bases, towers and FOBs have no stability analogue and are binary-held, so they use 1.0.

Alpha rather than hue, deliberately: hue is spent on faction identity and a second hue axis would fight the
"agrees with marker colour" requirement. Alpha also directly serves "legible without being loud" — the least
certain regions are the least intrusive.

`SupportPercentage()` (`:39`) was considered and rejected as the driver: it measures popular support, not
control, and a resistance-supporting town still held by the occupier would read as resistance territory. That
would make the overlay a *second opinion* about who holds what, which is exactly the boundary §3.4 forbids.

⚠️ **Superseded by D10, and this paragraph is the reason it needed arguing** (see `context.md`). Fill alpha is
no longer driven by `stability` at all — that lerp is **deleted**, because a continuous per-site shade
fragmented a one-faction island into a patchwork (D9). What replaced it *is* support-driven, and the
rejection above does **not** apply to it: the region is still drawn in the **occupier's** colour because the
occupier controls it, so support does not recolour anything — it **marks** the region as contested. That is a
second axis over an accurate first one rather than a competing answer, which is the distinction this
paragraph never had cause to draw. The full reasoning is D10 in `context.md` and must not be lost.

### K9 — Solve once, recolour often, re-solve only when the site set changes

`requirements.md` demands "compute in world space once per map open, project per frame". Taken literally, a town
changing hands mid-session would not update — but the MP/JIP requirement demands the overlay "update as towns
and bases change hands". Both are satisfied by splitting the work along the axis it naturally splits on:

- **Geometry** depends on site positions and weights, which change only when a site is created or destroyed.
- **Colour and alpha** depend on `faction` and `stability`, which change constantly.

So: full solve at map open; **O(sites) recolour** every `m_fRefreshInterval` (default 5 s, matching the marker
refresh interval `map/core` shipped for Town/Base/RadioTower/FOB via BUG-136); full re-solve only when
`HashSites()` changes.

**This is also the JIP fix.** A client that opens the map before `m_Towns`/`m_Bases` have replicated gets an
empty or partial overlay. Without the hash check that stays wrong for the whole map session; with it, the
overlay heals within one refresh interval. It is the cheapest possible answer to the hardest requirement.

### K10 — Manager access: per-call `OVT_Global`, at collect time only (epic tech debt T1)

The epic carries three different manager-access idioms across its location types (T1: inherited cache vs
shadowing member vs per-call `OVT_Global`). **This feature picks per-call `OVT_Global` at collect time,
deliberately**, and does not cache manager references on the layer.

Reason: the layer touches managers in exactly two places — `CollectSites` at map open, and the slow refresh
tick. Caching buys nothing at that frequency, and it introduces the one failure mode that matters here: a
reference cached during a JIP window when a manager is not yet resolvable stays null for the whole map session,
producing an empty overlay with no error. A per-call lookup self-heals on the next tick, which is the same
property K9 relies on.

### K11 — Territory does **not** appear on the respawn map

`Configs/Map/MapRespawn.conf` carries exactly one module — `SCR_MapCursorModule` — and **no
`OVT_MapCanvasLayer` at all**, so adding territory there would be a deliberate act, not an omission. It stays
out, for three reasons: the respawn screen is a 4-type, ownership-filtered picker where faction shading adds
nothing to a list of places you are already entitled to spawn; a full solve would add a hitch to the
death → screen path, which `map/respawn` identifies as the epic's highest-consequence path; and the screen
cannot be dismissed, so anything that goes wrong there strands the player. Adding it later is a one-line conf
edit.

### K12 — One seam for the future intel epic, built but not used

The epic anticipates a fog-of-war/intel epic and warns that this feature's canvas machinery is what it will
build on, so nothing here may assume the map always shows everything. The concrete, YAGNI-compatible
consequence: **every site is added through one `AddSite(...)` and every colour is resolved through one
`GetSiteFactionIndex(site)`.** A future intel epic changes those two methods — returning `-1` for "control
unknown to this player" and skipping unknown sites — and touches nothing else. No unknown-state handling ships
now; there is simply exactly one place it would go, and this plan names it.

### K13 — Markers draw above the canvas by layout order — confirm once, do not assume

The requirement is that territory sits "beneath the location markers so it never obscures them". Within vanilla's
`MapWidget`, the sibling order is `ManualCamera` → **`DrawingWidget`** (`UI/layouts/Map/Map.layout:43-44`) →
`DrawingContainer` → **`UIIconsContainer`** (`:83-84`), and `UIIconsContainer` is the container Overthrow's
markers are created into (`SCR_MapUIElementContainer.c:3` supplies the name;
`OVT_OverthrowMapUI.c:311` creates into it). Later siblings render on top, so markers are already above the
canvas and no code is needed.

It is a **layout-order fact plus an engine convention**, not a guarantee, so DoD Q-3 confirms it by eye once
rather than assuming it. If it were ever false the fix would be a layout change Overthrow does not own — worth
knowing early, which is why it is a criterion rather than a footnote.

---

## 7. Quality Bar

This is a **visual/rendering feature with a hard frame-time budget**. Correctness is necessary and nowhere near
sufficient — a geometrically perfect overlay that is illegible, that disagrees with the markers, or that costs
8 ms a frame has failed.

**Legibility (the primary bar).**

- Terrain, roads, contour lines and place names stay readable **through** every fill, at three zoom levels.
- All location markers remain fully legible; no marker is obscured or tinted by the overlay.
- Faction regions are distinguishable from each other at a glance, without reading a legend (there isn't one
  yet).
- Contested regions are visibly fainter than firmly-held ones — the difference must be apparent **without a
  side-by-side comparison.**
- The two textures (once they exist) are distinguishable from each other and from flat fill, and tile without
  visible seams at the shipped UV scale.

**Colour agreement (a hard gate, not a nicety).**

- Territory colour is produced by **the same function** as the corresponding marker colour, not by a matching
  constant. One implementation, two call sites.
- Verified by eye on at least three sites of different factions, and by grep showing a single implementation.

**Measured cost (a hard gate).**

- Solve ≤ **250 ms**, emit ≤ **1.5 ms/frame**, ≤ **250** composited commands/frame — measured on a
  fully-populated campaign, with the site count recorded. An unmeasured feature fails this bar regardless of how
  it feels.

**Structural.**

- The overlay adds no replicated state, no RPC, no persistence, and no write to any campaign record.
- The canvas-layer contract extension is **additive with safe defaults** (`map/respawn`'s discipline): existing
  layers behave identically with no config change.
- No `file:line` pointers in new code comments (epic **K-9**: keep the rationale, name the symbol, drop the
  line number). `docs/` citations are exempt and expected.
- Every new Logic case is **proven able to fail** before shipping, with the method recorded. ❌ No
  `maxAttempts` — a test that needs retries is a bug in the test.

---

## 8. Definition of Done

Written so an evaluator with **no implementation context** can verify each item.

### Functional

**F-1 — Territory renders.** Open the fullscreen map in a started campaign. Coloured regions surround towns,
military bases, radio towers and resistance FOBs. Every one of those four types has at least one visible region.

**F-2 — Regions are faction-coloured and match the markers.** Pick three sites held by different factions. For
each, the region's colour matches that site's marker colour. A resistance-captured town's region changes to the
resistance colour, matching its marker.

**F-3 — Bases project further than towns, and towns further than FOBs.** With a base and a town of comparable
isolation, the base's region is visibly larger. A FOB's region is visibly smaller than a town's. Where a base
and a town compete, the boundary sits closer to the town — not at the midpoint.

**F-4 — Cities out-project villages.** A CAPITAL/CITY town's region is visibly larger than a VILLAGE's at
similar isolation.

**F-5 — Regions stop at the coast.** No region extends into open sea. Following any coastline, the fill ends at
the shore within roughly one march step. **Rivers and inland lakes do NOT cut regions** — a region spanning a
river is correct behaviour, not a bug.

**F-6 — Contested areas are visibly fainter.** A town with low stability has a visibly more transparent region
than a high-stability town. Confirm the stability values on both info panels first.

**F-7 — Neutral bands appear only on faction frontiers.** Where two differently-coloured regions meet, a
distinct border band is drawn along the boundary. Along a coastline edge or the outer edge of an isolated site's
reach there is **no** band.

**F-8 — Nothing obscures the markers.** At every zoom level, all location markers draw fully on top of the
overlay: icons, names and distances are unobscured and untinted.

**F-9 — Territory is legible without being loud.** Terrain, roads and place names remain readable through every
fill at three zoom levels.

### Quality

**Q-1 — Measured frame cost, recorded.** On a **fully-populated** campaign save (all towns, all bases, all
radio towers, ≥ 3 FOBs — record the site count), with `m_bDebugTiming 1`:
- solve time at map open ≤ **250 ms**, recorded in `context.md`;
- rolling 60-frame `Draw()` average ≤ **1.5 ms**, recorded;
- total composited draw commands ≤ **250/frame**, recorded.
An unrecorded number is a failure of this criterion even if the overlay feels fine.

**Q-2 — Opening the map does not visibly hitch.** Open and close the map ten times in a row on the populated
save. No stutter that a player would notice on any open.

**Q-3 — Marker-above-canvas is confirmed, not assumed.** Explicitly confirmed by eye with a marker sitting
inside a filled region (K13).

**Q-4 — Textures degrade, never crash.** With `m_bUseTextures 1` and a deliberately broken texture
`ResourceName`, the map opens, regions render as flat fills, and the log carries an ERROR. No crash, no blank
canvas.

**Q-5 — Every new Logic case is proven able to fail.** For each case, the inversion that turned it red is
recorded in `context.md`. `grep -rn` over `Scripts/Game/Tests/TestSuites/Logic/` finds no `maxAttempts` and no
mention of the manager accessor or the game-mode getter — **including in comments.**

**Q-6 — No `file:line` in new code comments** (epic K-9).

### Integration

**I-1 — 🔴 Territory and restriction rings render SIMULTANEOUSLY.** This is the Phase 1 defect and the single
most important integration criterion. With the map open near an occupied base: the base's red restriction circle
**and** the territory fill are **both** visible, with the ring drawn **on top of** the fill. Verified at three
zoom levels. *(If either is missing, the compositor is not working — and the symptom is easy to misread as a
broken territory layer.)*

**I-1b — Three layers compose.** Temporarily set `m_bDisableModule 0` on `OVT_MapThreatGrid` and confirm all
three layers render at once, in `m_iDrawOrder`. **Set it back to `1`** — D3 keeps it disabled.

**I-2 — Toggling a layer off leaves the rest drawing.** Via a temporary debug hook, `SetLayerVisible(false)` on
the restricted-areas layer: the rings vanish, territory keeps rendering **and keeps updating** as the map is
panned and zoomed. (A frozen overlay here means the stale-command guard was not removed.)

**I-3 — BUG-070 has not regressed.** The FOB-deploy restriction rings still draw at exactly the radii the deploy
check enforces. Verified by walking to a ring edge and confirming FOB deployment is refused just inside and
permitted just outside, for **both** a base ring and a radio-tower ring. `git diff` shows **no** change to the
radius sources in `OVT_MapRestrictedAreas.OnMapOpen`.

**I-4 — The overlay is a projection, not a mechanic.** All of the following hold:
- `git diff` shows no `[RplProp]`, no `[RplRpc]`, no `RpcAsk_`/`RpcDo_`, and no `EPF_` class added anywhere.
- `git diff` shows no write to any field of `OVT_TownData`, `OVT_BaseData`, `OVT_RadioTowerData` or
  `OVT_FOBData`.
- Nothing is added to `OVT_PlayerCommsComponent`.
- Every new script file lives under `Scripts/Game/UI/Map/` or `Scripts/Game/Tests/`.

**I-5 — Multiplayer agreement.** Two clients on a dedicated server (`tools/launch-server.sh` + two
`tools/launch-game.sh --timeout 3600 --profile <name> --allow-concurrent -- -client 127.0.0.1:2001`) open the
map at the same time and see **identical** territory: same regions, same colours, same boundaries.

**I-6 — JIP agreement.** A client joining a campaign with accumulated state opens the map and sees the **same**
territory as the established client — including regions for bases captured before it joined. If the first open
is empty or partial, it must correct itself within one refresh interval **without closing the map**.

**I-7 — Live control changes propagate.** With the map open on both clients, capture a base. Within one refresh
interval that region changes to the capturing faction's colour **on both clients**, and the base marker's colour
changes to match.

**I-8 — The respawn map is unchanged.** Die and open the respawn screen: no territory, no bands, no new fills.
Its four eligible-location markers behave exactly as before (K11).

**I-9 — `map/map-layers` is unblocked.** `docs/features/map/map-layers/requirements.md` no longer contains an
open scope question; it records the generic-registration answer plus the two caveats
(`SetActive` is one-way; `OVT_MapPlayerLocation` is not a canvas layer).

### Verification Method

Run in order. Stop and fix at the first failure.

**V-1 — Compile.** `tools/compile-check.sh` → exit **0**. File count = baseline **5964** + the number of new
`.c` files. Any other delta is a finding to investigate.

**V-2 — Automated tests against the measured baselines.**
- `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0**, **54 + N** tests (baseline **54**).
- `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0**, **89 + N** tests (baseline **89**).
- `N` = new Logic cases. **A count that changed for any other reason is a finding, never a number to update.**

**V-3 — 🔴 Workbench clean load (the manual gate nothing automated can replace).** Open the project in
Workbench and confirm zero load errors. **This is the only gate that can see the `.conf` edits, the new module
entry's GUID, the two `.edds` textures and their `.meta` files.** `tools/compile-check.sh` and both test groups
are blind to every one of them — a dangling GUID or an unimported texture passes every automated gate and fails
in the world. Specifically confirm:
- `Configs/Map/MapFullscreen.conf` loads with the new `OVT_MapTerritoryLayer` entry and its fresh
  `{6A84…}` GUID resolving.
- Both texture `ResourceName`s resolve (or `m_bUseTextures` is `0` and this step is deferred to V-6).
- `grep -rn "{6A84" .` shows each new GUID used exactly where intended and nowhere else.

**V-4 — Single-player visual pass.** Load a started campaign. Run **F-1 … F-9**, **Q-2**, **Q-3**, **I-1**,
**I-1b**, **I-2**, **I-8**.

**V-5 — Performance pass.** Load the **fully-populated** save. With `m_bDebugTiming 1`, run **Q-1**: record the
site count, solve ms, per-frame emit ms and command count. Then set `m_bDebugTiming 0`.

**V-6 — Texture pass** (after the art lands). Set `m_bUseTextures 1`. Confirm both textures tile without seams
at three zoom levels, are distinguishable from each other, and satisfy **F-7** and **Q-4**.

**V-7 — FOB restriction regression.** Run **I-3** in the world, both ring types.

**V-8 — Two-client MP + JIP.** ⚠️ **Warn the user before launching — client windows open on their desktop and
can orphan.** Start `tools/launch-server.sh`, join with two clients (long `--timeout`, distinct `--profile`),
and run **I-5**, **I-6**, **I-7**.

**V-9 — Boundary audit.** Run **I-4**'s four greps/diffs and **I-9**.

---

## 9. Testing Strategy

**Automated coverage is a thin spine, and this section is honest about where it ends.** Baselines measured
2026-08-11: compile **exit 0 / 5964 files**, Fast **54**, All **89**. Both groups must be green at every phase
boundary.

**What each tier can and cannot see here:**

| Tier | Can it cover this feature? |
|---|---|
| **Logic** (world-free, ~25 cases today) | ✅ **Yes, and this is where the real value is.** The entire geometry — weighted ownership, radius clipping, the land test given stub heights, the march's stop reasons, the shrink-only smoothing — is pure maths on hand-built objects. This is exactly why `OVT_TerritorySolver` is split out of the layer: the split is not aesthetic, it is what makes any of this testable. |
| **Init** (managers resolve, controllers registered) | ⚠️ **Marginal.** It could assert that the four site collections exist and are non-null, but that asserts the *managers*, not this feature, and those managers already have coverage. **No new Init cases** — YAGNI. |
| **Campaign** (started-campaign state) | ❌ **No.** There is no map, no canvas and no rendering in the autotest world. A "territory" assertion here could only re-assert town/base faction fields that already have coverage. |
| **Persistence** | ❌ **No, and deliberately so.** This feature persists nothing (I-4). If a Persistence case ever becomes relevant, something has gone badly wrong with the §3.4 boundary. |

### Logic cases (Phase 3)

Each is deterministic, world-free, and built with `new`. Target ~10 cases:

| # | Case | Asserts |
|---|---|---|
| 1 | `IsLand(surfaceY, oceanY, margin)` above/below/exactly-at the margin | The land predicate, including the boundary |
| 2 | Two equal-weight sites | The boundary along the connecting line sits at the **midpoint** |
| 3 | Two sites weighted 2 : 1 | The boundary sits **2/3** of the way toward the weaker site (the Apollonius property — this is the case that pins D1) |
| 4 | One isolated site, no rivals, no coast | Every ray returns exactly `maxRadius`, and every stop reason is `MAX_RADIUS` |
| 5 | One site, stubbed coast at a known Z | Rays crossing the coast stop at it (within the refine tolerance) with reason `COAST`; rays away from it are unaffected |
| 6 | Two sites close together | Rays toward the rival stop with reason `RIVAL`; rays away stop with `MAX_RADIUS` |
| 7 | `SmoothRadii` shrink-only invariant | `∀i: smoothed[i] ≤ raw[i]` on a deliberately spiky radius array (K5) |
| 8 | `SmoothRadii` circular wrap | Index 0's window includes index `N-1`; a spike at index 0 is smoothed the same as a spike in the middle |
| 9 | `SmoothRadii(passes = 0)` | The array is returned unchanged — the "smoothing is tunable to off" contract |
| 10 | Candidate rival list | A site far beyond `2 × maxRadius` is excluded and its exclusion does not change the result (the optimisation is provably neutral) |

**Proving each can fail** — the inversions to run, batched, with the result recorded per case:

- Flip `<` to `>` in `OwnsPoint` → cases 2, 3, 6 red.
- Drop the `/ weight` division → case 3 red, case 2 green (which is *why* case 2 alone is insufficient and case
  3 exists).
- Flip `>` to `<` in `IsLand` → cases 1, 5 red.
- Remove the `Min(smoothed, raw)` clamp → case 7 red.
- Replace the circular index wrap with a clamp → case 8 red.
- Return early from `SmoothRadii` regardless of `passes` → case 9 red.
- Drop the candidate filter → case 10 still green (it asserts neutrality); **widen** the filter to exclude a
  near rival → cases 6, 10 red.

❌ **No `maxAttempts`.** Every case is deterministic by construction; one that needs a retry is a bug in the
case.

⚠️ **The tier rule is enforced by a reviewer grep that does not distinguish code from prose.** Neither
Overthrow's static manager accessor nor the engine's game-mode getter may appear anywhere in the new file,
**including comments** — a previous feature tripped exactly this by quoting the rule verbatim in its own header.

### What is not testable, and must be play-tested

Rendering, colour, alpha, texture tiling, marker occlusion, frame cost, the compositor's actual output, the
`.conf` module entry, the textures, and **all** multiplayer/JIP behaviour. That is most of the feature by
value, which is why §8's Verification Method is nine steps and why V-3 (Workbench load) is called out
separately: `.conf`, `.edds`, `.meta` and imageset edits are invisible to `compile-check.sh` **and** to both
test groups.

### Debugging: three signatures that cover nearly every silent failure

| Symptom | Most likely cause | First check |
|---|---|---|
| **Territory renders, rings vanish** (or the reverse) | The compositor is not composing — a layer is calling `SetDrawCommands` with its own list, or a bucket's frame stamp is never current | `Print` the composited command count per frame; if it equals one layer's count, the flush path is wrong |
| **Overlay is empty, no error** | `CollectSites` found nothing — a manager was null at map open (JIP window), or a site-type config entry is `m_bEnabled 0` | `Print` the site count per source in `CollectSites`. If it is zero on a client but non-zero on the host, it is a replication finding against the owning feature, not this one |
| **Cells look mirrored / inside-out / hinged on one corner** | Either the world-Z → screen-Y sign (P4) or the fan-from-vertex-0 artefact (P2) | Re-run probe P4 and P2. **These two look similar and have completely different fixes** — P4's failure mirrors the whole cell, P2's fills the concave notches while leaving the convex hull correct |

---

## 10. Dependencies

### Internal (code — all read-only)

- **`map/core`** — `OVT_MapCanvasLayer` (the base being extended), `MapFullscreen.conf`, and the
  `OVT_MapLocationType` colour path. This feature adds rows to the canvas-layer contract and **must record them**
  in `core/context.md` (Phase 8 task 3). It also inherits `core`'s outstanding `FindAnyWidget` name-sweep debt —
  not this feature's to close, but relevant since the compositor resolves `DRAWING_WIDGET_NAME`.
- **`map/location-types`** — the marker colour overrides territory must agree with (K6), and the N1
  house-privacy precedent that keeps camps out of the site list.
- **`map/legacy-retirement`** — the legacy-free map this is built against; `OVT_MapRestrictedAreas` and the
  disabled `OVT_MapThreatGrid` are both **explicitly retained** by that feature and neither may be deleted here.
- **`map/respawn`** — the additive-extension discipline this feature copies, and `MapRespawn.conf`, which stays
  untouched (K11).
- **`towns/core`** — `OVT_TownData.location` / `faction` / `stability`, `OVT_TownSize`, `GetTownRange`,
  `m_Towns`.
- **`occupying/core`** — `OVT_BaseData` / `OVT_RadioTowerData` `location` + `faction`, `m_Bases`,
  `m_RadioTowers`.
- **`resistance/fob`** — `m_FOBs`, `OVT_FOBData.location`; **`OVT_FOBData` has no faction field**, so FOB colour
  comes from `GetPlayerFactionIndex()`. Also `FOB_DEPLOY_BASE_BUFFER` / `FOB_DEPLOY_TOWER_RANGE`, read only via
  the existing restricted-areas code, which is not changed (I-3).
- **`core/game-mode`** — `OVT_Global` accessors: `GetTowns()`, `GetOccupyingFaction()`,
  `GetResistanceFaction()`, `GetConfig()`.

### Downstream (this feature unblocks)

- **`map/map-layers` (feature 7)** — blocked on the generic-vs-hardcoded answer (K1) and on the compositor
  existing at all. Phase 8 task 4 writes the answer into its requirements file.

### External — user / Workbench work

| Item | Blocking? | Notes |
|---|---|---|
| **Two hatch textures + `.meta`, imported in Workbench** | **For V-6 only** — not for the feature | Art brief below. `m_bUseTextures 0` ships a flat-fill fallback so every other gate runs without them |
| Workbench clean-load check (V-3) | **YES** | The **only** gate that can see the `.conf` entry, the new GUID, the textures and their `.meta` files |
| Fully-populated campaign save | **YES** for Q-1/V-5 | The measurement is meaningless on an early-game world |
| Two-client MP session (V-8) | **YES** for I-5…I-7 | `tools/launch-server.sh` + two `tools/launch-game.sh --profile` clients, **long `--timeout`**. Warn before launching |
| Regenerate the six `localization_Overthrow.<lang>.conf` exports | No | Only two layer-name ids are added and nothing renders them until feature 7. **Never hand-edit the exports** |

### 🎨 Art brief — the two hatch textures

Both must satisfy all of:

- **Seamlessly tileable** at the shipped `m_fUVScale`. This is the requirement most likely to be missed —
  **no existing Overthrow texture is a tiling pattern**; `UI/Imagesets/` holds three icon atlases
  (`overthrow_dark`, `overthrow_mapicons`, `overthrow_priceicons`) and `UI/Textures/Map/` holds their two
  `.edds` atlases, all sprite sheets. There is nothing to adapt, so both are new authored assets.
- **Alpha-bearing.** The pattern's *alpha* carries the shape; the polygon's `m_iColor` supplies the faction
  tint. A pattern baked with opaque colour would fight the faction palette and break K6's colour agreement.
- **Legible at map zoom.** The map is viewed zoomed out most of the time. A pattern whose period is a few pixels
  at that zoom aliases into noise; err coarse.
- **Visually distinct from each other**, and distinguishable at a glance — different *structure*, not just
  different spacing. Suggested: diagonal bars for restricted zones (the conventional "keep out" idiom) and a
  finer dotted/cross-hatch for neutral border bands.
- **Quiet.** The overlay must stay under the terrain. Low contrast within the pattern; the texture modulates
  alpha, it does not draw attention.
- Delivered as `.edds` **plus `.meta`**, imported through Workbench. Suggested paths:
  `UI/Textures/Map/overthrow_hatch_restricted.edds` and `UI/Textures/Map/overthrow_hatch_neutral.edds`.

### New and changed files

```
Scripts/Game/UI/Map/
├── Core/
│   ├── OVT_MapCanvasCompositor.c            NEW  shared ordered command list, layer registry
│   └── OVT_MapCanvasLayer.c                  ~   m_iDrawOrder/m_sLayerId/m_sDisplayName/m_bVisible,
│                                                 register/unregister, CacheProjection/ProjectWorld,
│                                                 optional texture on DrawCircle, Count()>0 guard removed
├── Territory/
│   ├── OVT_MapTerritoryLayer.c              NEW  collect → solve → cache → emit; all config attributes
│   ├── OVT_TerritorySolver.c                NEW  PURE geometry, world-free, virtual land test
│   ├── OVT_TerritorySite.c                  NEW  Managed record
│   ├── OVT_TerritoryCell.c                  NEW  Managed record + OVT_TerritoryStop enum
│   └── OVT_TerritorySiteConfig.c            NEW  BaseContainerProps per-site-type weight/radius/enable
├── Visualization/
│   └── OVT_MapRestrictedAreas.c              ~   textured fill + m_sLayerId (geometry UNCHANGED)
└── LocationTypes/
    ├── OVT_MapLocationTown.c                 ~   GetIconColor → shared helper (K6)
    ├── OVT_MapLocationBase.c                 ~   GetIconColor → shared helper
    └── OVT_MapLocationRadioTower.c           ~   GetIconColor → shared helper

Scripts/Game/Tests/TestSuites/Logic/
├── OVT_TEST_Logic_Territory.c               NEW  ~10 world-free cases
└── OVT_TEST_LogicSuite.c                     ~   register the new case class

Configs/Map/
└── MapFullscreen.conf                        ~   + OVT_MapTerritoryLayer entry (fresh {6A84…} GUID)
                                                  + m_iDrawOrder 200 on OVT_MapRestrictedAreas
                                                  OVT_MapThreatGrid entry UNCHANGED (stays disabled — D3)

UI/Textures/Map/
├── overthrow_hatch_restricted.edds (+ .meta) NEW  USER / Workbench
└── overthrow_hatch_neutral.edds    (+ .meta) NEW  USER / Workbench

Language/localization_Overthrow.st            ~   2 layer-name ids (master only; exports are the user's)

docs/features/map/
├── territory-overlay/context.md             NEW  probe results, chosen rung, measured numbers, triage
├── core/context.md                           ~   canvas-layer contract rows
├── map-layers/requirements.md                ~   the open scope question, answered (K1)
└── epic-overview.md                          ~   feature 6 status + D3 as accepted tech debt
```

> **GUIDs:** allocate from the free **`{6A84…}`** series — measured 2026-08-11, `{6A83…}` has 12 uses and
> `{6A84}`/`{6A85}`/`{6A86}` have none. `grep -rn` each new GUID before committing; a duplicate GUID is a
> Workbench-only failure that every automated gate passes.
>
> **Bug ids:** the highest allocated id is **BUG-144** (BUG-138…144 exist as *untracked* files —
> `ls docs/bugs/` before allocating, do not trust `git log`).

---

## 11. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | 🔴 **The shared-canvas defect ships unfixed and territory "breaks" the restriction rings.** Every layer resolves the same `CanvasWidget` (`OVT_MapCanvasLayer.c:89`) and overwrites the others (`:12-19`). The symptom — rings vanish when territory appears — reads exactly like a broken territory layer and will be debugged in the wrong file. | **Certain if unfixed** | High | **Phase 1, first, `component-developer-advanced`.** DoD **I-1** is a dedicated criterion, not a line inside another one. The three-layer case (I-1b) is exercised with the threat grid temporarily enabled, which is also the cheapest possible test of the compositor. |
| **R2** | **The render primitives do not work as hoped.** `PolygonDrawCommand.m_pTexture`/`m_fUVScale`, `TriMeshDrawCommand`, and non-convex polygon fill have **zero usages anywhere in vanilla** (`EnWidgets.c:101-116`). The user's confidence comes from other mods, not from this engine build. | Medium | High | **Phase 2 is a throwaway probe before anything depends on it**, with four unambiguous pass/fail signatures. The ladder (K3) has a **provably correct bottom rung** that needs no engine feature at all, and the cell geometry is identical on all three rungs so the probe result changes ~60 lines. Textures are behind `m_bUseTextures 0` by default. |
| **R3** | **Frame/solve cost at full campaign scale.** ~40 sites × 48 rays × march steps, plus per-frame emission. Rung 3 would put ~3,800 commands/frame on the canvas — worse than the threat grid that was probably disabled for this. | Medium | High | Candidate rival lists and early-stop bound the solve (K4); one 3-call affine basis replaces ~1,900 `WorldToScreen` calls per frame (K13/P4). **Q-1's three budget numbers are measured on a populated save and recorded**, with an ordered fallback list ending in a budgeted incremental solve (Phase 6). Rung 3 carries a mandatory ray-count cut with the visual cost stated. |
| **R4** | **A mirrored or notch-filled overlay is misdiagnosed.** The world-Z → screen-Y sign is untested — `DrawCircle`'s symmetry (`:33-40`) and `DrawRectangle`'s two projections (`:67-68`) both hide it — and the fan-from-vertex-0 artefact looks superficially similar. | Medium | Medium | P4 derives the basis empirically instead of assuming the sign, and asserts ≤ 2 px against direct `WorldToScreen`. §9's debugging table names both signatures side by side **and states that they have completely different fixes**. |
| **R5** | **Territory colour disagrees with marker colour**, introducing the second palette `requirements.md` forbids. | Low (after K6) | Medium | K6 found the three types that matter **already** resolve live faction colour identically; Phase 5 task 1 extracts one shared helper so there is literally one implementation. DoD **F-2** checks three sites of different factions by eye. |
| **R6** | **The sampled coastline is too coarse (blocky shore) or too slow (proto calls dominate).** | Medium | Medium | Bisection refine sharpens 50 m → ~3 m for 4 extra evaluations per ray (K4); `m_fMarchStep`, `m_iRefineSteps` and `m_bClipToCoast` are all config attributes; `BaseWorld.IsOcean()` short-circuits on ocean-free worlds. Documented fallbacks: sample every other step, or precompute a coarse shared land grid. |
| **R7** | **Rivers and lakes cut territory.** A river bisecting a town's region would look like a bug and would be one. | Low | Medium | D2 chose `GetSurfaceY` vs `GetOceanHeight` **specifically** because it ignores ponds and rivers by construction, unlike `TryGetWaterSurfaceSimple` which returns true for `WST_POND` and `WST_RIVER`. DoD **F-5** states explicitly that a region spanning a river is **correct**, so it is not "fixed" later by someone who assumes otherwise. |
| **R8** | **Territory is mistaken for a new campaign mechanic** — by players ("does territory affect income?"), or worse, by a future contributor who starts writing to it. | Medium | High | §3.4 states the boundary; DoD **I-4** enforces it with four greps/diffs (no `[RplProp]`, no RPC, no EPF, no writes to any campaign record, all new files under `Scripts/Game/UI/Map/`); Phase 8's `help-docs-sync` describes it as a display, with **every sentence backed by a `file:line` or cut**. |
| **R9** | **JIP client sees an empty or wrong overlay for the whole map session**, because the solve runs once at map open and the records had not replicated yet. | Medium | Medium | K9's `HashSites` re-solve on the refresh tick makes it self-heal within one interval without closing the map; K10's per-call manager lookups avoid caching a null through the JIP window. DoD **I-6** requires the correction to happen with the map still open. |
| **R10** | **The art never lands**, or lands late, and the feature is blocked on it. | Medium | Low | `m_bUseTextures` defaults **0** and every phase before Phase 5's texture step is testable without any new asset. The feature is shippable-but-plainer with flat fills; V-6 is a separate, later step. |
| **R11** | **A `.conf` / `.edds` / `.meta` fault passes every automated gate.** This feature adds a module entry with a new GUID and two imported textures — file classes invisible to `compile-check.sh` **and** to both test groups. | High | Medium | **V-3 is mandatory and is the only evidence those files are sound**, with a per-item checklist and a `grep -rn "{6A84"` uniqueness check. Fresh GUIDs come from a series measured to be unused. |
| **R12** | **The deferred threat grid (D3) is silently forgotten**, leaving written-but-disabled code in the tree indefinitely — the exact outcome `requirements.md` was written to prevent. | Medium | Low | D3 records it as an **accepted waiver with the user's date**, Phase 8 task 5 adds it to the epic's Tech Debt section, and D3 writes down the two candidate reasons it was disabled plus the fact that reviving it after Phase 1 is a one-line conf edit — so the next decision is made with evidence rather than blind. |
| **R13** | **A future overlay toggle is built on `SetActive` and cannot be undone.** `SetActive(false)` calls `DeactivateModule` and there is no script-reachable way back (`ActivateModules` and `m_aActiveModules` are both `protected`). | Medium | Medium | K1 makes `SetLayerVisible` the documented toggle primitive and Phase 8 task 4 writes the caveat directly into `map-layers/requirements.md`, where feature 7's planner will read it. |
| **R14** | **Parallel sessions commit to this tree mid-feature** — this epic has a documented history of it, and seven untracked bug files are sitting in the tree right now. | Medium | Low | Re-check `git status` and the highest `docs/bugs/` id at every phase boundary; commit per phase so there is a revert path. |

---

*Plan created 2026-08-11 by `/plan-feature map/territory-overlay`. Baselines in §5 Phase 0 were **measured, not
quoted**: compile exit 0 / 5964 files, Fast 54, All 89, free GUID series `{6A84…}`, highest bug id BUG-144.
§4 records five decisions settled with the user on 2026-08-11 — two of which **override** `requirements.md` and
`epic-requirements.md`, one of which is a **waiver**, and one of which is a **deferral the user chose
explicitly**. Those files remain the record of earlier intent and are deliberately not edited.*
