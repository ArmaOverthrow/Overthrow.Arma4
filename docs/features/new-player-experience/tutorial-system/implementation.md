# Tutorial System — Implementation Plan

**Epic:** new-player-experience (feature #1 of 5)
**Status:** 🟢 Build complete (all 9 phases) · ⏳ play-test and string-table export owed
**Started:** 2026-08-07
**Completed (build):** 2026-08-07
**Last Updated:** 2026-08-07 (Phase 8 — §5 corrected against shipped code; DoD verdict in `tasks.md`)

---

## Executive Summary

Overthrow teaches a new player nothing. It hands them a house, a car, $100 and one 20-second hint that repeats every session because its dedup set (`OVT_OverthrowGameMode.m_aHintedPlayers`) is allocated fresh in `EOnInit`. This feature builds the framework that replaces that: **config-driven tutorial entries that fire off the player's own actions, are delivered only to the acting player's client, and are shown at most once per machine, ever.**

Four commitments shape everything below.

1. **Server fires, client decides.** Triggers subscribe to *existing* server-side manager `ScriptInvoker`s — no call-site instrumentation. The server resolves the acting player and sends one `RplRcver.Owner` RPC to that player's `OVT_OverthrowController`. Nobody else's client hears it. This is deliberately *not* the `OVT_NotificationManagerComponent.SendTextNotification` pattern, which broadcasts to every client and filters client-side (`:175-212`).
2. **Every decision that can be a pure function is one.** Trigger matching, queue ordering, the can-show-now gate and the seen-store set logic live in engine-free classes under `Scripts/Game/Data/`, so the Logic tier can assert them without a world. The components keep only plumbing.
3. **Seen state is per-machine, in a mod-owned `ModuleGameSettings`.** Not EPF, not the campaign save, not `$profile:` JSON (which is a hard no-op on console). The engine settings store works on console, is per-profile, and needs no registration — declaring the class is the entire contract.
4. **Two presentations, declared per entry.** Non-modal is the default: an `SCR_InfoDisplay` HUD overlay that never takes movement or aim, costing exactly **one** new gameplay keybinding. Modal is an `OVT_UIContext` with real focusable buttons and multi-page Next/Back — what `first-spawn`'s welcome sequence needs, and the escalation target from any non-modal popup.

Two gaps get filled on the way: `OVT_PlayerWantedComponent` gains the wanted-level-changed invoker it has never had, and `m_OnPlayerSkill` gains the `playerId` it needs to be routable per-player. Two wrong doc comments on the economy manager get corrected.

The feature ships with **1–2 proof entries only**. Real content is `tutorial-content`'s job, authored against the contract in §5.

### A correction to the requirements doc

The brief describes `Configs/FieldManual/FieldManualConfigRoot.conf` as **orphaned — "nothing loads it"**. That is wrong, and the truth changes Phase 7's shape considerably.

Overthrow's `FieldManualConfigRoot.conf.meta` declares GUID `{17295EF80DC38D53}` — the *exact* GUID the base game's `UI/layouts/Menus/FieldManual/FieldManual.layout:16` hands to `SCR_ConfigUIComponent.m_ConfigPath`, which `SCR_FieldManualUI.OnMenuOpen` (`:48-70`) loads. Overthrow's file is a **same-GUID delta override of the vanilla field-manual root, and it is what the manual dialog actually loads.** It is not orphaned in any mechanical sense — only in the sense that no Overthrow code links *to* it and it holds one thin entry.

Better still, the delta appears to be **already correct**. Same-GUID `.conf` overrides in this engine merge element-wise **by element GUID**, so an element whose GUID is absent from the parent is an *append*, not a replacement. Overthrow's category GUID `{59908331EDFD9788}` is not among vanilla's five, so it should land as a sixth category beside Introduction / Editor / MP Modes / Gameplay / Equipment. Three in-repo proofs of that merge behaviour: `Configs/Systems/ChimeraSystemsConfig.conf` declares 2 systems over vanilla's 106 and deliberately re-declares vanilla's `SCR_GarbageSystem "{5F10E7EA29B0EDEC}"` by GUID to retune it (meaningless under replacement semantics, and the game would not boot with 2 systems); `Configs/Commanding/CommandingMenu.conf` reuses vanilla's root-category GUID and appends one new command while 27 vanilla commands survive; `Configs/System/chimeraInputCommon.conf` is 917 lines over vanilla's 9,583 and the game still has keybindings.

There is even a free runtime canary: `SCR_FieldManualUI.c:253` calls `m_ConfigRoot.m_aTileBackgrounds.GetRandomElement()` **unguarded**, and Overthrow's file never declares `m_aTileBackgrounds`. If the override were a replacement, opening the manual would null-deref immediately. So *"the manual opens at all"* is itself evidence the delta merged.

So Phase 7 is not a rescue. It is: **confirm the merge with a five-minute spike, fix two real nits, and build the open-by-id seam.** The two nits are (1) Overthrow's sub-category reuses vanilla's locale key `#AR-FieldManual_Category_Introduction_Title`, so it renders as a second button literally named "Introduction", and (2) its single entry declares no `m_eId`, so it defaults to `EFieldManualEntryId.NONE` and is unreachable by the vanilla opener — which is precisely the gap the seam closes.

---

## Goals

### Primary Goals

1. **A tutorial entry is data.** Id, title/body stringtable keys, optional image, optional field-manual link, presentation mode, and trigger bindings — all in `Configs/Tutorials/*.conf`, added to the game mode prefab. A server owner or modder adds, removes or retunes entries with no EnforceScript.
2. **Per-player-correct delivery.** A trigger fires for the player who caused it, on that player's client only, correctly on a dedicated server and under JIP. This is the exact failure that killed the starter jobs (BUG-037) and it must not recur.
3. **Never twice.** An entry id shows at most once per machine, ever, backed by an engine-managed per-profile settings store that survives campaigns, servers and reinstalls of the campaign save.
4. **A popup that respects the player.** Never during another modal Overthrow context, never during the map or a base-game menu, never more than one at a time, never stealing movement or aim in the default presentation. Gamepad-navigable per the project's established menu-input rules.
5. **A sequence primitive** (multi-page Next/Back) good enough for `first-spawn`'s welcome flow.
6. **The "Learn more" seam actually works** — a proof entry deep-links into an Overthrow field-manual page, and the vanilla manual categories are restored alongside Overthrow's.
7. **Logic/Init-tier coverage** for every decision that does not need a widget, each new case proven able to fail once.

### Secondary Goals

1. **Fill the invoker gaps** in the owning components (wanted-level changed; `m_OnPlayerSkill` playerId), so `tutorial-content` never has to hack a trigger in.
2. **Correct the two wrong economy doc comments** that would otherwise mislead every future listener.
3. **Publish a contract** (§5) that `tutorial-content`, `field-manual` and `first-spawn` can build against without reading this plan.
4. **Provide, but do not yet use, the replacement for `m_aHintedPlayers`** — the store exists; removing `#OVT-IntroHint` belongs to `first-spawn`.

### Explicitly Out of Scope

- Tutorial content beyond the 1–2 proof entries (→ `tutorial-content`).
- Field-manual *entries* (→ `field-manual`). This feature only makes the category load and adds the open-by-id seam.
- The first-spawn welcome *content* (→ `first-spawn`).
- Per-campaign / per-player-record seen persistence — epic-level decision, per-machine only.
- Widget-highlight walkthroughs, objective markers, completion tracking beyond "seen".
- Any client→server RPC. Delivery is one-way (see D6).
- A settings-menu page for the tips toggle. The toggle lives in the modal popup; a menu page is a follow-up if players ask.

---

## Architecture Overview

### The pipeline, end to end

```
  SERVER                                          │  OWNING CLIENT ONLY
  ────────────────────────────────────────────────┼────────────────────────────────────────
  manager ScriptInvoker fires                     │
  (m_OnPlayerBuy, m_OnPlace, m_OnBuild, …)        │
        │                                         │
        ▼                                         │
  OVT_TutorialManagerComponent  (game mode)       │
    · build OVT_TutorialEventContext              │
      { event, playerId, value, filter }          │
    · OVT_TutorialMatcher.FindMatches()   ← pure  │
    · per-session sent-set (persistent id)        │
    · resolve controller:                         │
      OVT_Global.GetPlayers().GetController(id)   │
        │                                         │
        ▼                                         │
  OVT_TutorialComponent.Notify(entryId)           │
        │  Rpc(RpcDo_ShowTutorial, entryId)       │
        └──── RplChannel.Reliable, RplRcver.Owner ┼──▶ RpcDo_ShowTutorial(string entryId)
                                                  │        │
  ┌───────────────────────────────────────────────┘        ▼
  │  CLIENT-LOCAL TRIGGERS (no server round-trip)   OVT_TutorialSeenStore.HasSeen(id)? ─ yes ─▶ drop
  │   · SCR_MapEntity.GetOnMapOpen()                       │ no
  │   · OVT_UIContext.ShowLayout  (menu opened)            ▼
  │   · OVT_OverthrowGameMode.OnPlayerSpawnedLocal   tipsDisabled? ─ yes ─▶ drop (not marked seen)
  │            │                                           │ no
  │            ▼                                           ▼
  └──▶ OVT_Global.GetTutorials().FireLocalEvent()    OVT_TutorialQueue.Enqueue(id, priority)   ← pure
                                                            │
                                          pump every 1000 ms│
                                                            ▼
                                          OVT_TutorialGate.CanShowNow(…)                       ← pure
                                          (no blocking UI, nothing showing, alive)
                                                            │ yes
                                        ┌───────────────────┴───────────────────┐
                                        ▼                                       ▼
                             presentation == NONMODAL                presentation == MODAL
                             OVT_TutorialInfo                        OVT_TutorialContext
                             (SCR_InfoDisplay HUD overlay)           (OVT_UIContext, pages)
                             auto-dismiss 20 s                       Dismiss / Don't show
                             [key] ▶ escalate to modal ──────────▶   again / Learn more
                                        │                                       │
                                        └──────────────▶ mark seen ◀────────────┘
                                                     OVT_TutorialSettings
                                                     (ModuleGameSettings, per profile)
```

### Config schema (`Scripts/Game/Configuration/`)

```cpp
enum OVT_TutorialPresentation { NONMODAL, MODAL }

[BaseContainerProps()]
class OVT_TutorialPage
{
    [Attribute()]  string m_sBody;                    // #OVT- key
    [Attribute(...)] ResourceName m_sImage;           // optional
}

[BaseContainerProps(configRoot: true)]
class OVT_TutorialEntryConfig
{
    [Attribute()]  string m_sId;                      // stable, lowercase-kebab, immutable
    [Attribute()]  string m_sTitle;                   // #OVT- key
    [Attribute("0", uiwidget: UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(OVT_TutorialPresentation))]
                   OVT_TutorialPresentation m_ePresentation;
    [Attribute("0")] int m_iPriority;                 // higher shows first; default 0
    [Attribute()]  string m_sFieldManualTitleKey;     // optional deep-link target (see §5)
    [Attribute("", UIWidgets.Object)] ref array<ref OVT_TutorialPage> m_aPages;     // >= 1
    [Attribute("", UIWidgets.Object)] ref array<ref OVT_TutorialTrigger> m_aTriggers;
    [Attribute("1")] bool m_bEnabled;
}
```

Modelled directly on `OVT_JobConfig` (`Scripts/Game/Configuration/OVT_JobConfig.c:9-46`) — `configRoot: true`, nested `ref array<ref …>` object arrays, one `.conf` per entry.

**Triggers are one class with a virtual match, not a class hierarchy** (D3):

```cpp
class OVT_TutorialTrigger : ScriptAndConfig      // same shape as OVT_JobCondition : ScriptAndConfig
{
    [Attribute("0", uiwidget: UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(OVT_TutorialEvent))]
                   OVT_TutorialEvent m_eEvent;
    [Attribute("0")] int m_iMinValue;      // 0 = no threshold (amount, level, xp …)
    [Attribute()]    string m_sFilter;     // "" = no filter (context class name, shop type …)

    bool Matches(OVT_TutorialEventContext ctx);   // virtual; base implements event+min+filter
}
```

The base class ships the only implementation anyone needs; the virtual exists so a modder can subclass for exotic matching without a framework change. Zero cost, real seam.

```cpp
class OVT_TutorialEventContext : Managed          // pure data — Logic-tier testable
{
    OVT_TutorialEvent m_eEvent;
    int    m_iPlayerId;    // -1 for global events, resolved before matching
    int    m_iValue;
    string m_sFilter;
}
```

### Server manager (`OVT_TutorialManagerComponent`, on the game mode)

Standard Overthrow manager shape (`s_Instance` + `GetInstance()`, `Init(owner)`, `PostGameStart()`) exactly as `OVT_JobManagerComponent.c:66-105`.

- Holds `[Attribute("", UIWidgets.Object)] ref array<ref OVT_TutorialEntryConfig> m_aEntries;`, authored on `Prefabs/GameMode/OVT_OverthrowGameMode.et` the way `m_aJobConfigs` is (`:24-28`).
- `PostGameStart()` — server only. Builds the `id → entry` map, **fails loudly on a duplicate or empty id**, then subscribes to every invoker in the catalog.
- Per-event handlers each build an `OVT_TutorialEventContext`, resolve `playerId`, call `OVT_TutorialMatcher.FindMatches(m_aEntries, ctx, out ids)` and dispatch.
- **Per-session sent-set**, `map<string, ref set<string>>` keyed on persistent id (not runtime playerId, which is reused across reconnects). Prevents the server re-sending the same entry to the same player on every subsequent buy. Volatile by design — the client's store is the permanent one.
- Global (playerId-less) events dispatch through `SendToPlayersNear(vector pos, float radius, string entryId)`.

### Delivery (`OVT_TutorialComponent`, on `OVT_OverthrowController`)

Mirrors `OVT_BaseServerProgressComponent` (`Scripts/Game/Components/Controller/OVT_BaseServerProgressComponent.c:27-105`) and `OVT_ShopTransactionComponent` (`:255-261`). Lives in `Scripts/Game/Components/Controller/` — **never** on `OVT_PlayerCommsComponent`, which is deprecated for new RPCs.

```cpp
[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
void RpcDo_ShowTutorial(string entryId)      // server → owning client only
```

Server side resolves the target with `OVT_Global.GetPlayers().GetController(playerId)` (`OVT_PlayerManagerComponent.c:563`) and calls a thin `Notify(entryId)` wrapper that does the `Rpc(...)`. Reachable from client code via a new `OVT_Global.GetTutorials()` accessor, in the shape of `GetShopTransactions()` (`OVT_Global.c:109-115`).

The component also owns the client pipeline state (queue, pump timer, seen store handle) and exposes `ref ScriptInvoker m_OnShowTutorial` for the two UI surfaces to subscribe to — the same arrangement `OVT_ProgressInfo` uses to reach the controller (`Scripts/Game/UI/HUD/OVT_ProgressInfo.c:53-67`).

### Client decision logic (`Scripts/Game/Data/`, all engine-free)

| Class | Responsibility |
|---|---|
| `OVT_TutorialMatcher` | `static void FindMatches(array<ref OVT_TutorialEntryConfig>, OVT_TutorialEventContext, out array<string> ids)` — enabled + any trigger matches; result ordered by priority then declaration order |
| `OVT_TutorialQueue` | `Enqueue(id, priority)` (rejects duplicates and over-cap), `bool TryDequeue(out id)` (highest priority first, FIFO within priority), `Count()`, `Contains()`, `Clear()` |
| `OVT_TutorialGate` | `static bool CanShowNow(bool tipsDisabled, bool alreadyShowing, bool blockingUiOpen, bool playerAlive)` |
| `OVT_TutorialSeenStore` | in-memory `set<string>` + schema version; `HasSeen()`, `MarkSeen()`, `LoadFrom(array<...>)`, `WriteTo(out array<...>)`. **No engine calls** — the settings module is injected as plain data |

`blockingUiOpen` is computed by the component from three facts, none of which the pure gate knows about: a new `OVT_UIManagerComponent.IsAnyContextActive()` (loop `m_aContexts`, any `IsActive()`), `GetGame().GetMenuManager().GetTopMenu() != null`, and `SCR_MapEntity.GetMapInstance().IsOpen()`.

### Settings store (`OVT_TutorialSettings : ModuleGameSettings`)

Declaring the class is the entire registration contract — verified: `grep -rn "SCR_HintSettings"` across the base game's configs returns zero hits; `UserSettings.GetModule(string className)` resolves off the script type registry. Storage lands in `$profile:.save/settings/ReforgerGameSettings.conf`, is **per profile** (which is what makes the two-client MP test in §8 meaningful), and works on console — `game.c:613-625` reads the store inside its `#ifdef PLATFORM_CONSOLE` branch.

```cpp
[BaseContainerProps()]
class OVT_SeenTutorialEntry
{
    [Attribute()] string m_sId;
}

class OVT_TutorialSettings : ModuleGameSettings
{
    [Attribute("1")]     int  m_iVersion;
    [Attribute("false")] bool m_bTipsDisabled;
    [Attribute()]        ref array<ref OVT_SeenTutorialEntry> m_aSeen;
}
```

The nested-struct shape is chosen over a top-level `ref array<string>` because it is the shape with **direct base-game precedent**: `SCR_FilterSetStorage` holds two `ref array<string>` members inside a `[BaseContainerProps()]` class inside `SCR_AllFilterSetsStorage : ModuleGameSettings`, and is round-tripped through this exact store on every server-browser filter save (`/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Game/UI/Menu/Common/SCR_FilterSet.c:276-345`). No base-game settings module has a top-level `array<string>`. See D7 and R1.

Access wrapper `OVT_TutorialSettingsAccessor` (thin, engine-touching, not unit-tested):

```cpp
BaseContainer c = GetGame().GetGameUserSettings().GetModule("OVT_TutorialSettings");
if (!c) return;                                        // always null-guard
OVT_TutorialSettings s = new OVT_TutorialSettings();
BaseContainerTools.WriteToInstance(s, c);              // container -> instance  (LOAD)
if (!s.m_aSeen) s.m_aSeen = {};                        // MANDATORY: load can leave arrays null
// … mutate …
BaseContainerTools.ReadFromInstance(s, c);             // instance -> container  (SAVE)
GetGame().UserSettingsChanged();
GetGame().SaveUserSettings();                          // explicit flush — see D8
```

Guarded with `if (System.IsConsoleApp()) return;` so a headless/dedicated server never touches the store, mirroring `SCR_HintManagerComponent.OnPostInit` (`:511`).

### UI

**Non-modal (default)** — `OVT_TutorialInfo : SCR_InfoDisplay`, layout `UI/Layouts/HUD/TutorialPopup.layout`, registered under `SCR_BaseHUDComponent.InfoDisplays` on `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et:154-166` beside `OVT_WantedInfo` / `OVT_EconomyInfo` / `OVT_ProgressInfo`. Subscribes to the controller invoker in `OnStartDraw`, unsubscribes in `OnStopDraw` (`OVT_ProgressInfo.c:20-83`).

It costs **one** new gameplay-context binding, `OverthrowTutorialOpen`, in a passive `OverthrowTutorialContext` (`Priority 10`, `Flags 2` — the `OverthrowPlaceContext` shape, `chimeraInputCommon.conf:682`). Dismissal needs no binding: the overlay auto-dismisses after 20 s (the same duration `#OVT-IntroHint` used) and hides immediately if the player opens the map or any menu. The one key escalates to the modal presentation, where the full control set lives with real focus navigation.

**Modal** — `OVT_TutorialContext : OVT_UIContext`, layout `UI/Layouts/Menu/TutorialPopup.layout`, registered as the 17th entry in `OVT_UIManagerComponent.m_aContexts` on `Character_Player.et:19-134`, opened programmatically via `m_UIManager.ShowContext(OVT_TutorialContext)` (the `OVT_MainMenuContext.c:224-264` pattern) with **no** `m_sOpenAction` — exactly how `OVT_ShopContext` is registered. Action context `OverthrowTutorialMenuContext` (`Priority 50`, `Flags 4`) listing `MenuBack`/`MenuSelect`/`MenuUp`/`MenuDown`/`MenuLeft`/`MenuRight` plus `OverthrowTutorialNext`/`OverthrowTutorialBack` for sequences. Buttons are `WLib_NavigationButton` + `SCR_InputButtonComponent.m_OnActivated` (`OVT_ManageVehicleContext.c:6-34`).

Controls: **Dismiss**, **Don't show tips again**, **Learn more** (visible only when the entry declares a field-manual key), plus **Next/Back** when `m_aPages.Count() > 1`.

**Multi-page entries are always presented modally.** An entry declaring NONMODAL with more than one page is logged once at load and treated as MODAL — the sequence primitive has exactly one consumer (`first-spawn`) and it wants modal anyway.

### Field-manual seam

**(a) Confirm and tidy the category delta.** The manual resolves exactly one config path — `SCR_ConfigUIComponent` is a five-line class holding a single `ResourceName` (`scripts/Game/Components/SCR_ConfigUIComponent.c`), and `SCR_FieldManualConfigLoader.LoadConfigRoot` only walks the object it was handed. There is **no** modding seam by design: no `array<ResourceName>` of extra categories, no directory scan, and `SCR_FieldManualConfigRoot.m_sModsTabName` is a declared-but-never-read vestige of an abandoned "Reforger / Mods" tab split — do not build on it. The only working mechanism is the same-GUID delta Overthrow is already using.

Phase 7 therefore restructures rather than rescues, mirroring vanilla's own shape: the same-GUID root keeps **one** appended element that inherits from a **fresh-GUID** `Configs/FieldManual/Categories/FM_Overthrow.conf`, exactly as vanilla's root inherits its five categories from `Configs/FieldManual/Categories/FM_*.conf`. This works because `SCR_FieldManualConfigCategory` is `[BaseContainerProps(configRoot: true)]`, and it keeps per-page churn out of the overridden file so `field-manual` never has to touch the delta again.

```
SCR_FieldManualConfigRoot {
 m_aCategories {
  SCR_FieldManualConfigCategory "{59908331EDFD9788}" : "{<fresh>}Configs/FieldManual/Categories/FM_Overthrow.conf" { }
 }
}
```

**(b) Open by string id.** `SCR_FieldManualUI.Open(EFieldManualEntryId)` (`:859-866`) opens `ChimeraMenuPreset.FieldManualDialog` and calls `OpenEntry` (`:412-434`), which matches `entry.m_eId` — a base-game enum. There is no `FindEntry`, `GetEntryById` or any name lookup anywhere in the class; the only other iteration over `m_aAllEntries` is the searchbar's fuzzy `ProcessSearch` (`:738-818`), which is protected, localized and populates the tile grid rather than opening an entry. So a seam has to be added.

Every entry does carry `string m_sTitle` — a localization key, mod-owned, stable and already unique (`SCR_FieldManualConfigEntry.c:10-11`). A `modded class SCR_FieldManualUI` (Overthrow already uses this pattern in `Scripts/Game/UI/Modded/`) reaches the protected `m_aAllEntries` (`:18`), `m_bOpenedFromOutside` (`:34`) and `SetCurrentEntry` (`:323`) because a modded class *is* the class:

```cpp
modded class SCR_FieldManualUI
{
    void OVT_OpenEntryByTitle(string titleKey);            // matches entry.m_sTitle, else SetCurrentEntry(null)
    static SCR_FieldManualUI OVT_OpenByTitle(string titleKey);
}
```

~15 lines, one new file, no config-schema change, no vanilla behaviour touched, and `field-manual` gains no obligation beyond keeping title keys unique.

**Two implementation constraints, both load-bearing:**
- `Open()` works only because `MenuManager.OpenMenu` drives `OnMenuOpen` **and** `OnMenuShow` before it returns — and `OnMenuShow` (`:153-162`) calls `SetCurrentEntry(null)`. The navigation must therefore happen **after** `OpenMenu` returns, never from inside an override of `OnMenuOpen`, or it will be immediately undone.
- Vanilla `Open()` does not null-guard its `OpenMenu` result. Ours must.

Also worth passing to `field-manual`: `SetAllEntriesAndParents` (`:600-663`) supports **only two category levels** (root category → sub-category → entries) and prunes disabled or empty nodes.

### File structure

```
Scripts/Game/
├── GameMode/Managers/
│   └── OVT_TutorialManagerComponent.c            (server: registry, subscriptions, dispatch)
├── Components/Controller/
│   └── OVT_TutorialComponent.c                   (Owner RPC + client pipeline)
├── Configuration/
│   ├── OVT_TutorialEntryConfig.c                 (entry, page, presentation enum)
│   └── OVT_TutorialTrigger.c                     (trigger base, event enum, event context)
├── Data/
│   ├── OVT_TutorialMatcher.c                     (pure)
│   ├── OVT_TutorialQueue.c                       (pure)
│   ├── OVT_TutorialGate.c                        (pure)
│   └── OVT_TutorialSeenStore.c                   (pure)
├── Global/
│   ├── OVT_TutorialSettings.c                    (ModuleGameSettings + OVT_SeenTutorialEntry)
│   └── OVT_TutorialSettingsAccessor.c            (engine-touching load/save wrapper)
├── UI/
│   ├── HUD/OVT_TutorialInfo.c                    (SCR_InfoDisplay, non-modal)
│   ├── Context/OVT_TutorialContext.c             (OVT_UIContext, modal + sequence)
│   └── Modded/SCR_FieldManualUI.c                (open-by-title seam)
├── Helpers/ (or Global/)
│   └── OVT_FieldManualHelper.c                   (one-call wrapper Overthrow code uses)
└── Tests/TestSuites/
    ├── Logic/OVT_TEST_Logic_Tutorial.c           (new file, 5 cases)
    └── Init/OVT_TEST_InitSuite.c                 (2 cases appended)

Configs/Tutorials/
├── proofFirstBuy.conf                            (non-modal, 1 page, field-manual link)
└── proofWelcome.conf                             (modal, 2 pages — exercises the sequence)

UI/Layouts/
├── HUD/TutorialPopup.layout        (+ .meta)
└── Menu/TutorialPopup.layout       (+ .meta)

Configs/FieldManual/
└── Categories/FM_Overthrow.conf                  (new, fresh GUID — the category's real home)

Edited:
  Configs/FieldManual/FieldManualConfigRoot.conf            (reduce to one inheriting element)
  Configs/System/chimeraInputCommon.conf                    (3 actions, 2 contexts)
  Language/localization_Overthrow.st                        (master only — never the .conf exports)
  Prefabs/GameMode/OVT_OverthrowGameMode.et                 (manager + entry array)
  Prefabs/GameMode/OVT_OverthrowController.et               (delivery component)
  Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et (context + info display)
  Scripts/Game/Global/OVT_Global.c                          (2 accessors)
  Scripts/Game/GameMode/OVT_OverthrowGameMode.c             (manager field, Init, PostGameStart)
  Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c   (2 doc comments)
  Scripts/Game/GameMode/Managers/OVT_SkillManagerComponent.c     (invoker signature)
  Scripts/Game/UI/Context/OVT_CharacterSheetContext.c            (skill listener)
  Scripts/Game/Components/Player/OVT_PlayerWantedComponent.c     (new static invoker)
  Scripts/Game/Components/Player/OVT_UIManagerComponent.c        (IsAnyContextActive)
```

**GUID block:** reserve `6B3A0000…` for this feature (verified unused — the repo's highest current prefix is `6B2256EB`).

---

## Contract for Sibling Features

> `tutorial-content`, `field-manual` and `first-spawn` should be able to work from this section alone.

### Entry-id scheme

- Lowercase ASCII letters, digits and dashes. Max 48 characters. Shape: `<system>-<event>`.
  Examples: `economy-first-buy`, `wanted-first-level`, `recruits-first-recruit`, `place-first-camp`, `welcome-intro`.
- **Ids are immutable once shipped and are never reused.** Changing an id makes the entry re-show for everyone; reusing a retired id makes it *never* show for veterans. Retire by setting `m_bEnabled 0`, do not recycle.
- Ids must be unique across all entries. The manager refuses to start and logs an error naming the duplicate.

### Stringtable keys

All in `Language/localization_Overthrow.st` (the editable master). **Never** touch `Language/localization_Overthrow.<lang>.conf` — those are Workbench-generated exports and hand-editing corrupts them. A layout referencing a key not yet exported uses literal text until the user regenerates.

| Purpose | Key |
|---|---|
| Entry title | `#OVT-Tutorial_<PascalId>_Title` |
| Single-page body | `#OVT-Tutorial_<PascalId>_Body` |
| Multi-page body | `#OVT-Tutorial_<PascalId>_Body1` … `_BodyN` |

The popup **chrome** keys (`#OVT-Tutorial_Dismiss`, `_Next`, `_Finish`, `_Back`, `_LearnMore`, `_DisableTips`, `_PageIndicator`, `_MoreInMenu`, `_NoneAvailable`, `_Title`, and `#OVT-MainMenu_Tips`) are owned by this framework and already exist — a content feature adds only its own `_Title` / `_Body*` keys.

Body text is drawn in a `RichTextWidget` on both surfaces, so `<br/>` and the base game's `<action name="…"/>` glyph markup work. Keep a NONMODAL body to two short sentences: it wraps inside a 460 px HUD panel.

### Presentation and priority

- `m_ePresentation` is `NONMODAL` (HUD overlay) or `MODAL` (focusable popup). **An entry with more than one page is always presented modally** (D10) — declaring NONMODAL with two pages is logged once and coerced, so declare MODAL and mean it.
- `m_iPriority` orders the queue: higher shows first, equal priorities show in arrival order, and the second entry appears only after the first is dismissed. `welcome-intro` uses `100` so a first-spawn welcome cannot be pre-empted by an incidental tip; ordinary content should stay at `0`.
- `m_bEnabled 0` is the retirement path. A retired entry never fires and its id is still never reused.

### Field-manual link

`m_sFieldManualTitleKey` on the entry holds the **`m_sTitle` localization key of the target field-manual entry** — e.g. `#OVT-FieldManual_MainMenu_Title`. That key *is* the manual's entry id for linking purposes.

`field-manual`'s obligations to this framework, in full:
- **Title keys are unique and stable.** They are the link ids; renaming one breaks every popup pointing at it. Match is exact and case-sensitive.
- **Two category levels only.** `SCR_FieldManualUI.SetAllEntriesAndParents` (`:600-663`) walks root category → sub-category → entries and no deeper; a third level is silently dropped. Disabled or empty nodes are pruned.
- **New categories go in `Configs/FieldManual/Categories/FM_Overthrow.conf`**, not in the same-GUID root delta. The root delta stays a single element and should not need editing again.
- Do **not** rely on `SCR_FieldManualConfigRoot.m_sDefaultTabName` / `m_sModsTabName`. Both are declared in the base game and read nowhere — the "Reforger / Mods" tab split was never implemented.
- An unknown link key is not an error: the manual opens on its front page and a warning is logged.

### Trigger catalog

`OVT_TutorialEvent` values, their source, and what `m_iValue` / `m_sFilter` carry. **Every row is per-player unless marked global.**

**There are TEN server-side invokers, not nine** — eight per-player rows plus the two global ones. (Phases 2 and 8 corrected the count; all ten are subscribed in `OVT_TutorialManagerComponent.SubscribeToInvokers()` and every one of them is asserted non-null by `OVT_TEST_Init_Tutorial_InvokerSeamsExist`.) The remaining three rows are client-local and involve no server round trip.

| Event | Source | Fired from | `m_iValue` | `m_sFilter` | Notes |
|---|---|---|---|---|---|
| `PLAYER_BUY` | `OVT_EconomyManagerComponent.m_OnPlayerBuy` | `OVT_PlayerCommsComponent.c:688` | cost | — | args are `(playerId, actualCost)` — 2, not the 4 the doc comment claimed |
| `PLAYER_SELL` | `m_OnPlayerSell` | `OVT_ShopTransactionComponent.c:376` **and `OVT_EconomyManagerComponent.c:1004`** | total | — | args are `(playerId, total)`. ⚠️ **`PLAYER_SELL` does NOT mean "sold at a shop".** The second site is `AddPlayerMoney(playerId, amount, doEvent = true)`, so *any* money grant routed through it — job rewards included — raises this event with a matching signature. **Prefer `PLAYER_TRANSACTION`**, which carries the shop, whenever an entry must mean a shop sale |
| `PLAYER_TRANSACTION` | `m_OnPlayerTransaction` | `OVT_PlayerCommsComponent.c:691`, `OVT_ShopTransactionComponent.c:377` | amount | shop type | live and correct today; args `(playerId, shop, isBuying, amount)` |
| `PLAYER_PLACE` | `OVT_ResistanceFactionManager.m_OnPlace` | `:756` | — | placeable name | args `(entity, placeable, playerId)` |
| `PLAYER_BUILD` | `m_OnBuild` | `:854` | — | buildable name | args `(entity, buildable, playerId)` |
| `PLAYER_RECRUIT_ADDED` | `OVT_RecruitManagerComponent.m_OnRecruitAdded` | `:489`, `:2336` | — | — | args `(OVT_RecruitData)`; playerId derived from `m_sOwnerPersistentId` |
| `PLAYER_SKILL` | `OVT_SkillManagerComponent.m_OnPlayerSkill` | `:120`, `:341` | — | skill key | **signature changed by this feature.** Phase 0.2 made it `ScriptInvoker<int>` (playerId only), which left `m_sFilter` permanently `""`; **task 3.0 widened it to `ScriptInvoker<int, string>` = `(playerId, skillKey)`**, so the `m_sFilter` column above is accurate as shipped and "bought THIS skill" is expressible |
| `PLAYER_WANTED` | `OVT_PlayerWantedComponent.GetOnWantedLevelChanged()` | `SetBaseWantedLevel` | new level | — | **new static invoker added by this feature.** Escalation only — the decay path (`SetWantedLevel`) deliberately does not fire it |
| `TOWN_CONTROL_CHANGE` | `OVT_TownManagerComponent.m_OnTownControlChange` | `:676` | — | — | **global** — no playerId exists; delivered to players within 500 m. ⚠️ Declared `ScriptInvoker<IEntity>` at `:143` but **invoked with an `OVT_TownData`**; the template argument is decorative because `ScriptInvoker.Invoke` is untyped at runtime. Both pre-existing listeners take `OVT_TownData` and so does ours. **Never trust a `ScriptInvoker<T>` declaration in this codebase — read the invoke site or an existing listener** |
| `BASE_CONTROL_CHANGE` | `OVT_OccupyingFactionManager.m_OnBaseControlChanged` | `:1375` | — | — | **global** — delivered to players within 300 m |
| `MAP_OPENED` | `SCR_MapEntity.GetOnMapOpen()` | client-local | — | — | no server round-trip |
| `MENU_OPENED` | `OVT_UIContext.ShowLayout` hook | client-local | — | context class name | filter e.g. `OVT_MainMenuContext` |
| `PLAYER_SPAWNED` | `OVT_OverthrowGameMode.OnPlayerSpawnedLocal` | client-local | — | — | `first-spawn`'s hook; the existing `#OVT-IntroHint` is untouched by this feature. The push is **retried** (10 × 500 ms) because `OnPlayerSpawnedLocal` can beat the async controller assignment; after 5 s it gives up silently |

**Advice to `tutorial-content`:** prefer the per-player rows. The two global rows have no acting player by construction, and a proximity fan-out is a heuristic, not a guarantee — if a base-capture tip matters, consider hanging it off `PLAYER_BUILD`/`PLAYER_PLACE` at the captured base instead. And prefer `PLAYER_TRANSACTION` over `PLAYER_SELL` — see that row's warning.

### Adding an entry (the whole procedure)

1. Add `#OVT-Tutorial_<PascalId>_Title` / `_Body*` to `Language/localization_Overthrow.st`, then ask the user to export the string table in Workbench (never hand-edit the `.<lang>.conf` exports).
2. Create `Configs/Tutorials/<camelCaseId>.conf` (+ a `.meta` with a fresh resource GUID) by copying a proof entry:
   - **`Configs/Tutorials/proofFirstBuy.conf`** — `economy-first-buy`: `NONMODAL`, one page, a `PLAYER_BUY` trigger, and a field-manual deep link. Copy this for an ordinary tip.
   - **`Configs/Tutorials/proofWelcome.conf`** — `welcome-intro`: `MODAL`, **two** pages (`_Body1`/`_Body2`), `m_iPriority 100`, a `PLAYER_SPAWNED` trigger, and **no** field-manual link (so the Learn-more-hidden branch ships covered). Copy this for a sequence.
   Every member is written out explicitly in both files, including the ones that equal their attribute default — they are templates first and data second.
3. Add one element to `m_aTutorialEntries` on `Prefabs/GameMode/OVT_OverthrowGameMode.et`, inheriting from the new `.conf` (`OVT_TutorialEntryConfig "{<fresh>}" : "{<confGuid>}Configs/Tutorials/<file>.conf" { }`).
4. Run `tools/compile-check.sh` and the Fast tier. No script changes required. `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` will fail the build on an empty or duplicate id, a page-less entry or a trigger-less entry, and `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` will fail it on a `m_sFieldManualTitleKey` that resolves to no manual page.

A third-party mod adds entries with a same-GUID delta of the game-mode prefab using `m_aTutorialEntries + { … }` (append form, proven in this repo).

---

## Implementation Phases

Every phase ends with `tools/compile-check.sh` clean. Phases 0–4 additionally end with `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) green.

### Phase 0 — Invoker gaps and wrong doc comments
*`component-developer` (standard). Small, but it edits four managers other systems listen to — do it first and alone so any ripple is unambiguous.*

| # | Task |
|---|---|
| 0.1 | Correct `OVT_EconomyManagerComponent.c:100` (`m_OnPlayerBuy`) to `Args: int playerId, int actualCost` and `:101` (`m_OnPlayerSell`) to `Args: int playerId, int total`. Leave `m_OnPlayerTransaction`'s comment alone — it is already accurate and the invoker is live at two call sites |
| 0.2 | `m_OnPlayerSkill` → `ScriptInvoker<int>` carrying `playerId`. Update both invoke sites (`OVT_SkillManagerComponent.c:119` server path, `:340` inside the local-player branch of `RpcDo_SetPlayerSkill`). Update the single listener at `OVT_CharacterSheetContext.c:33` by inserting a new `OnSkillChanged(int playerId)` wrapper that calls the existing `Refresh()` — **do not** rely on arity coercion |
| 0.3 | Add `static ScriptInvoker<int, int, int> GetOnWantedLevelChanged()` to `OVT_PlayerWantedComponent` — args `(playerId, newLevel, oldLevel)`. A **static** invoker (the `SCR_MapEntity.GetOnMapOpen()` pattern) because the component is per-character and respawns; a per-instance invoker would need subscribe/unsubscribe bookkeeping on every spawn |
| 0.4 | Fire it inside `SetBaseWantedLevel` in the existing `if(m_iWantedLevel < level)` block, after `Replication.BumpMe()` and beside the `SendWantedNotification(reason)` hook (`:62-69`). Resolve `playerId` exactly as `SendWantedNotification` does (`:147`) and return early for recruits (`if(!m_PlayerData) return;`). **Do not** fire from `SetWantedLevel` — that is the raw setter the decay path uses |
| 0.5 | Grep-verify no other listener of any changed invoker exists |

**Acceptance:** compile-check exit 0; All group green; `grep -rn "m_OnPlayerSkill" Scripts/` shows exactly the two invoke sites and one listener, all consistent; gaining a wanted level in-game still produces the existing notification and no duplicate; buying and selling still award skill XP and move the stability/support modifiers.

### Phase 1 — Config schema + pure decision logic + Logic tests
*`component-developer` (standard). Nothing here touches a manager, a widget or the world.*

| # | Task |
|---|---|
| 1.1 | `OVT_TutorialEvent` enum, `OVT_TutorialEventContext`, `OVT_TutorialTrigger` (base `Matches()` implementing event + `m_iMinValue` + `m_sFilter`) |
| 1.2 | `OVT_TutorialPresentation`, `OVT_TutorialPage`, `OVT_TutorialEntryConfig` |
| 1.3 | `OVT_TutorialMatcher.FindMatches()` — enabled entries whose *any* trigger matches, ordered by priority then declaration order, no duplicates in the result |
| 1.4 | `OVT_TutorialQueue` — priority-then-FIFO, duplicate-rejecting, capped at `MAX_QUEUE = 8` (over-cap enqueues are dropped and counted, never silently overwrite) |
| 1.5 | `OVT_TutorialGate.CanShowNow(tipsDisabled, alreadyShowing, blockingUiOpen, playerAlive)` |
| 1.6 | `OVT_TutorialSeenStore` — set + `m_iVersion`; `HasSeen`, `MarkSeen` (idempotent), `LoadFrom`/`WriteTo` over plain arrays, cap at 512 ids with a one-time warning |
| 1.7 | `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Tutorial.c` — five cases (§9) |
| 1.8 | Prove each new case red once; record the method |

**Acceptance:** compile-check clean; Fast group green with five additional cases; every case builds its subjects with `new` and hand-written values; no reference to `OVT_Global` or `GetGame().GetGameMode()` anywhere in the new Logic file (the Logic tier's grep rule).

### Phase 2 — Server manager and invoker subscriptions
*`component-developer` (standard). Escalate to `component-developer-advanced` if Phase 0 turned up listeners beyond the one documented.*

| # | Task |
|---|---|
| 2.1 | `OVT_TutorialManagerComponent` — `s_Instance`/`GetInstance()`, `Init(owner)`, `PostGameStart()`; entry map build with duplicate/empty-id validation that logs an error and names the offender |
| 2.2 | Subscribe to all **ten** server-side invokers in the catalog (the brief originally said nine), behind a `Replication.IsServer()` guard |
| 2.3 | Per-event playerId resolution, including `OVT_RecruitData.m_sOwnerPersistentId` → runtime id via `OVT_Global.GetPlayers()` |
| 2.4 | `SendToPlayersNear(pos, radius, entryId)` for the two global events; radii as named constants (`NEAR_RADIUS_TOWN = 500`, `NEAR_RADIUS_BASE = 300`) |
| 2.5 | Per-session sent-set keyed on **persistent id**; cleared never, allocated in the constructor |
| 2.6 | Game-mode wiring: field + `Init(this)` beside the other managers (`OVT_OverthrowGameMode.c:1089-1161`) + `PostGameStart()` in `DoStartGame()` (`:198-245`); add the component to `Prefabs/GameMode/OVT_OverthrowGameMode.et` with the entry array; add `OVT_Global.GetTutorialManager()` |
| 2.7 | Extend `OVT_TEST_Init_Globals_ManagersResolve`'s getter list, and add one Init case (§9) |
| 2.8 | Prove the new Init case red once |

**Acceptance:** compile-check clean; All group green; the new getter is in the Init suite's null sweep; starting a campaign logs the manager's entry count once and no duplicate-id error; no invoker subscription happens on a client (verified by log on a joined client).

### Phase 3 — Owner-RPC delivery and the client pipeline
*`network-specialist` (standard). This is the authority boundary and the JIP-correctness surface.*

| # | Task |
|---|---|
| 3.1 | `OVT_TutorialComponent : OVT_Component` in `Scripts/Game/Components/Controller/` with `[RplRpc(RplChannel.Reliable, RplRcver.Owner)] void RpcDo_ShowTutorial(string entryId)` and the server-side `Notify(entryId)` wrapper |
| 3.2 | Register on `Prefabs/GameMode/OVT_OverthrowController.et` (fresh GUID from the reserved block); add `OVT_Global.GetTutorials()` |
| 3.3 | Client receive path: seen check → tips-disabled check → `OVT_TutorialQueue.Enqueue` → 1000 ms `CallLater` pump → `OVT_TutorialGate` → `m_OnShowTutorial.Invoke(entry)` |
| 3.4 | `blockingUiOpen` computation + new `OVT_UIManagerComponent.IsAnyContextActive()` |
| 3.5 | `FireLocalEvent(OVT_TutorialEventContext)` for client-local triggers, feeding the same matcher and the same queue |
| 3.6 | Client-local trigger hooks: `SCR_MapEntity.GetOnMapOpen()`, an `OVT_UIContext.ShowLayout` notification for `MENU_OPENED`, and `OVT_OverthrowGameMode.OnPlayerSpawnedLocal` for `PLAYER_SPAWNED`. **Leave the existing `#OVT-IntroHint` call in place** — removing it is `first-spawn`'s task |
| 3.7 | Null-guard every path for a controller that has not yet been assigned (`OVT_OverthrowController.RpcDo_NotifyOwnerAssignment` is async); a dropped local trigger is acceptable, a script error is not |

**Acceptance:** compile-check clean; All group green; a `Print` at the RPC receive point fires on exactly one client in a two-client session (the acting player's) and never on the other; the queue holds an entry while the shop menu is open and releases it after close; nothing new appears on `OVT_PlayerCommsComponent` (`grep -rn "RpcAsk_\|RpcDo_" Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` unchanged).

### Phase 4 — Per-machine settings store
*`component-developer` (standard). Task 4.4 is a hard gate: do not build UI on an unverified store.*

| # | Task |
|---|---|
| 4.1 | `OVT_SeenTutorialEntry` + `OVT_TutorialSettings : ModuleGameSettings` with `m_iVersion`, `m_bTipsDisabled`, `ref array<ref OVT_SeenTutorialEntry> m_aSeen` |
| 4.2 | `OVT_TutorialSettingsAccessor` — get-module null guard, `WriteToInstance` LOAD, lazy array allocation, `ReadFromInstance` SAVE, `UserSettingsChanged()` + `SaveUserSettings()`, `System.IsConsoleApp()` early-out. **No parallel arrays, ever** — the base game's `SCR_HintSettings.LoadShownHints` has to nuke its own state to recover from exactly that (`SCR_GameplaySettings.c:204-210`) |
| 4.3 | Version handling: on `m_iVersion` mismatch, clear `m_aSeen` and rewrite at the current version |
| 4.4 | **SERIALIZATION SMOKE TEST (gate).** Write two ids + the disabled flag, quit, relaunch, read back. Inspect `$profile:.save/settings/ReforgerGameSettings.conf` with the Workbench "Edit Game Settings" plugin (User Settings category). If the nested-struct array does not round-trip, fall back per R1 before continuing |
| 4.5 | Wire the accessor into `OVT_TutorialComponent`: load once on first use, mark-seen on dismiss, write the tips-disabled flag on toggle |

**Acceptance:** compile-check clean; All group green; the smoke test round-trips across a full application restart and the file is human-readable in the Workbench plugin; a headless server run produces no settings access in the log.

### Phase 5 — Non-modal HUD overlay ⚠️ ADVANCED AGENT
*Route to `ui-developer-advanced`. New layout, a new `SCR_InfoDisplay`, and — the risky part — a **gameplay-context** keybinding that must not collide with movement, aim, interact, inventory, map or menu, and must have a working gamepad input. Console usability is decided here.*

| # | Task |
|---|---|
| 5.1 | `UI/Layouts/HUD/TutorialPopup.layout` (+ `.meta`, fresh GUIDs): title, body, optional image, one `SCR_InputButtonComponent` prompt. Model the frame on `UI/Layouts/HUD/ProgressInfo.layout` |
| 5.2 | `OVT_TutorialInfo : SCR_InfoDisplay` — `OnStartDraw` caches widgets + subscribes to `OVT_Global.GetTutorials()`; `OnStopDraw` unsubscribes; hidden by default; every `FindAnyWidget` result null-guarded |
| 5.3 | Register under `SCR_BaseHUDComponent.InfoDisplays` on `Character_Player.et:154-166` |
| 5.4 | One action `OverthrowTutorialOpen` + `ActionContext OverthrowTutorialContext` (`Priority 10`, `Flags 2`) in `Configs/System/chimeraInputCommon.conf`. **Avoid `KC_W/A/S/D`** and anything the gameplay scheme already owns. Verify with `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py --warnings` — exit 0 and **no new** collision, baselined `BASE` rows excepted |
| 5.5 | Auto-dismiss after `AUTO_DISMISS_MS = 20000` (marks seen); immediate hide when the map or any menu opens; the entry is marked seen on hide either way |
| 5.6 | Escalation: the action opens the modal presentation of the same entry (Phase 6) and cancels the auto-dismiss |
| 5.7 | `#OVT-` keys for the prompt label, added to `localization_Overthrow.st` only |

**Acceptance:** the overlay appears, is legible at 1080p and 4K, never captures movement/aim/fire, is dismissed by the timer, is hidden the instant the map or a menu opens, and the one binding is reachable and prompted on both keyboard and gamepad. The conflict script reports no new collision.

### Phase 6 — Modal popup and sequence ⚠️ ADVANCED AGENT
*Route to `ui-developer-advanced`. A 17th `OVT_UIContext` alongside 16 existing ones, a new modal action context, and the multi-page primitive `first-spawn` depends on.*

| # | Task |
|---|---|
| 6.1 | `UI/Layouts/Menu/TutorialPopup.layout` (+ `.meta`): title, page body, optional image, page indicator, and `WLib_NavigationButton`s for Dismiss / Don't show tips again / Learn more / Back / Next. Model on `UI/Layouts/Menu/ManageVehicleMenu.layout` |
| 6.2 | `OVT_TutorialContext : OVT_UIContext` — `SetEntry(entry)` before `ShowLayout()`; page index; Learn-more button hidden unless `m_sFieldManualTitleKey != ""`; Next/Back hidden on single-page entries; Next on the last page acts as Dismiss; **`OnClose` removes exactly what `OnShow` inserted** |
| 6.3 | Register in `OVT_UIManagerComponent.m_aContexts` on `Character_Player.et` with no `m_sOpenAction` (the `OVT_ShopContext` shape); opened via `m_UIManager.ShowContext(OVT_TutorialContext)` |
| 6.4 | `ActionContext OverthrowTutorialMenuContext` (`Priority 50`, `Flags 4`) listing `MenuBack`, `MenuSelect`, `MenuUp/Down/Left/Right`, `OverthrowTutorialNext`, `OverthrowTutorialBack`. Run the conflict script again |
| 6.5 | Dismiss and the last-page Next mark the entry seen; "Don't show tips again" writes `m_bTipsDisabled` and closes; both flush the store |
| 6.6 | *(Droppable, do last)* Remove the eight debug `Print()` calls from `OVT_UIContext.ShowLayout()` (`:114-157`). They fire on every menu open for all 17 contexts and would now fire on every popup. Behaviour-identical, but re-open every existing menu once afterwards |

**Acceptance:** every button performs its labelled action with mouse **and** with a gamepad stick/d-pad + A/B; a two-page proof entry pages forward and back and cannot page past either end; the Learn-more button is absent on an entry with no link; dismissing marks seen (the entry never returns, even after a restart); "Don't show tips again" suppresses all subsequent popups until re-enabled; no popup ever renders while another Overthrow context or the map is open.

### Phase 7 — Field-manual seam ⚠️ ADVANCED AGENT
*Route to `ui-developer-advanced` (or `component-developer-advanced`). This edits a **same-GUID override of a base-game config** and modded-classes a base-game menu. Getting it wrong takes the vanilla field manual with it. Fully parallelisable with Phases 1–6.*

| # | Task |
|---|---|
| 7.1 | **SPIKE (do this first, ~5 min).** Launch, open the Field Manual from the main menu and from the pause menu. Record: are the five vanilla categories present alongside `#OVT-FieldManual_Category_Overthrow_Title`? Do the tile backgrounds render? (They can only render if the delta merged — Overthrow's file never declares `m_aTileBackgrounds` and `SCR_FieldManualUI.c:253` dereferences it unguarded.) **Expected: yes to both.** If either is no, stop and re-plan — the merge semantics assumed throughout §4 are wrong and that affects far more than this feature |
| 7.2 | Restructure to vanilla's shape: move the Overthrow category's content into a **fresh-GUID** `Configs/FieldManual/Categories/FM_Overthrow.conf`, and reduce the same-GUID root delta to a single element inheriting from it (keeping element GUID `{59908331EDFD9788}`, which is what makes it an append). Re-verify 7.1's observations afterwards |
| 7.3 | Fix the two nits: retitle the sub-category from vanilla's `#AR-FieldManual_Category_Introduction_Title` to a new `#OVT-FieldManual_Category_GettingStarted_Title` (it currently renders as a second button named "Introduction"), and give the entry a real title key that `tutorial-content` can link to |
| 7.4 | `modded class SCR_FieldManualUI` in `Scripts/Game/UI/Modded/` — `OVT_OpenEntryByTitle(string titleKey)` walking `m_aAllEntries` on `entry.m_sTitle`, setting `m_bOpenedFromOutside` and calling `SetCurrentEntry`, falling back to `SetCurrentEntry(null)` (front page) on no match exactly as `OpenEntry` does; plus `static OVT_OpenByTitle(string titleKey)` mirroring `Open()` (`:859-866`) **but null-guarding the `OpenMenu` result, which vanilla does not** |
| 7.5 | Respect the ordering constraint: navigate **after** `OpenMenu` returns. `OnMenuShow` (`:153-162`) calls `SetCurrentEntry(null)`, so any navigation performed from inside an `OnMenuOpen` override is silently undone |
| 7.6 | `OVT_FieldManualHelper.Open(string titleKey)` — the single call Overthrow code makes; logs a warning and falls back to the front page on an unknown key |
| 7.7 | Wire the modal popup's Learn-more button to it; prove end-to-end with the proof entry linking to the Overthrow page |
| 7.8 | Record the id contract (§5, "Field-manual link") plus the **two-category-level limit** (`SetAllEntriesAndParents`, `:600-663`) in this feature's notes for `field-manual` to consume |

**Acceptance:** the Field Manual shows the five vanilla categories **and** the Overthrow category, with tile backgrounds rendering; the Overthrow sub-category is no longer named "Introduction"; opening the manual normally (main menu / pause) is unchanged; the proof popup's Learn-more opens the manual **directly on the Overthrow page**, not on the front page; a deliberately wrong title key opens the front page and logs a warning rather than erroring or crashing.

### Phase 8 — Proof entries, localization and final verification
*`component-developer` (standard).*

| # | Task |
|---|---|
| 8.1 | `Configs/Tutorials/proofFirstBuy.conf` — non-modal, one page, `PLAYER_BUY` trigger, field-manual link, id `economy-first-buy` |
| 8.2 | `Configs/Tutorials/proofWelcome.conf` — modal, **two** pages (exercises the sequence), `PLAYER_SPAWNED` trigger, id `welcome-intro` |
| 8.3 | Register both on `Prefabs/GameMode/OVT_OverthrowGameMode.et` under `m_aTutorialEntries` |
| 8.4 | All strings into `Language/localization_Overthrow.st` only. Hand back the list of new keys for the user to export |
| 8.5 | Full verification pass per §8 including the two-client MP protocol |

**Acceptance:** every item in the Definition of Done is checked or explicitly deferred with a reason.

---

## Key Technical Decisions

**D1 — Manager on the game mode, delivery component on the controller.**
The registry, the invoker subscriptions and the acting-player resolution are system-wide, server-authoritative state → Manager. The RPC target must be a per-player, replicated, client-owned entity → a component on `OVT_OverthrowController`, following `OVT_BaseServerProgressComponent` and `OVT_ShopTransactionComponent`. Not `OVT_PlayerCommsComponent`: it is deprecated for new RPCs by project rule.

**D2 — Per-entry `.conf` files registered on the game-mode prefab, not one root conf.**
Rejected alternative: a single `Configs/Tutorials/tutorials.conf` loaded with `BaseContainerTools.LoadContainer` (the `OVT_ResistanceFactionManager.c:189-204` pattern). The prefab-array form wins on three counts: it is what `m_aJobConfigs` already does so there is nothing new to learn; `tutorial-content` will add a dozen entries and one-file-per-entry keeps that a set of additions rather than a merge-conflict magnet; and a modder can append with the proven `m_aTutorialEntries + { … }` delta form without owning the whole array. Accepted cost: *removing* a shipped entry from a third-party mod means overriding the whole array — mitigated by `m_bEnabled 0`, which is the intended retirement path anyway.

**D3 — One trigger class with a virtual `Matches()`, not a class hierarchy.**
`OVT_JobCondition` has ten subclasses because job conditions genuinely differ. Tutorial triggers differ only by which event and two optional filters, so ten near-identical classes would be ceremony. The base class ships the only implementation, and the virtual keeps the modder seam open at zero cost.

**D4 — String entry ids on the wire, not array indices.**
An int index is cheaper and the client does have the same config (the game mode entity exists on clients). It is also exactly the append-only positional fragility that `starter-jobs-retirement` is filing complaints about with `jobIndex`. One reliable RPC per first-time action is not a bandwidth problem, and a string id is self-describing in a log.

**D5 — Two-layer dedup: volatile server-side sent-set + permanent client-side seen store.**
The client store is authoritative (epic decision: the server has no visibility into seen state). The server's per-session, per-persistent-id sent-set exists only so that a veteran's every shop purchase does not generate a fresh RPC forever. Keyed on persistent id because runtime player ids are reused across reconnects.

**D6 — Delivery is one-way. No client→server RPC in v1.**
A "mute me" upload would let the server skip a player entirely, but with D5's sent-set the total traffic is bounded at (entries × players) tiny reliable RPCs per session — negligible. Accepted consequence, documented: if a player has tips disabled, receives an entry (dropped, *not* marked seen), and re-enables tips mid-session, that entry will not re-fire until the next session, because the server's sent-set already has it. Self-healing across sessions; not worth a new RPC surface.

**D7 — Seen ids live in a nested `[BaseContainerProps()]` struct array, not a top-level `array<string>`.**
`ref array<string>` is *proven* to survive this exact settings store — but only one level down, inside a `[BaseContainerProps()]` class held in a settings module's object array (`SCR_FilterSetStorage` in `SCR_FilterSet.c:276-345`, round-tripped on every server-browser filter save). **No** base-game settings module has a top-level `array<string>`. Adopting the proven shape removes the risk instead of testing it, and leaves room for per-entry metadata (a seen count, a timestamp) without a schema break. Fallbacks, in order, if even this fails: `array<ResourceName>` (top-level, precedent `SCR_GameplaySettings.c:236`); a single delimited `string` split with `string.Split` (plain string scalars are proven top-level at `:24,27,30`); an `enum` + `array<enum>` in the `SCR_HintSettings` shape, which is the only option that costs us string ids and is therefore last.

**D8 — Flush with `SaveUserSettings()` on every mutation, not only on exit.**
The engine doc calls it "only on very important cases", and `SCR_HintSettings.SaveShownHints` settles for `UserSettingsChanged()`. Overthrow players alt-F4 and crash far more than they exit cleanly, and the whole value of this store is that a tip never repeats. Losing seen state to an unclean exit would reproduce the exact `m_aHintedPlayers` bug we are fixing. Precedent for explicit flushing exists (`SCR_FilterSet.c:344`).

**D9 — Non-modal by default, escalating to modal, with exactly one new gameplay binding.**
The requirement asks for Dismiss, "Don't show tips again" and "Learn more" on the popup. Three of those as gameplay-context keybindings would be three collision risks in a scheme where `a`/`b` are `MenuSelect`/`MenuBack` and `W/A/S/D` are menu nav — and on a gamepad during gameplay there is barely a free face button. So the non-modal overlay carries **one** binding (escalate), dismisses itself on a timer or when any UI opens, and hands the full control set to the modal, which gets real focus navigation for free via `MenuUp/Down/Select/Back`. Console usability is guaranteed by the modal rather than gambled on the HUD overlay. A second explicit-dismiss binding is a droppable nice-to-have if a genuinely free input turns up.

**D10 — Multi-page entries are always modal.**
The only sequence consumer is `first-spawn`'s welcome, which is modal by design. Paging a non-focusable HUD overlay would need two more gameplay bindings for zero current benefit. An entry declaring NONMODAL with >1 page is logged once and coerced.

**D11 — playerId-less invokers get a proximity fan-out, not a broadcast, and not a fabricated playerId.**
`m_OnTownControlChange` and `m_OnBaseControlChanged` genuinely have no acting player. Broadcasting is the `SendTextNotification` anti-pattern this feature exists to avoid. Sending to players within a radius of the town/base is honest about being a heuristic, and §5 tells `tutorial-content` to prefer a per-player trigger where one exists.

**D12 — Field-manual entries are linked by their `m_sTitle` localization key, via a modded-class overload.**
`SCR_FieldManualUI.OpenEntry` matches only on `EFieldManualEntryId m_eId`, and there is no name lookup anywhere in the class. Two options exist. *Rejected:* `modded enum EFieldManualEntryId { OVT_ECONOMY, … }` — legal (the base game mods its own enums, e.g. `EAIDangerEventType`) and it would light up the vanilla hint deep-link path for free, but it needs a modded enum **plus** a per-entry conf field **plus** a string→enum map to keep callers string-based, and the vanilla hint path is exactly the presentation this epic decided against. *Chosen:* a ~15-line `modded class` overload matching `m_sTitle`, which is mod-owned, already unique, already stable and already the thing `field-manual` authors. Zero config-schema change, zero new fields, and the "id" the sibling features consume is a key they can read straight out of the `.conf`. Accepted cost: renaming a title key breaks its links — which is why §5 freezes the key scheme. If the vanilla hint channel is ever wanted, the modded enum can be added later without disturbing this.

**D13 — The wanted invoker is static.**
`OVT_PlayerWantedComponent` is per-character and dies and respawns with the player. A per-instance invoker would force the manager to subscribe on every spawn and unsubscribe on every death, which is precisely the bookkeeping that produces leaks. A static invoker (`SCR_MapEntity.GetOnMapOpen()` precedent) gives one subscription for the session.

---

## Definition of Done

An independent evaluator should be able to verify all of the following without having read the implementation.

### Functional Criteria

- [ ] **F1** On a **fresh profile**, buying anything at a shop for the first time shows the `economy-first-buy` popup once. Dismiss it, buy again ten times: it never returns. Restart the game, start a **new campaign**, buy again: it still never returns.
- [ ] **F2** The popup shows the entry's title, body and (if configured) image, all from `#OVT-` keys — no raw English is drawn by new code.
- [ ] **F3** A non-modal popup never captures movement, aim or fire. The player can walk, look and shoot while it is on screen.
- [ ] **F4** A non-modal popup disappears on its own after ~20 s, and disappears immediately if the player opens the map or any menu. Either way it is marked seen.
- [ ] **F5** The single documented key escalates a non-modal popup into the modal popup for the same entry, with Dismiss, "Don't show tips again" and (when linked) "Learn more" as focusable buttons.
- [ ] **F6** The two-page `welcome-intro` proof entry presents modally, pages forward and back, shows a page indicator, cannot page past either end, and its last-page Next dismisses.
- [ ] **F7** "Don't show tips again" suppresses every subsequent popup, survives a game restart, and is the only control that does so.
- [ ] **F8** No popup ever appears while any Overthrow menu context, the map, or a base-game menu is open. If a trigger fires during one, the popup appears **after** it closes (within ~1 s), not never and not on top.
- [ ] **F9** Two entries triggering close together are shown one at a time, in priority order, second only after the first is dismissed.
- [ ] **F10** Adding a new entry requires only a `.conf` file, stringtable keys and one prefab line — no script change. Demonstrated by adding a throwaway third entry and seeing it fire.
- [ ] **F11** An entry with `m_bEnabled 0` never fires. A duplicate id logs a named error at campaign start.

### Quality Criteria

- [ ] **Q1 — Dedup cannot double-fire.** No entry is ever shown twice on one machine. Verified by triggering the same action 10× in one session and again after a restart.
- [ ] **Q2 — The settings store never corrupts.** No parallel arrays. A hand-corrupted or version-mismatched store is detected, cleared and rewritten rather than crashing or half-loading. `WriteToInstance` returning a null array is guarded everywhere.
- [ ] **Q3 — The store round-trips.** Seen ids and the disabled flag survive a full application restart and are readable in `$profile:.save/settings/ReforgerGameSettings.conf` via the Workbench "Edit Game Settings" plugin.
- [ ] **Q4 — No dead buttons.** Every button on the modal popup performs its labelled action with mouse **and** with a gamepad.
- [ ] **Q5 — No input regressions.** `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py --warnings` reports **no new** collision (the 13 baselined `BASE` rows excepted). All 16 pre-existing Overthrow menus still open, navigate and close on keyboard and gamepad.
- [ ] **Q6 — No manager regressions.** The four managers touched in Phase 0 behave identically: skill purchase still refreshes the character sheet, buy/sell still award XP and move stability/support modifiers, gaining a wanted level still produces exactly one notification.
- [ ] **Q7 — No new nulls.** Every `FindAnyWidget`, every `GetModule`, every `GetController` and every menu-manager result introduced by this feature is null-guarded. A player who triggers a tip before their controller is assigned gets no popup and no script error.
- [ ] **Q8 — Style.** No ternaries; `ref` on Managed in containers; `RplId` not `EntityID` over the network; `OVT_` prefix and `m_i`/`m_f`/`m_s`/`m_b`/`m_a`/`m_m` naming throughout; Doxygen `//!` on public members.
- [ ] **Q9 — Nothing added to `OVT_PlayerCommsComponent`.**
- [ ] **Q10 — Compile and tests clean.** `tools/compile-check.sh` exit 0; `tools/run-tests.sh "{6A6E29FF47ECB840}"` exit 0; `tools/run-tests.sh "{6A6E2A002F53A581}"` exit 0. Fast gains 5 Logic + 2 Init cases; each new case has a recorded way it was made to fail.
- [ ] **Q11 — Localization hygiene.** New keys exist in `Language/localization_Overthrow.st` **only**; no `localization_Overthrow.<lang>.conf` file is modified (`git diff --stat Language/` shows the `.st` and nothing else).

### Integration Criteria

- [ ] **I1 — Field manual intact and extended.** The Field Manual shows the five vanilla categories (Introduction, Editor, MP Modes, Gameplay, Equipment) **and** the Overthrow category, with tile backgrounds rendering. The Overthrow sub-category is no longer displayed as a second "Introduction". Opening the manual from the main menu and from the pause menu is unchanged.
- [ ] **I2 — Deep link works.** The proof popup's "Learn more" opens the Field Manual **on the Overthrow page**, not the front page. A wrong key opens the front page and logs a warning — no error, no crash.
- [ ] **I3 — MP delivery is per-player.** On a dedicated server with two clients, a trigger caused by client A produces a popup on client A **only**. Client B sees nothing and logs nothing.
- [ ] **I4 — Per-machine, not per-campaign.** With two client profiles on one machine, each has its own seen state; a tip dismissed on profile 1 still shows on profile 2 and never again on profile 1, on any server or campaign.
- [ ] **I5 — JIP safe.** A client joining a running campaign receives its own triggers normally and inherits none of the other player's.
- [ ] **I6 — The contract is published.** §5's entry-id scheme, trigger catalog and add-an-entry procedure are accurate against the shipped code, and `field-manual` / `tutorial-content` / `first-spawn` can be started from them.
- [ ] **I7 — Nothing removed yet.** `#OVT-IntroHint` and `m_aHintedPlayers` still exist and behave as before; the five starter jobs are untouched.

### Verification Method

**1. Automated (run these; record exit codes):**
```bash
tools/compile-check.sh                                    # expect 0
tools/run-tests.sh "{6A6E29FF47ECB840}"                   # Fast (38 + 7 new) - expect 0
tools/run-tests.sh "{6A6E2A002F53A581}"                   # All  (66 + 7 new) - expect 0
tools/run-tests.sh OVT_TEST_Logic_Tutorial_QueueOrdering  # single case, debugging
python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py --warnings
git diff --stat Language/                                 # expect ONLY localization_Overthrow.st
```

**2. Settings-store smoke test (Phase 4 gate, do before any UI work):**
1. Launch, trigger the proof entry, dismiss it.
2. Quit the game **cleanly**.
3. In the Workbench, run the **User Settings → Edit Game Settings** plugin. Confirm an `OVT_TutorialSettings` block exists containing the id and `m_iVersion 1`.
4. Relaunch, trigger the same action — nothing appears.
5. Repeat steps 1–4 killing the process instead of quitting cleanly. The id must still be there (this is what D8 buys).

**3. Single-player manual play-test:**

*A. Basic delivery*
1. Fresh profile (or delete the `OVT_TutorialSettings` block). Start a campaign, spawn — the `welcome-intro` modal appears.
2. Page forward, page back, page forward, Next on the last page → it closes.
3. Restart the game and start a new campaign — the welcome does **not** reappear (F1, Q1).

*B. Non-modal behaviour*
4. Buy something at a shop. The popup appears **after** the shop menu closes, not on top of it (F8).
5. While it is up: walk, aim, fire. All work (F3).
6. Let it time out (~20 s). Buy again — it does not return (F4).
7. Reset the store, buy again, and this time open the map while the popup is up. It disappears immediately and does not return (F4).

*C. Escalation and controls*
8. Reset the store, buy again, press the documented key. The modal opens with Dismiss / Don't show tips again / Learn more.
9. Drive the modal with a **gamepad only**: stick/d-pad to move focus, A to activate, B to close. Every button responds (Q4).
10. Press "Learn more" → the Field Manual opens **on the Overthrow page** (I2).
11. Reset the store, trigger again, press "Don't show tips again". Trigger a third entry — nothing appears. Restart — still nothing (F7).

*D. Queue*
12. Add a throwaway third entry bound to the same trigger with a higher priority. Trigger once: the higher-priority entry shows first, and the second only after the first is dismissed (F9, F10).

*E. Field manual and regressions*
13. Open the Field Manual from the pause menu **and** from the main menu. Confirm **six** categories — the five vanilla ones plus Overthrow — that the tile backgrounds render, and that no category is named "Introduction" twice (I1).
14. Open every one of the 16 existing Overthrow menus, navigate and close each with keyboard and with a gamepad (Q5).
15. Buy a skill (character sheet refreshes), buy and sell at a shop (money and XP correct), commit a crime (exactly one wanted notification) (Q6).

**4. Multiplayer / dedicated-server protocol (the harness cannot reach this):**
```bash
# terminal 1
tools/launch-server.sh --scenario eden
# terminal 2 — client A
tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001
# terminal 3 — client B
tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001
```
16. Both spawn. **Each** gets its own `welcome-intro` popup (not one, not zero) (I3, I5).
17. Client A buys at a shop. **Only** client A's screen shows the popup; check client B's log for the entry id — it must be absent (I3).
18. Client B buys at a shop. Now only B sees it. The two profiles have independent seen state (I4).
19. Dismiss on A, have A buy ten more times — nothing. Have B buy — B still gets its first-time popup (Q1, I4).
20. Disconnect B, reconnect it (JIP into a running campaign). B's already-seen entries stay silent; an untriggered entry still fires normally (I5).
21. Watch the server log throughout: no popup-related error, and no evidence of the server touching the user-settings store.

---

## Testing Strategy

**Logic tier (automated, world-free)** — `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Tutorial.c`. Every subject is built with `new` and fed hand-written values; nothing touches a component, a manager, a widget or the world.

| Case | Claim |
|---|---|
| `OVT_TEST_Logic_Tutorial_TriggerMatching` | A trigger matches on event alone when `m_iMinValue == 0` and `m_sFilter == ""`; a threshold rejects a lower value and accepts an equal one; a filter is exact-match and case-sensitive; a wrong event never matches regardless of the other fields |
| `OVT_TEST_Logic_Tutorial_MatcherSelectsAndOrders` | `FindMatches` returns only enabled entries with a matching trigger; an entry with two triggers where one matches is returned **once**; results are ordered by descending priority and by declaration order within a priority; no match returns an empty array, not null |
| `OVT_TEST_Logic_Tutorial_QueueOrdering` | Highest priority dequeues first; FIFO within equal priority; enqueueing an id already queued is a no-op; `TryDequeue` on an empty queue returns false and writes nothing; enqueue beyond `MAX_QUEUE` is dropped without displacing an existing item |
| `OVT_TEST_Logic_Tutorial_GatePredicate` | `CanShowNow` is false when tips are disabled, when something is already showing, when a blocking UI is open, or when the player is dead — and true only when all four are clear. Each of the four is asserted in isolation |
| `OVT_TEST_Logic_Tutorial_SeenStore` | `MarkSeen` is idempotent; `HasSeen` is exact-match; `LoadFrom`/`WriteTo` round-trip an id list without reordering loss or duplication; a version mismatch clears the set; the 512 cap refuses further ids without dropping existing ones |

**Init tier (automated, live managers, campaign not started)** — appended to `OVT_TEST_InitSuite.c`.

| Case | Claim |
|---|---|
| `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` | `OVT_Global.GetTutorialManager()` is non-null (also added to `OVT_TEST_Init_Globals_ManagersResolve`'s sweep) and its entry array has ≥ 1 entry, every entry has a non-empty unique id, ≥ 1 page and ≥ 1 trigger |
| `OVT_TEST_Init_Tutorial_InvokerSeamsExist` | Every invoker the catalog names is non-null on its live manager — `m_OnPlayerBuy`, `m_OnPlayerSell`, `m_OnPlayerTransaction`, `m_OnPlace`, `m_OnBuild`, `m_OnRecruitAdded`, `m_OnPlayerSkill`, `m_OnTownControlChange`, `m_OnBaseControlChanged`, and the new static `OVT_PlayerWantedComponent.GetOnWantedLevelChanged()`. This is the case that goes red the day someone renames or deletes one |

**Deliberately not automated, and therefore named explicitly above:**
- All rendering, input and gamepad navigation — project rule, UI is play-test only (steps A–E).
- The settings store's engine round-trip — needs a real profile directory and an application restart (verification step 2). `OVT_TutorialSeenStore`'s *logic* is Logic-tier; only the `BaseContainer` plumbing is manual.
- Per-player MP delivery and JIP — steps 16–21. **This is the most common regression class in this project and the harness structurally cannot reach it.**
- The field-manual override behaviour — a config-resolution question only the running game can answer (Phase 7.1, step 13).

**Fallibility rule:** every new automated case must be shown red once during development, by a recorded edit, before it ships. No `maxAttempts`, ever. A case that has never failed is not evidence.

---

## Dependencies

- **None within the epic.** This is feature #1. `field-manual` (#2) can be built entirely in parallel; the only contact point is §5's link contract, and Phase 7 hands it over.
- **Consumes** existing manager invokers listed in §5, and **modifies** two of them (`m_OnPlayerSkill` gains an argument; `OVT_PlayerWantedComponent` gains one) in Phase 0.
- **Base-game systems relied on:** `ModuleGameSettings` / `GetGameUserSettings()`, `SCR_InfoDisplay` + `SCR_BaseHUDComponent`, `SCR_InputButtonComponent` / `WLib_NavigationButton`, `SCR_MapEntity.GetOnMapOpen()`, `SCR_FieldManualUI` + `ChimeraMenuPreset.FieldManualDialog`, the stringtable pipeline.
- **Blocks:** `tutorial-content` (#3) and `first-spawn` (#4) both need Phases 1–6 complete. `starter-jobs-retirement` (#5) waits on #3.
- **Coordination:** none required with concurrent work, but Phase 0 edits `OVT_EconomyManagerComponent`, `OVT_SkillManagerComponent` and `OVT_PlayerWantedComponent` — check for in-flight branches touching those before starting.

---

## Risks & Mitigation

**R1 — The settings module might not serialize the shape we need.**
*Likelihood: low. Impact: high (it is the feature's whole persistence story).*
Mitigated at design time by adopting the shape with direct base-game precedent (D7) rather than the unproven top-level `array<string>`, and at build time by Phase 4.4, an explicit gate before any UI work. Three ranked fallbacks are pre-selected (`array<ResourceName>` → delimited string → enum array), each with its own base-game precedent, so a failure costs an afternoon and not a redesign.

**R2 — Restructuring the field-manual root could take the vanilla manual with it.**
*Likelihood: low. Impact: high (a visible regression in a base-game screen, and the manual has exactly one config path with no fallback).*
Overthrow's file is a same-GUID delta of the vanilla root and the evidence says it already merges correctly (three in-repo precedents, plus the `m_aTileBackgrounds` null-deref canary). Mitigation: Phase 7 opens with an observation spike so the restructure is made against a *known* state rather than an assumption; the restructure keeps the appending element GUID `{59908331EDFD9788}` unchanged, which is the single fact the merge depends on; and it mirrors vanilla's own root-inherits-category-conf shape rather than inventing one. Phase acceptance names the six categories and the tile backgrounds explicitly. **If the 7.1 spike shows vanilla categories missing, stop** — that would falsify the merge semantics this repo relies on in at least five other configs, and it is a bigger finding than this feature.

**R3 — A new gameplay-context keybinding collides with something.**
*Likelihood: medium. Impact: medium (an unusable popup, or worse, a broken gameplay input).*
Mitigated by needing only **one** such binding (D9), by running `check-input-conflicts.py --warnings` as a phase gate, by the reserved-input table in the `overthrow-ui-patterns` skill (`W/A/S/D` are menu nav; `a`/`b` are `MenuSelect`/`MenuBack`), and by putting the full control set in the modal where `MenuUp/Down/Select/Back` already work. If no genuinely free input exists, the fallback is to drop the escalation key entirely and open the modal from the Overthrow main menu instead — the popup still works, one convenience is lost.

**R4 — Changing invoker signatures ripples into listeners we did not find.**
*Likelihood: low. Impact: medium.*
Only `m_OnPlayerSkill` changes shape, and grep finds exactly one listener (`OVT_CharacterSheetContext.c:33`). Mitigation: Phase 0 is done first and alone; the listener is updated with an explicit wrapper method rather than relying on ScriptInvoker arity coercion; the new Init case asserts every catalogued invoker still exists; and Phase 0's acceptance re-tests the skill, economy and wanted paths by hand.

**R5 — MP delivery is untestable in the automated tiers.**
*Likelihood: certain. Impact: high — this is the exact failure mode (per-player correctness) that broke the starter jobs.*
Mitigated by making the correct behaviour structural rather than checked: `RplRcver.Owner` on a per-player controller entity **cannot** reach another client, unlike the broadcast-and-filter notification path. Verified by the six-step two-client protocol in §8.4, which `tools/launch-server.sh` + two `--profile` clients now makes a three-command setup. Step 21 (server log has no settings access, no errors) is the dedicated-server check.

**R6 — The popup interrupts at a bad moment anyway.**
*Likelihood: medium. Impact: medium (player annoyance is the thing this epic exists to reduce).*
The gate covers Overthrow contexts, the map and base-game menus, but not "in a firefight". Mitigated by the non-modal default never taking input, by the 20-second auto-dismiss, by one-at-a-time queueing, and by the guidance to `tutorial-content` to keep entry volume restrained. If play-testing shows it is still intrusive, the gate gains a "not while taking damage / weapon raised" term — a one-line addition to a pure predicate with an existing test.

**R7 — Adding a 17th UI context or a 4th info display destabilises the existing 16.**
*Likelihood: low. Impact: high (a broad, visible regression).*
Both are additive registrations in arrays that already hold peers. Mitigated by Q5's requirement to re-open, navigate and close **every** existing menu on keyboard and gamepad after Phases 5–6, and by making task 6.6 (stripping the debug `Print`s from the shared `OVT_UIContext.ShowLayout`) explicitly droppable and last.

**R8 — Entry ids drift between this framework and its consumers.**
*Likelihood: medium. Impact: medium (silently re-shown or never-shown tips).*
Mitigated by publishing §5 as a contract, by the manager failing loudly on duplicate or empty ids, by the "immutable, never reused, retire with `m_bEnabled 0`" rule, and by the settings store carrying a schema version so a deliberate reset is possible.

---

## Quality Bar

This is a **framework plus UI** feature. Two of its three failure modes are silent, so the bar is set on the things that do not announce themselves.

### 1. Framework reliability and data integrity

- **Dedup can never double-fire.** Two independent layers (server session set, client permanent store) and a queue that rejects duplicates. The Logic tier asserts all three, and Q1 checks it by hand at 10× the realistic rate.
- **The settings store can never corrupt.** No parallel arrays — the base game had to add self-nuking recovery code because it used them. A version field, an explicit clear-on-mismatch path, mandatory null-guarding after `WriteToInstance`, and an explicit disk flush on every mutation. A store that fails to load must degrade to "show the tip again", never to a crash or a half-loaded set.
- **The server never guesses.** An event with no acting player is routed as a proximity fan-out and documented as a heuristic; a playerId is never fabricated to make an API fit.
- **Every decision is either a pure function with a test or a two-line piece of plumbing.** If a behaviour needs a paragraph of explanation, it belongs in `Scripts/Game/Data/` with a Logic case.
- **A missing controller, a null settings module or an unknown entry id produces silence, not a script error.** Tips are the lowest-stakes system in the mod; they must never be the reason a session breaks.

### 2. UI polish and gamepad usability

- **Every interactive element is reachable with a gamepad**, driven by `MenuUp/Down/Left/Right/Select/Back` in the modal context, and every action shows its input prompt via `SCR_InputButtonComponent`. A console player must never have to guess.
- **Exactly one new gameplay-context binding**, verified collision-free by script, not by eye — and a documented fallback if even one is not available.
- **The non-modal overlay is genuinely non-modal.** Movement, aim and fire keep working. If it can steal an input, it is not done.
- **`OnClose` removes exactly what `OnShow` inserted.** Manager and controller invokers outlive the layout; one leaked subscription per popup would compound over a session.
- **Legible at 1080p and 4K**, and never overlapping the wanted, economy or progress HUD elements it shares the screen with.

### 3. Zero regressions in what already works

- **All 16 existing UI contexts** open, navigate and close on keyboard and gamepad after the 17th is added. This is a checklist item, not an assumption.
- **The four managers touched in Phase 0** behave identically afterwards: character-sheet refresh, buy/sell XP, stability/support modifiers, one wanted notification per escalation.
- **The base-game Field Manual gains a category and loses none.** Six categories, and the normal open path unchanged.
- **`#OVT-IntroHint`, `m_aHintedPlayers` and the five starter jobs are untouched.** Their removal belongs to `first-spawn` and `starter-jobs-retirement`; this feature only makes it possible.
- **The localization master is the only file edited under `Language/`.** The generated per-language exports are the user's to regenerate, and hand-editing has silently corrupted six of them before.
