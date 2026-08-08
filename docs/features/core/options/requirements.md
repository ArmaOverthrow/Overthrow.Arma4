# Options — Requirements

**Epic:** core
**Created:** 2026-08-09

## Overview

The pause/main menu has had a dead **Options** entry for as long as the menu has existed — `UI/layouts/Menu/MainMenu.layout:604` defines the button (label `#OVT-MainMenu_Options`) with `"Is Enabled" 0`, and `OVT_MainMenuContext` never wires it (every live button is registered via `SCR_ButtonTextComponent.GetButtonText` at `Scripts/Game/UI/Context/OVT_MainMenuContext.c:102-191`; "Options" is absent). This feature makes it real.

Rather than a hand-built settings screen, Options is a **declarative registry** in the style of Overthrow's other systems: any subsystem registers an option (id, scope, field type, default, constraints, labels) at init and reads the live value through the registry; the Options UI is *generated* from the registry, grouped by scope. Adding an option to a future system is a one-call registration, not a UI change.

The first consumer is the autosave system: a toggle to turn autosaves off and a slider for the time between autosaves, replacing the two prefab attributes on `OVT_PersistenceManagerComponent` (`m_bEnableAutosave` defvalue 1, `m_fAutosaveInterval` defvalue 600 — `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c:24-28`) as the source of truth at runtime.

## Requirements

### Registry & declaration

- **Declarative registration.** A new manager singleton (`OVT_OptionsManagerComponent` on the game-mode prefab, exposed the usual way) owns the registry. A subsystem registers each option once at init with: string id (namespaced, e.g. `autosave.interval`), scope (enum below), field type, default value, constraints (min/max/step for numbers; choice list for enums), and localization keys for label + description. Registration is idempotent and order-independent apart from the persistence note below.
- **Field type enum** — minimum viable set, extensible later:
  - `TOGGLE` (bool)
  - `SLIDER` (float/int with min/max/step)
  - `CHOICE` (one of a declared list; stored as int index or string key — decided in planning)
- **Typed access + change events.** Consumers read via typed getters (`GetBool/GetFloat/GetInt/GetChoice`) and may subscribe to a per-option (or registry-wide) `ScriptInvoker` change event so they can react at runtime instead of polling.

### Scope enum (`OVT_EOptionScope`)

| Scope | Who edits | Where it lives | Replication |
|---|---|---|---|
| `LOCAL` | every player, own value | client-side user settings | none — never leaves the machine |
| `FACTION` | resistance officers | server, persisted in the save | replicated to all clients + JIP |
| `WORLD` | admins | server, persisted in the save | replicated to all clients + JIP |

- **`LOCAL`** values are stored in a `ModuleGameSettings` user-settings module, following the proven `OVT_TutorialSettings` pattern (`Scripts/Game/Global/OVT_TutorialSettings.c:36`) and its accessor (`OVT_TutorialSettingsAccessor.c` — note the measured engine throttling of `SaveUserSettings()` flushes documented at `:100`; the accessor pattern already handles it). Local options apply immediately with no server round-trip.
- **`FACTION`** edit permission is `OVT_ResistanceFactionManager.IsOfficer(playerId)` (`Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c:443`) — the same check the existing officer-gated RPCs use.
- **`WORLD`** edit permission is the platform admin role, checked server-side the way the game mode already does (`SCR_Global.IsAdminRole(roleFlags)` in `OnPlayerRoleChange`, `Scripts/Game/GameMode/OVT_OverthrowGameMode.c:844-849`). Note the game mode already promotes admins to officer there, so an admin implicitly passes the `FACTION` check too.
- **Single player / listen host sees everything.** The SP/listen host is the admin and (via the promotion above) an officer, so all three sections are visible and editable with no special-casing. This must fall out of the permission checks, not be a hardcoded SP branch.

### Networking & authority

- **Server authority for `FACTION`/`WORLD`.** Edits go client→server through a new **`OVT_OptionsComponent` on `OVT_OverthrowController`** (per the project rule: nothing new on the legacy `OVT_PlayerCommsComponent`), following the proven controller-component pattern (`OVT_ContainerTransferComponent`). The server **re-validates the permission server-side** for every write (officer/admin as per scope) — the client-side gating is UX only.
- **Do not add a getter to `OVT_Global`.** The sibling `core/controller-migration` requirements define the compile-verified generic `OVT_ControllerComponent<Class T>` accessor precisely so new domains add nothing to the locator; this feature either adopts that snippet (it stands alone) or uses a plain `FindComponent` on `OVT_Global.GetController()` — decided in planning, but the constraint stands.
- **Replication + JIP.** Current `FACTION`/`WORLD` values replicate to all clients (broadcast on change, `RplSave`/`RplLoad` snapshot for JIP) so clients can render read-relevant state and consumers can run client-side. Value payloads are small scalars — `RplProp` or a compact RPC scheme, decided in planning.

### Persistence

- **`WORLD` and `FACTION` values persist in the save** via the vanilla persistence layer, declared conf-first in `Configs/Systems/Persistence/Overthrow.conf` like every other serializer (epic decision v2-5). Registered defaults apply when a save has no value (new option added to an old save); saved overrides win over registration defaults.
- **Mind "Continue ≠ connect"** (epic finding, BUG-104): a loaded save replaces the world without any player connecting, so applied option state must survive/re-apply on the load path itself — nothing may depend on a per-connect rebuild.
- `LOCAL` values persist via the user-settings module (above) and are independent of the campaign save.

### UI

- **Enable the dead button.** `MainMenu.layout` "Options" loses `"Is Enabled" 0`; `OVT_MainMenuContext` wires it to a new `OVT_OptionsContext` (standard `OVT_UIContext` lifecycle, gamepad-navigable like the other menus).
- **Generated form.** The screen renders the registry: one section per scope (Local / Faction / World), one row per option (label, description, widget matching the field type). Sections the player cannot edit are hidden entirely; a single player/host therefore sees all three.
- **Localization:** new strings go in `Language/localization_Overthrow.st` only (runtime `.conf` exports are Workbench-generated); the layout uses literal text until the user exports.

### First consumer: autosave

- Registers two `WORLD` options:
  - `autosave.enabled` — `TOGGLE`, default **on** (today's `m_bEnableAutosave` defvalue, `OVT_PersistenceManagerComponent.c:24-25`).
  - `autosave.interval` — `SLIDER` in seconds, default **600** (`:27-28`), sensible min/max decided in planning (min must stay above the save system's own busy-guard cadence).
- **Runtime changes must take effect without a restart.** Today `StartAutosaves()` (`:210`) schedules its `CallLater` at most once, guarded by `m_bAutosaveScheduled` (`:212`, `:218`) — so the persistence manager must gain cancel/reschedule support driven by the option change events (disable → remove the timer; interval change → re-arm with the new period; enable → start if the campaign is running).
- The two prefab attributes remain as registration defaults (server operators who edited the prefab keep their values as defaults) but the registry value is what the running system obeys.

## Non-goals

- Vanilla engine options (video/audio/controls/keybinds) — the engine's own settings menu owns those.
- Per-player state stored **on the server** (player-scoped-but-server-persisted settings); `LOCAL` is client-side only. If a future consumer needs server-side per-player settings, that is a scope-enum extension, not a retrofit.
- Migrating existing config surfaces (`Overthrow_Config.json`, difficulty presets, prefab attributes) into the registry wholesale — only autosave moves in this feature. The registry is the venue future migrations can target.

## Acceptance criteria

- Options button enabled and opens the generated menu; all three sections visible and editable in single player.
- On a dedicated server: a non-officer, non-admin client sees only Local; an officer additionally sees Faction; an admin sees all — and the server rejects a forged write for a scope the sender cannot edit (validation is server-side).
- Autosave can be disabled and its interval changed at runtime from the menu, effective without restart; both survive a save → Continue round trip.
- `WORLD`/`FACTION` values survive save/load and reach JIP clients; `LOCAL` values survive a game restart on the client.
- Automated coverage in the right tiers: registry logic (defaults, constraints, permission mapping) in Logic; manager resolution + registration in Init; persistence round-trip of a `WORLD` value in the round-trip suite — each new case proven able to fail.

## Dependencies

- `core/game-mode` — controller seam and manager lifecycle host the new manager + controller component.
- `core/persistence` — shipped; supplies the save path and the conf-first serializer convention.
- `core/controller-migration` — **not a blocker** (this feature lands its own controller component either way), but the generic accessor and validation conventions defined there should be followed so the two features converge rather than collide.
- `resistance` (officer model) and platform admin roles — permission sources, both already in place.
