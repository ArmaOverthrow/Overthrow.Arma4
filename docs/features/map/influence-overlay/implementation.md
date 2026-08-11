# Map Influence Overlay — Implementation Plan

**Status:** ✅ **COMPLETE — built and play-tested green (SP + MP) 2026-08-12 and closed by the user**
**Epic:** map (feature 9)
**Started:** 2026-08-11
**Target Completion:** TBD
**Last Updated:** 2026-08-12

> **This plan corrects two claims in its own research brief.** Both were measured against the tree during
> planning and both change what the feature has to do:
>
> 1. **A client does NOT hold the `[Attribute]` defaults for the two unstreamed difficulty fields.** It holds
>    **`Difficulty_Normal.conf`'s** values, because `Prefabs/GameMode/OVT_OverthrowGameMode.et` instantiates
>    `m_Difficulty` from that file. So a client reads `baseSupportRange` **1000**, not 750 — and therefore
>    **agrees with the server at Normal and disagrees at every other difficulty.** The verification step must
>    run at **Hard** or **Extreme/Insane**, never at Normal. See §4 D-JIP and §8 V-8.
> 2. **The `NearbyBase*` branch's missing `else` is not a bug and must not be filed.** Verified with the user:
>    `m_Bases` is insert-only, `OVT_BaseData` has no disabled state and bases do not move, so
>    "in range → not in range" is an unreachable transition. The tower branch needs its `else` precisely
>    because `IsDisabled()` (sabotage) _can_ take a tower off the air. The asymmetry is deliberate and correct.
>
> All `file:line`-style pointers in this document were re-resolved on 2026-08-11. Code comments written by
> this feature follow the epic's **K-9 discipline** instead: keep the rationale, name the symbol, drop the
> line number.

---

## 1. Executive Summary

Overthrow's campaign already models spatial influence between locations — a radio tower depressing a nearby
town's support, a military base doing the same, a captured town lending momentum to its neighbours — and the
player can see none of it. The town info panel shows _that_ a `Nearby Radio Tower` modifier is active; nothing
anywhere shows **which** tower, how far its reach extends, or that the friendly tower two valleys over is being
out-shouted by an enemy one.

This feature is a canvas overlay that answers those questions the moment a location is selected: **dashed,
faction-coloured lines between the selected location and every other location it is in an influence relationship
with, plus a dashed range ring for a selected tower or base.** Lines whose modifier the campaign is currently
applying are drawn **solid-alpha**; relations that exist geometrically but whose modifier the campaign is _not_
applying are drawn **dimmed and sparser**. That second class is the tactical half of the feature — it is the
first time the campaign's "enemy source wins over friendly source" rule, and the effect of sabotaging a tower,
have ever been visible.

Three facts found during planning shape the work and are worth stating up front:

1. **The modifier lists are already fully replicated, and that turns out to be the whole answer.** Every town's
   `supportModifiers` array (`{id, timer}` pairs) reaches clients through per-modifier broadcast RPCs _and_
   through `OVT_TownManagerComponent`'s `RplSave`/`RplLoad` JIP stream, and `OVT_MapLocationTown` already reads
   them client-side to render the panel's modifier chips. The client therefore never has to re-derive the
   campaign's _outcome_ — it re-derives only the **geometric qualification** ("is this tower in range of this
   town?") and asks the replicated list whether the corresponding modifier is actually present. **That single
   cross-check is what makes client-side derivation safe**, and it is the plan's central decision (§6 K2).
2. **The client's derivation only needs a subset of the server's rules, which shrinks the duplication risk the
   feature was chartered around.** `ResolveProximity` — the enemy-wins-over-friendly rule — stays a server-side
   concern; the client gets suppression _for free_ from the cross-check. What must agree between the two
   machines is the range test and the source→modifier mapping, and those are exactly what the shared helper
   holds.
3. **One real replication gap, and its cost is now measurable in both directions.** `radioTowerRange` and
   `baseSupportRange` are absent from `OVT_OverthrowConfigComponent`'s hand-rolled config JIP stream. Because
   the client boots on `Difficulty_Normal.conf`, at Hard (1250) it would **miss** real base edges, and at Easy
   (750) it would **invent** dim ones — a false dim edge is worse than a missing one, because it looks like a
   mechanic. Two floats and a version bump fix it (§5 Phase 3).

The feature adds **no RPC, no `[RplProp]`, no persistence and no write to any campaign record.** Its one edit to
campaign code is a **behaviour-preserving extraction** of predicates that `OVT_TownManagerComponent` and
`OVT_RevolutionaryMomentumSupportModifier` already implement inline — the "one rule set, two machines" pattern
`map/respawn` used for `CanRespawn` vs `OVT_RespawnService.CollectEligiblePositions`. That extraction is also
the feature's **only automatable surface**.

---

## 2. Goals

### Primary

1. **Influence is legible on selection.** Selecting a town, base or radio tower draws every influence relation
   it participates in, colour-coded by the faction exerting the influence.
2. **In-effect and suppressed relations are unmistakably different.** A dim line must read as _"this relation
   exists and is not currently applying"_, never as a rendering artefact.
3. **The selected source's reach is visible.** A selected tower or base draws a dashed ring at the exact radius
   the server applies, in the same visual idiom as the lines.
4. **The overlay never asserts an influence the campaign is not applying.** Solid requires the modifier to be
   present on the target town, whatever the reason it might be absent and whether or not this layer understands
   that reason.
5. **Correct on a real client, including JIP.** Both range values arrive over the wire; a joining client draws
   the same edges as an established one.
6. **It costs a measured, recorded amount of frame time** — including the number of composited draw commands,
   which is the resource a dash renderer actually spends.

### Secondary

7. **One rule set, two machines.** The range tests and the source→modifier mapping have exactly one
   implementation, called by both the server's `CheckUpdateModifiers` and this layer.
8. **A free toggle row.** Setting `m_sLayerId` + `m_sDisplayName` gets a `map/map-layers` row and per-profile
   persistence with zero code change in feature 7.
9. **Zero rows added to the `OVT_MapCanvasLayer` contract.** Everything this layer needs already exists.
10. **Tunables live in config** — alphas, dash lengths, widths, ring segments, caps and the refresh interval are
    all attributes.

### Explicit non-goals

- Any source of influence other than the three in `Configs/Modifiers/supportModifiers.conf` that have a second
  _location_: `NearbyRadioTower±`, `NearbyBase±`, `RevolutionaryMomentum`. Everything else in that file and in
  `stabilityModifiers.conf` is event-sourced (a kill at a world position, a transaction, a random roll, a
  low-stability tick) and has no source location to draw a line to.
- A general-purpose influence-graph system, a magnitude/strength channel on edges, animated flow direction
  beyond a static dash pattern, or job-granted modifiers.
- Recording source attribution in `OVT_TownModifierData` (§6 K1, rejected by the user).
- A legend or colour key. `map/map-layers` **K4** deliberately shipped none and this feature does not
  reintroduce one; the Field Manual carries the explanation instead (§5 Phase 9).
- Fog of war / per-player knowledge of influence. A future intel epic owns that; §6 K10 names the one seam.
- The respawn map. `Configs/Map/MapRespawn.conf` carries no `OVT_MapCanvasLayer` at all, and adding one there
  would be a deliberate act (`territory-overlay` K11).

### Out of Scope / Deferred — with reasons

| Item                                                        | Why not now                                                                                                                                                                                                                                                                                                                                              |
| ----------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **A momentum range ring on a selected player-held town**    | A town is simultaneously a _receiver_ of tower/base influence and a _projector_ of momentum. One ring around it would be ambiguous about which radius it represents. D4 asks for a ring on towers and bases; that is where a ring is unambiguous. One config attribute and ~10 lines to add later if wanted.                                             |
| **A "this location has no influence relations" affordance** | Drawing nothing is the correct output and inventing UI for the null case costs a layout change in `location-types`. The ambiguity is real and is handled by naming its two workarounds in §8 F-6 instead.                                                                                                                                                |
| **A textured single-command dashed line**                   | Costs a new art asset **and** carries two unresolved unknowns `territory-overlay` recorded and never settled (`m_fUVScale` units are undocumented; whether UVs derive from screen-space vertices — which would make dashes _slide_ as the map pans — was never established). Recorded as the measured fallback in §6 K6 if the command budget is missed. |
| **Promoting the dash emitters onto `OVT_MapCanvasLayer`**   | One consumer. `territory-overlay`'s contract survived its first consumer untouched precisely because rows were added when a second one appeared, not before. Promote if `map/shared-markers` wants lines.                                                                                                                                                |
| **Disabled/downgraded **bases\*\*\*\*                       | See §6 K11 — _designed for, not built for_.                                                                                                                                                                                                                                                                                                              |

---

## 3. Architecture Overview

### 3.1 Component hierarchy

```
SCR_MapEntity  (vanilla; drives module Update every frame while the map is open)
│
├── Configs/Map/MapOverthrow.conf ─ m_aModules   (same-GUID DELTA over vanilla's)
│   ├── OVT_MapTerritoryLayer    : OVT_MapCanvasLayer    ~   m_iDrawOrder 100
│   ├── OVT_MapRestrictedAreas   : OVT_MapCanvasLayer    ~   m_iDrawOrder 200
│   ├── OVT_MapInfluenceLayer    : OVT_MapCanvasLayer   NEW  m_iDrawOrder 300  ← on top of both
│   └── OVT_MapThreatGrid        : OVT_MapCanvasLayer    ~   stays m_bDisableModule 1
│
└── Configs/Map/MapOverthrow.conf ─ m_aUIComponents
    └── OVT_OverthrowMapUI       : SCR_MapUIElementContainer  ~  + GetPanelLocation()   ← the bridge

OVT_MapInfluenceLayer
├── poll     → OVT_OverthrowMapUI.GetPanelLocation()          the selection, once per frame
├── derive   → array<ref OVT_InfluenceEdge>                   rebuilt on change / refresh tick
│              via OVT_InfluenceRules  (PURE, world-free, SHARED WITH THE SERVER)
└── emit     → N × LineDrawCommand per edge + ring arcs        per frame, in screen space

OVT_InfluenceRules            NEW   static, pure, no engine call, no world
├── called by OVT_TownManagerComponent.CheckUpdateModifiers            (server)
├── called by OVT_RevolutionaryMomentumSupportModifier.UpdateAllTownMomentum (server)
└── called by OVT_MapInfluenceLayer                                    (client)
```

### 3.2 What crosses the client/server boundary, and what does not

**Nothing new crosses it.** This feature adds **no RPC, no `[RplProp]`, no EPF save data** and writes to no
campaign record. Everything the layer reads is already replicated for other reasons:

| Data                                                              | How it reaches a client today                                                                                                                                                            | Used for                   |
| ----------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------- |
| `OVT_TownData.location`                                           | **Not replicated** — each machine rediscovers it in its own `InitializeTowns()` world scan (`town.location = entity.GetOrigin()`), so both sides hold the same value from the same world | edge endpoints             |
| `OVT_TownData.faction`                                            | JIP `RplSave`/`RplLoad` + live broadcast on control change                                                                                                                               | momentum direction, colour |
| `OVT_TownData.supportModifiers` (`{id, timer}`)                   | JIP `RplSave`/`RplLoad` **and** `RpcDo_AddSupportModifier` / `RpcDo_RemoveSupportModifier` / `RpcDo_ResetSupportModifier`, all `Broadcast`                                               | **the cross-check**        |
| `OVT_BaseData.location` / `.faction`                              | `OVT_OccupyingFactionManager.RplSave`/`RplLoad`; live `RpcDo_SetBaseFaction(index, faction)`                                                                                             | edges, colour              |
| `OVT_RadioTowerData.location` / `.faction` / `.disabledRemaining` | same JIP pair; live `RpcDo_SetRadioTowerFaction(pos, faction)` and `RpcDo_SetRadioTowerDisabled(pos, seconds)`                                                                           | edges, colour              |
| Modifier **config** (names, titles, `baseEffect`)                 | `OVT_TownModifierSystem.LoadConfig` runs on **every** machine — only `PostInit`'s handler wiring is behind `if(!Replication.IsServer()) return;`                                         | name → index resolution    |
| `m_Difficulty.radioTowerRange` / `.baseSupportRange`              | ❌ **NOT replicated today.** Phase 3 appends them to the existing config JIP bitstream                                                                                                   | range tests                |

The only wire change in the entire feature is **two floats appended to an existing `RplSave`/`RplLoad` pair,
with the version stamp bumped.** No new RPC surface exists to attack, and nothing is added to
`OVT_PlayerCommsComponent`.

### 3.3 The selection bridge — and why `m_SelectedElement` is the wrong field

The map has two rendering paths that share nothing but the map entity: markers are **widgets** under
`m_aUIComponents`, overlays are **canvas draw commands** under `m_aModules`. The bridge is
`SCR_MapEntity.GetMapUIComponent(typename)`, inherited on `SCR_MapModuleBase`, so
`m_MapEntity.GetMapUIComponent(OVT_OverthrowMapUI)` works directly from a canvas layer. `OVT_MapLayersUI`
already does exactly this.

`OVT_OverthrowMapUI` holds four candidate fields, all `protected`, and **three of them are wrong**:

| Field                    | Why not                                                                                                                                                                                                                                                                                                                                                                           |
| ------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `m_SelectedElement`      | 🔴 **Sticky.** Neither `HideLocationInfo` nor `ForceHideLocationInfo` clears it. On hover-away with no pin the panel disappears and this field keeps pointing at the last hovered element. Lines driven from it would **outlive the panel**, which directly violates D2's "lines and panel always agree".                                                                         |
| `m_HoveredElement`       | Null while a pin is held and the cursor is elsewhere — the pinned panel would show with no lines.                                                                                                                                                                                                                                                                                 |
| `m_PinnedElement`        | Null during plain hover — the hover panel would show with no lines.                                                                                                                                                                                                                                                                                                               |
| **`m_PanelLocation`** ✅ | **It _is_ the panel's location.** Set when a panel is built, nulled by `HideLocationInfo`, `ForceHideLocationInfo` and `OnMapClose`, and explicitly re-pointed at the fresh record by the BUG-136 refresh reconciliation (which also force-hides the panel when the record it describes has gone). Agreement with the panel is by construction rather than by parallel reasoning. |

**So the bridge is one public getter, `GetPanelLocation()`, returning `OVT_MapLocationData`** — a record, not an
element. The layer therefore never holds a reference to a widget or an element across frames, which is what makes
it survive the refresh timer destroying and recreating elements mid-session.

One hardening goes with it: `m_PanelLocation` is currently assigned inside `SetupTravelButton`, whose early
return is travel-shaped and which `OVT_RespawnMapUI` overrides without calling `super`. Phase 2 also assigns it
in `ShowLocationInfo` immediately after the panel widget is created. On the living map the value is identical;
the change only makes the getter mean what its name says on any map config that suppresses travel affordances.

### 3.4 Data flow, one map session

```
OnMapOpen
  ├─ super.OnMapOpen(config)                    resolves the canvas, allocates the bucket, REGISTERS
  ├─ resolve OVT_OverthrowMapUI                 (lazily re-resolved if null — module vs UI-component
  │                                              init order is not guaranteed)
  └─ null the edge cache

Update (every frame, via the base class)
  └─ Draw()
       ├─ selection = mapUI.GetPanelLocation()
       ├─ key = selection.m_sTypeName + "|" + selection.m_iID      ("" when nothing is selected)
       ├─ if key changed  OR  refresh interval elapsed  OR  source counts changed:
       │      BuildEdges(selection)     ← geometry + faction + live state + cross-check, ALL of it,
       │                                  on ONE path for every source type (§6 K5)
       ├─ if no edges and no ring:  m_Commands stays empty  → the layer contributes NOTHING
       ├─ CacheProjection()                                   once per frame, 3 WorldToScreen calls
       └─ for each edge: project both endpoints, tessellate dashes in SCREEN space, emit
          for the ring:  project centre + radius, emit alternating arcs

OnMapClose
  └─ super.OnMapClose(config)  (unregisters) + null every cache
```

**The world-space / screen-space split is load-bearing.** Edge endpoints are world-space and cached; the _dash
tessellation_ is inherently screen-space because dash length is a pixel quantity and the on-screen length of an
edge changes with every pan and zoom. So the per-frame path is exactly: two `ProjectWorld` calls per edge, one
length, one clamped dash count, then the emit loop. No manager read, no distance test, no faction lookup runs on
the per-frame path.

### 3.5 Where the boundary is

The overlay is a **read-only projection**, exactly as `territory-overlay` §3.4 states for itself. Its one edit
outside `Scripts/Game/UI/Map/` is `OVT_InfluenceRules`, which is deliberately filed under
`Scripts/Game/GameMode/Systems/Modifiers/` **because it is the campaign's rule set and the map is its second
caller, not its owner.** That placement makes the boundary clearer, not muddier: the map does not own an
influence model; it borrows the campaign's.

---

## 4. Settled Planning Decisions

Settled with the user before this plan. **They are not open.** Restated so the plan is self-contained, with the
consequences this plan derives from them.

- **D1 — The client RE-DERIVES; no source attribution is recorded server-side.** Plus the two-float versioned
  append to the config JIP stream. §6 K1 records the rejected alternative and why it was rejected.
- **D2 — Lines follow the SELECTED element (hover or pin).** Hover already selects and already shows the panel,
  so lines and panel always agree and no new gamepad affordance is needed. Accepted cost: lines appear and
  disappear as the cursor sweeps across markers. **Recommendation on the open sub-question: ship with no fade
  and no dwell.** The panel itself already flickers on the same sweep and nobody has complained; adding a
  timing behaviour to the lines alone would make them _disagree with the panel_, which is the one property D2
  exists to guarantee. If the sweep proves distracting at play-test, the cheap fix is a dwell on the panel (a
  `map/core` change benefiting both), not on this layer. Recorded as a Phase 8 observation, not a build task.
- **D3 — In-effect edges SOLID, suppressed edges DIMMED.** Both dashed, both faction-coloured. ⚠️ **The
  suppressed class is defined by the general rule, not by today's cause** — see §6 K3.
- **D4 — A selected tower or base ALSO draws its reach, as a STROKE OUTLINE ONLY, no fill, in the same style as
  the lines.** This rules out `OVT_MapCanvasLayer.DrawCircle`, which emits a **filled** `PolygonDrawCommand`.
  §6 K7 covers how it reads against `OVT_MapRestrictedAreas`.
- **D-JIP — the two range floats are appended to the existing config bitstream with the version bumped.**
  🔴 **Corrected during planning:** the brief's "the client default is 750" is wrong. The client holds
  `Difficulty_Normal.conf`'s **1000**, because the game-mode prefab instantiates `m_Difficulty` from that file
  and `RplLoad` **patches the existing object in place** rather than selecting a preset. Consequences:
  - server **Normal** (1000) → client 1000 → **agree**. A verification run at Normal proves nothing.
  - server **Hard** (1250) / **Extreme**, **Insane** (1500) → client 1000 → client **misses** real base edges.
  - server **Easy** / **Test World** (750) → client 1000 → client **invents** dim edges for bases the server
    does not count. A false dim edge is worse than a missing one: it looks like a mechanic.
  - `radioTowerRange` is overridden by **no** shipped preset, so it agrees today **by coincidence**. It is
    appended anyway — "agrees by coincidence" is a landmine that detonates the first time a preset sets it,
    and it costs one float.

---

## 5. Implementation Phases

Effort is **S / M / L** relative to a single focused session. "Agent" is the routing hint for `/proceed`.
**Phases 2 and 3 need the advanced variants**: Phase 2 rewrites a method on the server's 10-second campaign
tick that four modifier ids depend on, and Phase 3 edits a hand-rolled positional bitstream where a
write/read mismatch corrupts every field after it and fails **only** on a real client.

---

### Phase 0 — Baseline — **S — no agent (already measured)**

Measured on `new-map` at `a4e71f41`, working tree clean, **2026-08-11**. These numbers were **run, not quoted.**

| Gate                                             | Baseline                                                                                     |
| ------------------------------------------------ | -------------------------------------------------------------------------------------------- |
| `tools/compile-check.sh`                         | **exit 0, 5994 files, Game module, 5 s**                                                     |
| `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) | **OK, 89 tests, 36 s**                                                                       |
| `tools/run-tests.sh "{6A6E2A002F53A581}"` (All)  | **OK, 127 tests, 48 s**                                                                      |
| Highest allocated bug id                         | **BUG-145**                                                                                  |
| Free GUID series                                 | **`{6A86…}`** — zero uses anywhere in the tree (`{6A87}`…`{6A8A}` also free; `{6A85}` has 6) |

⚠️ **`CLAUDE.md` says Fast 38 / All 66; `territory-overlay` says 66 / 101; `map-layers` says 89 / 127. Only the
last matches today and only by luck.** Parallel sessions commit to this tree. Re-check `git status`, the highest
`docs/bugs/` id and all three gate numbers at **every** phase boundary. A changed count is a **finding to
investigate**, never a number to update.

Expected end state: compile **5994 + 4 = 5998 files**, Fast **89 + N**, All **127 + N**, where **N** is the
number of new Logic cases (§9 plans **10**).

---

### Phase 1 — 🔴 Selection-bridge and canvas spike — **S — `ui-developer`, then user-run**

> **A deliberately throwaway-shaped spike, before anything depends on it.** F7 names the selection bridge as
> the feature's one genuine unknown, and this epic has been burned by reading-instead-of-looking exactly once
> already (`map-layers` D2: vanilla's tool menu was genuinely absent from Overthrow's `m_aUIComponents` and the
> `m_aModules` merge evidence did not transfer). One session buys the answer to five questions at once.

**Tasks**

1. Add `OVT_MapLocationData GetPanelLocation()` to `OVT_OverthrowMapUI`, and assign `m_PanelLocation = location;`
   in `ShowLocationInfo` immediately after the panel widget is created (§3.3).
2. Create `Scripts/Game/UI/Map/Influence/OVT_MapInfluenceLayer.c` as a **stub** `OVT_MapCanvasLayer` with
   `m_iDrawOrder 300`, `m_sLayerId "influence"`, `m_sDisplayName "#OVT-Map_Layer_Influence"`, whose `Draw()`
   clears the bucket, polls `GetPanelLocation()`, and — when something is selected — emits **one solid straight
   `LineDrawCommand`** from the selection to a fixed world point plus one `Print` of the selection's
   `m_sTypeName`/`m_iID`.
3. Register it in `Configs/Map/MapOverthrow.conf` `m_aModules` with a fresh `{6A86…}` GUID.
4. Add the `#OVT-Map_Layer_Influence` id to `Language/localization_Overthrow.st` (master only —
   ❌ **never** touch `localization_Overthrow.<lang>.conf`). It renders as a raw key until the user regenerates.

**Acceptance — five spike questions, each answered by observation and recorded verbatim in `context.md`**

- **P1 — Does `GetMapUIComponent(OVT_OverthrowMapUI)` resolve from inside a module's `Draw()`?** If it returns
  null on the first frames, record whether the lazy re-resolve recovers (module vs UI-component init order is
  an assumption, not a guarantee).
- **P2 — Does `m_PanelLocation` track hover _and_ pin, and go null on hover-away and on close?** Watch the
  printed selection while sweeping the cursor across markers, pinning, and pressing the panel's close button.
- **P3 — Does the line render, in the right place, above territory and the restriction rings?** Confirm the
  compositor composes four layers (three live + the disabled grid) and that territory and rings still draw.
- **P4 — Does a `LineDrawCommand` behave as vanilla's waypoint lines suggest?** `m_fWidth 2`,
  `m_fOutlineWidth 0`, packed-int colour, flat `[x0,y0,x1,y1]` vertex array in **screen pixels**. Confirm width
  and colour are honoured and the line does not scale with zoom (it should not — the vertices are screen space).
- **P5 — Does a `map/map-layers` row appear for the new layer, labelled, and does toggling it hide only this
  layer?** This is the free-integration claim; confirm it rather than assume it.

**Gates:** compile **exit 0**, file count **5994 + 1 = 5995**. Fast **89**, All **127** — unchanged; nothing
here is assertable. ⚠️ The `.conf` and `.st` edits are invisible to every gate. **The play-test is this phase's
only evidence and is its entire point.**

---

### Phase 2 — The shared rule set, extracted and tested — **M — `component-developer-advanced`**

> **Advanced.** It rewrites `OVT_TownManagerComponent.CheckUpdateModifiers`, which runs on the server's
> 10-second campaign tick and governs four permanent modifiers, and it edits
> `OVT_RevolutionaryMomentumSupportModifier`. The extraction must be **behaviour-preserving** — this is the one
> place the feature touches campaign code, and the epic forbids the map changing another system's state model.

**Tasks**

1. `Scripts/Game/GameMode/Systems/Modifiers/OVT_InfluenceRules.c` — **pure: no engine call, no world, no
   manager, no `BaseContainer`, no widget.** It holds:
   - `enum OVT_InfluencePolarity { NONE, POSITIVE, NEGATIVE }`
   - `enum OVT_InfluenceSourceKind { RADIO_TOWER, MILITARY_BASE, MOMENTUM_TOWN }`
   - `enum OVT_InfluenceEdgeState { ACTIVE, SUPPRESSED }` — ⚠️ **switched on with a default branch, never
     treated as a boolean** (§6 K11).
   - `static const float MOMENTUM_RANGE = 2000.0;`
   - `static bool IsProximitySource(vector townPos, vector sourcePos, float range)` → `vector.Distance(...) < range`
     — **3D, and strictly less-than**, matching the server exactly.
   - `static bool IsMomentumSource(vector townPos, vector otherTownPos)` → `vector.Distance(...) <= MOMENTUM_RANGE`
     — **inclusive**, and the asymmetry with the line above is real and deliberate; a case pins each.
   - `static bool TownQualifiesForMomentum(int townFaction, int playerFactionIndex)`
   - `static OVT_InfluencePolarity PolarityForSource(bool isOccupyingFaction)`
   - `static string ModifierNameFor(OVT_InfluenceSourceKind kind, OVT_InfluencePolarity polarity)` — the single
     home of the five id strings.
   - `static OVT_InfluencePolarity ResolveProximity(bool hasEnemy, bool hasFriendly)` — the enemy-wins rule.
     **Server-only consumer today**; extracted and tested anyway because it is the rule this overlay exists to
     make legible and it must never be re-implemented.
2. Rewrite `OVT_TownManagerComponent.CheckUpdateModifiers`'s two branches against the helper. **The add/remove
   policy stays in the caller** — the helper returns an outcome; the caller decides what to do with it. That is
   what keeps the tower branch's trailing `else` and the base branch's deliberate absence of one **exactly as
   they are** (§6 K4). The four id strings now come from `ModifierNameFor`.
3. Rewrite `OVT_RevolutionaryMomentumSupportModifier.UpdateAllTownMomentum`'s two predicates against the helper,
   and make `MOMENTUM_RANGE` read from `OVT_InfluenceRules` instead of the class's own `protected const`
   (compile-time only; no wire cost).
4. `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Influence.c` — **10 cases** (§9), each **proven able to
   fail** with the inversion recorded. Register the case class in `OVT_TEST_LogicSuite.c`.

**Acceptance**

- compile **exit 0**, file count **5995 + 2 = 5997**.
- Fast **89 + 10 = 99**, All **127 + 10 = 137**. **Any other movement is a finding.**
- All 10 inversions recorded in `context.md`.
- 🔴 **Behaviour preservation, demonstrated not assumed:** `git diff` on `CheckUpdateModifiers` shows the same
  branch structure, the same comparison operators, the same four id strings and the same add/remove calls in the
  same order. `grep` proves the strings `"NearbyRadioTowerNegative"`, `"NearbyRadioTowerPositive"`,
  `"NearbyBaseNegative"`, `"NearbyBasePositive"` and `"RevolutionaryMomentum"` now appear in **exactly one**
  file, `OVT_InfluenceRules.c`.
- The **Campaign** group is green, which is the only automated evidence the 10-second tick still behaves.

---

### Phase 3 — 🔴 The config JIP append — **S — `network-specialist-advanced`**

> **Advanced despite being small.** `OVT_OverthrowConfigComponent.RplSave`/`RplLoad` is a hand-rolled,
> **positional** 24-value bitstream. A write without its matching read (or in a different order) corrupts every
> field after the mismatch, and the failure is invisible to `compile-check.sh`, to both test groups, and to a
> single-machine session — it appears only on a real client.

**Tasks**

1. Append **two `WriteFloat`s** to `RplSave` — `m_Difficulty.radioTowerRange`, then
   `m_Difficulty.baseSupportRange` — immediately after `disguiseDetectionDistance`, keeping the difficulty block
   contiguous, and **two matching `ReadFloat`s in the same position** in `RplLoad`.
2. Bump `CONFIG_STREAM_VERSION` **1 → 2**. This is what makes the insert position safe: a client on the old
   version aborts the whole load with the existing ERROR line rather than reading shifted garbage. That is the
   designed behaviour (BUG-078) and it means **server and client must be the same build** —
   `tools/launch-server.sh` runs the working tree, so this is automatic, but say it out loud so nobody debugs
   "the config didn't arrive" against a stale client.
3. Edit both methods **in the same commit**, adjacent, and review them as a pair. The existing header comment on
   `CONFIG_STREAM_VERSION` already states this rule; follow it.
4. Add a temporary `Print` of both values behind the layer's `m_bDebugTiming` at map open, so V-8 can read the
   received value directly instead of inferring it from geometry. **Keep it** — it is two values printed once per
   map open behind a debug flag, and it is the only direct evidence the append works.
5. **Investigate, do not assume, whether this is testable.** Check whether `ScriptBitWriter`/`ScriptBitReader`
   can be constructed from script for a round-trip Logic case. If they cannot, record that finding in
   `context.md` and **do not** substitute a test that asserts something else. A structural symmetry lint
   (counting `writer.Write*` vs `reader.Read*` in the file) is a plausible future `dev-ops` tool and is
   explicitly **not** built here.

**Acceptance**

- compile **exit 0**, file count **5997** (no new files).
- Fast **99**, All **137** — unchanged, unless task 5 finds a constructible round-trip, in which case +1 with
  its inversion recorded.
- `git diff` shows the writer and reader diffs are **mirror images**: same two fields, same order, same
  position, and the version constant bumped in the one place it is declared.

---

### Phase 4 — Edge derivation — **M — `component-developer`**

**Tasks**

1. `Scripts/Game/UI/Map/Influence/OVT_InfluenceEdge.c` — a `Managed` record:
   `m_vFrom`, `m_vTo` (world), `m_iSourceFaction`, `m_eState`, `m_eKind`, `m_sModifierName`, `m_iModifierIndex`.
   🔴 **The modifier is carried as data.** The renderer must never contain a modifier name or a
   source-kind ⇒ modifier assumption (§6 K11 requirement 3).
2. `BuildEdges(OVT_MapLocationData selection)` on the layer. One uniform path for all three source kinds
   (§6 K5). It dispatches on `selection.m_sTypeName`:
   - **`OVT_MapLocationTown`** — resolve `m_Towns[selection.m_iID]` (**index-guarded**; the epic already carries
     one unguarded `m_aModifiers[...]` index as a known non-blocker and this feature adds none). Then:
     towers within `radioTowerRange` → edges **into** this town; bases within `baseSupportRange` → edges into it;
     and momentum — if this town is **not** player-held, player-held towns within `MOMENTUM_RANGE` are sources
     **into** it; if it **is** player-held, non-player towns within `MOMENTUM_RANGE` are targets **out of** it.
   - **`OVT_MapLocationRadioTower`** — resolve `m_RadioTowers[m_iID]`; towns within `radioTowerRange` are targets.
     Ring at `radioTowerRange`.
   - **`OVT_MapLocationBase`** — resolve `m_Bases[m_iID]`; towns within `baseSupportRange` are targets. Ring at
     `baseSupportRange`.
   - **Anything else** (house, shop, FOB, camp, vehicle, bus stop, …) — no edges, no ring, empty bucket.
3. **The cross-check** (§6 K2), applied uniformly to every candidate edge:
   `m_sModifierName = OVT_InfluenceRules.ModifierNameFor(kind, PolarityForSource(source))`;
   resolve it to an index by scanning `GetModifierSystem(OVT_TownSupportModifierSystem).m_Config.m_aModifiers`
   for `config.name == m_sModifierName` (there is no public `GetModifierIndexByName`; add one **read-only,
   client-safe** helper on `OVT_TownModifierSystem` rather than scanning inline in the UI); then
   `m_eState = ACTIVE` **iff** the target town's replicated `supportModifiers` contains that index, else
   `SUPPRESSED`.
4. Rebuild triggers and cache invalidation (§6 K5): selection key changed, **or** `m_fRefreshInterval` (default
   **5 s**, matching the marker refresh) elapsed, **or** `m_Towns` / `m_Bases` / `m_RadioTowers` counts differ
   from those recorded at the last build (both occupying-faction arrays are inserted into by JIP `RplLoad`, so
   they can grow mid-session).
5. Manager access: **per-call `OVT_Global` at build time only**, never cached on the layer — `territory-overlay`
   K10's reasoning applies unchanged (a reference cached through a JIP window stays null for the whole map
   session and produces an empty overlay with no error; a per-call lookup self-heals on the next tick). This is
   epic tech debt **T1**: do not add a fourth idiom.
6. Behind `m_bDebugTiming`, print the derived edge count and per-state breakdown on every rebuild — the single
   most useful diagnostic for "the overlay is empty and I don't know why".

**Acceptance**

- compile **exit 0**, file count **5997 + 1 = 5998**.
- Fast **99**, All **137** — unchanged; nothing here is assertable in the test world (no map, no selection).
- With the Phase 1 stub renderer still in place, `Print` output confirms: selecting a town near a tower yields
  ≥ 1 edge with the expected state; selecting a tower yields one edge per town in range; selecting a house
  yields zero.
- A deliberately out-of-range `m_iID` (forced in a debug build) produces no crash and one ERROR.

---

### Phase 5 — The dash renderer and the range ring — **M — `ui-developer`**

**Tasks**

1. Replace the Phase 1 stub `Draw()` with the real emitter:
   - `CacheProjection()` **once** per frame, before any per-vertex work.
   - Per edge: `ProjectWorld` both endpoints → screen length → dash count
     `Clamp(length / (dash + gap), 1, m_iMaxDashesPerEdge)` → derive the **actual period from the clamped
     count**, so at extreme zoom the dashes get _longer_ rather than more numerous (§6 K6). Emit one
     two-vertex `LineDrawCommand` per dash.
   - Per ring: `m_iRingSegments` (default 72) points around the centre at `range × m_MapEntity.GetCurrentZoom()`,
     emitted as **alternating open arcs** — 36 commands, each an independent short polyline.
     `m_bShouldEnclose` is therefore never used and that is deliberate: an enclosed polyline is a _continuous_
     ring, which is the solid fallback in §6 K6, not the dashed one D4 asks for.
2. **Colour.** Promote the byte-for-byte-identical `ResolveFactionArgb` / `ResolveRingColour` bodies from
   `OVT_MapTerritoryLayer` and `OVT_MapRestrictedAreas` into one
   `static int OVT_MapLocationType.GetFactionArgbByIndex(int factionIndex, int alpha)` beside the existing
   `GetFactionColorByIndex`, and route all three layers through it. Both existing bodies fall back to **white**,
   so the extraction is behaviour-preserving by inspection. **This is a `map/core` contract row** (§6 K8).
3. Style is resolved through a **switch on `m_eState` with a default branch** that logs once and falls back to
   the suppressed style — never `if (ACTIVE) … else …` (§6 K11).
4. All tunables as attributes, with the proposed defaults:

   | Attribute                                            | Default    | Purpose                                                              |
   | ---------------------------------------------------- | ---------- | -------------------------------------------------------------------- |
   | `m_iActiveAlpha`                                     | 220        | in-effect edge alpha                                                 |
   | `m_iSuppressedAlpha`                                 | 70         | suppressed edge alpha                                                |
   | `m_fLineWidth`                                       | 2.0        | matches vanilla's waypoint lines                                     |
   | `m_fActiveDashLength` / `m_fActiveGapLength`         | 14 / 10 px | in-effect dash rhythm                                                |
   | `m_fSuppressedDashLength` / `m_fSuppressedGapLength` | 6 / 14 px  | **shorter and sparser** — the second cue                             |
   | `m_iMaxDashesPerEdge`                                | 32         | the command-count bound                                              |
   | `m_iRingSegments`                                    | 72         | 36 emitted arcs                                                      |
   | `m_fRingWidth`                                       | 1.5        | thinner than an edge; the ring is context, the edges are the message |
   | `m_fRefreshInterval`                                 | 5.0 s      | matches the marker refresh                                           |
   | `m_bDebugTiming`                                     | 0          | §5 Phase 7                                                           |

**Acceptance**

- compile **exit 0**, file count **5998**. Fast **99**, All **137**.
- Selecting a town with a nearby enemy tower draws a red dashed line to it; selecting that tower draws a line
  to every town in range **and** a dashed ring at the tower's reach.
- The ring is an **outline** — the map is visible through its interior at every zoom.
- Zooming from fully out to fully in does not multiply the dash count: the emitted command count for a fixed
  selection is stable within the cap (verify with the Phase 7 counter).

---

### Phase 6 — Legibility — **M — `ui-developer`** (build), then user-driven look

> **This phase is a look-then-tune loop, not a build.** `territory-overlay` changed its design after _each_ of
> its two renders; expect the same here and budget for it.

**Tasks**

1. Render on a populated save and judge against §7's primary bar: does a dim line read as _suppressed_ or as
   _broken_? Tune `m_iSuppressedAlpha` and the two suppressed dash lengths until the difference is apparent
   **without a side-by-side comparison**.
2. Check the four visual collisions this layer can have, at three zoom levels each:
   territory fill beneath it, restriction rings beneath it, markers above it, and **two edges of different
   factions crossing** (a town between an enemy and a friendly tower — the canonical D3 picture).
3. Confirm edge colour agrees with the corresponding marker colour for three sources of different factions —
   guaranteed by construction after Phase 5 task 2, checked by eye anyway.
4. Record the shipped values with the observation that justified each, in `context.md`.

**Acceptance**

- The suppressed/active difference is legible at three zoom levels and does not depend on the two being visible
  at once.
- No edge is mistakable for a restriction ring boundary or a territory frontier.
- Markers remain fully legible and untinted above every line.

---

### Phase 7 — Performance: measure, budget, record — **S — user-driven measurement**

**Tasks**

1. Instrument behind `m_bDebugTiming`, copying `OVT_MapTerritoryLayer`'s pattern exactly — including its
   `DRAW_SAMPLE_FRAMES = 60` rolling window and the reason for it (`System.GetTickCount` is integer
   milliseconds, so a sub-millisecond `Draw()` measures as 0 or 1 and only a window estimates it honestly), and
   including reading `OVT_MapCanvasCompositor.GetCompositedCommandCount()` at the **start** of `Draw()` so it
   reports the previous frame's finished composite rather than a partial one.
2. Measure on a **fully populated** campaign — all towns, all bases, all radio towers — with the selection that
   produces the most edges (find it; it will be a town in a dense cluster). Record the edge count.
3. **Budget** (these are the numbers §8 Q-1 checks against):
   - **Emit ≤ 0.5 ms/frame** rolling 60-frame average, _with a selection active_. An order of magnitude less
     geometry than territory, hence a budget well under its 1.5 ms.
   - **≤ 400 own draw commands** with the worst-case selection active, and **exactly 0** with nothing selected.
   - **Edge build ≤ 5 ms**, on selection change and on the 5 s tick.
   - **Composited total** (this layer + territory + rings) recorded, not budgeted — 🔴 **`territory-overlay`'s
     own three numbers were never measured, so there is no inherited total to check against.** Record ours and
     say so.
4. Fallbacks, in order, only if a budget is missed: lower `m_iMaxDashesPerEdge`; lower `m_iRingSegments`; then
   §6 K6's textured single-command route, which is a **new art dependency and a probe**, not a tweak.

**Acceptance**

- All four numbers measured and recorded on a named save with the edge count that produced them.
- An unrecorded number fails this phase even if the overlay feels fine.

---

### Phase 8 — MP/JIP, boundary audit, contract records and docs — **M — `component-developer`** (code+docs) + user-driven gate

**Tasks**

1. **Boundary audit by grep**, all four checks: no `[RplProp]`, no `[RplRpc]`, no `RpcAsk_`/`RpcDo_`, no `EPF_`
   class added; no write to any field of `OVT_TownData`, `OVT_BaseData` or `OVT_RadioTowerData`; nothing added
   to `OVT_PlayerCommsComponent`; every new file under `Scripts/Game/UI/Map/`, `Scripts/Game/Tests/` **or** the
   one named exception `Scripts/Game/GameMode/Systems/Modifiers/OVT_InfluenceRules.c` (§3.5).
2. **Contract records in `docs/features/map/core/context.md`** — that file is the epic's record of the base
   contracts and **it is never changed silently**:
   - a new **`OVT_OverthrowMapUI` selection surface** table (there is no existing table for this class), with
     `GetPanelLocation()` and the `ShowLocationInfo` assignment, naming this feature and stating plainly why
     `m_SelectedElement` is **not** the field to read.
   - a row on the `OVT_MapLocationType` table for `GetFactionArgbByIndex`.
   - an explicit note that this feature added **zero** rows to the `OVT_MapCanvasLayer` table — the second time
     that contract has survived a consumer untouched.
3. Write `docs/features/map/influence-overlay/context.md`: the five spike answers verbatim, the 10 inversions,
   the shipped tunables with their justification, the measured numbers, and a **"where to look when it doesn't
   work"** triage section (§9's table is its seed).
4. Update `docs/features/map/epic-overview.md`: feature 9 row and status.
5. Run the two-client gate (§8 V-8). ⚠️ **Warn the user before launching** — client windows open on their
   desktop and can orphan. Always pass a long `--timeout`.

**Acceptance**

- All four boundary greps clean.
- `core/context.md` carries the new table and the new row, each naming this feature.
- The two-client and JIP criteria (§8 I-4 … I-6) pass.

---

### Phase 9 — Verification gate — **M — user-driven, no agent**

Run §8's Verification Method end to end. **This is the only evidence that exists for most of this feature**:
rendering, colour, alpha, dash geometry, frame cost, the `.conf` module entry, the `.st` id, and every
multiplayer behaviour.

---

### Phase 10 — Help & documentation sync — **S — `help-docs-sync`**

This feature is player-facing, so the closing phase is mandatory.

**Tasks**

1. Extend the Field Manual's map page (`Configs/FieldManual/Categories/FM_Overthrow.conf`) with a short
   influence-overlay section. It must state: what selecting a location draws; that colour is the faction
   exerting the influence; and — the sentence the whole feature depends on — **that a dim line means the
   relation exists but the campaign is not currently applying it**, with the two causes that exist today
   (an enemy source in range out-weighing a friendly one; a sabotaged radio tower).
2. Sync the public wiki.
3. ⚠️ **Every sentence must be backed by a symbol in the tree or cut.** Two tutorial tips have shipped
   inventing mechanics in this project, and no gate catches a well-formed lie. In particular: do **not** write
   that the overlay shows influence "strength", that it shows anything about stability modifiers, or that it
   reveals information the player would not otherwise have — it renders replicated state that is already on the
   info panel.

**Acceptance**

- The Field Manual explains the dim class in one sentence a player can act on.
- No help or wiki text claims the overlay affects the campaign or reveals hidden information.

---

## 6. Key Technical Decisions

### K1 — The client re-derives; source attribution is **not** recorded — **USER DECISION (D1)**

**Rejected: add a source field to `OVT_TownModifierData`.** The class is exactly `{int id; int timer;}` and a
town in range of three enemy towers carries **one** `NearbyRadioTowerNegative` record — added by name once and
thereafter only timer-refreshed. One record cannot hold three sources without changing the stacking model, and
the record shape is serialized in four places: the add/remove broadcast RPCs, the town-manager JIP bitstream,
`ApplyPersistedModifiers` on load, and the persistence save records. The modifier `id` is a **positional index
into `m_aModifiers` that is persisted in saves**, so the config array can never be reordered either. The epic
also forbids the map changing another system's state model.

**Rejected: ship derived edges from the server over a new RPC.** It would make the map a second source of truth
about influence — the exact thing `epic-requirements.md` forbids — for data that is already fully replicated.

**Chosen: re-derive client-side, cross-checked against the replicated modifier list** (K2). The requirements'
caveat — _"if location data isn't currently stored and JIPd with the modifiers we can add it easily enough"_ —
turns out to be already satisfied for the modifier list itself. The only thing genuinely missing was two floats.

### K2 — 🔴 The cross-check is the fail-safe, and it is what makes re-derivation safe

**The rule, in full: an edge is drawn ACTIVE if and only if (a) the geometric relation qualifies under the
shared range test, and (b) the corresponding modifier is actually present in the target town's replicated
`supportModifiers` list. Otherwise it is drawn SUPPRESSED.**

This one rule does a surprising amount of work:

- **It makes the overlay robust to campaign rule changes it does not understand.** Because a solid edge requires
  the modifier to be _present_, the layer **cannot assert an influence the campaign has stopped applying** —
  whatever the reason, and whether or not this layer knows the reason exists. A future job that disables a base,
  or downgrades it to a lesser modifier, renders on day one as a dim edge: geometric relation present, modifier
  absent. Correct, informative, and requiring no code change.
- **It means the client does not re-implement the enemy-wins rule at all.** Suppression falls out. The
  duplication the feature was chartered around is therefore _smaller_ than expected: what has to agree between
  the two machines is the range test and the source→modifier mapping, not the resolution policy.
- **It absorbs the two divergences that genuinely exist.** `RevolutionaryMomentum` has `timeout 3600` but is
  recomputed only on `m_OnTownControlChange`, so a client can legitimately observe it expire while the geometry
  still holds → a dim line, which is true. And a client's `OVT_RadioTowerData.IsDisabled()` reads the raw
  `disabledRemaining` snapshot, which stays true until the server broadcasts `0` on its 9-second
  `CheckRadioTowers` pass → the edge resolves late by up to ~9 s, in agreement with the server rather than ahead
  of it. **Late-and-agreeing beats early-and-wrong**, and it is why the layer does not use the extrapolating
  `GetDisabledRemaining()`.
- **It is why the client never needs `IsDisabled()` at all.** A sabotaged enemy tower is in range (qualifies) and
  its `NearbyRadioTowerNegative` is absent (the server's `else` removed it) → SUPPRESSED. The most tactically
  valuable picture in the feature — _"I took that tower off the air and the town stopped hurting"_ — comes out
  of the general rule with no special case.

### K3 — The dimmed class is defined by the **general rule**, not by today's cause — **USER DIRECTIVE**

**Definition, and this wording is load-bearing:** a _suppressed_ edge is **a relation that exists geometrically
but whose modifier the campaign is not currently applying.**

Today that has exactly two causes, and both are named as **examples**, never as the definition:

1. an enemy source in range out-weighing a friendly one of the same kind (enemy wins over friendly), and
2. a sabotaged radio tower, which broadcasts nothing for either side.

Defined narrowly — "suppressed by an enemy source of the same kind" — a future disabled or downgraded base
produces an edge the layer has no class for. Defined generally, it renders correctly the day that lands. The
narrow definition would also have _missed the sabotage case that already exists_, which is the best available
evidence that the general one is right.

### K4 — The `NearbyBase*` branch's missing `else` is **deliberate and correct** — do not file it, do not code around it

`CheckUpdateModifiers`'s tower branch ends with an `else` that removes both tower modifiers; the base branch has
no `else`. That asymmetry is correct:

- `OVT_OccupyingFactionManager.m_Bases` is **insert-only** — written at world init and in `RplLoad`, never
  removed, never cleared.
- `OVT_BaseData` has **no disabled state at all** — no `disabledRemaining`, no `IsDisabled()`.
- `OVT_BaseData.location` is fixed.
- The two `NearbyBase*` ids are referenced in exactly one place in the codebase: this method.

So for a given town the set of bases within `baseSupportRange` is fixed for the campaign, and
`hasEnemyBase || hasFriendlyBase` can never go true → false. The only reachable transition is a base changing
hands, which the `else if` handles. The tower branch needs its `else` precisely because sabotage _can_ take a
tower off the air.

**Consequences for this feature:** the overlay must **not** be designed around, or defensively coded for, "a
stale base edge with no base in range" — that state does not occur. What _does_ occur, and what D3's dimmed
class must handle, is a friendly base in range being out-weighed by an enemy base in range, which is live and
driven by faction change. And the Phase 2 extraction leaves both branches structurally identical to what they
are today.

### K5 — One uniform derivation path; cache **geometry**, re-evaluate **state** — **USER DIRECTIVE**

Every source kind — towers, bases and momentum towns alike — goes through the same build, and faction, live
state and suppression are re-evaluated on every rebuild for all of them. **No per-source-type fast path.**

An earlier draft proposed caching base candidate pairs on the grounds that base qualification is static (K4
proves it is). That is **retracted.** It is a real invariant but a fragile one to spend, and it buys nothing
measurable: a full rebuild is on the order of 40 `vector.Distance` calls plus a few array scans, which is
cheaper than the bookkeeping needed to avoid it.

What the cache _is_ for is keeping the **per-frame** path to projection and emit only — that is the
`OVT_MapCanvasLayer` contract's own rule ("precompute in world space, project per frame"), not a performance
guess. Rebuild when: the selection key changes, the refresh interval elapses, **or** any of the three source
collections changes count (both occupying-faction arrays are inserted into by JIP `RplLoad` and can grow
mid-session). If measurement ever demands a fast path, whoever adds it must state the invariant it rests on.

### K6 — Dashes are N short `LineDrawCommand`s, with the textured route as a recorded fallback

`LineDrawCommand.m_Vertices` is a **polyline** — consecutive vertices connect — so one command cannot contain
gaps. Two routes exist:

| Route                                                               | Commands/edge                | Cost                                                                                                                                                                                                                                                                                                          |
| ------------------------------------------------------------------- | ---------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Chosen — N short two-vertex lines**                               | ≤ `m_iMaxDashesPerEdge` (32) | Correct by construction, no dependency, no unknown                                                                                                                                                                                                                                                            |
| Rejected for now — one textured line with a dash texture + UV scale | **1**                        | A new art asset, **plus** two unknowns `territory-overlay` recorded and never settled: `m_fUVScale`'s units are undocumented (its own hatch scale was "dialled in by eye"), and whether UVs derive from screen-space vertices — which would make the dashes **slide as the map pans** — was never established |

**The dash count is bounded by construction**, which matters because dash count scales with _screen_ length: the
same 1500 m edge is ~200 px zoomed out and thousands of px zoomed in. `count = Clamp(length / period, 1, max)`
and then the **actual period is derived from the clamped count**, so at extreme zoom the dashes lengthen instead
of multiplying. Without this the layer's command count would be unbounded in the one situation a player is most
likely to be in — zoomed in on the thing they selected.

If Phase 7 misses the command budget, the ordered fallback is: lower the cap; lower the ring segments; then
probe the textured route (a probe, not a tweak). The solid-ring degradation is one enclosed `LineDrawCommand`
with `m_bShouldEnclose = true` — 36 arcs become 1 command — at the cost of D4's "same style".

### K7 — The range ring vs `OVT_MapRestrictedAreas` — they read differently on three axes, with the numbers

`OVT_MapRestrictedAreas` does **not** draw rings around FOBs; it draws **filled** discs around **bases and radio
towers** — the ground where a FOB _cannot_ be deployed. So a selected tower or base will carry both a restricted
disc and an influence ring, concentric. They are distinguishable by:

1. **Fill vs outline.** The restricted disc is a filled `PolygonDrawCommand`; the influence ring is a dashed
   outline with a visible map beneath it.
2. **Scale, by a wide margin.** Tower: restricted **70 m** (`FOB_DEPLOY_TOWER_RANGE`) vs influence **1500 m** —
   a factor of **21**. Base: restricted `baseCloseRange + 50` = **270 m** at the shipped 220 vs influence
   **1000–1500 m** — a factor of **3.7–5.6**. At map zoom the restricted disc is a dot inside the ring.
3. **Persistence.** Restricted discs are always drawn; the influence ring exists only while its location is
   selected.

That is sufficient without changing anything about the restricted layer. 🔴 **`OVT_MapRestrictedAreas`'
geometry, radii and centres are untouched by this feature** — BUG-070 must not regress, and §8 I-3 checks it by
diff.

### K8 — Faction colour: promote the third copy instead of writing it

`OVT_MapLocationType.GetFactionColorByIndex(int)` is already the one implementation of "what colour is faction
N", and it deliberately returns **null** rather than a fallback because the three marker overrides disagree
about what unknown looks like. But the _ARGB packing_ around it has been copied **twice, byte for byte**:
`OVT_MapTerritoryLayer.ResolveFactionArgb` and `OVT_MapRestrictedAreas.ResolveRingColour`, both falling back to
white.

**Rejected: copy it a third time.** Epic tech debt **T1** exists precisely because this codebase let three
manager-access idioms proliferate across ten location types. A third identical body is how the fourth gets
written.

**Chosen: promote it** to `static int OVT_MapLocationType.GetFactionArgbByIndex(int factionIndex, int alpha)`
and route all three call sites through it. Two existing consumers plus this one satisfies the "wait for a second
consumer" rule twice over, and because both existing bodies are byte-identical the extraction is
behaviour-preserving by inspection.

### K9 — Edges are per (source, target) **pair**, not per modifier record

The campaign collapses N sources into one record: a town in range of three enemy towers carries exactly one
`NearbyRadioTowerNegative`. The overlay deliberately does **not** collapse them — it draws three lines, because
which towers matter is the entire question the player is asking. `requirements.md`'s own second example
establishes the principle from the other end ("3 green lines drawn to each town").

The same applies to momentum: the server breaks out of its scan on the first qualifying neighbour, but every
player-held town within `MOMENTUM_RANGE` independently satisfies the condition, so all of them are drawn.

### K10 — One seam for the future intel epic, named but not built

The epic anticipates a fog-of-war/intel epic and warns that this machinery is what it will build on. The
YAGNI-compatible consequence: **every edge is produced by one `BuildEdges` and every edge's colour resolves
through one faction index on the edge record.** An intel epic filters or blanks edges in one place. No
unknown-state handling ships now; there is simply exactly one place it would go.

⚠️ And the negative constraint, restated because it is easy to get wrong: `map/map-layers`' `m_bPlayerVisible`
is recorded as a **presentation preference, never campaign visibility.** This layer's toggle is the same kind
of thing and must not be reused as a knowledge model either.

### K11 — "Designed for, not built for" — what is left open, and why — **USER DIRECTIVE**

A future job may be able to disable a base, or downgrade it to a lesser modifier. **Nothing about that is built
here.** The requirement is only that nothing has to be _undone_ when it lands:

1. **No invariant asserted in a comment.** K4's reasoning lives in this document, not in a code comment that
   says "a base cannot be disabled". Nothing in the shipped code depends on it.
2. **`OVT_InfluenceEdgeState` is switched on with a `default:` branch**, never treated as a boolean. A third
   value slots in without touching the renderer's control flow.
3. **The renderer contains no modifier name and no source-kind ⇒ modifier assumption.** Each edge carries
   `m_sModifierName` / `m_iModifierIndex` as **data**, resolved by `OVT_InfluenceRules`. The rules decide what
   an edge _means_; the layer only draws it. One source kind mapping to different modifiers over time is
   therefore a change in one function.
4. **No `else` is omitted "because it cannot happen"** in any new code.
5. **No speculative attribute, no disabled-base flag, no strength/magnitude channel.** YAGNI stands.

---

## 7. Quality Bar

**This is a rendering feature whose entire output is invisible to every automated gate.** `compile-check.sh`
cannot see a pixel, a colour, an alpha, a dash, a `.conf` entry or an `.st` id, and neither can either test
group. Correctness is necessary and nowhere near sufficient: a geometrically perfect overlay whose dim lines
read as a rendering fault has failed at the thing it was built for.

**Legibility — the primary bar.**

- 🔴 **A dim line reads as "suppressed", not as "broken".** Apparent to someone who has not read this document,
  **without a side-by-side comparison with a solid line**. Two independent cues carry it — alpha _and_ a
  shorter, sparser dash rhythm — because either one alone is a plausible rendering artefact.
- Terrain, roads, contours and place names stay readable **under** every line at three zoom levels.
- All markers remain fully legible and untinted above the overlay.
- Edge colour is unambiguously the **source's** faction, and agrees with that source's marker colour.
- The ring reads as an outline, not as a boundary of a filled region, and is not mistakable for a restriction
  disc or a territory frontier.
- Two crossing edges of different factions remain individually traceable.

**Measured cost — a hard gate.**

- Emit ≤ **0.5 ms/frame** with a selection active; ≤ **400** own commands worst case; **exactly 0** commands
  when nothing is selected; build ≤ **5 ms**. Measured on a populated save with the edge count recorded.
  🔴 An unmeasured feature fails this bar regardless of how it feels, and there is **no inherited budget** —
  `territory-overlay`'s three numbers were never recorded.

**Structural.**

- No `[RplProp]`, no RPC, no EPF, no write to any campaign record, nothing added to `OVT_PlayerCommsComponent`.
- The Phase 2 extraction is **behaviour-preserving**, demonstrated by diff and by the four id strings existing
  in exactly one file.
- The Phase 3 bitstream diff is a **mirror image** — reviewed as a pair, version bumped.
- Contract extensions are **recorded** in `core/context.md`, naming this feature. Nothing changes silently.
- No `file:line` pointers in new code comments (epic **K-9**). `docs/` citations are exempt.
- Every new Logic case is **proven able to fail**, with the method recorded. ❌ No `maxAttempts`.

---

## 8. Definition of Done

Written so an evaluator with **no implementation context** can verify each item.

### Functional

**F-1 — Selecting a town shows what influences it.** Open the fullscreen map in a started campaign and hover a
town that has a `Nearby Radio Tower` chip on its info panel. A dashed line is drawn from that town to the radio
tower responsible. Its colour is the tower's controlling faction's colour, and it matches that tower's marker
colour.

**F-2 — Selecting a source shows what it influences.** Hover a radio tower that is within its reach of two or
more towns. One dashed line is drawn to **each** of those towns, all in the tower's faction colour. Repeat with
a military base: same behaviour, **no visual difference in the lines** other than length and colour.

**F-3 — Momentum draws in both directions.** Hover a town your faction controls that has another town within
2 km which is **not** yours and which shows a `Revolutionary Momentum` chip: a line is drawn **out** to it.
Now hover that second town: the same line is drawn, from the same pair, now **in**.

**F-4 — In-effect edges are solid; suppressed edges are dim and sparser.** Find a town with **both** an enemy
and a friendly radio tower in range (its panel will show `Nearby Radio Tower` with a _negative_ effect). Hover
it. **Two** lines are drawn: the enemy tower's at full alpha with the longer dash rhythm, the friendly tower's
visibly dimmer and with shorter, sparser dashes. The difference is apparent without covering one of them up.

**F-5 — A sabotaged tower's edge goes dim, and comes back.** Sabotage an enemy radio tower that is in range of a
town. Within ~10 seconds, that town's edge to it changes from solid to dim (the town's `Nearby Radio Tower` chip
disappears at the same time). When the sabotage timer expires, both come back.

**F-6 — A location with no relations draws nothing, and that is distinguishable from a broken layer.** Hover a
town with none of the three modifiers on its panel: **no lines**. Confirm the layer is alive by then hovering a
radio tower — its **ring** appears even when it influences no town, and a house or shop draws nothing at all by
design. _(Accepted, recorded ambiguity: an empty draw is the correct output for an uninfluenced town, and the
two checks above are how it is told apart from a fault.)_

**F-7 — The range ring is an outline at the right radius.** Hover a radio tower: a dashed circle appears,
centred on it, at its reach. The map is fully visible **inside** the ring — it is not a filled disc. Its
towns-in-range are exactly the towns with lines to them. Repeat for a base.

**F-8 — The ring is not the restriction disc.** With the same tower selected, the small filled FOB-restriction
disc (70 m) and the large dashed influence ring (1500 m) are both visible and obviously different things.

**F-9 — The overlay follows the panel exactly.** Sweep the cursor across several markers: lines appear and
disappear **in lockstep with the info panel**, never lagging it and never persisting after it closes. Pin a
location, move the cursor away: the panel stays and **so do the lines**. Press the panel's close button: both go.

### Quality

**Q-1 — Measured frame cost, recorded.** On a **fully populated** campaign save (all towns, bases and radio
towers — record the counts), with `m_bDebugTiming 1`, and with the highest-edge-count selection active:
rolling 60-frame `Draw()` average ≤ **0.5 ms**; own draw commands ≤ **400**; commands **exactly 0** with nothing
selected; edge build ≤ **5 ms**. Every number recorded in `context.md`. An unrecorded number fails this
criterion.

**Q-2 — Zooming does not multiply commands.** With a fixed selection, zoom from fully out to fully in. The own-
command count stays within the cap (it should be flat once the cap binds). No stutter a player would notice.

**Q-3 — Degrades on null or partial state.** All of the following produce no crash and no spam:
opening the map before any campaign manager has resolved (join a server and open the map immediately); a
selection whose `m_iID` is out of range for its manager array; a faction index that resolves to nothing (the
line falls back to white, matching both existing layers).

**Q-4 — No regression to the sibling layers.** With the influence overlay live: territory fill still renders,
restriction discs still render **on top of** territory and **underneath** the influence lines, and all three are
visible simultaneously at three zoom levels. _(If any vanishes, the compositor is the suspect — and the symptom
reads exactly like a broken influence layer.)_

**Q-5 — Every new Logic case is proven able to fail.** For each of the 10, the inversion that turned it red is
recorded in `context.md`. `grep -rn` over `Scripts/Game/Tests/TestSuites/Logic/` finds no `maxAttempts`, and the
new case file mentions neither Overthrow's static manager accessor nor the engine's game-mode getter —
**including in comments** (a previous feature tripped exactly this by quoting the rule in its own header).

**Q-6 — No `file:line` in new code comments** (epic K-9).

### Integration

**I-1 — The `map/map-layers` row appears and toggles this layer only.** Open the layer-filter panel: a row
labelled with the influence layer's name is present alongside Territory and Restricted Areas. Unchecking it
makes the lines and ring vanish **while territory and the restriction discs keep drawing and keep updating** as
the map is panned. Rechecking brings them back instantly. Close and reopen the map: the setting persisted.

**I-2 — 🔴 The shared rule set produces identical results to the pre-refactor server.** All of:

- `git diff` on `OVT_TownManagerComponent.CheckUpdateModifiers` shows the same branch structure, the same
  comparison operators (`<` for both proximity tests), the same add/remove calls in the same order, and the
  **tower branch's `else` and the base branch's absence of one both unchanged**;
- `grep -rn` finds each of the five modifier id strings in **exactly one** file, `OVT_InfluenceRules.c`;
- the **Campaign** test group is green;
- in game, a town's `Nearby Radio Tower` / `Nearby Base` / `Revolutionary Momentum` chips appear and disappear
  under the same conditions as before the refactor — checked by capturing a base and sabotaging a tower.

**I-3 — BUG-070 has not regressed.** `git diff` shows **no** change to the radius sources in
`OVT_MapRestrictedAreas`. In the world, FOB deployment is still refused just inside and permitted just outside a
base ring and a radio-tower ring.

**I-4 — 🔴 The two new JIP floats arrive correctly on a real client.** On a dedicated server configured to a
difficulty **other than Normal** (see V-8), a connected client's `m_bDebugTiming` print at map open reports the
**server's** `baseSupportRange` (e.g. 1250 at Hard), not 1000. _(1000 is what the client's prefab-instantiated
`Difficulty_Normal.conf` holds, which is why Normal cannot be used to test this.)_

**I-5 — Two clients agree.** Two clients on one dedicated server select the same town at the same time and see
the same edges, in the same colours, with the same solid/dim classification.

**I-6 — JIP agreement.** A client joining a campaign with accumulated state opens the map and sees the same
edges as the established client, including for bases and towers that changed hands before it joined. If the
first selection is empty, it corrects itself within one refresh interval **without closing the map**.

**I-7 — The overlay is a projection, not a mechanic.** All of:

- `git diff` shows no `[RplProp]`, no `[RplRpc]`, no `RpcAsk_`/`RpcDo_` and no `EPF_` class added anywhere;
- no write to any field of `OVT_TownData`, `OVT_BaseData` or `OVT_RadioTowerData`;
- nothing added to `OVT_PlayerCommsComponent`;
- every new script file is under `Scripts/Game/UI/Map/`, `Scripts/Game/Tests/`, or the single named exception
  `Scripts/Game/GameMode/Systems/Modifiers/OVT_InfluenceRules.c`.

**I-8 — The respawn map is unchanged.** Die and open the respawn screen: no lines, no rings, no new drawing.

**I-9 — Contract records exist.** `docs/features/map/core/context.md` carries the new `OVT_OverthrowMapUI`
selection-surface table (including why `m_SelectedElement` is the wrong field), the `GetFactionArgbByIndex` row,
and the explicit note that zero rows were added to the `OVT_MapCanvasLayer` table.

### Verification Method

Run in order. Stop and fix at the first failure.

**V-1 — Compile.** `tools/compile-check.sh` → exit **0**, **5998** files (baseline 5994 + 4 new `.c`). Any other
delta is a finding to investigate.

**V-2 — Automated tests against the measured baselines.**
`tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0**, **99** tests (baseline 89).
`tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0**, **137** tests (baseline 127).
The +10 are the new Logic cases and are the **only** expected change. **A count that moved for any other reason
is a finding, never a number to update.**

**V-3 — 🔴 Workbench clean load.** Open the project in Workbench; zero load errors. **This is the only gate that
can see the `Configs/Map/MapOverthrow.conf` module entry and its new GUID.** Confirm the entry loads, and
`grep -rn "{6A86"` shows each new GUID used exactly where intended and nowhere else. A duplicate or dangling
GUID passes every automated gate and fails in the world.

**V-4 — Single-player visual pass.** Load a started campaign at **Normal**. Run **F-1 … F-9**, **Q-2**, **Q-4**,
**I-1**, **I-3**, **I-8**. F-5 needs a sabotage, so allow time for it.

**V-5 — Legibility judgement.** With a fresh eye, answer the primary bar's question directly: _does a dim line
read as suppressed, or as broken?_ If broken, tune and re-run V-4's F-4.

**V-6 — Performance pass.** Load the fully-populated save, set `m_bDebugTiming 1`, run **Q-1**, record all four
numbers plus the edge count and the selection that produced them. Set `m_bDebugTiming 0` afterwards **except**
for V-8, which needs the range print.

**V-7 — Resilience pass.** Run **Q-3**'s three cases.

**V-8 — 🔴 Two-client MP + JIP, at a NON-NORMAL difficulty.** ⚠️ **Warn the user before launching — client
windows open on their desktop and can orphan.**

1. Set the server's `Overthrow_Config.json` `difficulty` to **`"Hard"`** (`baseSupportRange` **1250**;
   `DoStartGame` selects the preset by matching this string against `preset.name`). **Do not use Normal** —
   a client already holds Normal's 1000 from its prefab, so the whole criterion would pass with the JIP append
   missing. Extreme or Insane (1500) work equally well.
2. `tools/launch-server.sh`, then two clients:
   `tools/launch-game.sh --timeout 3600 --profile <name> --allow-concurrent -- -client 127.0.0.1:2001`.
   **Always pass the long timeout** — the 600 s default kills the client mid-test.
3. With `m_bDebugTiming 1`, open the map on client 1 and read the printed `baseSupportRange`: it must be
   **1250**. That is **I-4**, and it is a direct check of the append rather than an inference from geometry.
4. If a base/town pair exists between 1000 m and 1250 m apart, confirm client 1 now draws that edge (it would
   have been missing before the append). Secondary evidence; the print in step 3 is primary.
5. Run **I-5** with both clients selecting the same town.
6. **JIP:** with client 1 still connected, capture a base and sabotage a tower, **then** connect client 2 and
   run **I-6**. This is the only step that can distinguish "replicated" from "was there when I joined".
7. Confirm selection is **per client**: selecting a location on client 1 draws nothing on client 2.

**V-9 — Boundary and contract audit.** Run **I-7**'s four greps/diffs, **I-2**'s three static checks, and
**I-9**.

---

## 9. Testing Strategy

**Automated coverage is a thin spine, and this section is honest about where it ends.** Baselines **measured**
2026-08-11: compile **exit 0 / 5994 files**, Fast **89**, All **127**. Both groups must be green at every phase
boundary.

| Tier                                  | Can it cover this feature?                                                                                                                                                                                                                                                                             |
| ------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Logic** (world-free)                | ✅ **Yes, and this is the feature's only automatable surface.** `OVT_InfluenceRules` is pure arithmetic and boolean resolution on hand-built values. The split is not aesthetic — it is what makes any of this testable, and it is the same reason `map/respawn` split its eligibility predicates out. |
| **Init** (managers resolve)           | ⚠️ **Marginal, and declined.** A case asserting the difficulty fields are reachable would assert the _config component_, which already has coverage, and would not touch the wire at all. **No new Init cases — YAGNI**, matching `territory-overlay`'s call.                                          |
| **Campaign** (started-campaign state) | ⚠️ **Existing cases only, and they matter.** No new cases, but the Campaign group is the only automated evidence that Phase 2's rewrite of the 10-second modifier tick did not break it. It must be green at Phase 2's boundary and is named in **I-2**.                                               |
| **Persistence**                       | ❌ **No, deliberately.** This feature persists nothing. A Persistence case becoming relevant would mean the §3.5 boundary had been breached.                                                                                                                                                           |

### Logic cases (Phase 2) — 10, each deterministic and built with `new`

| #   | Case                                                                       | Asserts                                                                                      |
| --- | -------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------- |
| 1   | `IsProximitySource` just inside a range                                    | Qualifies                                                                                    |
| 2   | `IsProximitySource` at **exactly** the range                               | Does **not** qualify — the server's `<` is strict                                            |
| 3   | `IsProximitySource` with a large **Y** delta, in-range in XZ but out in 3D | Does not qualify — pins `vector.Distance` (3D) against a plausible XZ implementation         |
| 4   | `IsMomentumSource` at **exactly** `MOMENTUM_RANGE`                         | **Does** qualify — momentum uses `<=` while proximity uses `<`, and that asymmetry is real   |
| 5   | `ResolveProximity(true, true)`                                             | `NEGATIVE` — **enemy wins over friendly**, the rule the whole overlay exists to make legible |
| 6   | `ResolveProximity(false, true)` / `(false, false)`                         | `POSITIVE` / `NONE`                                                                          |
| 7   | `PolarityForSource(true)` / `(false)`                                      | `NEGATIVE` / `POSITIVE` — occupying ⇒ negative                                               |
| 8   | `ModifierNameFor` over all five valid (kind, polarity) combinations        | The exact five id strings, so a rename cannot silently desync from `supportModifiers.conf`   |
| 9   | `TownQualifiesForMomentum` for a player-held and a non-player-held town    | Player-held towns are **not** momentum targets                                               |
| 10  | `MOMENTUM_RANGE` is exactly `2000.0`                                       | Pins the constant that moved out of a `protected const` on the modifier class                |

**Proving each can fail — the inversions to run, batched, with the result recorded per case:**

- Change `<` to `<=` in `IsProximitySource` → case 2 red, case 1 green.
- Swap `vector.Distance` for an XZ-only distance → case 3 red, cases 1–2 green (which is _why_ case 3 exists).
- Change `<=` to `<` in `IsMomentumSource` → case 4 red.
- Swap the `hasEnemy`/`hasFriendly` precedence in `ResolveProximity` → case 5 red, case 6 green.
- Invert `PolarityForSource` → cases 5 and 7 red.
- Typo one id string in `ModifierNameFor` → case 8 red.
- Drop the `!=` in `TownQualifiesForMomentum` → case 9 red.
- Change `MOMENTUM_RANGE` to 2500 → cases 4 and 10 red.

❌ **No `maxAttempts`.** Every case is deterministic by construction; one that needs a retry is a bug in the case.

⚠️ **The tier rule is enforced by a reviewer grep that does not distinguish code from prose.** Neither
Overthrow's static manager accessor nor the engine's game-mode getter may appear anywhere in the new test file,
**including comments**.

### What no automated gate can see — which is most of this feature

Every line, dash, colour and alpha; the ring; the range-ring geometry; frame cost; the compositor's actual
output; the `Configs/Map/MapOverthrow.conf` module entry and its GUID; the `.st` id; the layers-panel row; the
gamepad path to it; the selection bridge at runtime; **the JIP append**; and all multiplayer behaviour. That is
why §8's Verification Method is nine steps, why V-3 (Workbench load) is called out separately, and why V-8 names
a specific difficulty rather than saying "test in MP".

### Debugging: four signatures that cover nearly every silent failure

| Symptom                                                         | Most likely cause                                                                                                                                        | First check                                                                                                                                                                                                                    |
| --------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Nothing draws, ever**                                         | `GetPanelLocation()` returns null — the UI component was not resolved (module vs UI-component init order) or the getter was never wired                  | The Phase 4 debug print of the selection key. Null every frame ⇒ the bridge; non-null with zero edges ⇒ the derivation                                                                                                         |
| **Everything draws dim**                                        | The cross-check is failing for every edge — the name→index resolution returned `-1`, or the wrong modifier **system** was fetched (support vs stability) | Print the resolved index for one known-good edge. `-1` means the name scan; a valid index with an absent modifier means the wrong system                                                                                       |
| **Edges appear on the host and not on a client, or vice versa** | The range values disagree — the Phase 3 append is missing, mis-ordered, or the client is a stale build                                                   | The `m_bDebugTiming` range print on both machines. If they differ, it is the stream; if they match, it is the modifier list                                                                                                    |
| **Lines render but territory or the restriction discs vanish**  | The compositor is not composing — a layer called `SetDrawCommands` itself, or a bucket's frame stamp is never current                                    | `OVT_MapCanvasCompositor.GetCompositedCommandCount()` at frame start. If it equals one layer's own count, the flush path is wrong. **This reads exactly like a broken influence layer and will be debugged in the wrong file** |

---

## 10. Dependencies

### Internal (code — read-only unless noted)

- **`map/core`** — `OVT_MapCanvasLayer` (extended by **nothing**; consumed as-is), `OVT_MapCanvasCompositor`,
  `OVT_OverthrowMapUI` (**one new getter + one assignment**, §3.3), `OVT_MapLocationType`
  (**one new static**, K8), `MapOverthrow.conf`, `OVT_MapLocationData`. Two contract records are owed to
  `core/context.md` (Phase 8).
- **`map/territory-overlay`** — everything this layer stands on: the compositor, `m_iDrawOrder` / `m_sLayerId` /
  `m_sDisplayName` / `SetLayerVisible`, `CacheProjection` / `ProjectWorld`, and the removed `Count() > 0` guard.
  Its `m_bDebugTiming` / `DRAW_SAMPLE_FRAMES` measurement pattern is copied deliberately. 🔴 **Its performance
  numbers were never recorded, so no budget is inherited.**
- **`map/map-layers`** — the free toggle row and its per-profile persistence. Consumed with **zero** code change
  in feature 7; verified rather than assumed (Phase 1 P5, DoD I-1).
- **`map/location-types`** — `OVT_MapLocationTown` / `Base` / `RadioTower` all set `locationData.m_iID = i`,
  which is what makes a selection resolvable back to a campaign record. Also the client-side modifier-chip
  reading path this feature's cross-check mirrors.
- **`map/respawn`** — the "one rule set, two machines" precedent, and `MapRespawn.conf`, untouched (DoD I-8).
- **`towns/core`** — `OVT_TownData.location` / `.faction` / `.supportModifiers`, `m_Towns`,
  `CheckUpdateModifiers` (**modified, behaviour-preserving**), `OVT_TownModifierSystem` (**one read-only,
  client-safe name→index helper added**), `OVT_RevolutionaryMomentumSupportModifier` (**modified**).
- **`occupying/core`** — `m_Bases`, `m_RadioTowers`, `OVT_BaseData` / `OVT_RadioTowerData` `location` +
  `faction` + `IsOccupyingFaction()` + `IsDisabled()`, and both live-change RPC paths.
- **`core/game-mode`** — `OVT_OverthrowConfigComponent.RplSave`/`RplLoad` (**modified**, Phase 3),
  `GetPlayerFactionIndex()`, `GetOccupyingFactionIndex()`, and `OVT_Global`'s accessors.
- **`resistance/fob`** — only indirectly: `OVT_MapRestrictedAreas`' radii must not move (BUG-070, DoD I-3).

### Vanilla (read-only, never modified)

`SCR_MapEntity.GetMapUIComponent`, `SCR_MapModuleBase`, `LineDrawCommand` (`EnWidgets.c`),
`CanvasWidget.SetDrawCommands`, and `SCR_WaypointLinesEditorUIComponent` as the working `LineDrawCommand`
precedent (`m_fWidth 2`, `m_fOutlineWidth 0`, packed-int colour, flat screen-space vertex array).

### External — user / Workbench work

| Item                                                            | Blocking?                          | Notes                                                                                                                                                   |
| --------------------------------------------------------------- | ---------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Workbench clean-load check (V-3)                                | **YES**                            | The only gate that can see the `.conf` module entry and its GUID                                                                                        |
| Regenerate the six `localization_Overthrow.<lang>.conf` exports | **For the layer's row label only** | One new id. It renders as a raw key until regenerated. ❌ **Never hand-edit the exports** — `Language/localization_Overthrow.st` is the editable master |
| A populated campaign save                                       | **YES** for Q-1/V-6                | The measurement is meaningless on an early-game world                                                                                                   |
| A dedicated server at **Hard** (or Extreme/Insane)              | **YES** for I-4/V-8                | Set via `Overthrow_Config.json`'s `difficulty` string. **Normal cannot test the JIP append**                                                            |
| Two-client MP session (V-8)                                     | **YES** for I-4…I-6                | Long `--timeout`, distinct `--profile`. Warn before launching                                                                                           |
| Art                                                             | **NO**                             | The chosen dash route needs none. Only §6 K6's rejected textured fallback would                                                                         |

### New and changed files

```
Scripts/Game/GameMode/Systems/Modifiers/
├── OVT_InfluenceRules.c                       NEW  PURE shared predicates + 3 enums + MOMENTUM_RANGE
│                                                   (the one file outside UI/Map — it is the CAMPAIGN's
│                                                    rule set; the map is its second caller, §3.5)
└── Support/OVT_RevolutionaryMomentumSupportModifier.c   ~  predicates via the rules; MOMENTUM_RANGE moved

Scripts/Game/GameMode/Managers/
└── OVT_TownManagerComponent.c                  ~   CheckUpdateModifiers via the rules (BEHAVIOUR-PRESERVING)

Scripts/Game/GameMode/Systems/
└── OVT_TownModifierSystem.c                    ~   + read-only, client-safe name→index helper

Scripts/Game/GameMode/Managers/
└── OVT_OverthrowConfigComponent.c              ~   +2 floats in RplSave/RplLoad, CONFIG_STREAM_VERSION 1→2

Scripts/Game/UI/Map/
├── Influence/
│   ├── OVT_MapInfluenceLayer.c                NEW  poll → derive → emit; all config attributes
│   └── OVT_InfluenceEdge.c                    NEW  Managed record (carries its modifier as DATA)
├── OVT_OverthrowMapUI.c                        ~   + GetPanelLocation(); m_PanelLocation set in ShowLocationInfo
├── Core/OVT_MapLocationType.c                  ~   + static GetFactionArgbByIndex (K8)
├── Territory/OVT_MapTerritoryLayer.c           ~   ResolveFactionArgb → the shared static
└── Visualization/OVT_MapRestrictedAreas.c      ~   ResolveRingColour → the shared static (GEOMETRY UNCHANGED)

Scripts/Game/Tests/TestSuites/Logic/
├── OVT_TEST_Logic_Influence.c                 NEW  10 world-free cases
└── OVT_TEST_LogicSuite.c                       ~   register the new case class

Configs/Map/
└── MapOverthrow.conf                          ~   + OVT_MapInfluenceLayer entry (fresh {6A86…} GUID,
                                                     m_iDrawOrder 300, m_sLayerId, m_sDisplayName)

Language/localization_Overthrow.st              ~   1 layer-name id (master only; exports are the user's)

Configs/FieldManual/Categories/FM_Overthrow.conf ~  influence-overlay section (Phase 10)

docs/features/map/
├── influence-overlay/context.md               NEW  spike answers, inversions, tunables, measurements, triage
├── core/context.md                             ~   OVT_OverthrowMapUI selection table + GetFactionArgbByIndex
└── epic-overview.md                            ~   feature 9 status
```

> **GUIDs:** allocate from the free **`{6A86…}`** series — measured 2026-08-11: zero uses tree-wide
> (`{6A87}`…`{6A8A}` also free; `{6A85}` has 6 and belongs to `map-layers`). `grep -rn` each new GUID before
> committing.
>
> **Bug ids:** highest allocated is **BUG-145** (`ls docs/bugs/` before allocating — several exist as
> _untracked_ files, so `git log` will lie).

---

## 11. Risks & Mitigation

| #       | Risk                                                                                                                                                                                                          | Likelihood                             | Impact   | Mitigation                                                                                                                                                                                                                                                                                                                    |
| ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------- | -------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **R1**  | 🔴 **A dim line reads as a rendering fault, not as "suppressed".** This is the feature's headline capability and the failure is a judgement call no gate can make.                                            | **Medium**                             | **High** | Two independent cues (alpha **and** a shorter, sparser dash rhythm), both config attributes; Phase 6 is a dedicated look-then-tune loop; §7 makes it the primary bar and §8 F-4 requires the difference to be apparent **without a side-by-side comparison**; Phase 10's Field Manual sentence is the durable explanation.    |
| **R2**  | 🔴 **The JIP append is written but the read is wrong**, silently corrupting every field after it. Invisible to `compile-check.sh`, to both test groups, and to a single-machine session.                      | Low                                    | **High** | Phase 3 is `network-specialist-advanced`, both halves edited in one commit and reviewed as a mirror-image diff; `CONFIG_STREAM_VERSION` bumped so a mismatched build **aborts loudly** rather than reading shifted garbage; V-8 step 3 reads the received value directly off a client instead of inferring it.                |
| **R3**  | 🔴 **V-8 is run at Normal and proves nothing.** The client already holds Normal's `baseSupportRange`, so the criterion passes with the append entirely missing.                                               | **Medium** (it is the obvious default) | **High** | §4 D-JIP, §8 I-4 and V-8 all name the difficulty explicitly and state _why_ Normal cannot work; the header block flags the corrected premise so a reader who only skims the top still gets it.                                                                                                                                |
| **R4**  | **The Phase 2 extraction changes campaign behaviour.** It rewrites a method on the server's 10-second tick governing four permanent modifiers.                                                                | Low                                    | **High** | `component-developer-advanced`; the helper returns an **outcome** and the add/remove policy stays in the caller, so both branch structures — including the deliberate base-branch asymmetry — are untouched; DoD **I-2** checks the diff, the single-file grep, the green Campaign group, **and** the in-game chip behaviour. |
| **R5**  | **The selection bridge is driven from `m_SelectedElement`** (the obvious-looking field) and lines outlive the panel on hover-away.                                                                            | **Medium**                             | Medium   | §3.3 names the trap and the reason; `m_PanelLocation` is the panel's own field, so agreement is by construction; DoD **F-9** tests the sweep, the pin and the close button explicitly.                                                                                                                                        |
| **R6**  | **Command count explodes when zoomed in.** Dash count scales with _screen_ length, and the player is most likely to be zoomed in on the thing they just selected.                                             | **Medium**                             | Medium   | The dash count is clamped and the **period derived from the clamped count**, so dashes lengthen instead of multiplying (K6); Q-2 tests the zoom sweep directly; the ordered fallback ends at the textured single-command route.                                                                                               |
| **R7**  | **A layer that draws nothing looks broken.** An uninfluenced town legitimately produces an empty bucket.                                                                                                      | **Medium**                             | Low      | DoD **F-6** names the two ways to tell them apart (a tower's ring appears even with no lines; the panel's modifier chips corroborate); the Phase 4 edge-count debug print is the diagnostic; §9's triage table leads with this signature.                                                                                     |
| **R8**  | **The compositor is blamed on the wrong layer.** Four registered layers now share one canvas; "the influence overlay broke the restriction rings" is a plausible-sounding, wrong diagnosis.                   | Low                                    | Medium   | Phase 1 P3 confirms four-layer composition **before** anything depends on it; DoD **Q-4** is a dedicated criterion; §9's triage table states the misdiagnosis in as many words.                                                                                                                                               |
| **R9**  | **The overlay disagrees with the campaign after a future rule change** it does not understand.                                                                                                                | Low (after K2)                         | Medium   | K2's cross-check makes a solid edge _require_ the modifier to be present, so the layer cannot assert an influence the campaign has stopped applying; K11 keeps the enum, the mapping and the comments open for the disabled/downgraded-base case without building it.                                                         |
| **R10** | **A third copy of the faction-ARGB body ships**, and the fourth follows.                                                                                                                                      | Medium                                 | Low      | K8 promotes the two existing byte-identical copies into one static instead. Epic tech debt **T1** is the precedent that this is how the problem starts.                                                                                                                                                                       |
| **R11** | **`.conf` / `.st` faults pass every automated gate.** This feature adds a module entry with a new GUID and a localization id.                                                                                 | **High**                               | Medium   | **V-3 is mandatory** and is the only evidence those files are sound, with a `grep -rn "{6A86"` uniqueness check; the GUID series was measured unused; the `.st` master is the only file touched and the exports stay the user's.                                                                                              |
| **R12** | **Parallel sessions commit to this tree mid-feature.** This epic has a documented history of it, and the gate counts in `CLAUDE.md`, `territory-overlay` and `map-layers` all disagree with each other today. | Medium                                 | Low      | Phase 0's numbers were **run, not quoted**; re-check `git status`, the highest `docs/bugs/` id and all three gate numbers at every phase boundary; commit per phase so there is a revert path.                                                                                                                                |

---

_Plan created 2026-08-11. §5 Phase 0's baselines were **measured, not quoted**: compile exit 0 / 5994 files,
Fast 89, All 127, free GUID series `{6A86…}`, highest bug id BUG-145, tree clean at `a4e71f41`. This plan
**corrects two premises in its own research brief** — the client's difficulty values come from
`Difficulty_Normal.conf` rather than from the attribute defaults (which moves the MP verification difficulty),
and the `NearbyBase_`branch's missing`else` is deliberate and correct rather than a campaign bug. Both
corrections are recorded rather than silently applied.\*
