# Overthrow

## Technical Design Document

**A companion to the [Mission Statement](mission-statement.md) — this document guides all technical decisions for the project, from architecture to data formats.**

Overthrow is a community-developed, MIT-licensed mod for Arma Reforger with a small distributed contributor base and a live player population on the Workshop. That shapes everything below: there is no CI, no automated test suite and no debugger, so correctness has to come from conservative patterns and disciplined review rather than from tooling. It is production software — servers run it for months — but runtime validation still happens inside the Arma Reforger Workbench by hand. (Compile verification is now automated — `tools/compile-check.sh`, delivered by the `dev-ops` epic, §12 — and that epic is building the rest of the pipeline.)

---

## Table of Contents

1. Stack & Environment
2. Constraints That Shape Everything
3. Project Structure
4. Architecture Overview
5. Managers, Controllers & Components
6. Networking & Replication
7. Persistence
8. Configuration
9. UI Layer
10. Testing Strategy
11. Development Principles
12. Current Phase

---

## 1. Stack & Environment

### EnforceScript

Not a choice — Enfusion runs EnforceScript and nothing else. It is a statically-typed, C-like scripting language with garbage collection but explicit strong-reference semantics for `Managed` classes. Notably it has **no ternary operator**, no generics beyond `array<T>`/`map<K,V>`, and no exceptions.

### Core Technologies

| Layer | Technology | Rationale |
|-------|-----------|-----------|
| Language | EnforceScript | The only language the Enfusion engine executes |
| Engine | Enfusion (Arma Reforger) | Target platform; entity-component architecture |
| Build / IDE | Arma Reforger Tools — Workbench | The only supported toolchain; compiles and hot-reloads scripts |
| Project file | `addon.gproj` | Declares GUID `59B657D731E2A11D`, dependencies, script defines, localization tables |
| Replication | Enfusion `Replication` / `RplComponent` | Engine-native; the only way to reach clients |
| Persistence (current) | EPF — Enfusion Persistence Framework | Mature third-party save system; predates first-party persistence |
| Persistence (target) | Reforger vanilla persistence | Native C++, console-safe, less custom serialization — migration in progress |
| Config format | Enfusion `.conf` resources | Editable in Workbench; retunable by servers without recompiling |
| Localization | `Language/localization_Overthrow.st` | 6+ locales: en-us, ru-ru, uk-ua, fr-fr, ko-kr, … |
| Distribution | Arma Reforger Workshop (`59B657D731E2A11D`) | Where players actually get it |

### Declared Dependencies (`addon.gproj`)

| GUID | Addon |
|------|-------|
| `58D0FB3206B6F859` | Arma Reforger base data |
| `5D6EBC81EB1842EF` | Enfusion Persistence Framework (EPF) |

Reference trees used during development (read-only, not vendored):

- `/mnt/n/Projects/Arma 4/ArmaReforger` — all base game scripts, configs and UI layouts
- `/mnt/n/Projects/Arma 4/EnfusionPersistenceFramework` — EPF source
- `/mnt/n/Projects/Arma 4/EnfusionDatabaseFramework` — EDF, EPF's database dependency

`update-arma-scripts.ps1` refreshes the Reforger reference tree from the Steam install.

### Workbench Script Defines

The `workbench` configuration in `addon.gproj` defines `DEBUG_NAVMESH_REBUILD_AREAS`, `PLATFORM_WINDOWS`, `ENF_WB`, `WORKBENCH`, `PERSISTENCE_DEBUG`. Code guarded by these will **not** run on a shipped server — never put load-bearing logic behind them.

### Why This Stack

There is essentially no discretion in the stack: modding Reforger means EnforceScript in Workbench against Enfusion's component model. The real technical decisions are all *architectural* — how state is owned, replicated and persisted — which is what the rest of this document covers.

---

## 2. Constraints That Shape Everything

These are not preferences. They are hard properties of the environment, and most of the architecture exists to work around them.

### What We Don't Have

> ⚠️ **This section is being actively invalidated.** Reforger 1.7.0 ships a script test framework with JUnit output, and the Workbench has CLI automation flags — neither was usable when this project's workflow was established. The `dev-ops` epic (§12) is building on both. Each entry below is struck through by the feature that makes it false; do not treat this list as permanent.

- ~~**No automated builds.** Compilation happens when a human presses Build in Workbench. Claude and CI can never verify that a change compiles.~~ *(Invalidated 2026-08-01 by `dev-ops/workbench-automation`: `tools/compile-check.sh` compiles all of Overthrow's EnforceScript headlessly from WSL and returns a verified exit code plus `file:line: message` errors — see `tools/README.md`.)*
- **No unit or integration tests.** Correctness is established by play-testing, hosted and joined. *(Addressed by `dev-ops/autotest-foundation` + `dev-ops/test-coverage`.)*
- **No debugger.** `Print()` is the debugging tool. Debug output is read out of the Workbench console.
- **No exceptions, no stack unwinding.** Null checks and defensive returns instead.
- **No ternary operator.** Always full `if`/`else` — this is a compile error, not a style rule.

### What Follows From That

- **Changes must be reviewable by reading.** A pattern that's easy to get subtly wrong is a bad pattern here, even if it's terser.
- **Every change ships with test steps.** "Host a game, join with a second client, do X, restart the server, verify Y" — because that's the only verification that exists.
- **Failure modes must be loud.** Silent null propagation costs a full Workbench round-trip to diagnose. Print on the unexpected path.

### What We Avoid

- **`EntityID` across the network.** IDs are process-local; sending one produces a reference that resolves to the wrong entity or nothing on the other machine. Use `RplId`.
- **Client-authoritative state.** A client that decides something is a client that can be desynced or exploited.
- **Managed objects in collections without `ref`.** Missing the `ref` keyword lets the GC collect entries out from under an array or map — a crash with no useful trace.
- **Load-bearing logic behind `WORKBENCH`/`PERSISTENCE_DEBUG` defines.** It works for the developer and nobody else.

---

## 3. Project Structure

```
Overthrow.Arma4/
├── addon.gproj                 # Enfusion project: GUID, dependencies, defines, localization
├── CLAUDE.md                   # Working instructions for AI assistance
├── Scripts/Game/               # All EnforceScript (~246 .c files)
│   ├── GameMode/               # Game mode entity, managers, save data, systems
│   │   ├── Managers/           # Singleton manager components (see §5)
│   │   ├── Persistence/        # EPF SaveData classes + components
│   │   ├── SaveData/           # Loadout save data
│   │   ├── Systems/            # Town modifiers, jobs, skill effects
│   │   ├── Deployments/, Events/, Placeables/
│   │   ├── OVT_OverthrowGameMode.c        # Host for every manager
│   │   ├── OVT_OverthrowController.c      # Per-player client→server bridge
│   │   └── OVT_OverthrowFactionManager.c
│   ├── Controllers/            # Per-entity controllers
│   │   ├── OccupyingFaction/   # Base, QRF, tower controllers + base upgrades
│   │   ├── ResistanceFaction/  # FOB controller
│   │   ├── OVT_TownController.c, OVT_PortController.c
│   ├── Components/             # Reusable entity/UI components
│   │   ├── Controller/, Damage/, Economy/, Player/
│   │   └── OVT_Component.c, OVT_PlaceableComponent.c, OVT_BuildableComponent.c, …
│   ├── Persistence/Serializers/# Vanilla-persistence serializers (Components/Entities/States)
│   ├── Global/OVT_Global.c     # Static accessor for every manager
│   ├── UI/                     # Context/, HUD/, Map/, Menu/, Nametags/, Components/
│   ├── AI/, Commanding/, Faction/, Player/, Respawn/, Entities/
│   ├── Configuration/          # Config class definitions
│   ├── UserActions/            # Player interaction actions
│   ├── Utilities/, Data/
│   └── Modded/                 # `modded class` overrides of vanilla Reforger classes
├── Configs/                    # Tuning data (see §8)
├── Prefabs/, PrefabsEditable/  # Entity prefabs and editable compositions
├── Worlds/MP/                  # OVT_Campaign_Eden.ent (full map), OVT_Campaign_Test.ent (dev)
├── UI/                         # Layouts, textures, imagesets
├── Language/                   # .st string table + per-locale .conf
├── Assets/, Sounds/, Missions/
└── docs/                       # This document, mission statement, features/, reforger/
```

### Directory Responsibilities

**`Scripts/Game/GameMode/Managers/`** — one file per system-wide singleton. If a thing needs to be asked a question from anywhere ("what does this cost?", "who owns this house?"), it is a manager.

**`Scripts/Game/Controllers/`** — one file per *kind of place*. A town, a base, a port, a FOB each have a controller component on their entity that owns that instance's behaviour and state.

**`Scripts/Game/Components/`** — reusable components attached to arbitrary entities (buildable, placeable, parking, spawn point, player owner).

**`Scripts/Game/Modded/`** and the `Modded/` subfolders — `modded class` extensions of vanilla Reforger classes. Kept separate because they are the most fragile code in the project: a Reforger update can break them without any change on our side.

**`Scripts/Game/Persistence/Serializers/`** — the vanilla-persistence target structure (`Components/`, `Entities/`, `States/`), being populated by the in-flight migration.

**`Configs/`** — no code. See §8.

**`docs/features/`** — Beast Mode feature docs: `requirements.md` → `implementation.md` → `context.md` + `tasks.md` per feature.

---

## 4. Architecture Overview

### Overview

Everything hangs off a single game mode entity. Managers live on it as components; `OVT_Global` is the static front door to all of them; per-player `OVT_OverthrowController` entities carry client→server traffic; controllers on world entities own local behaviour.

```
                    ┌──────────────────────────────────────────┐
                    │        OVT_OverthrowGameMode             │   SERVER
                    │  (single entity — hosts all managers)    │   AUTHORITY
                    ├──────────────────────────────────────────┤
                    │ EconomyManager      TownManager          │
                    │ PlayerManager       JobManager           │
                    │ RealEstateManager   VehicleManager       │
                    │ InventoryManager    LoadoutManager       │
                    │ RecruitManager      SkillManager         │
                    │ OwnerManager        RplOwnerManager      │
                    │ NotificationManager PersistenceManager   │
                    │ RespawnSystem       OverthrowConfig      │
                    │ FactionManager      TimeAndWeatherHandler│
                    └───────────┬──────────────────┬───────────┘
                                │                  │
              OVT_Global.GetX() │                  │ owns / ticks
                                │                  ▼
   ┌────────────────────────────┴───┐   ┌──────────────────────────────────┐
   │  Any script, anywhere          │   │  Entity Controllers (per place)  │
   │  OVT_Global.GetEconomy()       │   │  TownController  BaseController  │
   │  OVT_Global.GetTowns() …       │   │  QRFController   TowerController │
   └────────────────────────────────┘   │  PortController  FOBController   │
                                        └──────────────────────────────────┘
                                │
        ════════════════════════╪══════════ network boundary ═══════════════
                                │  RplProp / RpcDo (server→client)
                                │  RpcAsk (client→server)
                                ▼
                    ┌──────────────────────────────────────────┐
                    │   OVT_OverthrowController (per player)   │   CLIENT
                    │   + OVT_PlayerCommsComponent             │
                    │   + OVT_UIManagerComponent               │
                    ├──────────────────────────────────────────┤
                    │   UI Contexts (OVT_UIContext) — map,     │
                    │   HUD, menus, nametags, shops, build     │
                    └──────────────────────────────────────────┘
```

### The Access Pattern

`OVT_Global` is the single static accessor. It resolves the game mode via `GetGame().GetGameMode()` and hands back manager components; it also resolves *client-side* objects (`GetUI()`, `GetController()`) from `SCR_PlayerController.GetLocalControlledEntity()`. `GetServer()` is deliberately dual-natured — on the server it returns the game mode's comms component, on a client it returns the local player's — so calling code can be written once.

This means **there is no dependency injection and no service locator beyond `OVT_Global`.** Adding a manager means adding a component to the game mode prefab and a static getter to `OVT_Global`.

### Server Authority

The server owns all campaign state. Clients never mutate it directly: a client action becomes an `RpcAsk_*` on the player's comms/controller component, the server validates and applies it, and the result propagates back via `RplProp` or an `RpcDo_*` broadcast. Any code path where a client writes authoritative state is a bug regardless of whether it currently misbehaves.

---

## 5. Managers, Controllers & Components

### Managers — one system, one singleton

A manager is a component on `OVT_OverthrowGameMode` that owns an entire subsystem for the whole session. Current managers: **Economy** (prices, shops, money), **Town** (control, stability, population, support), **Player**, **Job**, **RealEstate**, **Vehicle**, **Inventory**, **Loadout**, **Recruit**, **Skill**, **Owner** / **RplOwner**, **Notification**, **Persistence**, **RespawnSystem**, **OverthrowConfig**, plus **FactionManager** and **TimeAndWeatherHandler**.

Reach for a manager when the answer to "who owns this state?" is "the campaign".

### Controllers — one place, one instance

A controller is a component on a world entity that owns that instance's behaviour: `OVT_TownController`, `OVT_BaseControllerComponent`, `OVT_QRFControllerComponent`, `OVT_TowerControllerComponent`, `OVT_PortController`, `OVT_ResistanceFOBControllerComponent`. Controllers hold per-instance state and are driven by (and report to) the relevant manager.

Reach for a controller when the answer is "this town" / "this base".

### `OVT_OverthrowController` — the client bridge

A per-player entity carrying the components a client needs to talk to the server (`OVT_PlayerCommsComponent`) and to drive its own UI (`OVT_UIManagerComponent`, `OVT_ContainerTransferComponent`, …). It exists because the game mode entity has no viewport and no player context — anything that needs *this player's* screen or *this player's* input belongs here, not on the game mode.

This distinction has already caused one significant refactor (see `CHANGES.md`: the start menu had to move from the game mode to the player controller because the game mode has no viewport).

### Naming & Style Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| Class prefix | `OVT_` | `OVT_TownManagerComponent` |
| Member variable | `m_` | `m_Towns` |
| Typed members | `m_i` `m_f` `m_s` `m_b` `m_a` `m_m` | `m_iPopulation`, `m_bIsOccupied`, `m_aTowns` |
| Static singleton | `s_Instance` | |
| Client→server RPC | `RpcAsk_*` | `RpcAsk_BuyItem` |
| Server→client RPC | `RpcDo_*` | `RpcDo_UpdateStability` |
| Docs | Doxygen `//!` | |

### Memory Management

`Managed` classes stored in `array<>`/`map<>` **must** use the `ref` keyword. Without it the collection holds a weak reference, the GC reclaims the object, and the next access crashes with no useful diagnostic. This is the single most common source of hard-to-diagnose crashes in the codebase.

---

## 6. Networking & Replication

Multiplayer is the design target, not a mode. Three mechanisms, chosen by what's being moved:

### `RplProp` — simple values

Ints, floats, bools, short strings that change occasionally and are read by everyone. Cheapest option; the engine handles the diffing. Use for things like town stability or control percentages.

### RPC — actions and complex data

```
Client action  →  RpcAsk_DoThing()   →  [SERVER validates + applies]
                                              ↓
Every client   ←  RpcDo_ThingHappened()  ←  broadcast
```

`RpcAsk_*` is the only legitimate way for a client to change the world. The server must re-validate everything — the client is not trusted about money, ownership, position, or permissions.

### `RplSave` / `RplLoad` — join-in-progress

A player joining a running server must receive current state, not the state at map load. Any replicated component holding non-trivial state implements JIP serialization. **This is part of the feature, not a follow-up** — a late joiner seeing a stale or empty world is the most common multiplayer regression in this project.

### Identity Across the Boundary

**`RplId` for anything that crosses the network. `EntityID` only within a single machine.** `EntityID` values are not stable across processes; sending one is silent corruption, not an error.

---

## 7. Persistence

### Current: EPF

Persistence is currently built on the Enfusion Persistence Framework. Components that persist state have a paired SaveData class extending `EPF_ComponentSaveDataClass` (e.g. `OVT_TownSaveData`, `OVT_BuildingSaveData`, `OVT_BaseUpgradeSaveData`, `OVT_PlaceableSaveData`, `OVT_LoadoutManagerSaveData`), coordinated by `OVT_PersistenceManagerComponent`. EPF writes to disk, which consoles do not permit — hence `#ifdef PLATFORM_CONSOLE` guards around EPF usage.

Roughly 59 script files currently reference `EPF_`.

### Target: Reforger vanilla persistence

Reforger now ships first-party persistence, and Overthrow is migrating to it wholesale. The goals are native C++ save/load performance, far less custom serialization boilerplate, console support without platform carve-outs, and the `WhenAvailable` pattern for async entity references. `Scripts/Game/Persistence/Serializers/` (`Components/`, `Entities/`, `States/`) is the target structure.

**This is an explicit breaking change**: existing saves will not be migrated, and it is being done big-bang rather than dual-running both systems. Full scope, file-by-file, in `docs/features/vanilla-persistence/`.

### What Persists

Town state (control, stability, population, support), base and FOB state and upgrades, player money and skills, real estate ownership, vehicles, container and player inventories, loadouts, recruits, and placed structures. If a player earned it or changed it, it persists.

---

## 8. Configuration

Tuning lives in `Configs/` as Enfusion `.conf` resources, not in EnforceScript constants — so a server operator can retune the campaign in Workbench without recompiling, and so the same code serves very different server flavours.

```
Configs/
├── Factions/      Difficulty/    Pricing/       Jobs/
├── Civilians/     Commanding/    Deployment/    Modifiers/
├── Map/           NameTags/      Player/        Resistance/
├── FieldManual/   System/        Systems/       UI/
├── TestServerConfig.conf
└── overthrowBroadcastMessages.conf
```

Config *classes* are defined in `Scripts/Game/Configuration/` and surfaced through `OVT_OverthrowConfigComponent` (reachable as `OVT_Global.GetConfig()`).

**Rule of thumb:** a number a server owner might reasonably want to change belongs in a config, not in a script.

---

## 9. UI Layer

UI is entirely client-side and hangs off the player, never the game mode.

- **`OVT_UIContext`** (`Scripts/Game/UI/OVT_UIContext.c`) is the base for every screen — shops, build menus, the map, the start menu. Contexts are shown and hidden by `OVT_UIManagerComponent` on the player's controller entity.
- **`UI/Layouts/`** holds `.layout` files authored in Workbench; script binds to named widgets.
- **`Scripts/Game/UI/`** splits into `Context/` (screens), `HUD/`, `Map/`, `Menu/`, `Nametags/`, `Components/`.
- All player-visible strings go through `Language/localization_Overthrow.st` — the mod ships 6+ locales and hardcoded English is a regression.

**Anything needing a viewport must live on the player controller entity.** The game mode has none.

---

## 10. Testing Strategy

### There is no test suite

No unit tests, no integration tests, no CI. This is a property of the platform, not a backlog item.

### What we do instead

1. **Automated compile check first** — the assistant runs `tools/compile-check.sh` after code changes: exit 0 is a positively-verified clean compile, exit 1 prints errors as `file:line: message` (see `tools/README.md`). The Workbench GUI (Build → Compile and Reload Scripts) remains the interactive alternative for the user.
2. **Test in the fast world** — `Worlds/MP/OVT_Campaign_Test.ent` loads far faster than the full Eden map. Use it for everything except map-specific work.
3. **Test hosted, then joined** — a change that works in a hosted session but not for a joining client is the default failure mode. Both paths, every time.
4. **Test the restart** — anything touching persistence is only verified after a save, a shutdown and a reload.
5. **`Print()` liberally** — the console is the debugger.

### Every change ships with test steps

Because verification is manual, a change is not complete until someone has written down exactly what to do to check it: which world, hosted or joined, what actions, what to observe, whether a restart is needed. Vague test instructions produce untested code.

### The three dimensions that break

Almost every regression in this project is one of: **join-in-progress state**, **persistence round-trip**, or a **vanilla `modded class` override** broken by a Reforger update. Weight testing accordingly.

---

## 11. Development Principles

1. **The server decides.** If a client can change authoritative state without the server validating it, the design is wrong — fix the design, not the symptom.

2. **Late join is part of "done".** A feature that works for players present at map load and not for the person who joins ten minutes later is unfinished. Ask "what does a joining client see?" before calling anything complete.

3. **Persistence is designed in, not added later.** Decide what survives a restart while designing the feature. Retrofitting save/load onto finished state is where the bugs live.

4. **Manager for the campaign, controller for the place.** When adding state, answer "who owns this?" first. Getting this wrong produces state that is either unreachable or duplicated.

5. **Config over constant.** Any number a server owner might want different belongs in `Configs/`.

6. **Write for the reader, because there is no compiler in the loop.** Explicit `if`/`else`, explicit null checks, explicit `ref`, Doxygen on anything non-obvious. Cleverness costs a Workbench round-trip to debug.

7. **Vanilla first.** Prefer engine and base-game systems over bespoke reimplementation — every `modded class` override is a hostage to the next Reforger patch.

---

## 12. Current Phase

**Active priority: the `dev-ops` epic** — building an automated compile/test/release pipeline on Reforger 1.7.0's shipped `SCR_Autotest` framework and Workbench CLI automation. Five features in build order: `workbench-automation` → `autotest-foundation` → `test-coverage` → `ci-pipeline` → `release-automation`. See `docs/features/dev-ops/epic-overview.md`.

This epic supersedes the persistence migration in priority. The reasoning: the migration is a big-bang, breaking rewrite of every persisted system, and there is currently no way to verify it beyond manual restart testing. Building the test harness first turns that migration from unverifiable into gated — `test-coverage` writes behaviour-level persistence tests that pass against EPF today and become the migration's acceptance criteria.

**Paused: `vanilla-persistence`** — migrating the persistence layer from EPF to Reforger's native system. Big-bang, breaking, no save migration. Not abandoned; resumes once the test harness can validate it. See `docs/features/vanilla-persistence/`.

**Branch policy:** `main` is under a **bugfix-only code freeze** until the persistence migration lands. All feature work — including this epic — happens on the `vanilla-persistence` branch.

---

*This is a living document. Update it as technical decisions are made, patterns emerge, and the architecture evolves.*
