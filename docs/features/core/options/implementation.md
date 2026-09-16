# Options: Implementation Plan

**Status:** Ready for Review. Built 2026-09-08 in one autorun (9 phases, 54/54). Compile 0, Logic 319/319, Init 213/213, round-trip 46/46, All 611/614 (three load timeouts outside this feature). Play-test, gamepad, MP and JIP checks owed.
**Started:** 2026-09-08
**Target Completion:** built 2026-09-08
**Last Updated:** 2026-09-08 23:20 AEST
**Epic:** `core` (feature #7)
**Requirements:** `docs/features/core/options/requirements.md`
**Approach:** registry + generated form (user decision, 2026-09-08)
**Branch:** `main`

---

## 1. Executive Summary

The pause menu carries a dead **Options** button. `UI/Layouts/Menu/MainMenu.layout:563-575` defines it
with `"Is Enabled" 0`, and `OVT_MainMenuContext` never wires it. This feature makes the button real,
and it does so through a registry rather than a hand-built screen.

A new manager singleton, `OVT_OptionsManagerComponent`, owns a declarative registry of options. Each
option carries an id, a scope, a field type, a default, constraints and two localization keys. Any
subsystem registers an option one time and then reads the live value. The Options screen is
**generated** from the registry and grouped by scope, so a later subsystem adds an option with one
call and no UI edit.

Three scopes exist. `LOCAL` values stay on the player's machine in a `ModuleGameSettings` profile
block. `FACTION` and `WORLD` values live on the server, replicate to every client, and persist in the
campaign save. The server re-validates the permission for every write, so the client-side gate is
only a courtesy.

The first consumer is autosave. Two `WORLD` options replace the two prefab attributes on
`OVT_PersistenceManagerComponent` as the runtime source of truth. A player who turns autosave off, or
who moves the interval, sees the effect at once and after a save and a Continue.

This version ships all three field types (`TOGGLE`, `SLIDER`, `CHOICE`) and the `LOCAL` profile store,
but it ships **no** `LOCAL` option and **no** `CHOICE` consumer. Both exist so the next subsystem finds
a complete venue. A section with zero options never renders, so the Local section stays hidden.

---

## 2. Goals

### Primary

1. A declarative option registry that a subsystem joins with one registration call.
2. A generated Options screen that needs no edit when an option arrives.
3. Server authority on `FACTION` and `WORLD` writes, re-validated from scratch on the server.
4. Replication of `FACTION` and `WORLD` values to every client, and to a client that joins in progress.
5. Persistence of `FACTION` and `WORLD` values in the campaign save, conf-first.
6. Autosave enable and interval driven by the registry, effective with no restart.

### Secondary

1. A per-profile `LOCAL` store that follows the proven map-layer settings pattern.
2. A `CHOICE` field type with Logic coverage, ready for the first consumer.
3. Logic-tier coverage of every registry rule that can corrupt a value silently.
4. One persistence round-trip case that proves the save path end to end.

### Non-goals

Vanilla engine settings, server-persisted per-player settings, and a wholesale migration of
`Overthrow_Config.json` or the difficulty presets. The requirements list all three.

---

## 3. Architecture Overview

### 3.1 Component map

```
Game mode entity (Prefabs/GameMode/OVT_OverthrowGameMode.et)
└── OVT_OptionsManagerComponent          NEW - the singleton, the wire, the JIP snapshot
    └── ref OVT_OptionsRegistry          NEW - pure logic, no engine call
        ├── map<string, ref OVT_OptionDefinition>
        └── map<string, string>          the stored values

Controller entity (Prefabs/GameMode/OVT_OverthrowController.et)
└── OVT_OptionsRequestComponent          NEW - the client to server seam

Player character (Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et)
└── OVT_UIManagerComponent.m_aContexts
    └── OVT_OptionsContext                NEW - the generated form

Persistence (Configs/Systems/Persistence/Overthrow.conf)
└── OVT_OptionsManagerSerializer          NEW - version 1, one record array

Player profile (ModuleGameSettings)
└── OVT_OptionsLocalSettings              NEW - the LOCAL block
    └── OVT_OptionsLocalSettingsAccessor  NEW - the BaseContainer plumbing
        └── ref OVT_OptionsLocalStore     NEW - pure logic, no engine call

Consumer
└── OVT_PersistenceManagerComponent       EDITED - stop, restart, registry-driven start
```

The split between `OVT_OptionsRegistry` and `OVT_OptionsManagerComponent` is the same split that
`OVT_MapLayerPrefsStore` and `OVT_MapLayerSettingsAccessor` already use. Every rule that can corrupt a
value quietly lives in the pure class, where a Logic case can pin it. The manager is the engine half:
the singleton, the RPC, the JIP stream and the change invoker.

### 3.2 The pure registry

`OVT_OptionDefinition` is a plain `Managed` record:

| Field | Meaning |
|---|---|
| `m_sId` | namespaced id, for example `autosave.interval` |
| `m_eScope` | `OVT_EOptionScope.LOCAL`, `FACTION` or `WORLD` |
| `m_eType` | `OVT_EOptionType.TOGGLE`, `SLIDER` or `CHOICE` |
| `m_sDefault` | the default, in the one string encoding (D1) |
| `m_fMin`, `m_fMax`, `m_fStep` | `SLIDER` constraints, in the option's own units |
| `m_fShownMultiplier` | display scale for the row, default 1 (D8) |
| `m_sFormatKey` | display format for the row, for example `#OVT-Options_Minutes` |
| `m_aChoiceKeys` | `CHOICE` keys, the stored form (D2) |
| `m_aChoiceLabelKeys` | one label key for each choice key |
| `m_sLabelKey`, `m_sDescriptionKey` | the two localization keys the row draws |

`OVT_OptionsRegistry` holds the definitions, the stored values, and every rule:

- `Register(def)` is idempotent. The first definition for an id wins and a later one is refused.
- `Normalize(id, raw)` clamps and snaps. A `TOGGLE` becomes `"0"` or `"1"`. A `SLIDER` clamps to
  `[min, max]` and snaps to the nearest step from `min`. A `CHOICE` that names no declared key falls
  back to the default.
- `SetValue(id, raw)` normalizes and stores. An id with no definition stores the raw value and marks
  it **pending** (D5).
- `Register` re-normalizes a pending value and clears the pending mark.
- `GetValue(id)` answers the stored value, else the definition default, else `""`.
- `GetBool`, `GetFloat`, `GetInt` and `GetChoice` parse the stored string.
- `GetOverrides(ids, values)` returns every non-pending value that differs from its default. The
  serializer and the JIP stream both read this, so both carry the same set.
- `GetPendingIds(ids)` names the values that never found a definition. The manager logs them one time
  at save (D6).
- Three static predicates map a scope to a permission: `ScopeIsServerAuthoritative`,
  `ScopeRequiresOfficer` and `ScopeRequiresAdmin`. They are pure, so Logic pins the table.

### 3.3 The manager

`OVT_OptionsManagerComponent : OVT_Component` holds `s_Instance`, a `GetInstance()` accessor and
`OVT_Global.GetOptions()`. It exposes:

- `RegisterOption(def)` for a subsystem that ships its own definition.
- `OverrideDefault(id, value)` so a server operator's prefab attribute stays the registration default.
  It touches the default only and never a stored value, so a saved override wins in either order.
- Typed getters that delegate to the registry.
- `SetOption(id, value)`, server only. It normalizes, stores, calls `RpcDo_SetOption` directly, sends
  the broadcast, and fires the invoker.
- `ref ScriptInvoker m_OnOptionChanged`, invoked with `(string id, string value)` on every machine.
- `RplSave` and `RplLoad` for the join-in-progress snapshot.
- `ApplyPersisted(records)` and `GetPersistableOverrides(records)` for the serializer.

`RegisterDefaults()` runs from the manager's own `OnPostInit`, on **every** machine, and registers the
built-in options including the two autosave ones. Definitions are therefore machine-local and only
values travel (D3).

### 3.4 The request seam

`OVT_OptionsRequestComponent : OVT_ControllerRequestComponent` carries one request pair. The public
method branches on `Replication.IsServer()`, because an `RplRcver.Server` RPC marshalled by the
authority reaches nobody. `OVT_FuelRequestComponent` is the pattern to copy verbatim.

The handler validates in this order and rejects at the first failure:

1. `Replication.IsServer()`.
2. `ResolveOwningPlayerId()` above zero. Identity comes from the entity, never from a payload.
3. The manager resolves.
4. The id names a known definition.
5. The scope is not `LOCAL`. A `LOCAL` value must never travel.
6. `FACTION` needs `OVT_ResistanceFactionManager.IsOfficer(playerId)`.
7. `WORLD` needs `PlayerMayEditWorld(playerId)` (D7).

A rejection logs one WARNING line that names the player id, the option id and the scope.

### 3.5 The generated form

`OVT_OptionsContext : OVT_UIContext` builds its rows at `OnShow` and destroys them at `OnClose`. It
walks the three scopes in order, asks the registry for the ids in each scope, keeps the ids the local
player may edit, and skips a scope with an empty list (D9). For each id it creates a row from
`OptionsRow.layout` and then creates the matching control into the row's `ControlSlot`:

| Type | Layout | Populate with | Write on |
|---|---|---|---|
| `TOGGLE` | `WLib_Checkbox.layout` | `SetChecked(v, false, false)` | `m_OnChanged` |
| `SLIDER` | `OptionsSlider.layout` | `SetValue(v)` | `GetOnChangedFinal()` |
| `CHOICE` | `WLib_SpinBox.layout` | `SetCurrentItem(i, false, false, false)` | `m_OnChanged` |

The context also holds an `m_bPopulating` flag and every write handler returns early while it is set.
Section 5, decision D14, explains why the flag and the argument values are both necessary.

### 3.6 Data flow

**LOCAL edit.** The row writes straight into `OVT_OptionsLocalStore` in memory and fires the manager
invoker locally. Nothing reaches the network. `OnClose` flushes the whole record one time through
`OVT_OptionsLocalSettingsAccessor.Save()`.

**FACTION or WORLD edit.**

```
row widget change
  -> OVT_OptionsContext handler (skipped while populating)
  -> OVT_ControllerComponent<OVT_OptionsRequestComponent>.Get().SetOption(id, value)
  -> server branch, or Rpc(RpcAsk_SetOption, id, value)
  -> server: re-resolve player, definition, scope, permission
  -> OVT_OptionsManagerComponent.SetOption(id, value)   normalizes and stores
  -> RpcDo_SetOption(id, stored) locally, then Rpc(RpcDo_SetOption, ...) to every client
  -> each machine stores the value in its own registry mirror
  -> m_OnOptionChanged.Invoke(id, stored)
  -> consumers react (autosave re-arms, the open row re-reads the mirror)
```

**Join in progress.** The game-mode entity carries an `RplComponent`. `RplSave` writes the stream
version, the definition signature, the pair count, and one id and value pair for each non-pending
`FACTION` or `WORLD` override. `RplLoad` reads them and applies each one that names a definition it
knows. `OVT_OverthrowConfigComponent:833-900` is the model.

**Save.** `OVT_OptionsManagerSerializer.Serialize` writes `version` and then one array of
`OVT_PersistedOption` records, built from `GetPersistableOverrides`. The manager logs any pending id
one time, because a value with no definition is meaningless (D6).

**Load and Continue.** `Deserialize` reads the version, reads the record array, and hands it to
`ApplyPersisted`. That call fires `m_OnOptionChanged` for each applied value. The load path therefore
carries the whole re-application, and nothing waits for a player to connect. This is the BUG-104 rule.
`OVT_OverthrowGameMode.c:465` calls `StartAutosaves()` after the load, so the autosave consumer reads
the loaded values on its own start path.

### 3.7 File map

**New script files**

| Path | Purpose |
|---|---|
| `Scripts/Game/Data/OVT_OptionDefinition.c` | the definition record and the two enums |
| `Scripts/Game/Data/OVT_OptionsRegistry.c` | the pure registry |
| `Scripts/Game/Data/OVT_OptionsLocalStore.c` | the pure `LOCAL` store |
| `Scripts/Game/GameMode/Managers/OVT_OptionsManagerComponent.c` | the manager |
| `Scripts/Game/Components/Controller/OVT_OptionsRequestComponent.c` | the request seam |
| `Scripts/Game/Persistence/Serializers/Components/OVT_OptionsManagerSerializer.c` | the serializer |
| `Scripts/Game/Global/OVT_OptionsLocalSettings.c` | the profile module and its entry class |
| `Scripts/Game/Global/OVT_OptionsLocalSettingsAccessor.c` | the profile plumbing |
| `Scripts/Game/UI/Context/OVT_OptionsContext.c` | the generated form |

**New layouts**

| Path | Purpose |
|---|---|
| `UI/Layouts/Menu/OptionsMenu.layout` | title, close button, scrolling `Sections` list |
| `UI/Layouts/Menu/OptionsMenu/OptionsRow.layout` | `Label`, `ControlSlot`, `Description` |
| `UI/Layouts/Menu/OptionsMenu/OptionsSectionTitle.layout` | one `Title` text widget |
| `UI/Layouts/Menu/OptionsMenu/OptionsSlider.layout` | `WLib_Slider` delta with rounding on (D8) |

Each layout needs a matching `.layout.meta` in the format of
`UI/Layouts/HUD/TutorialPopup.layout.meta`.

**New test files**

| Path | Tier |
|---|---|
| `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Options.c` | Logic |
| `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_OptionsLocalStore.c` | Logic |
| `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_OptionsManager.c` | Init |
| `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_OptionsAutosaveRearm.c` | Init |

**Edited files**

| Path | Region | Change |
|---|---|---|
| `Scripts/Game/Global/OVT_Global.c` | near `:375` | add `GetOptions()` beside `GetHighCommand()` |
| `Prefabs/GameMode/OVT_OverthrowGameMode.et` | component list | add the manager block |
| `Prefabs/GameMode/OVT_OverthrowController.et` | component list | add the request block |
| `Prefabs/Characters/.../Character_Player.et` | `m_aContexts`, `:20-176` | add the context entry |
| `Configs/Systems/Persistence/Overthrow.conf` | `:21-72` | add the serializer entry |
| `UI/Layouts/Menu/MainMenu.layout` | `:563-575` | remove `"Is Enabled" 0` |
| `Scripts/Game/UI/Context/OVT_MainMenuContext.c` | `OnShow` `:90-191`, and a new handler beside `Tips()` at `:286-297` | wire the button |
| `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c` | `:34-46`, `:253-264`, `:951-952` | stop, restart, registry-driven start, unsubscribe |
| `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ControllerSeam.c` | `FindFirstUnresolvedComponent` `:103+` | one line for the new component |
| `Language/localization_Overthrow.st` | append | ten new items |

### 3.8 GUID allocation

The series `6B8B0000000000xx` is unused across `Prefabs/`, `PrefabsEditable/`, `Configs/`, `Scripts/`,
`Worlds/`, `Language/`, `UI/` and the vanilla reference tree. The check ran on 2026-09-08 and returned
zero matches. A colliding GUID fails silently, so the implementer marks each row as taken when it
lands.

P8.2 ran the same check again on 2026-09-08. Every GUID below is taken and each has exactly one
definition site (a `.et` component block, a `.layout.meta` `Name`, or a `.layout` widget block). A
GUID that also appears as a reference, such as a prefab attribute default naming a layout path, is not
a second definition and is not listed twice here.

| GUID | Where |
|---|---|
| `6B8B000000000001` | `OVT_OptionsManagerComponent` block on the game-mode prefab |
| `6B8B000000000002` | `OVT_OptionsRequestComponent` block on the controller prefab |
| `6B8B000000000003` | `OVT_OptionsContext` entry in `Character_Player.et` |
| `6B8B000000000004` | `OVT_OptionsManagerSerializer` entry in `Overthrow.conf` |
| `6B8B000000000010` | `OptionsMenu.layout` |
| `6B8B000000000011` | `OptionsRow.layout` |
| `6B8B000000000012` | `OptionsSectionTitle.layout` |
| `6B8B000000000013` | `OptionsSlider.layout` |
| `6B8B000000000020` to `6B8B000000000029` | the ten `.st` items, in the order of section 3.9 |
| `6B8B00000000002A` to `6B8B00000000002F` | spare, unused |
| `6B8B000000000040` to `6B8B00000000004F` | spare, unused. P7 used the `0050..61` and `0080..83` rows below instead (see `context.md`, "Widget GUIDs use `6B8B000000000050..61` and `80..83`") |
| `6B8B000000000050` | `OptionsMenu.layout`, `Window`, root `HorizontalLayoutWidgetClass` |
| `6B8B000000000051` | `OptionsMenu.layout`, `Content`, the overlay behind the whole screen |
| `6B8B000000000052` | `OptionsMenu.layout`, `Background`, the dim image |
| `6B8B000000000053` | `OptionsMenu.layout`, `Blur`, the background blur |
| `6B8B000000000054` | `OptionsMenu.layout`, `SpaceLayout`, the three-column centring row |
| `6B8B000000000055` | `OptionsMenu.layout`, `SpaceLeft`, the left filler column |
| `6B8B000000000056` | `OptionsMenu.layout`, `Alignment`, the fixed-size centre column |
| `6B8B000000000057` | `OptionsMenu.layout`, `ContentLayout`, the vertical stack of header, list and footer |
| `6B8B000000000058` | `OptionsMenu.layout`, `HeaderLayout`, the row that holds the title |
| `6B8B000000000059` | `OptionsMenu.layout`, `Title`, the heading text |
| `6B8B00000000005A` | `OptionsMenu.layout`, `UpperStripe`, the rule under the title |
| `6B8B00000000005B` | `OptionsMenu.layout`, `SectionsScroll`, the scroll view for the section list |
| `6B8B00000000005C` | `OptionsMenu.layout`, `Sections`, the vertical list `OVT_OptionsContext` fills |
| `6B8B00000000005D` | `OptionsMenu.layout`, `LowerStripe`, the rule above the footer |
| `6B8B00000000005E` | `OptionsMenu.layout`, `Footer`, the overlay that holds the close button |
| `6B8B00000000005F` | `OptionsMenu.layout`, `Buttons`, the footer's button row |
| `6B8B000000000060` | `OptionsMenu.layout`, `CloseButton`, the `MenuBack` navigation button |
| `6B8B000000000061` | `OptionsMenu.layout`, `SpaceRight`, the right filler column |
| `6B8B000000000080` | `OptionsRow.layout`, `RowLine`, the label-and-control row |
| `6B8B000000000081` | `OptionsRow.layout`, `Label`, the option name text |
| `6B8B000000000082` | `OptionsRow.layout`, `ControlSlot`, where the row builder attaches the control widget |
| `6B8B000000000083` | `OptionsRow.layout`, `Description`, the option description text |

### 3.9 Localization

Every new string goes into `Language/localization_Overthrow.st` and nowhere else. The runtime `.conf`
exports are Workbench build output. The layouts may carry literal text until the user exports.

| Id | English |
|---|---|
| `OVT-Options_Title` | Options |
| `OVT-Options_Close` | Close |
| `OVT-Options_Section_Local` | This Player |
| `OVT-Options_Section_Faction` | Resistance |
| `OVT-Options_Section_World` | Server |
| `OVT-Options_Autosave_Enabled` | Automatic saving |
| `OVT-Options_Autosave_EnabledDesc` | The campaign saves itself while it runs. |
| `OVT-Options_Autosave_Interval` | Time between saves |
| `OVT-Options_Autosave_IntervalDesc` | The campaign waits this long between automatic saves. |
| `OVT-Options_Minutes` | %1 min |

`OVT-MainMenu_Options` already exists at `Language/localization_Overthrow.st:11164-11172`, GUID
`{598AB6769A9A471C}`, in seven languages. The button needs no new string.

The two description strings state a fact and give no instruction. Player-facing help text informs and
never instructs, and that rule beats the imperative form that Simplified Technical English prefers.

---

## 4. Implementation Phases

Every phase gates on `tools/compile-check.sh` at exit code 0, plus the named suite run **one time** by
the orchestrator after the phase. Planning never runs `tools/run-tests.sh`. Suites are not
deterministic under load, so the orchestrator runs a suite alone.

### P1: The pure registry and its Logic cases

**Agent:** `component-developer` · **Estimate:** 6 h

| Id | Task |
|---|---|
| P1.1 | Write `OVT_EOptionScope` and `OVT_EOptionType` and the `OVT_OptionDefinition` record. |
| P1.2 | Write `OVT_OptionsRegistry`: register, idempotency, definition lookup, ids for a scope. |
| P1.3 | Write the value codec and `Normalize`: bool, clamped and snapped slider, choice key. |
| P1.4 | Write `SetValue`, `GetValue`, the four typed getters, and default precedence. |
| P1.5 | Write the pending-override path and `Register` re-normalization. |
| P1.6 | Write `GetOverrides` and `GetPendingIds`. |
| P1.7 | Write the three static scope predicates. |
| P1.8 | Write `OVT_TEST_Logic_Options.c`. Prove each case able to fail and record the method in the case header. |

**Acceptance:** the registry makes no engine call, touches no widget and names no manager. Every case
builds its subject with `new` and hand-written values. A `SLIDER` value outside its range never leaves
the registry. A `CHOICE` value that names no key never leaves the registry. A value stored before its
registration arrives is applied when the registration arrives.

**Gate:** `tools/compile-check.sh`, then `OVT_TEST_LogicSuite`.

### P2: The manager

**Agent:** `component-developer` · **Estimate:** 5 h

| Id | Task |
|---|---|
| P2.1 | Write `OVT_OptionsManagerComponent` with `s_Instance`, `GetInstance()` and `OnPostInit`. |
| P2.2 | Add `OVT_Global.GetOptions()` beside `GetHighCommand()` at `OVT_Global.c:375`. |
| P2.3 | Add the manager block to `Prefabs/GameMode/OVT_OverthrowGameMode.et` with GUID `6B8B000000000001`. |
| P2.4 | Write `RegisterDefaults()` and register the two autosave options with the shipped constants. |
| P2.5 | Write `RegisterOption`, `OverrideDefault`, the typed getters and `m_OnOptionChanged`. |
| P2.6 | Write `OVT_TEST_Init_OptionsManager.c`: the manager resolves off the game mode and the two built-in definitions exist. |

**Acceptance:** `OVT_Global.GetOptions()` answers on a client, on a listen host and on a dedicated
server. The manager never caches a pointer across a world reload. `RegisterDefaults` runs on every
machine.

**Gate:** `tools/compile-check.sh`, then `OVT_TEST_InitSuite`.

### P3: The request seam, the permissions and the wire

**Agent:** `network-specialist-advanced` · **Estimate:** 6 h

| Id | Task |
|---|---|
| P3.1 | Write `OVT_OptionsRequestComponent` with the `Replication.IsServer()` branch. |
| P3.2 | Write `RpcAsk_SetOption` with the seven-step validation of section 3.4. |
| P3.3 | Write `PlayerMayEditWorld` and verify the single-player role answer in Workbench (D7). |
| P3.4 | Write `SetOption` and `RpcDo_SetOption` on the manager, with the direct call before the broadcast. |
| P3.5 | Write `RplSave` and `RplLoad` with `OPTIONS_STREAM_VERSION` and the definition signature. |
| P3.6 | Add the request block to `Prefabs/GameMode/OVT_OverthrowController.et` with GUID `6B8B000000000002`. |
| P3.7 | Add one line for `OVT_OptionsRequestComponent` to `OVT_TEST_Init_ControllerSeam.c`. |
| P3.8 | Hand-audit the arity of every `Rpc()` call site and record the audit in a comment. |

**Acceptance:** a forged write for a scope the sender cannot edit is rejected and logged. The client
supplies an id and a value and nothing else. A listen host and a single player both reach the handler.
Every `Rpc()` call site carries the audited argument count.

**Gate:** `tools/compile-check.sh`, then `OVT_TEST_InitSuite`. The join-in-progress stream stays a
manual play-test item, because `ScriptBitWriter` cannot be constructed from script.

### P4: Persistence

**Agent:** `component-developer` · **Estimate:** 4 h

| Id | Task |
|---|---|
| P4.1 | Write `OVT_PersistedOption` and `OVT_OptionsManagerSerializer` on the `OVT_HighCommandManagerSerializer` model. |
| P4.2 | Write `ApplyPersisted` and `GetPersistableOverrides` on the manager. |
| P4.3 | Add the serializer entry to `Configs/Systems/Persistence/Overthrow.conf` with GUID `6B8B000000000004`. |
| P4.4 | Log the dropped pending ids one time at save (D6). |
| P4.5 | Add one case to `OVT_TEST_PersistenceRoundTripSuite`. |

**Acceptance:** the local variable that carries the record array is spelled `records` in both
`Serialize` and `Deserialize`. A save with no version returns true and applies nothing. A `WORLD`
value moved away from its default comes back after a round trip. An option left at its default still
reads the default afterwards.

**Gate:** `tools/compile-check.sh`, then `OVT_TEST_PersistenceRoundTripSuite`.

### P5: The LOCAL profile store

**Agent:** `component-developer` · **Estimate:** 4 h

| Id | Task |
|---|---|
| P5.1 | Write `OVT_OptionsLocalStore`: the pure key and value set, the version rule and the cap. |
| P5.2 | Write `OVT_LocalOptionEntry` and `OVT_OptionsLocalSettings` as a nested record in an object array. |
| P5.3 | Write `OVT_OptionsLocalSettingsAccessor` with `Load`, `Save`, `Reset` and `GetModuleContainer`. |
| P5.4 | Wire the store into the manager for `LOCAL` reads and writes. |
| P5.5 | Write `OVT_TEST_Logic_OptionsLocalStore.c`. |

**Acceptance:** the accessor early-returns on `System.IsConsoleApp()`. The loader's null array is
allocated before any reader touches it. A version mismatch clears the record and rewrites it at the
current version. `Save` writes the whole record, never a delta. The only flush point is the menu close.

**Gate:** `tools/compile-check.sh`, then `OVT_TEST_LogicSuite`.

### P6: The autosave consumer

**Agent:** `component-developer` · **Estimate:** 4 h

| Id | Task |
|---|---|
| P6.1 | Add `StopAutosaves()` and `RestartAutosaves()` to `OVT_PersistenceManagerComponent`. |
| P6.2 | Make `StartAutosaves()` read the registry, and fall back to the two attributes when the manager is absent. |
| P6.3 | Call `OverrideDefault` twice from the persistence manager's `OnPostInit`, server only. |
| P6.4 | Subscribe to `m_OnOptionChanged` inside `StartAutosaves()`, one time, behind a flag. |
| P6.5 | Unsubscribe in `OnDelete` beside the existing timer removal at `:951-952`. |
| P6.6 | Write `OVT_TEST_Init_OptionsAutosaveRearm.c`. |

**Acceptance:** an interval change removes the old timer and adds a new one in the same call. A
disable removes the timer. An enable adds it back while the campaign runs. The Init case asserts the
scheduled flag and reads no wall clock. A case that waits on real time belongs in
`OVT_TEST_SoakSuite`, and this one does not wait.

**Gate:** `tools/compile-check.sh`, then `OVT_TEST_InitSuite`.

### P7: The generated form

**Agent:** `ui-developer-advanced` · **Estimate:** 10 h

| Id | Task |
|---|---|
| P7.1 | Author the four layouts and their `.meta` files with the reserved GUIDs. |
| P7.2 | Write `OVT_OptionsContext` with `OnShow`, `OnClose` and the section walk. |
| P7.3 | Write the three row builders and the binding list. |
| P7.4 | Write the three change handlers behind the `m_bPopulating` guard. |
| P7.5 | Subscribe to `m_OnOptionChanged` and re-populate the affected row. |
| P7.6 | Set the initial gamepad focus after the rows are built. |
| P7.7 | Wire the close button through `m_OnClicked`, not the action listener. |
| P7.8 | Remove `"Is Enabled" 0` from `MainMenu.layout:566` and add the handler to `OVT_MainMenuContext.OnShow`. |
| P7.9 | Add the `OVT_OptionsContext` entry to `Character_Player.et` with GUID `6B8B000000000003`. |
| P7.10 | Add the ten items to `Language/localization_Overthrow.st`. |

**Acceptance:** the menu opens from the main menu and closes with `MenuBack`. A gamepad reaches every
row. An empty section does not render. The autosave interval row reads in whole minutes. Populating a
row sends no request.

**Gate:** `tools/compile-check.sh`, then `OVT_TEST_InitSuite`. Gamepad feel and the visual result stay
manual.

### P8: Review, docs and the play-test checklist

**Agent:** `component-developer` · **Estimate:** 3 h

| Id | Task |
|---|---|
| P8.1 | Review every phase against the Definition of Done in section 7. |
| P8.2 | Confirm every reserved GUID is marked as taken and that no GUID appears two times. |
| P8.3 | Run `git diff --stat Language/` and confirm that only the `.st` master changed. |
| P8.4 | Score the comments against `docs/features/dev-ops/code-cleanup.md` and cut any essay. |
| P8.5 | Write the play-test checklist into the feature docs. |

**Gate:** `tools/compile-check.sh`, then `OVT_TestGroup_All`.

### P9: Help and documentation sync

**Agent:** `help-docs-sync` · **Estimate:** 3 h

The feature adds a menu a player opens and two settings a player changes, so the help content must
follow. This phase updates the tutorial popups in `Configs/Tutorials/`, the Field Manual in
`Configs/FieldManual/`, and the public wiki, so all three describe the shipped behaviour.

**Total: 9 phases, 45 hours.**

---

## 5. Key Technical Decisions

**D1: One string encoding for every value, in memory, on the wire and in the save.**
A bool becomes `"0"` or `"1"`. A number becomes its float string. A choice becomes its key. Typed
getters parse. The alternative was a packed `RplProp` string that carries the whole table. It lost for
two reasons. EnforceScript has no map `RplProp`, so the table would need hand-packing on a hazard
(`ScriptBitWriter` cannot be constructed from script, so no unit case could ever read the packing
back). A per-change RPC is also smaller than a whole table re-send, and this table changes rarely.
One encoding means one storage shape and one place where a value can be malformed.

**D2: A `CHOICE` value stores the key, never the index.**
A reordered choice list must not change a saved value. An index makes the meaning of a stored value
depend on the order of a list that a later version is free to re-order. The key is stable by
construction. The cost is one lookup at render time, which the row does one time.

**D3: Definitions are machine-local. Only values travel.**
`OVT_OptionsManagerComponent.RegisterDefaults()` runs on every machine from the manager's own
`OnPostInit`, so a client draws a row from a definition it registered itself. The alternative was to
put the definitions in the join-in-progress stream and in the change broadcast. It lost because the
first consumer proves the problem: `OVT_Global.GetPersistence()` is null on a client, so the
persistence manager cannot register anything there, and a client with no definition cannot draw a
row. Definitions in the stream would also make the stream grow with every option. The risk that
definitions drift between a client build and a server build is covered by the definition signature in
the stream (see R7).

**D4: Apply on change, with no client echo.**
Each widget change sends one request. The client updates its mirror only when the broadcast arrives.
A batch on close lost because it needs a dirty set, a cancel path and an all-or-nothing failure story
for a screen that changes two values. The known cost: a forged write that the server refuses leaves
the widget showing the value the player dragged it to, because a refusal does not broadcast. The
sections are already permission-gated, so the only way to reach that state is the script console. The
row re-reads the mirror on the next broadcast and on the next open.

**D5: A persisted value for an unregistered id is held as a pending override.**
The serializer's `Deserialize` may run before or after a consumer registers, because the world is
still being built. The registry therefore accepts a value for an id it does not know, and applies the
constraints when the registration arrives. The alternative was to force the options manager to
initialize before every consumer. It lost because the requirement asks for order independence, and a
hand-maintained init order is the epic's known weak point. Logic cases cover both orders.

**D6: An unknown persisted id is dropped with one WARNING line.**
A value with no definition has no type, no constraints and no meaning, so re-writing it keeps a string
nobody can read. The alternative, a side map that survives every save, lost because it grows without
bound and hides the removal of an option. The WARNING names the dropped ids one time at save.

**D7: `WORLD` permission is the admin role, with a host fallback.**
The check is `SCR_Global.IsAdminRole(GetGame().GetPlayerManager().GetPlayerRoles(playerId))`. The
method is `proto external EPlayerRole GetPlayerRoles(int iPlayerId)`, confirmed at
`scripts/Game/generated/Network/PlayerManager.c:64`. Where the role flags are empty in single player,
the fallback is `Replication.IsServer() && playerId == SCR_PlayerController.GetLocalPlayerId()`. P3.3
verifies the single-player answer in Workbench and records it. The fallback stays either way. It costs
one comparison, and `GetLocalPlayerId()` is 0 on a dedicated server, so it can never fire there. The
game mode already promotes an admin to officer at `OVT_OverthrowGameMode.c:995-1010`, so an admin
passes the officer check too, and the requirement's "single player sees everything" falls out of the
checks with no special case.

**D8: The slider stores seconds and shows minutes, through a rounded delta layout.**
`autosave.interval` is a `SLIDER` with min 120, max 3600 and step 60, so every reachable value is a
whole minute between 2 and 60. The row calls `SetShownValueMultiplier(1.0 / 60.0)` and
`SetFormatText("#OVT-Options_Minutes")`.

The trap that forced the delta layout: `SCR_SliderComponent` applies `m_fShownValueMultiplier` **only**
inside the `if (m_bRoundValue)` branch of `UpdateValue` (`SCR_SliderComponent.c:139-143`), and
`m_bRoundValue` has no script setter. `WLib_Slider.layout` leaves it at the attribute default of 0, so
`SetShownValueMultiplier` is dead on the stock layout. Vanilla sets `m_bRoundValue 1` in its own
`GameplaySettings.layout` for exactly this reason. `OptionsSlider.layout` is therefore a one-file
delta of `WLib_Slider.layout` with `m_bRoundValue 1` and `m_iDecimalPrecision 0`. The alternative was
to run the slider in display units and convert in the row. It lost because it moves the option's
units out of the registry and into the UI, where a second consumer would have to repeat the
conversion.

**D9: An empty section does not render.**
The Local section holds zero options in this version, so it stays hidden. The alternative, an empty
section with a placeholder line, lost because it advertises a screen that does nothing. A future
`LOCAL` option makes the section appear with no UI edit, which is the point of the registry.

**D10: The `LOCAL` store copies `OVT_MapLayerSettings` exactly.**
The requirements cite `OVT_TutorialSettings` and `OVT_TutorialSettingsAccessor`. Those files were
deleted on 2026-08-18 when the tutorial seen-store moved into the campaign save. The live pattern is
`Scripts/Game/Global/OVT_MapLayerSettings.c` and `OVT_MapLayerSettingsAccessor.c`. Every rule is
inherited rather than re-derived: a nested `[BaseContainerProps()]` record inside an object array and
never a top-level `array<string>`, whole-record writes, a `System.IsConsoleApp()` early-out, a null
guard on the array the loader may not have written, and a version field.

**D11: No `OVT_Global` getter for the request component.**
`OVT_ControllerComponent<OVT_OptionsRequestComponent>.Get()` is the only accessor. The header of
`OVT_ControllerComponent.c` names `docs/features/core/options/requirements.md:41` as a committed
consumer of that exact spelling. A manager getter is different and expected, so `GetOptions()` does
go into `OVT_Global`.

**D12: `RegisterDefaults` lives on the options manager, not on each consumer.**
It runs on every machine, which is what D3 needs. The consumer keeps its prefab attributes and pushes
them through `OverrideDefault` on the server, so a server operator who edited the prefab keeps the
edited value as the default. `OverrideDefault` touches the default only, so a saved override wins in
either order and the call is safe before or after the load.

**D13: One registry-wide change invoker, not one for each option.**
A consumer filters on the id in two lines. A map of invokers costs a lifetime problem for each entry
and buys nothing at this scale. The requirement allows either shape.

**D14: The form guards against re-entrant writes two ways.**
Populating a widget can fire its own change event. The measured answers differ for each type.
`SCR_SpinBoxComponent.SetCurrentItem` takes an `invokeOnChanged` parameter
(`SCR_SpinBoxComponent.c:139-142`), so the form passes false. `SCR_SliderComponent.SetValue` always
invokes `m_OnChanged` (`SCR_SliderComponent.c:198-207`), but it never invokes `m_OnChangedFinal`, so
the form writes on `GetOnChangedFinal()` and populating is silent. That hook also stops one request
for each pixel of a drag. `SCR_CheckboxComponent.SetChecked(v, animate, playSound)` reaches
`SCR_ButtonBaseComponent.SetToggled`, whose third parameter is `invokeChange`, through two swapped
argument lists in vanilla, so `playSound = false` is what suppresses the event. That chain is a
vanilla accident and could change in a Reforger update. The form therefore also holds an
`m_bPopulating` flag and every handler returns early while it is set. The flag alone is enough. The
argument values are the belt beside it.

---

## 6. Quality Bar

**Data integrity.** No value ever leaves the registry outside its constraints, on any machine. Clamp,
snap and choice-key validation run in `Normalize`, which every write path calls. A client mirror
receives an already-normalized value, so a client and the server can never disagree about what is
stored.

**Server authority.** A forged write for a scope the sender cannot edit is rejected and logged with
the player id, the option id and the scope. The client sends an id and a value and nothing else. The
caller's identity comes from the controller entity the request arrived on.

**User interface.** Every row is reachable with a gamepad. The initial focus lands on the first row
after the form is built. A row shows its label and its description. The interval row reads in whole
minutes. An empty section does not render.

**No autosave regression.** The default state of a new campaign matches the shipped prefab attributes:
enabled, 600 seconds. A campaign that never opens the Options menu behaves exactly as it did before.

---

## 7. Definition of Done

### Functional

- [ ] (manual) The Options button is enabled and opens the generated menu. Wiring evidence only:
      `UI/Layouts/Menu/MainMenu.layout:563-572` no longer carries `"Is Enabled" 0`, and
      `Scripts/Game/UI/Context/OVT_MainMenuContext.c:187-191,307-316` calls
      `OVT_OptionsContext.OnShow`. The visual open is on the Workbench SP checklist.
- [ ] (manual) A single player sees all three sections and edits every row. `OVT_OptionsContext.c:113-115`
      walks `LOCAL`, `FACTION`, `WORLD` in order and `OVT_OptionsContext.c:170-217` builds a row for
      each id. Seeing and editing the rendered form is a Workbench SP item.
- [ ] (manual) `TOGGLE` and `SLIDER` render and write. Builder evidence:
      `OVT_OptionsContext.c:242-246` branches on `OVT_EOptionType`, `:504-514` write on
      `m_OnChanged`/`GetOnChangedFinal()`. `CHOICE` has the same builder branch
      (`OVT_OptionsContext.c:246`, `:516`) but the plan ships no `CHOICE` consumer
      (`RegisterDefaults` in `OVT_OptionsManagerComponent.c:70-85` registers only the two autosave
      options), so no `CHOICE` row exists anywhere to render or write. That half of this item cannot
      be play-tested until a later feature registers a `CHOICE` option. Logic coverage
      (`OVT_TEST_Logic_Options_ChoiceStoresKey`) proves the registry side only.
- [x] Autosave turns off and back on from the menu, with no restart.
      `OVT_TEST_Init_OptionsAutosaveRearm.c` drives `SetOption` on `OPTION_AUTOSAVE_ENABLED` through
      `OVT_PersistenceManagerComponent.OnOptionChanged` (`:351-357`) into `StopAutosaves`/
      `RestartAutosaves` (`:299-318`) and reads `IsAutosaveScheduled()` after each change, with no
      restart of the campaign. Init suite 213/213.
- [x] The autosave interval changes from the menu, with no restart. Same case, same chain, the
      interval branch of `RestartAutosaves` (`OVT_PersistenceManagerComponent.c:310-318`).
- [ ] (manual) Both autosave values survive a save and a Continue. The interval is automated:
      `OVT_TEST_PersistenceRoundTrip_Options_SurvivesSaveAndReload`
      (`Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c:14924-15069`)
      moves it off its default, saves, dirties it, reloads, and reads the saved value back. The
      enabled toggle is only proven to keep its own default across the same reload, never proven to
      round-trip a non-default value, and no case exercises a real Continue. Round-trip suite 46/46.
      The Workbench SP save/quit/Continue step covers the remaining ground.
- [ ] (manual) `FACTION` and `WORLD` values reach a client that joins in progress. `RplSave`/`RplLoad`
      are written (`OVT_OptionsManagerComponent.c:401-450`) but `ScriptBitWriter` cannot be
      constructed from script, so no case can read the stream back. Section 8 names this manual.
- [x] A `LOCAL` value survives a client restart. The store is exercised through a Logic case, because
      no `LOCAL` option ships. `OVT_TEST_Logic_OptionsLocalStore_RoundTrip` proves the array round
      trip is lossless. Logic suite 319/319.

### Quality

- [x] `tools/compile-check.sh` exits 0.
- [x] Every new test case is proven able to fail, and the case header records the method. Confirmed
      in all four new test files and in the appended round-trip case
      (`OVT_TEST_PersistenceRoundTripSuite.c:14924-14926`).
- [x] No comment carries a rationale narrative. Rationale lives in this plan. Trimmed under P8.4, see
      `context.md`.
- [x] Every reserved GUID is used one time, and section 3.8 is marked up. See the P8.2 audit below.
- [x] `git diff --stat Language/` shows a change to `localization_Overthrow.st` and to nothing else.
      `git diff --stat Language/` reports one file, 170 insertions, 0 deletions.

### Integration

- [x] `OVT_Global.GetOptions()` exists (`OVT_Global.c:381`). No `OVT_Global` getter exists for the
      request component (grep for `OptionsRequestComponent` in `OVT_Global.c` returns nothing).
- [x] `OVT_TEST_Init_ControllerSeam.c` carries one line for the new request component
      (`OVT_TEST_Init_ControllerSeam.c:118`).
- [x] The serializer is listed in `Configs/Systems/Persistence/Overthrow.conf`
      (`Overthrow.conf:58`).
- [x] The persistence manager unsubscribes in `OnDelete`
      (`OVT_PersistenceManagerComponent.c:1043-1055`).

### Verification method

An evaluator with no context can follow these steps.

**Single player, in Workbench**

1. Start a new campaign and open the main menu.
2. Select **Options**. Confirm that a menu opens with three section titles.
3. Read the autosave interval row. Confirm that it shows a whole number and the word `min`.
4. Turn **Automatic saving** off. Read the console log. Confirm that no further
   `Autosave scheduled every ...` line appears.
5. Turn it back on and move the interval to 5 minutes. Confirm one new
   `Autosave scheduled every 300 seconds` line.
6. Close the menu, save the campaign, quit to the main menu, then select Continue.
7. Open Options again. Confirm that the interval still reads 5 min and the toggle is still on.

NOTE: the log line `[Overthrow] Save point FAILED` on every autosave is normal in Workbench. It is not
a defect, and autosaves do write in a deployed single-player or multiplayer session.

**Dedicated server**

1. Start `tools/launch-server.sh` and join with a second client.
2. As a client with no officer status and no admin role, open Options. Confirm that no section
   renders, because the Local section is empty and the other two are hidden.
3. Promote that player to officer. Re-open Options. Confirm that the Resistance section appears.
4. Give that player the admin role. Re-open Options. Confirm that all three permitted sections appear.
5. Change the autosave interval as the admin. Confirm that the other client's mirror receives the
   value, and that a client that joins after the change reads the new value.
6. Forge a write. From the Workbench script console on a non-admin client, resolve
   `OVT_ControllerComponent<OVT_OptionsRequestComponent>.Get()` and call
   `SetOption("autosave.interval", "3600")`. Confirm that the server log carries one WARNING naming
   the player id, the option id and the `WORLD` scope, and that no value changed.

**Automated**

1. `tools/compile-check.sh` exits 0.
2. `tools/run-tests.sh OVT_TEST_LogicSuite` passes.
3. `tools/run-tests.sh OVT_TEST_InitSuite` passes.
4. `tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite` passes.
5. Each new case fails when the change it guards is reverted. Each case header records the reversal.

---

## 8. Testing Strategy

### Logic tier

`OVT_TEST_Logic_Options.c` and `OVT_TEST_Logic_OptionsLocalStore.c`. Every subject is built with `new`
and hand-written values. No engine call, no manager, no widget.

| Case | Claim |
|---|---|
| `OVT_TEST_Logic_Options_RegisterIsIdempotent` | A second registration for an id is refused and the first definition wins. |
| `OVT_TEST_Logic_Options_SliderClampsAndSnaps` | A value below min, above max, or between steps never leaves the registry. |
| `OVT_TEST_Logic_Options_ChoiceStoresKey` | A choice stores its key. A key that is not declared falls back to the default. A reordered list keeps the stored value. |
| `OVT_TEST_Logic_Options_DefaultPrecedence` | An unset option reads its default. A stored value beats the default. |
| `OVT_TEST_Logic_Options_PendingOverride` | A value stored before the registration is applied and normalized when the registration arrives. Both orders. |
| `OVT_TEST_Logic_Options_OverridesExcludePending` | `GetOverrides` omits a pending value and `GetPendingIds` names it. |
| `OVT_TEST_Logic_Options_ScopePermissionTable` | The three scope predicates answer the table in the requirements. |
| `OVT_TEST_Logic_Options_ValueCodec` | Round trip of bool, float and choice through the string encoding. |
| `OVT_TEST_Logic_OptionsLocalStore_SetAndRead` | Set, read, overwrite and remove are all idempotent. An empty id is refused. |
| `OVT_TEST_Logic_OptionsLocalStore_VersionInvalidates` | A version mismatch clears the record rather than half-loading it. |
| `OVT_TEST_Logic_OptionsLocalStore_RoundTrip` | The array round trip is lossless and leaves nothing stale in a reused buffer. |

No case asserts that a default equals a configured number. That rule comes from the user on
2026-09-03 and it stands.

### Init tier

| Case | Claim |
|---|---|
| `OVT_TEST_Init_OptionsManagerResolves` | `OVT_Global.GetOptions()` answers off the game mode, and both built-in definitions exist. |
| `OVT_TEST_Init_Controller_ComponentsResolve` (edited) | One line for `OVT_OptionsRequestComponent`. |
| `OVT_TEST_Init_OptionsAutosaveRearm` | An interval change flips the scheduled flag off and on. A disable clears it. No wall-clock wait. |

### Persistence round-trip tier

One case. It moves a `WORLD` value away from its default, triggers a save, dirties the live value,
calls `ReapplyLatestSaveData()`, polls `IsReapplyInProgress()`, and asserts two things: the saved value
came back, and an option left at its default still reads the default. Every assertion reads through
the manager's public API. That is the suite's standing rule.

The case also proves that the serializer is bound in `Overthrow.conf`. A missing entry means no value
comes back, and the case fails. There is no separate binding tripwire in the Init suite today, so this
case carries the claim.

### Manual only

- The join-in-progress stream. `ScriptBitWriter` cannot be constructed from script, so no unit case
  can read a stream back.
- Gamepad feel, focus travel and the visual result of the generated rows.
- Dedicated-server permission behaviour for a non-officer, an officer and an admin.
- The forged-write refusal, through the script console.
- The real Continue path. The round-trip suite proves storage, not the restart.

---

## 9. Dependencies

| Dependency | State | What this feature needs from it |
|---|---|---|
| `core/game-mode` | documented legacy | The manager lifecycle and the controller entity. |
| `core/persistence` | shipped | The conf-first serializer convention and `ReapplyLatestSaveData()`. |
| `core/controller-migration` | shipped | `OVT_ControllerComponent<T>.Get()` and `OVT_ControllerRequestComponent`. |
| `resistance` officer model | shipped | `OVT_ResistanceFactionManager.IsOfficer(playerId)` at `:419`. |
| Platform admin roles | vanilla | `PlayerManager.GetPlayerRoles` and `SCR_Global.IsAdminRole`. |

Nothing in the `core` epic depends on this feature.

---

## 10. Risks and Mitigation

**R1: Registration and load ordering.** The serializer can run before a consumer registers. A value
would then land on an unknown id and disappear. Mitigation: the pending-override path of D5, plus two
Logic cases that drive both orders.

**R2: The save-context variable-name trap.** A save context keys a property by the local variable's
name. A rename in `Deserialize` silently reads zeros and still reports success, because
`LoadContext.c` says the binary container always returns true. Mitigation: the variable is spelled
`records` in both methods, a comment on the `Serialize` side says so, and only the round-trip case can
catch a regression. That case is mandatory, not optional.

**R3: Listen-host owner responses.** An `RplRcver.Server` RPC marshalled by the authority reaches
nobody. Mitigation: the `Replication.IsServer()` branch in `SetOption`, copied verbatim from
`OVT_FuelRequestComponent`. The broadcast follows the `OVT_ResistanceFactionManager.AddOfficer` shape:
call the handler directly, then send.

**R4: `Rpc()` arity.** `Rpc()` is an untyped variadic prototype. A wrong argument count compiles
clean and dies at the wire with no message. Mitigation: task P3.8 audits every call site by hand and
records the audit in a comment.

**R5: Re-entrant writes while the form populates itself.** Covered by D14. The `m_bPopulating` flag is
the load-bearing guard, because the checkbox suppression depends on a vanilla argument-order accident.

**R6: The `SaveUserSettings` throttle.** The engine drops the second of two flushes microseconds
apart. Mitigation: whole-record writes, and one flush at the menu close rather than one for each
toggle. Both rules come straight from `OVT_MapLayerSettingsAccessor`.

**R7: Definition drift between a client build and a server build.** A client with an older build has
no definition for a newer option. Mitigation: the join-in-progress stream carries a stream version and
a definition signature. A version mismatch logs an ERROR and rejects the stream, as
`OVT_OverthrowConfigComponent` does. A signature mismatch logs a WARNING, applies every pair the
client knows, and holds the rest as pending. A partial mirror is better than an empty one.

**R8: The hidden empty section.** A future feature may expect the Local section to exist and find
nothing on screen. Mitigation: D9 is recorded here, and the section appears by itself when the first
`LOCAL` option registers. Nothing needs an edit.

**R9: The Workbench autosave log.** `[Overthrow] Save point FAILED` appears on every autosave in
Workbench. It is normal, and the user prefers it, because it forces manual saves. An evaluator who
reads it as a defect will chase nothing. The verification method in section 7 says so.

**R10: Slider text formatting.** `SetTextFormat` receives a float. A whole number could render with
decimals. Vanilla drives its own percentage sliders the same way, so the risk is low. Mitigation:
step 5 of the single-player verification reads the row and confirms a whole number.
