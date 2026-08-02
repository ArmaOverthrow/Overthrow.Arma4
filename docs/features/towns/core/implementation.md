# Towns Core - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (pre-dates Beast Mode; defined-town controllers superseded map-marker auto-detection in `9bdd81d`)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-03 00:15

---

## Executive Summary

The town system is the civilian heart of the campaign: `OVT_TownManagerComponent` (1579 lines) discovers towns from world-authored `OVT_TownControllerComponent` entities, holds every town's record (`OVT_TownData`: population, stability, support, faction, modifiers, dealer position, heat), ticks the stability/support modifier systems every 10 s, grows population every 70 s, flips village control peacefully at support/stability thresholds, and answers nearest-town/house queries for nearly every other subsystem. `OVT_TownController` is the per-town world entity: authoring attributes (name/size/population/range/QRF geometry), the civilian spawn/despawn loop, and gun-dealer spawning. Nine broadcast RPCs + a hand-rolled JIP stream keep clients' parallel town lists in sync; the vanilla `OVT_TownManagerSerializer` persists state (never towns themselves) matched back by location.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase (`/discover-feature`, 2026-08-02). The feature has already been implemented and shipped.

---

## Goals

### Primary Goals
- One authoritative record per town driving economy (tax, stock, NPC buying), occupying-faction decisions (threat, QRF), jobs, deployments and UI.
- World-authored town definitions (position, size, population, QRF attack geometry) editable in Workbench with debug gizmos.
- Multiplayer-correct state sync incl. JIP, and save/load that survives map edits.

### Success Criteria
- [x] Defined-town discovery via controllers (legacy map-marker auto-detect kept behind a deprecation warning)
- [x] Population growth from stability; supporter counts; peaceful village flips (75/85 in, 25/15 out, 50% rolls)
- [x] Full-state JIP stream + 9 broadcast RPCs; modifiers included (fixed in `96ee803`)
- [x] Vanilla-persistence round trip, location-matched, idempotent, with modifier-id validation
- [ ] Bounds-validated RPC surface (BUG-060)
- [ ] Legacy-map path actually functional end-to-end (BUG-061)

---

## Current Architecture

### Key Components

| Area | Files | Role |
|---|---|---|
| Manager | `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c` (1579 L) | `OVT_TownData`/`OVT_TownModifierData`/`OVT_TownSize`; discovery; 10 s modifier tick + 70 s support/population sub-tick; queries; RPC + JIP; persistence apply |
| Controller | `Scripts/Game/Controllers/OVT_TownController.c` (350 L) | World-authored data source + local actor: name/size/pop/range/QRF geometry attributes, civilian spawn loop (10 s presence check), gun-dealer spawn, Workbench gizmos |
| Persistence | `Scripts/Game/Persistence/Serializers/Components/OVT_TownManagerSerializer.c` (177 L) | `OVT_PersistedTown` (location-matched) + parallel modifier id/timer arrays; version-1 guard |
| World data | `Worlds/MP/OVT_Campaign_Eden_Layers/towns.layer` (20 towns), test world (1 town), `Prefabs/Controllers/OVT_TownController.et` | Authored town set |
| Wiring | `Prefabs/GameMode/OVT_OverthrowGameMode.et:197-209` | Manager + both modifier systems + `m_aIgnoreTowns` |
| Tests | `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Town.c` (555 L) + Init/Campaign/Persistence cases | See Testing |

### `OVT_TownData` (every field)

`location` (centre = controller origin; **persistence match key**), `population` (drives civilians, tax, stock, NPC buying, `SupportPercentage()` denominator), `targetPopulation` (growth ceiling), `stability` (0–100, **fully derived** from stability modifiers), `support` (absolute supporter **count**, not a percentage), `faction` (engine FactionManager *index*), `size` (`[NonSerialized]`, re-derived per session; VILLAGE=1…CAPITAL=4), `stabilityModifiers`/`supportModifiers` (live `{id, timer}` pairs; id = config index), `gunDealerPosition` (zero = none; see towns/gun-dealers), `areaHeat` (write-only — incremented by the wanted system, consumed by nothing).

Key methods: `SupportPercentage()` (guards pop 0; **no upper clamp** — can exceed 100, test-pinned), `IsWithinTownBounds()` (**hardcoded 500 m, 3D, strict <** — ignores all configured ranges, test-pinned), `CopyFrom()` (dead production code — only tests call it; its doc claims a persistence seam that no longer exists).

### Discovery (two mutually exclusive paths, decided at `Init`)

Whole-world sphere query at `EOnInit` (before persistence deserialization — the ordering contract documented in the serializer):
1. **Defined towns (current):** every entity with `OVT_TownControllerComponent` → build `OVT_TownData` from its attributes; append into `m_Towns` + `m_TownNames` + `m_TownControllers` in one loop (index alignment is load-bearing). `m_bUseDefinedTowns = true`.
2. **Legacy markers (deprecated, prints a warning):** map descriptors CITY/TOWN/VILLAGE → size from descriptor type, population accumulated from house queries (4/6/8 occupants by building type). `PostGameStart` then spawns controller prefabs — but never inserts them into `m_TownControllers` (BUG-061).

**Discovery deliberately runs on clients too** (the `Replication.IsServer()` guard sits *after* `InitializeTowns()`): clients build their own parallel `m_Towns`, and every RPC and the JIP stream address towns **by array index**. Nothing validates client/server index alignment — a mod/world mismatch silently shifts every update onto the wrong town.

### Tick cadences (server)

- `CheckUpdateModifiers` every 10 s (`MODIFIER_FREQUENCY`): per town — stability system `OnTick` (recalc flag honoured), support system `OnTick` (**return discarded** — support feature's BUG), radio-tower/base proximity support modifiers, `RecalculateStability` if flagged.
- Every 7th tick (**70 s**, off-by-one on `SUPPORT_FREQUENCY = 6`): `RecalculateSupport` + `RecalculatePopulationGrowth`.
- Controller: `CheckSpawnCivilian` every 10 s (population-derived count, presence-gated, despawn during QRF).

Population growth: `round(rand(1..3) × stability/100 × min(gap/10, 1))`, only up toward `targetPopulation`; no decay path — population only drops via `TakeSupportersFromNearestTown` (removes support and population 1:1).

Village flips (`RecalculateSupport`): **only size 1** flips peacefully — to player at support% ≥ 75 ∧ stability ≥ 50 (certain at ≥ 85, else 50% roll), back at < 25 (< 15 certain). `ChangeTownControl` fires `m_OnTownControlChange`, broadcasts, and builds the notification tag — size 4 (CAPITAL) falls through to "Village" (one of many unhandled-CAPITAL gaps, see Technical Debt).

### Networking

No `RplProp` anywhere. Hand-rolled positional JIP stream (`RplSave`/`RplLoad`: per town — population, target, stability, support, faction, both modifier lists) + 2 server RPCs (`RpcAsk_Add{Stability,Support}Modifier`) + 9 broadcast RPCs (set stability/support/population/faction, add/remove/reset both modifier kinds). **Not streamed:** `location`/`size` (client-derived), `gunDealerPosition`, `areaHeat`.

**Settled convention** (empirically, dev-ops `test-coverage/findings.md:1000-1016`): on the server, `Rpc(RpcAsk_X, …)` executes locally and synchronously; `Rpc(RpcDo_X, …)` does not execute locally. That's why mutators write locally then broadcast, and why `AddStabilityModifier`/`AddSupportModifier` being "RPC-only" is correct rather than dead code.

None of the 9 `RpcDo_*` handlers bounds-check `m_Towns[townId]`; the `RpcAsk_*` pair validates neither `townId` nor modifier `index` (BUG-060 — same client-trust class as the skills/resistance epic findings).

### Persistence (vanilla SaveGame; see `core/persistence` for the framework)

`OVT_PersistedTown`: location + scalars + four parallel modifier id/timer arrays; version < 1 → no-op (never zero a live campaign). `ApplyPersistedTowns` matches by `GetNearestTown(record.location)` (tolerant of map edits), assigns scalars, rebuilds modifier lists (dropping negative/out-of-range ids with a warning — ids are config indices), then **recomputes stability from modifiers** (saved int is a fallback only) while **support restores raw** (its recalc is relative and would double-apply). Idempotent; no RPCs on the restore path (clients sync via JIP). Deliberately not persisted: the town list itself, `location`/`size`, civilians, the dealer entity, controller/name caches, handler-internal state (hour-gates re-fire after load).

### Queries (heavily consumed by every other epic)

`GetNearestTown` (true nearest, never null once towns exist), `GetNearestTownId` (**returns 0 on an empty list** — uninitialised), `GetNearestTownInRange` (**first-in-array within range, not nearest** — BUG-062: deterministic wrong-town crediting for death modifiers in overlapping radii), `GetTownRange` (deprecated global radii; CAPITAL falls to city), `GetTownName` (lazy marker lookup — null-marker deref for authored towns with empty `m_sName` placed away from vanilla markers), `GetRandomUnownedHouse[InTown]` (the InTown variant lacks the empty-array guard its sibling has — BUG-055), `GetTownsWithinDistance` (appends without clearing), house/bus-stop/marker sphere queries. Query callbacks share mutable scratch state on the manager (`m_Houses`, `m_CheckTown`) — structurally re-entrancy-unsafe. `GetTownID` is a linear `Find` called in the hot tick (O(n²) per 10 s with both systems).

---

## Key Technical Decisions

### Decision 1: Towns are world-derived; saves carry state only
**Implementation:** Discovery every session; persistence matches records by nearest-location; a save can never create or move a town.
**Trade-offs:** Robust against map edits; but town identity everywhere else is the **array index** (wire ID, save-adjacent key, shop-map key, job key, controller key) — a purely positional identity nothing validates.

### Decision 2: Client-side discovery + index-addressed patches instead of `RplProp`
**Context:** Variable-length town list; per-town records with nested modifier arrays.
**Implementation:** Clients run the same world query, then JIP stream + 9 broadcast RPCs patch by index.
**Trade-offs:** Compact and simple; but correctness rests on identical query ordering across machines, with no checksum or count validation anywhere (JIP `RplLoad` doesn't even bounds-check).

### Decision 3: Stability derived / support absolute
**Implementation:** Stability = `clamp(100 + Σ effects)` recomputed on every change; support is a raw supporter count restored verbatim.
**Trade-offs:** Stability can never drift (the serializer recomputes it on load); support can — it's unclamped (`AddSupport` lets support exceed population; `SupportPercentage()` > 100 is test-pinned) and gates misfire (towns/support's BUG-064).

### Decision 4: Controller as authoring surface, manager as owner
**Implementation:** Controllers carry attributes + QRF attack geometry (read back by `StartTownQRF`) + Workbench gizmos; the manager scrapes them once and stores `EntityID`s.
**Trade-offs:** Great authoring UX; but controllers re-resolve their record **by position**, `PostGameStart`'s activation loop derefs unguarded, and the legacy path breaks the controller array contract (BUG-061).

### Decision 5: Filter-as-visitor query idiom
**Implementation:** `QueryEntitiesBySphere(..., null, FilterX)` where the filter side-effects a member array and returns false (town discovery, dealer houses, markers, bus stops).
**Trade-offs:** Avoids callback plumbing; but relies on unspecified filter-call semantics — an engine change silently breaks town discovery itself.

---

## Current State

### What's Working
- 20-town Everon campaign: discovery, population growth, village flips, civilian presence, QRF geometry hand-off, JIP incl. modifier chips, save/load round trips (4 persistence + 4 round-trip suite cases green)
- The deliberate seams: derived stability, raw support restore, location matching, id validation on load

### Known Issues (filed)
- **BUG-055**: `GetRandomUnownedHouseInTown` unguarded `GetRandomElement()` on empty array (fully-owned town → campaign-start crash via dealer spawn, or job-stage crash)
- **BUG-060**: unvalidated town RPC surface — `RpcAsk_Add*Modifier` trusts `townId`/`index` (crafted client can crash the server or floor every town with RecentBattle spam); all 9 `RpcDo_*` handlers index client arrays unguarded
- **BUG-061**: legacy (non-authored) maps crash on the first town QRF — `SpawnTownControllers` never fills `m_TownControllers`, which `StartTownQRF` reads by index
- **BUG-062**: `GetNearestTownInRange` returns the *first* town in array order within range, not the nearest — civilian/OF death modifiers deterministically credit the wrong town where radii overlap

### Technical Debt (unfiled)
- Diag-menu keys 251/252 mutate town state client-side with no RPC (permanent listen-client divergence)
- `m_OnTownControlChange` typed `ScriptInvoker<IEntity>` but invoked with `OVT_TownData` (subscribers disagree; works only because invokers are untyped)
- CAPITAL (size 4) unhandled across range/notification/deployment-cap logic; offered in the authoring combo box
- Deprecated global radii (`m_iCityRange` etc.) still live inputs to 9 consumers while the controller's own range only drives civilians + dealer search; `IsWithinTownBounds` hardcodes 500 m over both
- `TakeSupportersFromNearestTown` fails silently when the town is short (callers never check → free recruits; see towns/support)
- `areaHeat` write-only (persisted, tested, consumed by nothing — unfinished undercover feature); `CopyFrom` dead; `ProcessTown` dead statements; `m_aIgnoreTowns` honoured only by real estate (Erquy still gets everything)
- Shared mutable query scratch (`m_Houses`/`m_CheckTown`); `GetTownID` linear-find in the hot tick; `GetModifierSystem` string-compare per call; `array<ref EntityID>`/`array<ref string>` idiom; support cadence off-by-one (70 s not 60); `GetTownsWithinDistance` appends without clearing; controller range slider bounds (50–500) contradict its default (800); debug-only compile error (`closestTown.name`, `OVT_OccupyingFactionManager.c:890`); stale serializer doc line refs

---

## Future Enhancements

### High Priority
- [ ] Validate the RPC surface (BUG-060) — bounds + stack-limit re-check server-side, guards client-side
- [ ] Guard the activation loop and `GetRandomUnownedHouseInTown` (BUG-055) — the campaign-start crash class
- [ ] Fix or formally drop the legacy-map path (BUG-061): fill `m_TownControllers` or refuse to start with a clear error

### Medium Priority
- [ ] Make `GetNearestTownInRange` actually nearest (BUG-062)
- [ ] Clamp support to population at the mutation points (with towns/support)
- [ ] Resolve CAPITAL: implement (range, notifications, flips?) or remove from the combo box
- [ ] Unify town ranges on the controller attribute; retire the deprecated globals (9 consumers)

### Low Priority / Nice to Have
- [ ] Town-list checksum/count in the JIP stream; bounds-check `RplLoad` and the `RpcDo_*` handlers
- [ ] Index-map `GetTownID`; typed control-change invoker; delete `CopyFrom`/`areaHeat` or finish undercover heat
- [ ] Honour `m_aIgnoreTowns` in the manager itself

---

## Testing

### Current Coverage
- **Logic (world-free, 6 cases):** `SupportPercentage` boundaries incl. the unclamped >100 pin; `Recalculate` sum/clamp/round/null-skip; support-system deterministic branches; `IsWithinTownBounds` 500 m/strict/3D pins; `areaHeat` clamp pins; `CopyFrom` copies-record-not-location (its doc comment is stale — cites a persistence seam that moved)
- **Init:** controllers non-empty, resolve, `GetNearestTown`/`GetTownID` agree
- **Campaign:** town activation chain via the dealer-position observable
- **Persistence + RoundTrip:** control/population/stability/support round trips (same-session and save→dirty→reload)

### Testing Gaps
- The manager itself: tick loop, recalcs, growth formula, flip thresholds, `TryAdd/Remove` stacking, `AddSupport`, `TakeSupportersFromNearestTown`
- Discovery (both paths, the `m_bUseDefinedTowns` fork); `ApplyPersistedModifiers` defenses; RplSave/RplLoad symmetry; index-alignment assumption (needs two clients — out of harness scope)
- Query helpers (`GetNearestTownInRange` would immediately expose BUG-062; empty-list `GetNearestTownId`; null-marker `GetTownName`); the controller (activation, civilians, dealer fallback tiers)
- RNG-gated support branches: deliberately untested — `s_AIRandomGenerator` has no injection seam (recorded in the suite)

---

## Dependencies

### Internal Dependencies
- **towns/stability + towns/support**: the manager owns/ticks both systems and hosts their entire add/remove/RPC/persistence transport (this doc owns the transport; those docs own the semantics)
- **towns/gun-dealers**: controller spawns dealers; `gunDealerPosition` on the town record
- **towns/map-info**: reads records + modifier lists client-side
- **economy**: shops keyed by town id; tax/donation/stock/NPC-buying formulas read population/stability/support
- **occupying**: control-change threat, QRF town selection + controller geometry, threat scoring, town patrols
- **jobs**, **resistance** (supporter draw-down), **deployments**, **real-estate** (starting town, ignore list), **recruits** (hometowns), **undercover** (areaHeat)
- **core/persistence**: serializer registration + apply ordering contract

### External Dependencies
- Engine world queries, `FactionManager` indices, map descriptors (legacy path), `CallLater` scheduling

---

## Notes

**Discovered Information:**
- The `RpcAsk` local-synchronous-execution convention (settled empirically by dev-ops) is what makes the "RPC-only" mutators correct — worth keeping documented, it looks like a bug until you know it
- `96ee803` fixed JIP modifier replication; `9bdd81d` deprecated town auto-detection; the legacy path has likely been broken (BUG-061) since controllers became authoritative
- Client/server town-index alignment is the system's single biggest unvalidated assumption — nothing in the harness can test it (needs two clients)

**Retrospective Assessment:**
- The data model is sound: derived stability, location-matched persistence, and world-authored controllers are all good calls that survived the EPF→vanilla migration cleanly
- The weak layers are validation (RPC bounds, index alignment, null guards on the activation/query paths) and the half-dead legacy path
- Perf debt is real but small-n today (20 towns); the O(n²) tick and string-compare system lookup become relevant on bigger maps

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature towns/core` to begin making improvements.*
