# Configuration System - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing legacy code)
- ✅ Retrospective documentation created (2026-08-02, via `/discover-feature`)

**What's Next:**
- 📋 Review for potential improvements — headline: the client difficulty replication gap and unguarded faction-key setters (see `implementation.md`)

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c` (619 ln) — the hub: difficulty, faction roles, `Overthrow_Config.json` (`LoadConfig()` L195), item limits, waypoint factory, `RplSave`/`RplLoad` (L530-618)
- `Scripts/Game/Configuration/` — 11 pure-data config classes (`OVT_DifficultySettings` etc.); more leaked into `Faction/`, `Deployments/`, `Systems/Modifiers/` and inline in managers
- `Scripts/Game/GameMode/OVT_OverthrowFactionManager.c` + `Scripts/Game/Faction/OVT_Faction.c` — parallel faction registry keyed by faction key
- `Configs/**` — 46 `.conf` files (difficulty presets, prices, jobs, skills, modifiers, deployments, faction data, buildables/placeables)
- `Prefabs/GameMode/OVT_OverthrowGameMode.et` + `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer` — duplicated config wiring (~180 lines each)

---

## Important Decisions

- **Three loading mechanisms coexist**: (A) Workbench attribute inlining in prefab/layers — dominant, no script; (B) `ResourceName` + `BaseContainerTools` runtime load — 3 attributes only, fails silently; (C) `$profile:Overthrow_Config.json` — server-only, console-guarded, defaults written on first run.
- **Faction roles as key + cached index**: occupying/supporting settable (JSON/UI, "FIA" refused); player faction fixed at the attribute; downstream comparisons use int indices via `GetOccupyingFactionIndex()`.
- **Three-layer difficulty override**: prefab preset → JSON preset-by-name → JSON `overrideDifficulty` fields — applied in `DoStartGame()` *after* `PostGameStart()`.

---

## Gotchas & Learnings

- **World-layer preset override APPENDS, not replaces**: the test world has 5 presets and 'Test World' is index 4 — always select difficulty **by name** (`OVT_TEST_SuiteBase.FindDifficultyPreset`), never by index.
- **7 difficulty fields are read on clients but never replicated** (`baseRange`, `baseCloseRange`, `fastTravelCost`, `minFastTravelDistance`, `QRFFastTravelMode`, `QRFPointsToWin`, `disguiseDetectionDistance`) — clients see prefab defaults, not the server's preset.
- `m_Difficulty` **aliases the preset object** and is mutated in place by JSON overrides; that same reference is what `OVT_ConfigSaveData` (EPF) persists.
- `m_ConfigFile` is null on clients until `RplLoad` lazily creates it — several client sites dereference it unguarded.
- Unknown faction keys from JSON crash `SetOccupyingFaction`/`SetSupportingFaction` (no null guard).
- Full oddity list (20 items, incl. `SpawnGetInWaypoint` copy-paste bug and meta-file issues): see `implementation.md` § Technical Debt.

---

*This context file was created retrospectively by analyzing existing code.*
