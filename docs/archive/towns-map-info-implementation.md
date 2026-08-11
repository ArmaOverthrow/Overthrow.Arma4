# Towns Map Info - Implementation Plan (Retrospective)

> ## 📦 ARCHIVED 2026-08-10 — superseded by the `map` epic
>
> **Was:** `docs/features/towns/map-info/implementation.md` (feature 5 of the `towns` epic).
> **Successor:** `docs/features/map/` — see `epic-overview.md` and the features `core`,
> `location-types`, `fast-travel`, `legacy-retirement`.
>
> This document describes Overthrow's **legacy** map: the hand-rolled `OVT_MapIcons` icon layer, the
> three flag-based modes inside `OVT_MapContext` (map info / fast travel / bus travel), the
> "Map Info" and "Fast Travel" main-menu rows, and the client-side travel paths. **That code no longer
> exists.** It was deleted by `map/legacy-retirement` on **2026-08-10** (`OVT_MapIcons.c`, three
> layouts and their `.meta` files, the `OVT_MapIcons` config block, `OVT_MapContext` stripped
> 591 → 79 lines, and four unvalidated `RequestFastTravel*` RPCs removed from
> `OVT_PlayerCommsComponent`).
>
> **Read this only as history.** Its `file:line` pointers, line counts and code descriptions are no
> longer resolvable. Of the bugs it records: **BUG-067, BUG-068 and BUG-069 are structurally
> impossible** against the current code — the classes that could exhibit them are deleted. **BUG-070
> is still live** and belongs to the `map` epic: it concerns `OVT_MapRestrictedAreas`, which
> `legacy-retirement` deliberately **retained** and did not touch. The "unmerged `new-map` branch"
> this document repeatedly points at as the designed successor is exactly what the `map` epic landed.

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (pre-dates Beast Mode; a redesigned successor exists on the unmerged `new-map` branch)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-03 00:40

---

## Executive Summary

Overthrow extends the vanilla fullscreen map by a **layered config delta at vanilla's own GUID** (`Configs/Map/MapOverthrow.conf`), appending two canvas modules (`OVT_MapRestrictedAreas`, `OVT_MapThreatGrid` — the latter shipped disabled) and two widget components (`OVT_MapIcons`, `OVT_MapPlayerLocation`). `OVT_MapContext` (the only map-driving UI context, on the player character) owns three flag-based modes — **map info** (click a spot → nearest town's panel: faction, name, population, distance, stability, support, modifier chips), **fast travel** (anchor whitelist + wanted/QRF/distance rules + payment) and **bus travel** (distance-priced teleport between bus-stop markers). `OVT_MapIcons` draws one widget per POI/house/warehouse/dealer/shop/port/camp/FOB/vehicle/tower/job-waypoint with zoom-ceiling decluttering and an RplId retry/fallback subsystem for replication races.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase (`/discover-feature`, 2026-08-02). The feature has already been implemented and shipped.

---

## Goals

### Primary Goals

- Surface the campaign state (town records, faction control, restrictions, own assets) on the vanilla map without reimplementing it.
- Provide the two map-driven verbs: fast travel and bus travel.
- Keep everything client-renderable from replicated state.

### Success Criteria

- [x] Vanilla map extension via config delta (all vanilla zoom/pan/cursor for free); gamepad works via vanilla `MapSelect` (`gamepad0:a`)
- [x] Town info panel incl. deduped, stack-summed modifier chips from client-side config
- [x] Fast-travel rule set (anchors, wanted, QRF policy, min distance, cost + per-recruit fee)
- [x] Icon set with zoom ceilings, POI registry, RplId retry + cached-position fallback
- [ ] Panel/mode lifecycle surviving all close paths (BUG-069)
- [ ] Drawn restrictions matching enforced restrictions (BUG-070)

---

## Current Architecture

### Key Components

| Area        | Files                                                                                                                                                                                                                                                            | Role                                                                                                                                                       |
| ----------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Context     | `Scripts/Game/UI/Context/OVT_MapContext.c` (515 L)                                                                                                                                                                                                               | 3 mode flags, town panel, fast/bus travel rules + payment, map open/close via `SCR_MapGadgetComponent`                                                     |
| Canvas base | `Scripts/Game/UI/Map/OVT_MapCanvasLayer.c` (103 L)                                                                                                                                                                                                               | `SCR_MapModuleBase` + workspace-root `CanvasWidget`; world→screen `DrawCircle/Image/Rectangle`; command list rebuilt per frame                             |
| Overlays    | `OVT_MapRestrictedAreas.c` (102 L), `OVT_MapThreatGrid.c` (114 L)                                                                                                                                                                                                | Red base/tower/QRF circles + faction flags; threat rects sampled once at open (**shipped disabled**: `m_bDisableModule 1`)                                 |
| Icons       | `OVT_MapIcons.c` (835 L)                                                                                                                                                                                                                                         | Per-entity widgets, parallel `m_Centers`/`m_Ranges`/`m_Widgets` arrays, static `RegisterPOI` registry, RplId retry/fallback, zoom ceiling `m_fCeiling 0.6` |
| Players     | `OVT_MapPlayerLocation.c` (111 L)                                                                                                                                                                                                                                | One faction-tinted arrow per player, gated on `showPlayerOnMap`; set frozen at map open                                                                    |
| Wiring      | `Configs/Map/MapOverthrow.conf` (delta at vanilla GUID), `Character_Player.et:37-43` (context config, chip colors), `UI/Layouts/Map/*` (MapInfo/Modifier/MapIcon/MapCanvasLayer/MapPlayerLocation layouts), `overthrow_mapicons.imageset` (15 keys, all resolve) |                                                                                                                                                            |
| POI source  | `OVT_MainMenuContextOverrideComponent.c`                                                                                                                                                                                                                         | Entities self-register via static `OVT_MapIcons.RegisterPOI` on first frame                                                                                |

### Mode lifecycle

Entry: main menu → `EnableMapInfo()`/`EnableFastTravel()`, or bus-stop action → `EnableBusTravel()`; each calls `ShowMap()` through the map **gadget** (no map in inventory → "MustHaveMap"). Inputs are **global** listeners on vanilla actions (`MapSelect`/`MenuBack`/`GadgetMap`) — no Overthrow map ActionContext exists; gamepad support is inherited from vanilla's `MapSelect = gamepad0:a`. Exit has two unequal paths: the input handler (`MapExit`) closes layout + all three modes + map; the engine's map-close invoker (`OnMapExit`) clears only map-info/fast-travel — no `CloseLayout()`, no bus disable (BUG-069).

### Town-info data path

Click → `GetNearestTown(pos)` (unbounded — ocean clicks select the nearest town; click-only, no hover) → panel widgets from the town record (all fields replicated: scalars via RPCs, modifier lists via JIP stream) → modifier chips deduped by id, stackable entries summed, colored by sign from `Character_Player.et` config. Chip metadata comes from the modifier configs, which load on **all** machines (only handler bootstrap is server-gated) — the deliberate seam that makes the panel client-renderable. Chip text truncates float effects to int and concatenates the `#OVT-…` title mid-string, where Enfusion does **not** localise it (the shipped placeholder layout shows the raw key too) — the panel likely renders raw localisation keys.

### Fast travel (whitelist + rules)

`CanFastTravel`: min distance, wanted level 0, QRF policy (`QRFFastTravelMode`: FREE/DISABLED/radius check), then an anchor within range — owned/rented non-warehouse house ≤ 25 m, own-or-public camp ≤ 40 m, any FOB ≤ 40 m, friendly base ≤ `baseCloseRange`. Cost = `fastTravelCost` × (1 + recruits within 50 m). Execution: driver-in-vehicle and with-recruits paths go through server RPCs; **on-foot-alone teleports client-locally** — 3 of 5 paths are server-authoritative. Money check/charge is client-side (BUG-053's class; BUG-053 itself — recruits stranded because the server searches after teleporting — remains open).

### Bus travel

Origin: any `MDT_BUSSTOP` marker within 15 m; destination: click within 15 m of another; fare = straight-line distance × `busTicketPrice`. No routes are modelled. Weaker than fast travel by omission: no wanted/QRF/min-distance checks, no `FindSafeSpawnPosition` (teleports to the raw click, up to 14 m off the stop — into geometry or water), client-side charge.

### Replication correctness (what the map may safely read)

Verified safe: town records + modifier lists, base/tower locations+factions, QRF active/location/points/timer, camps/FOBs, house ownership, shop/dealer/port RplIds (ports client-derived by world query), and all map-relevant difficulty fields (**BUG-013 is fixed/closed**). Server-only but read anyway: `GetThreatByLocation` (known-targets list never replicates → the threat grid would render empty for every client if enabled — the map-side analogue of the occupying epic's `m_CurrentQRF` finding); `m_iCurrentQRFBase/Town` (broadcast on start only, reset without RPC, absent from JIP → stale ring suppression + JIP divergence, already logged in the occupying epic).

---

## Key Technical Decisions

### Decision 1: Config delta at the vanilla GUID

**Implementation:** Same-GUID override merges Overthrow's modules/components into vanilla's fullscreen map config; `SCR_MapConfigComponent` left at defaults.
**Trade-offs:** All vanilla map behaviour for free and mod-compatible appends; but Overthrow is coupled to vanilla's config shape, and nothing owns "towns as map locations" (no town icons/coloring at all — the unmerged `new-map` branch's `OVT_OverthrowMapUI`/`OVT_MapLocationTown` is the designed successor).

### Decision 2: Two rendering strategies

**Implementation:** Geometry overlays = canvas command lists rebuilt per frame; point markers = per-entity widgets repositioned per frame.
**Trade-offs:** Right tool per job; but per-frame allocation (a `PolygonDrawCommand` + vertex array per circle; 2601 rects/frame at the threat grid's 250 m cell size — almost certainly why it ships disabled) and no screen-space caching.

### Decision 3: RplId-addressed icons with retry + fallback

**Implementation:** Failed `Replication.FindItem` resolutions queue for retry; successes cache position/type; fallbacks draw at 0.7 opacity; caches survive map close.
**Trade-offs:** Handles streaming races honestly; but never-streamed entities have no cache → no icon ever (see towns/gun-dealers), and the validation pass has a units bug making it per-frame (BUG-067).

### Decision 4: Modeless flags + global input listeners on vanilla actions

**Implementation:** Three booleans, one `MapClick` handler, no ActionContext, empty `m_sContextName`.
**Trade-offs:** Tiny code, free gamepad; but mode-flag lifecycle is exactly where the bugs cluster (BUG-069), and `ActivateContext("")` runs every active frame.

### Decision 5: Client-side config for chips, replicated records for values

**Implementation:** Modifier titles/effects from the locally loaded `.conf`; ids off the wire index into it unguarded.
**Trade-offs:** No extra replication; but a server/client config mismatch is an out-of-bounds read in the map UI (towns/stability's id-index coupling).

---

## Current State

### What's Working

- The full panel + icons + overlays on vanilla's map, JIP-correct for towns/bases/ownership; POI self-registration; zoom decluttering; camp/FOB privacy filtering; QRF rings and fast-travel vetoes honoring replicated difficulty

### Known Issues (filed)

- **BUG-067**: `GetWorldTime()` returns ms but refresh/retry intervals are seconds — `ValidateReplicationReferences` (walking every dealer/shop/port with `Replication.FindItem`) and `RetryFailedIcons` run **every frame** while the map is open
- **BUG-068**: parallel-array desyncs shift icons — POI skips (`m_bMustOwnBase`, failed creates) misalign `m_POIWidgets` vs the static registry (wrong icon at wrong POI as bases flip), and `m_Centers`/`m_Ranges` are inserted before widget creation is validated (one failure shifts every subsequent icon)
- **BUG-069**: close-path asymmetry — the engine-invoker exit never closes the layout nor bus mode: holstering the map with the panel open sticks it on screen; bus mode surviving close makes the next map click charge a fare; re-entering Map Info stacks orphaned panels (`m_bOpenActionCloses 0` + unconditional `m_wRoot` overwrite); the static `GetOnMapClose` subscription is never removed (accumulates per respawn)
- **BUG-070**: drawn restricted areas ≠ enforced: FOB deploy enforces `baseCloseRange + 50` and tower 70 m vs drawn `baseCloseRange` and 20 m; resistance-held bases/towers enforce with **no circle at all**; server silently drops invalid deploys ("client should have already validated") — player gets zero feedback
- **BUG-053** (pre-existing, open): fast-travel-with-recruits charges per recruit then strands them (server searches after teleport)

### Technical Debt (unfiled)

- `OVT_UIContext.Init` self-assignment (`OVT_Global.GetConfig() = OVT_Global.GetConfig();`) leaves `m_Config` permanently null for every context — map context dodges it by re-fetching per use; same broken-refactor pattern exists in the economy manager
- Bus travel bypasses wanted/QRF/min-distance and safe-position snapping (manhunt escape; teleport into geometry/water 14 m off a stop)
- Chip text: float→int truncation + mid-string `#key` never localises (renders raw keys); color and text use different sources of truth
- Unguarded derefs: camp/FOB icon loops (hardcoded `"FIA"` faction key where config exists — crashes custom-faction worlds), `ShowTownInfo` (faction/UIInfo/town-name marker/`m_aModifiers[id]`), `CanFastTravel` (wanted component, real-estate config, zero-vector FOB default legalising travel near world origin), vehicle path (`slot` deref; map already closed before failure notifications)
- `OVT_MapRestrictedAreas.Draw` iterates arrays its `OnMapClose` nulls (threat grid has the guard, this doesn't); canvas base's `OnMapClose` unguarded; emptied command lists never cleared (last QRF rings stay painted)
- One 40 m sphere query per owned/rented building on every map open (only to find warehouses) — visible hitch on property-rich saves; job-waypoint failure `return`s out of `OnMapOpen` skipping all fallback icons; player-arrow set frozen at open, widgets keyed by reusable player id
- Per-frame allocation in `Draw()`; hardcoded colors/radii/sizes throughout; 9 `Print`s per layout open + prints on hot paths; dead code (`ZoomInOnPlayer`, `m_ThreatLevels`, `m_ToolMenuEntry`); stray hand-GUID `OVT_MapThreatGrid.c.meta` (only committed script `.meta` in the tree); float-into-`out int` `WorldToScreen` calls (sub-pixel loss pre-DPIUnscale)

---

## Future Enhancements

### High Priority

- [ ] Fix the ms/s units (BUG-067) and the parallel-array construction (BUG-068) — both small, both constant-cost wins
- [ ] Unify the close paths + unsubscribe on context teardown (BUG-069)
- [ ] Draw what's enforced (BUG-070) + a "deploy rejected" notification server-side

### Medium Priority

- [ ] Bus travel parity: wanted/QRF checks, safe-position snap to the actual stop, server-side charge
- [ ] Localise chip text (`WidgetManager.Translate`/`SetTextFormat`); guard the deref cluster; config-driven faction key in icon loops
- [ ] Batch/queue the warehouse discovery instead of per-property sphere queries at map open

### Low Priority / Nice to Have

- [ ] Screen-space geometry caching invalidated on zoom/pan; re-enable the threat grid client-side (needs threat replication first — occupying epic)
- [ ] Live player-arrow roster; town icons/control coloring (or adopt the `new-map` branch design)

---

## Testing

### Current Coverage

- **None.** No map/UI/widget tests exist anywhere in the test tree (grep-confirmed) — consistent with the project's "UI is uncovered, play-testing is the gate" stance.

### Testing Gaps

- Everything; the only automatable seams without a UI harness are `CanFastTravel`'s rule table (pure given injected managers — a Logic-tier candidate if the manager lookups gain seams) and the fare/cost arithmetic

---

## Dependencies

### Internal Dependencies

- **towns/core**: town records + queries (`GetNearestTown`, `GetTownName`, `GetNearestBusStop`); modifier lists for chips
- **towns/stability + towns/support**: chip metadata from the client-loaded configs (id-index coupling)
- **towns/gun-dealers**: dealer icons via the economy registry (never-streamed = no icon)
- **occupying**: bases/towers/QRF for overlays and travel vetoes; threat (server-only — grid disabled); known JIP gaps (`m_iCurrentQRFBase/Town`)
- **economy**: shop/port registries, money for fares (client-side today); real estate for house/warehouse icons + travel anchors
- **resistance**: camps/FOBs (icons, anchors, deploy-radius mismatch); recruits (travel fees, BUG-053)
- **jobs**: `m_vCurrentWaypoint` scratchpad (written by jobs/recruits contexts, never cleared)

### External Dependencies

- Vanilla `SCR_MapEntity`/module/component framework, `MapSelect`/`MenuBack`/`GadgetMap` actions, map gadget, `SCR_MapDescriptorComponent` town/bus markers

---

## Notes

**Discovered Information:**

- The threat grid ships disabled in config rather than deleted — enabling it in MP would render empty for all clients (threat is server-only) _and_ allocate 2601 draw commands per frame; treat both as prerequisites
- BUG-013 (map-relevant difficulty fields unreplicated) is confirmed fixed and closed — all fields the map reads are in the config JIP stream
- An unmerged `new-map` branch (`12960cb`…) redesigns this system with towns as first-class map locations — check it before investing heavily here

**Retrospective Assessment:**

- The config-delta extension and the two-pattern rendering split are the right architecture; the RplId retry subsystem is honest engineering around real races
- The defect mass is in lifecycle (close paths, parallel arrays, units) — mechanical fixes, no redesign needed
- The panel's data path is exemplary (client config + replicated records); its presentation layer (localisation, truncation, hardcodes) is where the polish debt lives

---

_This retrospective plan was created by analyzing existing code. Use `/start-feature towns/map-info` to begin making improvements._
