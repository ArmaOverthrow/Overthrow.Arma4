# Configuration System - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (legacy — predates Beast Mode)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02

---

## Executive Summary

Overthrow's configuration layer spans `OVT_OverthrowConfigComponent` (a 619-line component on the game-mode entity holding difficulty, the three faction roles, the server JSON config, item limits and a waypoint factory), ~11 pure-data config classes under `Scripts/Game/Configuration/` (plus several that leaked elsewhere), 46 `.conf` files under `Configs/`, and the faction registry (`OVT_OverthrowFactionManager` + `OVT_Faction`). Three completely separate loading mechanisms coexist: Workbench attribute inlining in prefabs/layers (dominant), `ResourceName` + `BaseContainerTools` runtime loads (three attributes), and a `$profile:Overthrow_Config.json` server config.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Data-drive gameplay tuning (prices, jobs, skills, difficulty, buildables, deployments) via Workbench-editable `.conf` files
- Let server admins configure factions/difficulty/officers via `Overthrow_Config.json` without Workbench
- Provide the faction role model (occupying / resistance-player / supporting) every system keys off

### Success Criteria
- [x] Difficulty presets selectable by menu (SP/host), JSON (dedicated), and name-lookup (tests)
- [x] Config resolves globally via `OVT_Global.GetConfig()` (69 consumer files; asserted by the Init tier)
- [x] Console safety: `#ifdef PLATFORM_CONSOLE` skips all disk I/O (defaults only)
- [ ] Client/server config consistency — 7 difficulty fields are read on clients but never replicated (see Known Issues)

---

## Current Architecture

### Key Components

| File | Role |
|---|---|
| `Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c` (619 ln) | The hub: `OVT_FactionType`/`OVT_FactionTypeFlag`/`OVT_PatrolType` enums, `OVT_OverthrowConfigStruct` (JSON DTO), difficulty, faction keys+indices, item limits, waypoint factory, `RplSave`/`RplLoad` |
| `Scripts/Game/Configuration/*.c` (11 files) | Pure data classes: `OVT_DifficultySettings`, prices, vehicle prices, shops, gun dealers, jobs, skills, loadouts, buildables, placeables, real estate |
| `Scripts/Game/GameMode/OVT_OverthrowFactionManager.c` | `SCR_FactionManager` subclass with a parallel `array<ref OVT_Faction>` registry keyed by faction key |
| `Scripts/Game/Faction/OVT_Faction.c` (514 ln) | Per-faction Overthrow data: new weighted group/vehicle registries + ~15 legacy prefab-slot arrays + composition configs |
| `Configs/**` (46 `.conf`) | Difficulty presets, prices, jobs, skills, modifiers, deployments, factions, buildables/placeables, shop/gun-dealer, broadcast messages |
| `Prefabs/GameMode/OVT_OverthrowGameMode.et` + `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer` | Duplicate (~180 lines each) attribute wiring of configs onto managers |

### Data Flow — the three loading mechanisms

1. **Mechanism A — attribute inlining (dominant):** most `.conf`s are referenced as object attributes in the prefab/world layer; the engine instantiates them at entity creation. No script runs. The test world's layer overrides `m_aDifficultyPresets` — and **appends rather than replaces** (5 presets at runtime, 'Test World' = index 4).
2. **Mechanism B — `BaseContainerTools` runtime load (3 attributes):** placeables/buildables (`OVT_ResistanceFactionManager.LoadConfigs()`) and town modifiers (`OVT_TownModifierSystem.LoadConfig()`). Failure is silent — config stays null, no log.
3. **Mechanism C — `$profile:Overthrow_Config.json`:** `LoadConfig()` (called once, server-only, from `OVT_OverthrowGameMode.EOnInit:869`) builds defaults, writes the file if absent, else JSON-reads into `OVT_OverthrowConfigStruct`. Console: defaults only, no disk. `SaveConfig()` has no runtime write path.

### Data Flow — faction roles

Three roles held as string key + lazily cached int index: player/resistance (`m_sPlayerFaction`, fixed "FIA", no setter), occupying (default "USSR"), supporting (default "US"). JSON/UI set occupying/supporting; `"FIA"` is refused for both. `SetBaseAndTownOwners()` stamps the occupying index on every base/town/radio tower. Downstream identity checks compare **indices** via `GetOccupyingFactionIndex()` (25+ sites) — except `OVT_BaseControllerComponent` (key comparison) and `OVT_DeploymentComponent.GetFactionType()` (returns 0 = OCCUPYING on no-match).

### Integration Points
- Consumers: economy (prices/shops), occupying faction (threat/QRF/resources), resistance (buildables/placeables), wanted system, towns/modifiers, deployments, UI (map/shop/build ranges & costs), item limits, notifications (Discord webhook URL), player prep (starting cash, officers).
- Replication: hand-written `RplSave`/`RplLoad` bitstream — 11 difficulty fields + 4 config-file fields, initial-state only (no `RplProp`, no `BumpMe`).
- Persistence: `OVT_ConfigSaveData` (EPF) saves exactly one thing — the `m_Difficulty` object. Occupying faction key persists via `OVT_OccupyingFactionSaveData`; the supporting faction is not persisted anywhere.

---

## Implementation Details

### Phase 1: Attribute-driven config classes (COMPLETED)
`ScriptAndConfig`/`configRoot` data classes + `.conf` files + prefab wiring; Workbench custom titles for list entries.

### Phase 2: Server JSON + faction roles + replication (COMPLETED)
`OVT_OverthrowConfigStruct`, `LoadConfig()/SaveConfig()`, faction key/index model, `RplSave`/`RplLoad`, difficulty preset selection from menu/JSON/tests.

### Phase 3: Potential Improvements (NOT STARTED)
Close the replication gap, consolidate the god object, unify the duplicate enums and legacy faction slots (see Future Enhancements).

---

## Key Technical Decisions

### Decision 1: Config-as-prefab-attributes over runtime loading
**Context:** Enfusion instantiates `[BaseContainerProps]` object attributes natively.
**Implementation:** Managers declare typed config members; prefab/layer supplies the `.conf`.
**Trade-offs:** Zero loading code, Workbench-editable; but wiring is duplicated between prefab and world layer, and layer overrides have append semantics (the 'Test World' preset trap, recorded in test-coverage findings and `OVT_TEST_SuiteBase` — always select difficulty **by name**).

### Decision 2: Three-layer difficulty override
**Context:** SP menu, dedicated JSON, and per-field admin overrides all need to set difficulty.
**Implementation:** prefab preset → JSON preset-by-name → JSON field-level `overrideDifficulty`, applied in `DoStartGame()`.
**Trade-offs:** Flexible; but the JSON layers run *after* `PostGameStart()` (and `NewGameStart()` reads difficulty even earlier), and the preset object is mutated in place — the shared preset instance is what EPF persists.

### Decision 3: Parallel faction registry keyed by string
**Context:** vanilla `SCR_FactionManager` can't carry Overthrow's per-faction data.
**Implementation:** `OVT_OverthrowFactionManager.m_aOverthrowFactions` looked up by key; config caches int indices for hot-path comparisons.
**Trade-offs:** Non-invasive; but index↔key mapping logic is duplicated with inconsistent mechanisms downstream, and lookups are linear scans.

---

## Current State

### What's Working
- All Mechanism-A configs load and drive gameplay; Init/Logic/Campaign tiers exercise config resolution, modifier maths, price seams and faction-index town-control assertions
- JSON server config round-trips (writes defaults, reads overrides) with console guards

### Known Issues
- **BUG-013 — client replication gap:** 7 difficulty fields read on clients but absent from `RplSave` (`minFastTravelDistance`, `QRFFastTravelMode`, `baseCloseRange`, `fastTravelCost`, `baseRange`, `QRFPointsToWin`, `disguiseDetectionDistance`) — clients silently use prefab defaults; `m_Difficulty.name` also never replicates.
- **BUG-014 — no null guards** in `SetOccupyingFaction`/`SetSupportingFaction` and `OVT_Faction.Init()` on user-editable faction keys — an unknown key crashes.
- `OVT_OccupyingFactionSaveData.c:80` uses `OVT_Global().GetConfig()` (constructor-call syntax on a static class).
- Copy-paste: `SpawnGetInWaypoint` spawns the get-*out* prefab; `SpawnWaitWaypoint` ignores its `time` parameter.
- `OVT_DeploymentComponent.GetFactionType()` silently returns OCCUPYING on no-match.
- Metadata: `USSR_OverthrowData.conf.meta` names the wrong file; `DialogPresets_Campaign.conf` has no `.meta` and is referenced under three different GUIDs.

### Technical Debt
- `OVT_OverthrowConfigComponent` is a god object (config + waypoint factory + patrol behaviour).
- `OVT_FactionType` vs `OVT_FactionTypeFlag` — hand-maintained duplicate enums, no converter.
- Config classes scattered across 5 directories + inline in managers; the `Configuration/` convention is half-honoured.
- `OVT_Faction` carries two generations of group/vehicle definitions; only 1 of ~15 legacy slots is marked LEGACY.
- Prefab + managers.layer duplicate ~180 lines of wiring each.
- Hand-written unversioned replication bitstream — adding a field desyncs old clients.
- `m_ConfigFile` null on clients until `RplLoad`; several client sites dereference unguarded.
- `OVT_Global.GetDifficulty()` bypassed by 60+ call sites; orphaned `Prefabs/GameMode/OVT_FactionManager.et` references a field that no longer exists.

---

## Future Enhancements

### High Priority
- [ ] Replicate the 7 missing difficulty fields (+ `name`) or move them into the replicated set
- [ ] Null-guard faction-key setters and `OVT_Faction.Init()` against bad JSON/conf input

### Medium Priority
- [ ] Fix `SpawnGetInWaypoint`/`SpawnWaitWaypoint` copy-paste bugs
- [ ] Split the waypoint factory + patrol behaviour out of the config component
- [ ] Single source for prefab/layer config wiring

### Low Priority / Nice to Have
- [ ] Unify the faction-type enums; retire legacy `OVT_Faction` slots
- [ ] Log Mechanism-B load failures instead of silent null

---

## Testing

### Current Coverage
- **Framework:** `Setup_StartCampaign()` selects the 'Test World' preset **by name** (append-trap documented in `OVT_TEST_SuiteBase.c:34-41`)
- **Init tier:** 18-getter resolution (`GetConfig()` before `GetDifficulty()`); economy price seams derive expectations from `m_fShopProfitMargin` rather than hardcoding
- **Logic tier:** modifier maths against synthetic in-memory `OVT_ModifiersConfig` (bypasses loading entirely)
- **Campaign tier:** live Mechanism-B modifier round-trips; faction-index town-control flips read from config
- Observed test-world values: taxIncome 25, donationIncome 10, startingCash 100000

### Testing Gaps
- `LoadConfig()`/`SaveConfig()` JSON round-trip & malformed-file handling; `RplSave`/`RplLoad` symmetry; invalid faction keys; `OVT_ConfigSaveData` persistence; Mechanism-B failure path; `OVT_Faction` registry lookups/weighted selection; `overrideDifficulty` ordering

---

## Documentation

### Current Documentation
- `docs/technical-design.md`; the difficulty-preset trap in `dev-ops/test-coverage` findings (§ bug 7) and `OVT_TEST_SuiteBase` comments

### Documentation Needs
- A stated rule for when to use attribute-inlining vs `BaseContainerTools` loading

---

## Dependencies

### External Dependencies
- Arma Reforger `SCR_FactionManager`, `SCR_Faction`, `BaseContainerTools`, `SCR_JsonLoadContext`; base-game US/FIA faction `.conf`s (referenced by GUID)
- EPF (`OVT_ConfigSaveData`) — until `core/persistence` lands

### Internal Dependencies
- `core/game-mode` (hosts the component; `LoadConfig()` and role-index caching run in its lifecycle)
- Consumed by every gameplay system (economy, occupying faction, resistance, wanted, towns, deployments, UI)

---

## Notes

**Discovered Information:**
- Three config mechanisms coexist with no stated rule; Mechanism A dominates.
- The difficulty preset the test world runs is index 4, not 0 — layer overrides append.
- Difficulty replication is initial-state-only; late changes never reach clients.

**Retrospective Assessment:**
- The data-driven layer itself is solid and Workbench-friendly; the debt concentrates in the hub component (god object, replication gap, unguarded string keys) rather than the data classes.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature core/config` to begin making improvements.*
