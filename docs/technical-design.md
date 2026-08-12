# Overthrow

## Technical Design Document

**A companion to the [Mission Statement](mission-statement.md) — this document guides all technical decisions for the project, from architecture to data formats.**

Overthrow is a community-developed, MIT-licensed mod for Arma Reforger with a small distributed contributor base and a live player population on the Workshop. That shapes everything below: there is no CI and no debugger, and the automated test suite covers a spine (30 assertions in the default targets) rather than the surface, so correctness still has to come from conservative patterns and disciplined review rather than from tooling. It is production software — servers run it for months — but validation of anything multiplayer, UI-facing or save/reload-dependent still happens inside the Arma Reforger Workbench by hand. (Compile verification and the autotest loop are automated — `tools/compile-check.sh` and `tools/run-tests.sh`, delivered by the `dev-ops` epic, §12 — and that epic is building the rest of the pipeline.)

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
| Persistence | Reforger vanilla (first-party) persistence | Native C++, console-safe, less custom serialization. **Migrated off EPF 2026-08-02** |
| Config format | Enfusion `.conf` resources | Editable in Workbench; retunable by servers without recompiling |
| Localization | `Language/localization_Overthrow.st` | 6+ locales: en-us, ru-ru, uk-ua, fr-fr, ko-kr, … |
| Distribution | Arma Reforger Workshop (`59B657D731E2A11D`) | Where players actually get it |

### Declared Dependencies (`addon.gproj`)

| GUID | Addon |
|------|-------|
| `58D0FB3206B6F859` | Arma Reforger base data |

That is the **only** declared dependency. EPF (`5D6EBC81EB1842EF`) and EDF were removed by the persistence migration on 2026-08-02.

Reference trees used during development (read-only, not vendored):

- `/mnt/n/Projects/Arma 4/ArmaReforger` — all base game scripts, configs and UI layouts (includes `scripts/Game/Plugins/Persistence/`, the persistence reference)
- ⚠️ `/mnt/n/Projects/Arma 4/EnfusionPersistenceFramework` and `/mnt/n/Projects/Arma 4/EnfusionDatabaseFramework` may still exist on disk but are **no longer used** — do not consult them for current patterns

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
- ~~**No unit or integration tests.** Correctness is established by play-testing, hosted and joined.~~ *(Invalidated 2026-08-02 by `dev-ops/autotest-foundation`: Reforger's shipped `SCR_Autotest` framework is wired in, suites live in `Scripts/Game/Tests/`, and `tools/run-tests.sh` runs them in the real client for an honest exit code from `junit.xml` — see `tools/README.md`. Extended 2026-08-02 by `dev-ops/test-coverage` (#3): **30 assertions across four tiers** reachable as one command (§10). **Coverage is a spine, not the surface** — JIP/multiplayer, UI, performance and the save/reload round-trip are still uncovered, and play-testing remains the gate for all of them.)*
- **No debugger.** `Print()` is the debugging tool. Debug output is read out of the Workbench console.
- **No exceptions, no stack unwinding.** Null checks and defensive returns instead.
- **No ternary operator.** Always full `if`/`else` — this is a compile error, not a style rule.

### What Follows From That

- **Changes must be reviewable by reading.** A pattern that's easy to get subtly wrong is a bad pattern here, even if it's terser.
- **Every change ships with test steps.** "Host a game, join with a second client, do X, restart the server, verify Y" — because for join-in-progress, UI and save/reload that is still the only verification that exists (§10).
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
│   │   ├── Persistence/        # (see Scripts/Game/Persistence/Serializers/ for save code)
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
│   ├── Modded/                 # `modded class` overrides of vanilla Reforger classes
│   └── Tests/                  # Autotest integration: TestFramework/ (glue) + TestSuites/<Area>/
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

**`Scripts/Game/Tests/`** — the entire autotest integration: `TestFramework/` holds the glue (the `modded class SCR_AutotestHelper` that picks the test world and keeps Overthrow loaded across the scenario transition, plus `OVT_TEST_SuiteBase`), and `TestSuites/<Area>/` holds the suites and their cases. **Deliberate deviation:** that `modded class` would by convention live in `Modded/`, but it is kept here so the whole test integration is one self-contained, deletable directory rather than two files that only make sense together sitting in different trees. Run with `tools/run-tests.sh` (see §10).

**`Scripts/Game/Persistence/Serializers/`** — the core/persistence target structure (`Components/`, `Entities/`, `States/`), being populated by the in-flight migration.

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

### Reforger vanilla (first-party) persistence — shipped 2026-08-02

Persistence is built on Reforger's own system. **EPF is retired**: zero `EPF_` references in `Scripts/`, EPF/EDF removed as dependencies, and the `#ifdef PLATFORM_CONSOLE` carve-outs deleted (the vanilla system handles console storage internally).

Two layers, deliberately not conflated:

1. **`PersistenceSystem` / `SCR_PersistenceSystem`** — a server-only `WorldSystem` that tracks instances and serializes them. Config-driven.
2. **`SaveGameManager`** — the engine singleton owning save *points* and load/restart transitions. Wrapped by `OVT_PersistenceManagerComponent` (`SaveGame()` → `RequestSavePoint(ESaveGameType.MANUAL)`).

**Serializers, not SaveData classes.** State is persisted by `ScriptedComponentSerializer` / `ScriptedEntitySerializer` / `ScriptedStateSerializer` subclasses under `Scripts/Game/Persistence/Serializers/` (`Components/`, `Entities/`), overriding `GetTargetType()`, `Serialize(owner, component, SaveContext)` and `Deserialize(owner, component, LoadContext)`. 16 component serializers ship today.

**Binding is config, not script.** A serializer is bound by an entry in `Configs/Systems/Persistence/Overthrow.conf` (which inherits vanilla `Common.conf`); one that compiles but is not listed there is silently never called. There is no per-serializer registration API. ⚠️ Scripted config *rules* are dead code — the engine never calls a scripted `IsMatch()`; use `GetConfig`/`SetConfig` (BUG-018).

**Format.** Binary contexts are positional — write order must equal read order — and `version` is written first so an older payload still loads (`if (version < 1) return true;`).

**Spawned entities must be tracked.** Buildables and placeables have no authored world instance, so their configuration carries `SelfSpawn` and the spawn site calls `OVT_PersistenceTracking.Track()`. Component state alone restores nothing.

**Reservation model.** A released record does not outlive its entity (BUG-086), so offline player bodies, locked vehicles and recruit bodies are kept alive, tracked and hidden in place rather than saved-and-released — see `OVT_PersistenceReservation.c`.

⚠️ **Breaking:** EPF-era saves are dead, with no converter.

API truth, with file:line citations against retail 1.7.0.54: `docs/features/core/persistence/vanilla-api-reference.md`.

**The migration's acceptance gate passed.** `OVT_TEST_PersistenceRoundTripSuite` (§10) mutates state through Overthrow's public manager API, saves, reloads and reads it back — no persistence API type appears in any assertion, so it could not report the backend change as a regression by construction. It **flipped exit 1 → 0 on 2026-08-02** and is now de-quarantined and part of the All group. ⚠️ It covers save→dirty→**in-session re-apply**, not the true quit-and-continue path: `SaveGameManager.Load`'s world transition restarts the autotest harness, so restart/continue remains manual play-testing.

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

### The test suite covers a spine, not the surface

Reforger 1.7.0 ships a script test framework with JUnit output; `dev-ops/autotest-foundation` wired it into Overthrow and `dev-ops/test-coverage` put assertions in it (both 2026-08-02). What exists today is **32 cases across the six non-quarantined suites** — of which the 30 in the four tier suites are reachable as one command — plus a seventh suite (9 cases, 41 in the tree in total) that is quarantined and red by design. Suites are organised by setup cost rather than by subject, because the world transition and the campaign start are per-*suite* costs; coverage grows by adding a case file to an existing tier:

| Tier | Suite | Cases | Covers |
|---|---|---|---|
| A | `OVT_TEST_LogicSuite` | 14 | Pure maths, world-free (~8 s): town record maths and modifier recalculation, job conditions, skill effects, player levelling. |
| B | `OVT_TEST_InitSuite` | 4 | World loaded, campaign **not** started: the `OVT_Global` manager sweep, towns populated, town/base controllers registered, economy price/demand seams. |
| C | `OVT_TEST_CampaignSuite` | 4 | Campaign started in Setup: start flags and difficulty preset, town activation, shop stocking, tax/donation income. |
| D | `OVT_TEST_PersistenceSuite` | 8 | Same-session state round-trips through Overthrow's public manager API: money, skills/XP, real-estate ownership, recruits, town control/support/population/stability. |
| D' | `OVT_TEST_PersistenceRoundTripSuite` | 9 *(not in the 32)* | Save + reload. **Quarantined and red by design** — it is the `core/persistence` acceptance gate (§7), and is in no group. |
| — | `OVT_TEST_SmokeSuite` / `OVT_TEST_MetaSuite` | 1 / 1 | The harness's own green and always-red proofs, inherited from #2. Neither asserts anything about Overthrow. |

Every case has a recorded proof that it can be made to fail, and `maxAttempts` appears nowhere in the tree — a test that cannot go red is treated as a defect. Method and verbatim failure text: `docs/features/dev-ops/test-coverage/findings.md`, which also carries the eleven pre-existing gameplay bugs the work uncovered (logged, deliberately not fixed).

**Still entirely manual:** join-in-progress and everything else multiplayer (it needs two coordinated processes), UI, performance, AI movement (navmesh does not load in the test world), the save/reload round-trip (gated — see §7), and `modded class` overrides broken by a Reforger update. Play-testing is the only verification of any of those.

### What we do instead

1. **Automated compile check first** — the assistant runs `tools/compile-check.sh` after code changes: exit 0 is a positively-verified clean compile, exit 1 prints errors as `file:line: message` (see `tools/README.md`). The Workbench GUI (Build → Compile and Reload Scripts) remains the interactive alternative for the user.
2. **Automated tests second** — `tools/run-tests.sh [<target>]` launches the real game client with `-autotest`, runs the named target in `Worlds/MP/OVT_Campaign_Test.ent`, and derives an honest verdict from `junit.xml`: 0 = passed, 1 = test failures, 2 = indeterminate (missing artifact, bad target), 124 = timeout. Artifacts in `.tmp/run-tests/`. Two stable group targets cover the tiers in one launch — **Fast** `"{6A6E29FF47ECB840}"` (Logic + Init, 18 cases, ~16 s) for every change, **All** `"{6A6E2A002F53A581}"` (+ Campaign + Persistence, 30 cases, ~19 s) before a merge; a bare class name still runs one suite or one case for debugging. Suites live under `Scripts/Game/Tests/TestSuites/<Tier>/`, inherit `OVT_TEST_SuiteBase`, and are named `OVT_TEST_<Tier>Suite` / `OVT_TEST_<Tier>_<Subject>_<ExpectedBehaviour>`. Campaign- and persistence-tier suites need a fresh save DB first (`.scripts/reset_save.sh --profile OverthrowCI` — never without the profile). Contract: `tools/README.md`; authoring patterns: the `workbench-workflow` skill.
3. **Test in the fast world** — `Worlds/MP/OVT_Campaign_Test.ent` loads far faster than the full Eden map. Use it for everything except map-specific work.
4. **Test hosted, then joined** — a change that works in a hosted session but not for a joining client is the default failure mode. Both paths, every time.
5. **Test the restart** — anything touching persistence is only verified after a save, a shutdown and a reload.
6. **`Print()` liberally** — the console is the debugger.

### Every change ships with test steps

Because verification of behaviour is still manual for everything the suites do not cover — which is every dimension below except one — a change is not complete until someone has written down exactly what to do to check it: which world, hosted or joined, what actions, what to observe, whether a restart is needed. Vague test instructions produce untested code.

### The three dimensions that break

Almost every regression in this project is one of: **join-in-progress state**, **persistence round-trip**, or a **vanilla `modded class` override** broken by a Reforger update. Only one of the three is even partly automated:

- **Join-in-progress / multiplayer — entirely manual.** Two coordinated client processes are needed and the autotest harness runs one. This is the most common regression class and it has no machine check at all: host, join with a second client, and check what the joining player sees.
- **Persistence round-trip — half automated.** `OVT_TEST_PersistenceSuite` proves state written through a manager's public mutator reads back through its public accessor within a session, which catches "setting town control doesn't stick". The save/reload half is written but **gated** behind the migration (§7), so a restart still has to be tested by hand.
- **`modded class` overrides — entirely manual.** A base-game signature change usually fails at compile time (`tools/compile-check.sh` catches that), but an override that still compiles and no longer *does* anything is invisible to both gates. Play-test it.

Weight testing accordingly: the suites are a floor, not a substitute.

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

This epic supersedes the persistence migration in priority. The reasoning: the migration is a big-bang, breaking rewrite of every persisted system, and there is currently no way to verify it beyond manual restart testing. Building the test harness first turned that migration from unverifiable into gated. `test-coverage` (complete, 2026-08-02) wrote the behaviour-level persistence tests; because this branch has no working save path in *either* system, the same-session suite ships green and the save/reload suite ships quarantined and red, with its flip to exit 0 as the migration's acceptance criterion (§7).

**Shipped: `core/persistence`** — the EPF → vanilla persistence migration completed **2026-08-02** (53/53 tasks; SP and MP/dedicated play-tests green; every linked bug closed, including post-ship BUG-104). Big-bang and breaking: EPF-era saves are dead with no converter. See `docs/features/core/persistence/` and §7.

**Branch policy:** the bugfix-only freeze on `main` was lifted once the persistence migration landed; `main` is open again.

---

*This is a living document. Update it as technical decisions are made, patterns emerge, and the architecture evolves.*
