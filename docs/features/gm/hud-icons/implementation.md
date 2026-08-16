# HUD Icons — Implementation Plan

**Status:** ✅ Complete — user-verified 2026-08-15 (incl. MP/JIP and the `_base`/`_medium_base` tower
variants). ⚠️ DESCOPED same day after user testing: the click-detail panel surface (§4 Phases 2–3's
rendering, goal 2, F-4…F-8/F-10) was **removed** — per-entity info is tooltip-only, base tooltip =
"Resources - Garrison Groups". See `context.md` descope note.
**Epic:** gm (feature 3 of 5 — Phase 1 of the 3-phase epic)
**Started:** 2026-08-15
**Target Completion:** TBD
**Last Updated:** 2026-08-15 19:05 AEST

> All `file:line` citations are load-bearing and were **re-verified during planning**. Overthrow-side
> citations against the working tree at **`b01782c3`** (`feat: gm/overthrow-panel`, tree clean); base-game
> citations against the Reforger 1.8 reference tree at `/mnt/n/Projects/Arma 4/ArmaReforger`. Keep them when
> editing. Where this plan and `requirements.md` disagree, **this plan wins** and the disagreement is
> recorded in §5.

---

## 1. Executive Summary

A Game Master looking at an Overthrow campaign today sees the base game's world: characters, groups, vehicles.
The things the campaign is actually *about* — towns, occupying-faction bases, radio towers — are invisible,
and the things that are visible carry no Overthrow meaning. This feature makes the campaign legible in the
GM view: an icon over every town, base and radio tower, and a detail readout in the Overthrow panel for
whatever the GM clicks — town, base, tower, AI group or player.

**The shape, in one paragraph.** Overthrow rides the vanilla editable-entity system rather than drawing a
parallel icon layer (user decision, §5 D1). Three prefab families gain an Overthrow subclass of
`SCR_EditableSystemComponent`; the vanilla per-frame reprojection loop
(`SCR_EntitiesEditorUIComponent.OnMenuUpdate` :181) then draws, hovers, selects and tooltips them for free.
A client-side widget component subscribes to the vanilla `SELECTED` entity filter
(`SCR_BaseEditableEntityFilter.GetOnChanged()` :220), classifies whatever came back, and writes text into a
mod-owned layout parented into `overthrow-panel`'s `DetailSection` through its four-call contract. Hover
tooltips need **no** config fork at all: every entity type's tooltip already runs a
`SCR_DescriptionTooltipDetail` that calls `entity.GetInfo().SetDescriptionTo(...)`, so a per-instance
`SCR_UIInfo` subclass that computes its description on demand renders live Overthrow state in the vanilla
tooltip. **Nothing in this feature sends or receives a single byte.**

**Five findings from planning that change the work and are worth stating up front:**

1. **`EEditableEntityType.SYSTEM` is the right type, and Conflict already proves it.**
   `Prefabs/Systems/MilitaryBase/ConflictBase_Base.et:36-45` is the exact analogue — a game-mode base object
   rendered as a GM icon — and it uses `SCR_EditableSystemComponent`, `m_EntityType SYSTEM`,
   `m_bAutoRegister ALWAYS`, `m_Flags 2052` (`NON_DELETABLE | HAS_FACTION`) and a custom `Icon`. SYSTEM's slot
   is **48 px** (`EditableEntities.layout` authors no `m_iSize` for it, so the `[Attribute("48")]` default at
   `SCR_EntitiesEditorUIComponent.c:403-404` applies) versus GENERIC's 24, its icon layout
   (`EditableEntity_System.layout`) sets the icon straight from `info.SetIconTo()`
   (`SCR_CustomEditableEntityUIComponent.c:7-14`), and it carries a faction-tinted frame. `ValidateType()`
   (`SCR_EditableEntityComponent.c:1975-1999`) has **no SYSTEM case**, so nothing has to inherit from anything
   in particular — unlike COMMENT, which asserts `IsInherited(SCR_EditableCommentComponent)` at `:1992-1993`.

2. **Tooltips are free — the config fork the brief feared does not have to happen.** Every one of the ten
   `SCR_EntityTooltipDetailType` blocks in `Configs/Editor/Tooltips/EntityTooltips.conf` contains a
   `SCR_DescriptionTooltipDetail` (GENERIC :12, GROUP :92, CHARACTER :124, VEHICLE :220, WAYPOINT :310,
   FACTION :335, SYSTEM :377, TASK :431, SLOT :449, COMMENT :470), and that detail does exactly
   `entity.GetInfo().SetDescriptionTo(m_Text)` (`SCR_DescriptionTooltipDetail.c:9`), gated on
   `entity.GetInfo().HasDescription()` (`:17`). Both go through `GetDescription()`
   (`SCR_UIDescription.c:31`, `:41`, `:58`), which a subclass may override. **Zero conf fork, zero layout
   fork, zero modded class.** §5 D5.

3. **The `SCR_UIInfo` on a prefab is shared by every instance, so per-entity text needs
   `SetInfoInstance()`.** `GetInfo()` falls back to `prefabData.GetInfo()` — the *component class*, one
   object for all towns (`SCR_EditableEntityComponent.c:151-162`). The per-instance override is
   `SetInfoInstance()` (`:182-185`), documented as a **weak** ref, so the component must hold the info in a
   `ref` member. Vanilla does precisely this in `SCR_EditableGroupComponent.c:324-329`. Everything
   per-entity — the town's name on the icon tooltip, its live description, even a village-vs-city icon —
   comes from that one call.

4. > ⚠️ **CORRECTED DURING PHASE 1 (2026-08-15): this finding was wrong.** Overthrow's tower prefabs are
   > **same-GUID deltas** over vanilla `TransmitterTower_01{,_medium,_small}_base.et` (Overthrow `.et.meta`
   > GUIDs = vanilla prefab GUIDs), and each vanilla base carries an `RplComponent` (vanilla `_base.et:129`).
   > The grep below looked at the *grandparent* `Tower_Base.et` only. Towers therefore inherit RplComponent,
   > LOCAL would null the component, and **all five prefabs shipped `m_Flags 2052` — no LOCAL anywhere**.
   > `IsReplicated()→false` is the only movement lock for towers. See `context.md` Phase 1 As Built.
   >
   > Original (wrong) finding kept for the record:
   **The three transmitter-tower prefabs have no `RplComponent`, and that is not optional to get right.**
   `TransmitterTower_01_base.et` and its two siblings delta onto `Prefabs/Structures/Core/Tower_Base.et`,
   which carries only `SignalsManagerComponent` and `SCR_BuildingSoundComponent`. `SCR_EditableEntityComponent`
   hard-fails **both ways** at `:2237-2256`: LOCAL **with** an RplComponent logs an ERROR and nulls the
   component; not-LOCAL **without** one does the same. So towers must carry `EEditableEntityFlag.LOCAL` and
   towns/bases must not (both controllers do carry `RplComponent` — `OVT_TownController.et:9`,
   `OVT_BaseController.et:77`). Adding `RplComponent` to dozens of map-placed static towers instead would be
   a real replication cost for zero benefit. §5 D3.

5. **`IsReplicated()` is the movement lock, and LOCAL gets it for free.**
   `SCR_TransformingEditorComponent` only ever edits entities for which `entity.IsReplicated(id)` is true
   (`:88`, `:98`, `:121`), and `IsReplicated()` returns **false** unconditionally for LOCAL entities
   (`SCR_EditableEntityComponent.c:193-197`). The same predicate gates re-parenting
   (`SCR_EntitiesManagerEditorComponent.c:140-141`) and layer moves (`SCR_LayersEditorManager.c:138`, `:259`).
   Towers are therefore un-draggable by construction; towns and bases get the same protection from a
   three-line `override bool IsReplicated()` on the Overthrow component. Deletion is blocked separately by
   `NON_DELETABLE` (`SCR_EditableEntityComponent.c:851-853`, `SCR_DeleteSelectedContextAction.c:64`).
   **`NON_INTERACTIVE` must never be set** — it kills hover (`SCR_HoverEditableEntityFilter.c:73`) and
   selection, which is the whole feature.

---

## 2. Goals

### Primary

1. **A GM can see the campaign.** Every town, occupying-faction base and radio tower carries an icon in the
   Game Master view, visible from a strategic camera altitude, tinted by controlling faction.
2. **Clicking anything Overthrow-shaped explains it.** Selecting a town, base, radio tower, AI group or
   player fills the Overthrow panel's detail section with that entity's campaign state — support/stability/
   population, resources/garrison/upgrades, sabotage downtime, group origin and reason, player money and level.
3. **Hovering gives the one-line version** before the GM commits to a click, through the vanilla tooltip.
4. **The GM cannot break the campaign by touching an icon.** Town, base and tower controller entities are
   non-deletable and non-movable through the editor, while remaining fully selectable.
5. **Zero new networking.** Detail is composed from (a) already-replicated Overthrow data read locally and
   (b) the gm-state client store. No RPC, no `RplProp`, no wire change (§5 D8).
6. **Conventions gm-map can copy.** Icon assets, the kind enum, the format helpers and the detail row model
   are named and shaped so `gm-map` reuses them rather than reinventing them.

### Secondary

7. **The pure formatting logic is unit-tested** (Logic tier) and the prefab blocks are gate-tested (Init tier),
   because a missing prefab component block is otherwise completely silent.
8. **No custom art.** Icons come from the existing `UI/Imagesets/overthrow_mapicons.imageset` (`town`,
   `village`, `city`, `tower` are already in it) and one vanilla editor texture for bases — matching the
   epic constraint to prefer existing icons over new art.

### Explicit non-goals

- **No actions.** Epic Phase 1 is read-only. No context-menu entries, no popup menus, no buttons — but
  nothing here forecloses them (§5 D2).
- **No change to group icons.** Vanilla military symbols stay exactly as they are; group origin is
  click-detail only (user decision, §5 D7).
- **No group or player tooltips.** Tooltip coverage is towns, bases and towers only (§5 D6).
- **No map work** (`gm-map` owns map icons and panels) and **no waypoint work** (`waypoint-viz`).
- **No help/wiki phase.** One consolidated `help-docs-sync` pass runs after `gm-map` closes Phase 1
  (`epic-overview.md`, Tech Debt / Findings). Do not add one here.
- **No new `EEditableEntityType`.** The enum is closed at 11 values (`EEditableEntityType.c:5-18`).

---

## 3. Architecture Overview

### 3.1 Component hierarchy

```
WORLD SIDE — runs on every machine, renders only inside an editor mode

SCR_EditableSystemComponent                     BASE GAME (SCR_EditableSystemComponent.c)
│   GetFaction() from SCR_FactionAffiliationComponent; GetOnUIRefresh() invoker
└── OVT_GMEditableCampaignComponent             NEW  Scripts/Game/Components/GM/
      [Attribute] OVT_EGMIconKind m_eKind        TOWN | BASE | RADIO_TOWER
      ref OVT_GMCampaignUIInfo m_Info            strong ref - SetInfoInstance() takes a WEAK one
      OnPostInit()   build m_Info, SetInfoInstance(m_Info)
      GetFaction()   town/tower faction from Overthrow data; else super (bases have the vanilla component)
      IsReplicated() -> false                    movement/re-parent/layer lock (§1 fact 5)

OVT_GMCampaignUIInfo : SCR_EditableEntityUIInfo NEW  Scripts/Game/Components/GM/
      holds IEntity m_Owner + OVT_EGMIconKind
      override GetName()          "Lamentin" / "Base 3" / "Radio Tower"      -> icon + tooltip header
      override GetDescription()   live one-or-two-line Overthrow state       -> vanilla tooltip body
      Icon / IconSetName set once from the imageset (village/town/city/tower)

PREFABS (component block added, one per family)
  Prefabs/Controllers/OVT_TownController.et                     kind TOWN,        flags 2052
  Prefabs/Controllers/OVT_BaseController.et                     kind BASE,        flags 2052
  Prefabs/.../TransmitterTower_01{,_medium,_small}_base.et      kind RADIO_TOWER, flags 2060 (+LOCAL)


UI SIDE — exists only while the Game Master editor is in EDIT mode

modded SCR_EditModeEditorUIComponent            EXISTING (overthrow-panel's injection point)
      + after creating GMPanel.layout, CallLater one frame, then create GMIconDetail.layout
        into OVT_GMPanelUIComponent.GetInstance().GetDetailSlot()

UI/Layouts/GM/GMIconDetail.layout                NEW  GUID series {6B09…}
└── root VerticalLayoutWidgetClass
      components { OVT_GMDetailUIComponent "{6B09…}" {} }
      ├── Detail_Title      TextWidget
      ├── Detail_Subtitle   TextWidget
      ├── Detail_Row_0 … Detail_Row_7   (Detail_Label_N + Detail_Value_N)
      └── Detail_Note       TextWidget   ("no Overthrow record", "+3 more upgrades", "waiting for data")

Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c     NEW  : SCR_ScriptedWidgetComponent
      subscribes SELECTED filter + gm-state GetOnSnapshotUpdated/GetOnStateCleared
      classifies selection -> fills rows -> panel.ShowDetail(true/false)

Scripts/Game/UI/GM/OVT_GMIconFormat.c            NEW  PURE statics - world-free, Logic-testable
```

Nothing here is a Manager and nothing is a Controller. There is no new entity, no new prefab, no new
persistence, **no new replication**, and no new `OVT_Global` accessor.

### 3.2 Data flow

```
WORLD LOAD (every machine, including a dedicated server)
  OVT_GMEditableCampaignComponent.OnPostInit
    ├─ super (SCR_EditableSystemComponent caches SCR_FactionAffiliationComponent)
    ├─ build OVT_GMCampaignUIInfo (kind + owner + icon), CopyFrom(GetInfo())
    └─ SetInfoInstance(m_Info)                       SCR_EditableEntityComponent.c:182
  SCR_EditableEntityComponent.EOnInit                :2237-2263
    ├─ LOCAL / RplComponent consistency check        (§1 fact 4 - errors are fatal to the component)
    ├─ m_fMaxDrawDistance squared
    └─ Register()   (m_bAutoRegister ALWAYS - required, world-placed entities are not "spawned")

EDIT MODE ACTIVATES  (authorized GM only - structural, see D4)
  Mode_Edit.layout:47 instantiates EditableEntities.layout
    └─ SCR_EntitiesEditorUIComponent.OnMenuUpdate (:181, per frame)
         reprojects every RENDERED entity's slot, computes hover,
         pushes it to SCR_HoverEditableEntityFilter.SetEntityUnderCursor (:246-247)

  RENDERED membership is decided by SCR_RenderedEditableEntityFilter.IsNear (:102-111):
       DistanceSq(entity, camera) < GetMaxDrawDistanceSq() * m_fCameraDisCoef
       coef = lerp(0.1 .. 1.0) over camera altitude 15 m .. 150 m   (:9-19, defaults unmodified
       by EditorModeEdit.et:109-113)                                 <-- THE DRAW-DISTANCE TRAP, §9 R1

  modded SCR_EditModeEditorUIComponent.HandlerAttachedScripted
    ├─ (existing) CreateWidgets(GMPanel.layout, Mode_Edit root)
    └─ (new) CallLater(0) -> CreateWidgets(GMIconDetail.layout, panel.GetDetailSlot())
         └─ OVT_GMDetailUIComponent.HandlerAttached
              ├─ SCR_BaseEditableEntityFilter.GetInstance(EEditableEntityState.SELECTED).GetOnChanged().Insert(OnSelectionChanged)
              ├─ gm.GetOnSnapshotUpdated().Insert(OnSnapshot)   /  GetOnStateCleared().Insert(OnCleared)
              └─ panel.ShowDetail(false)          nothing selected yet

  GM clicks an icon (or the world geometry under it)
    SCR_SelectionEditorUIComponent.EditorSetSelection (:143) reads FOCUSED
      -> m_SelectedManager.Replace(focused, true) (:158)
        -> SELECTED filter OnChange fires
          -> OVT_GMDetailUIComponent.OnSelectionChanged -> Render()

  GM hovers an icon
    HOVER filter OnChange -> SCR_TooltipManagerEditorUIComponent.OnHover (subscribed :331-333)
      -> SCR_EntityTooltipEditorUIComponent.SetTooltip (:43)
        -> per-type SCR_DescriptionTooltipDetail.InitDetail/UpdateDetail
          -> entity.GetInfo().SetDescriptionTo(text)
            -> OVT_GMCampaignUIInfo.GetDescription()   <-- our live text, no config touched

EDIT MODE DEACTIVATES
  panel widget tree destroyed by the engine -> OVT_GMDetailUIComponent.HandlerDeattached
    unsubscribes all three invokers, nulls its cached widgets
```

### 3.3 Where every detail value comes from

The gm-state contract's rule is absolute: **never ask the seam for data that is already replicated**
(`gm-state/context.md`, "Already replicated — read locally"). The split below is that rule applied.

| Selection | Row | Source | Citation |
|---|---|---|---|
| **Town** | Faction | `OVT_TownData.faction` → `ControllingFactionData()` | `OVT_TownManagerComponent.c:26`, `:51` |
| | Support | `support` / `SupportPercentage()` | `:25`, `:39-44` |
| | Stability | `stability` | `:24` |
| | Population | `population` / `targetPopulation` | `:22-23` |
| | resolve | `OVT_Global.GetTowns().GetNearestTown(pos)` + `GetTownID()` | `:761`, `:377` |
| **Base** | Faction / location | `OVT_BaseData.faction`, `.location` (JIP-streamed) | `OVT_OccupyingFactionManager.c:20-38` |
| | Resources, groups, upgrades | gm-state `m_aBases` record | `gm-state/context.md`, Record arrays |
| | Per-upgrade breakdown | gm-state `m_aBaseUpgrades` (non-empty only) | same |
| | join key | `GetBaseIndex(GetNearestBase(pos))` → `FindBase(idx)` | `OVT_OccupyingFactionManager.c:656`, `:812` |
| **Radio tower** | Faction | `OVT_RadioTowerData.faction` | `OVT_OccupyingFactionManager.c:59` |
| | Sabotage downtime | `disabledRemaining` / `IsDisabled()` | `:64`, `:80-84` |
| | resolve | `GetNearestRadioTower(pos)` | `:672` |
| **Group** | Origin type / index / reason | gm-state `m_aGroups` record | `gm-state/context.md`, Record arrays |
| | join key | selected entity's `RplComponent.Id()` → `FindGroup(id)` | gm-state contract, Join keys |
| | Faction | `SCR_EditableEntityComponent.GetFaction()` | base game |
| **Player** | Money | `OVT_Global.GetEconomy().GetPlayerMoney(persId)` — broadcast | `OVT_EconomyManagerComponent.c:1004-1009`, `:2013`, `:2021-2026` |
| | Level / XP | `OVT_PlayerData.GetLevel()` | `OVT_PlayerData.c:115-118` |
| | resolve | `entity.GetPlayerID()` → `OVT_PlayerData.Get(int playerId)` | `OVT_PlayerData.c:168` |

**Two consequences worth naming.** Base and group detail are the *only* rows that come from the gated seam,
so for an unauthorized player (or before the first snapshot) they are the only rows that read
"waiting for campaign data" — town, tower and player detail render immediately from local state. And
because a swept or stale group simply vanishes from `m_aGroups` (known epic debt, `epic-overview.md`),
**a selected group with no record is a normal case**: render "no Overthrow record", never an error.

### 3.4 Icon and flag table (the exact prefab values)

| | Town | Base | Radio tower |
|---|---|---|---|
| Prefab | `Prefabs/Controllers/OVT_TownController.et` | `Prefabs/Controllers/OVT_BaseController.et` | `TransmitterTower_01{,_medium,_small}_base.et` |
| Has `RplComponent` | yes (`:9`) | yes (`:77`) | **no** (base `Tower_Base.et` has none) |
| `m_EntityType` | SYSTEM | SYSTEM | SYSTEM |
| `m_bAutoRegister` | ALWAYS | ALWAYS | ALWAYS |
| `m_Flags` | **2052** = `NON_DELETABLE` (2048) + `HAS_FACTION` (4) | **2052** | ~~2060~~ **2052 as built** — towers are same-GUID deltas inheriting vanilla `RplComponent`; LOCAL is fatal (§1 fact 4 correction) |
| `m_fMaxDrawDistance` | ~~20000~~ **2000 as built** (2026-08-16 user feedback: full-map icons too busy; 2000 = vanilla GROUP/VEHICLE distance, F-2 revised) | **2000** | **2000** |
| `m_vIconPos` | `0 10 0` | `0 8 0` | `0 40 0` / `0 25 0` / `0 6 0` (per height) |
| Icon | imageset `village` / `town` / `city` by `m_Size` | `{DD5F23CBB1731598}UI/Textures/Editor/EditableEntities/Systems/EditableEntity_System_Base.edds` | imageset `tower` |
| Faction source | `OVT_TownData` (override `GetFaction()`) | `SCR_FactionAffiliationComponent` (`OVT_BaseController.et:32`, vanilla path) | `OVT_RadioTowerData` (override) |

Flag values verified in `EEditableEntityFlag.c:7-26`. `m_Flags 2052` is copied verbatim from
`ConflictBase_Base.et:45`. Imageset is `{C7691945DE01FB28}UI/Imagesets/overthrow_mapicons.imageset` —
`SCR_UIInfo.SetIconTo` (`:102-113`) branches on the `.imageset` extension and calls `LoadImageFromSet` with
`GetIconSetName()`, so `Icon` + `IconSetName` is all that is needed. Available image names include
`town village city tower camp fob warehouse port` (verified in the imageset).

**Draw distance, made concrete.** `GetMaxDrawDistanceSq()` returns the *squared* value
(`SCR_EditableEntityComponent.c:2258-2260` squares `m_fMaxDrawDistance` at init), and the type default is
applied only when the prefab leaves it at zero (`SCR_EditableEntityCore.c:127-130`, `:814-816`) — SYSTEM's
default is **1000 m** (no `m_fMaxDrawDistance` line for SYSTEM in `Configs/Core/EditableEntityCore.conf:52`,
so `SCR_EditableEntityCoreTypeSetting.c:7-8`'s `[Attribute("1000")]` applies). At 1000 m a strategic GM sees
**nothing**: the altitude coefficient shrinks the radius to 316 m at ground level. `20000` yields 6.3 km at
ground level and 20 km above 150 m — the whole of Everon either way.

> ⚠️ **REVISED 2026-08-16 after user testing: 20000 was wrong in the other direction.** Every icon on the
> map rendering at once made the GM view too busy — vanilla icons all fade with camera distance, and these
> should too. **As built: `m_fMaxDrawDistance 2000`** on all five prefabs (= the vanilla GROUP/VEHICLE
> distance; ~632 m radius at ground altitude, 2 km above 150 m). One number per prefab to tune if the feel
> is off. F-2 in §6 revised to match.

### 3.5 What is *not* touched

- No base-game file is forked, copied or same-GUID-overridden. Overthrow still ships **no**
  `Configs/Editor/*` and **no** `UI/layouts/Editor/*` (verified: the mod has only
  `UI/Layouts/{Dialogs,GM,HUD,Map,Menu,Respawn}`).
- `Configs/Editor/Tooltips/EntityTooltips.conf` — untouched (§5 D5).
- `Configs/Editor/EditableEntityUI/EditableEntityUI.conf` — untouched; SYSTEM already maps to
  `EditableEntity_System.layout` at `:46-49`.
- `UI/layouts/Editor/EditableEntities/EditableEntities.layout` — untouched; the 48 px SYSTEM slot is what we
  want and per-type slot sizes cannot be changed without forking it.
- Group icons, waypoint rendering, map screens — other features' scope.
- `Configs/Core/EditableEntityCore.conf` — untouched, so **player/character icons keep their vanilla 75 m
  draw distance** (`:15-16`). A GM must be near a player to see their icon; that is vanilla behaviour and
  changing it is not this feature's business.

---

## 4. Implementation Phases

Effort is **S / M / L** relative to one focused session. "Agent" is the routing hint for `/proceed`.

> **Phase 1 needs an advanced agent** (`component-developer-advanced`). It is the epic's declared riskiest
> base-game integration: five prefab edits across three families, a subclass of a base-game editable-entity
> component with two overrides, and at least four failure modes that produce **no compile error and no log
> line** (wrong flag arithmetic, LOCAL/RplComponent mismatch, `WHEN_SPAWNED` auto-register, unset draw
> distance). Phases 2–4 are ordinary UI/script work.

---

### Phase 0 — Baseline — **S — no agent**

Record in `context.md` before any code:

| Gate | How |
|---|---|
| `tools/compile-check.sh` | exit 0 + file count |
| `git status` / `git rev-parse --short HEAD` | plan citations taken at **`b01782c3`**, tree clean; this tree receives concurrent bugfix commits — **re-check at every phase boundary** |
| Highest allocated bug id | `ls docs/bugs/` — **BUG-174** at planning time |
| Free GUID series | **`{6B09…}` proven free** — 0 hits for the literal `{6B09` across `Prefabs Configs Scripts UI Language` at planning time (`{6B0A…}` also free as a spare). **Re-grep before minting.** Note: a bare `6B09` grep gives false hits inside unrelated GUIDs — always include the brace. |
| Seam + panel citations resolve | `OVT_GMPanelUIComponent.c:232/:247/:261/:278` (`GetInstance` / `GetDetailSlot` / `ShowDetail` / `ClearDetail`); `OVT_GMCampaignState.c:90` (`HasData`); `OVT_ControllerComponent.c:36` (`Get`) |

**Do NOT run `tools/run-tests.sh`** — planning and implementation stop at `compile-check.sh` exit 0; the
orchestrator runs suites after a phase completes (`.claude/test-policy.md`).

**Acceptance:** baseline table filled in `context.md`.

---

### Phase 1 — Editable components, prefab blocks and the Init gate — **M — `component-developer-advanced`**

> Everything unknown lives here. When this lands, a GM opening the editor sees faction-tinted icons over
> every town, base and radio tower, from any altitude, and cannot delete or drag any of them. No detail, no
> tooltips yet.

**Tasks**

1. `Scripts/Game/Components/GM/OVT_GMCampaignUIInfo.c`
   - `enum OVT_EGMIconKind { TOWN, BASE, RADIO_TOWER }` (public — `gm-map` will reuse it).
   - `class OVT_GMCampaignUIInfo : SCR_EditableEntityUIInfo` holding `IEntity m_Owner` (plain ref, the info
     lives on the component that lives on the entity) and the kind.
   - `void Configure(IEntity owner, OVT_EGMIconKind kind)` — sets the owner/kind and writes `Icon` +
     `IconSetName` (both `protected` on `SCR_UIInfo` at `:10`, `:13`, so a subclass may assign them).
   - `override LocalizedString GetName()` — the per-entity header: town name via
     `OVT_Global.GetTowns().GetTownName(townId)` (`OVT_TownManagerComponent.c:858`), `"Base %1"` with the
     index, `"#OVT-GMIcon_RadioTower"`. **Bounds-check before `GetTownName`** — it indexes `m_Towns` unguarded
     (same trap overthrow-panel hit, `overthrow-panel/context.md` Phase 2 findings).
   - `GetDescription()` is authored in **Phase 4**; leave the base behaviour here.
2. `Scripts/Game/Components/GM/OVT_GMEditableCampaignComponent.c`
   - `class OVT_GMEditableCampaignComponentClass : SCR_EditableSystemComponentClass {}` +
     `class OVT_GMEditableCampaignComponent : SCR_EditableSystemComponent` with
     `[ComponentEditorProps(category: "GameScripted/Editor (Editables)")]` on the class descriptor
     (pattern: `SCR_EditableSystemComponent.c:1-4`).
   - `[Attribute]` `OVT_EGMIconKind m_eKind`.
   - `protected ref OVT_GMCampaignUIInfo m_Info;` — **must be a `ref`**: `SetInfoInstance()` stores a weak
     reference (`SCR_EditableEntityComponent.c:180-185`).
   - `override void OnPostInit(IEntity owner)` → `super`, build + `Configure` + `CopyFrom(GetInfo())` +
     `SetInfoInstance(m_Info)`. Vanilla precedent: `SCR_EditableGroupComponent.c:324-329`.
   - `override Faction GetFaction()` — TOWN: `GetNearestTown(pos).ControllingFactionData()`
     (`OVT_TownManagerComponent.c:51-54`); RADIO_TOWER: faction index from
     `GetNearestRadioTower(pos).faction` (`OVT_OccupyingFactionManager.c:59`, `:672`); BASE: `super`
     (the vanilla `SCR_FactionAffiliationComponent` path). **Null-guard every manager** — this runs at world
     init, before managers may exist, and on a dedicated server.
   - `override bool IsReplicated(out RplId replicationID = -1) { return false; }` with a comment naming the
     three call sites it is protecting against (§1 fact 5, §5 D3).
   - Call `GetOnUIRefresh().Invoke()` when the faction changes so the icon retints
     (`SCR_EditableSystemComponent.c:15-18` already does this for the vanilla affiliation path; towns/towers
     need their own trigger or accept a refresh on the next snapshot tick — record which was chosen).
3. **Prefab blocks — five files**, values exactly per §3.4. `.et` files are plain text; edit directly. Mint
   each component GUID from the `{6B09…}` series.
   - ⚠️ **A missing or malformed block is completely silent.** Task 4 is the only gate.
   - ⚠️ Diff the tower prefabs against `Prefabs/Structures/Core/Tower_Base.et` before assuming anything about
     inherited components (project memory: Overthrow prefabs can lack vanilla components).
4. `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_GMIcons.c` — Init tier, cases in §7. The test world
   `Worlds/MP/OVT_Campaign_Test.ent` (via `OVT_Campaign_Test_Layers/default.layer`) contains exactly one
   `OVT_TownController.et` (`:298`), one `OVT_BaseController.et` (`:216`) and one
   `TransmitterTower_01_small_base.et` (`:150`), so all three families are provable; the `_base` and
   `_medium_base` tower variants are **not** in the test world and must be verified by eye in Phase 5 —
   say so in the case's own header comment.
5. 🖐️ **USER SMOKE CHECK — Workbench, 2 minutes.** Open Play mode, open Game Master, and answer only:
   (a) does an icon appear over the town, the base and the tower? (b) is it still there when the camera
   climbs to a strategic altitude and moves a kilometre away? (c) does the delete key refuse them and does
   dragging do nothing? (d) any red script errors on world load? A "no" to (a) is a **finding to report**,
   not a reason to keep building.

**Acceptance**

- `tools/compile-check.sh` → exit **0**, file count +3.
- `git status` shows exactly the five prefabs plus this feature's new files — no base-game path, no `.conf`.
- Init tier grows by the new case count; **each case proven able to fail** by removing one prefab block and
  observing a named `SetFailure` (the method recorded in `context.md`, following gm-state's Phase 2 precedent).
- User confirms icons visible at altitude and distance, non-deletable, non-draggable, no script errors.

---

### Phase 2 — The detail surface and selection plumbing — **M — `ui-developer`**

**Tasks**

1. `UI/Layouts/GM/GMIconDetail.layout` **+ `.meta`**. New GUIDs from `{6B09…}`; slot GUIDs copy freely from
   sibling widgets. All rows **pre-authored and named** — nothing created at runtime (D8 in
   `overthrow-panel/implementation.md`, which this feature follows):
   `Detail_Title`, `Detail_Subtitle`, `Detail_Row_0`…`Detail_Row_7` (each with `Detail_Label_N` and
   `Detail_Value_N`), `Detail_Note`.
   - ⚠️ **A missing `.meta` makes the layout unresolvable** and `compile-check.sh` cannot see it. Keep all
     five console configurations; run the duplicate-GUID script from `overthrow-ui-patterns/layouts.md`.
   - ⚠️ Put spacing on child `LayoutSlot` paddings, not on an `AlignableSlot` — the panel's Phase 4 finding
     was that `AlignableSlot` vertical padding is applied at arrange but **not measured**, spilling content
     past the background (`overthrow-panel/context.md`, Phase 4 Workbench findings). Align enums are
     **0 = left/top, 1 = center, 2 = right/bottom, 3 = stretch** — do not guess.
   - Chrome must read as a continuation of the panel: same label/value colours and row rhythm as
     `GMPanel.layout`'s rows.
2. `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c` extending `SCR_ScriptedWidgetComponent`.
   - `HandlerAttached`: `super`, cache every named widget once, subscribe three invokers, render "nothing
     selected" (i.e. `panel.ShowDetail(false)`).
   - `HandlerDeattached`: remove all three subscriptions, null cached widgets. Hygiene template:
     `SCR_BudgetEditorUIComponent.c` `HandlerAttachedScripted` :207 / `HandlerDeattached` :240.
   - Selection subscription:
     `SCR_BaseEditableEntityFilter.GetInstance(EEditableEntityState.SELECTED)` (`:54-66`) then
     `GetOnChanged().Insert(...)` (`:220`). The handler signature is
     `(EEditableEntityState state, set<SCR_EditableEntityComponent> inserted, set<SCR_EditableEntityComponent> removed)`
     (`SCR_BaseEditableEntityFilter.c:1`). **Do not trust the sets** — re-read the filter's current contents
     and render from that, so multi-select and rapid clicks converge to one truth.
   - If the filter is null at attach (editor components initialise around the UI), retry once via
     `CallLater(..., 0)` and give up quietly after that. Precedent for the deferral idiom:
     `MenuRootSubComponent.c:82`.
   - **Multi-selection rule:** render the *first* selected entity and put the count in `Detail_Note`
     ("3 selected — showing Lamentin"). Do not attempt a merged view.
3. Panel integration — **through the four-call contract only**
   (`overthrow-panel/context.md`, Detail Seam Contract):
   ```
   OVT_GMPanelUIComponent panel = OVT_GMPanelUIComponent.GetInstance();  // may be null - NORMAL
   if (!panel) return;
   Widget slot = panel.GetDetailSlot();                                   // may be null
   ```
   - **Never cache the instance or the slot across frames.** EDIT → PHOTO → EDIT rebuilds everything.
   - **Never call `ClearDetail()`** — it removes every child of the slot, which is *this component's own
     widget tree*. Visibility is `ShowDetail(bool)`; content is `SetText`. Write that rule in the class's
     Doxygen block.
4. `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c` (existing, ~6 new lines): after the existing
   `CreateWidgets(PANEL_LAYOUT, …)`, `CallLater(CreateDetail, 0)`; `CreateDetail` resolves
   `GetInstance()` → `GetDetailSlot()` → `CreateWidgets(DETAIL_LAYOUT, slot)`, null-checking each step and
   logging one `LogLevel.WARNING` if the layout fails to resolve (otherwise silent). The one-frame deferral
   removes any dependency on whether widget components' `HandlerAttached` runs synchronously inside
   `CreateWidgets`.
5. `Scripts/Game/UI/GM/OVT_GMIconFormat.c` — pure statics, **world-free**, used by both the detail rows and
   Phase 4's tooltips: `FormatSupport(int support, int population)`, `FormatPopulation(int, int)`,
   `FormatUpgradeType(string className)` (strip the `OVT_BaseUpgrade` prefix for readability),
   `FormatOriginType(int originType)`, `FormatOrigin(int originType, int originIndex, string reason)`.
   Reuse `OVT_GMPanelFormat.FormatCountdown` for the tower downtime — do not duplicate it.
   - ⚠️ **Logic-tier rule:** the guard on `TestSuites/Logic/` is a directory-wide grep that does not
     distinguish code from prose. Keep `GetGame()`, `OVT_Global` and every world reference out of both this
     file and its test, **including comments** (two features have tripped this).
6. Town, base and radio-tower detail rendering (§3.3 table). Base rows read the gm-state store; when
   `!state.HasData()` show the campaign rows as "waiting for campaign data" while faction/location stay live
   — the same honest split the panel uses (its D6).
7. **Strings.** Every `#OVT-GMIcon_*` item goes into `Language/localization_Overthrow.st` in this phase, with
   `Comment` filled in. **Never touch** `Language/localization_Overthrow.<lang>.conf` — Workbench-generated;
   hand-editing corrupts them silently. Raw keys render until the user regenerates exports; that is expected,
   not a bug.

**Acceptance**

- `tools/compile-check.sh` → exit **0**.
- Duplicate-GUID script over `GMIconDetail.layout` reports none; `.meta` present with all five configurations.
- Logic tier grows; **every new case proven able to fail** (inversion method recorded in `context.md`).
- Grep proves **zero** `Rpc(`, `[RplProp]`, `RplRcver` and zero `ClearDetail(` in this feature's files.
- On a Workbench host: clicking a town / base / tower icon fills the panel's detail section; clicking empty
  ground clears it.

---

### Phase 3 — Group and player detail — **S — `ui-developer`**

**Tasks**

1. **Group.** Selected entity of type `GROUP` → `entity.GetOwner().FindComponent(RplComponent)` → `Id()` →
   `state.FindGroup(id)`. Render origin type, origin index resolved to a human label (base index → base,
   townID → town name, tower id → tower, `-1` → reason string only) and the reason. The join key is
   verified safe: every Overthrow-spawned group prefab deltas onto `Group_Base.et`, which carries
   `RplComponent {524EC5D51F101B32}` (`:103`) — recorded in `gm-state/context.md` Phase 3 findings.
2. **"No Overthrow record" is a first-class state, not an error.** Swept and stale groups vanish from
   `m_aGroups` because of known group-cleanup defects in the `occupying` epic (`epic-overview.md`, Tech Debt).
   Render the row and move on; do not log, do not warn.
3. **Player.** Selected entity of type `CHARACTER` with `GetPlayerID() > 0` → `OVT_PlayerData.Get(playerId)`
   (`OVT_PlayerData.c:168`) → money (`GetPlayerMoney`, `OVT_EconomyManagerComponent.c:1004`) and level
   (`GetLevel()`, `OVT_PlayerData.c:115`). Both are broadcast to every client already
   (`RpcDo_SetPlayerMoney`, `:2013`/`:2021`), so this adds no leak and needs no seam.
4. **A non-player character selects to nothing.** AI characters get no Overthrow rows —
   `panel.ShowDetail(false)`, not an empty box.
5. Re-render the current selection on `GetOnSnapshotUpdated()` so base and group numbers track the ~8 s poll;
   clear the seam-sourced rows on `GetOnStateCleared()`.

**Acceptance**

- `tools/compile-check.sh` → exit **0**.
- Selecting an Overthrow-spawned AI group shows its origin and reason; selecting a player shows money and
  level; selecting a plain AI character shows nothing at all.
- A group with no record renders "no Overthrow record" with no log output.

---

### Phase 4 — Hover tooltips — **S — `component-developer`**

> Small because the pipeline is already built and points at us (§1 fact 2). This phase is one method plus
> some strings.

**Tasks**

1. `override LocalizedString GetDescription()` on `OVT_GMCampaignUIInfo` — one or two short lines per kind,
   built with the Phase 2 format helpers:
   town → support % and stability; base → faction and garrison group count; tower → online, or
   "sabotaged — 4:12". Keep it to **two lines**; the tooltip is 276 px wide
   (`Tooltip_Entity.layout:31`).
2. **`HasDescription()` must be true at tooltip-creation time** or the detail is destroyed before it renders
   (`SCR_DescriptionTooltipDetail.c:17`, and `SCR_EntityTooltipDetail.CreateDetail` :37-46 removes the widget
   when `InitDetail` returns false). `HasDescription()` calls `GetDescription()`
   (`SCR_UIDescription.c:39-41`), so an override of the latter satisfies both — but never return an empty
   string for a live entity.
3. **The tooltip is written once per hover, not ticked.** `SCR_EntityTooltipDetail.NeedUpdate()` returns
   `false` (`:17-20`) and `SCR_DescriptionTooltipDetail` does not override it, so `UpdateDetailType`
   (`SCR_EntityTooltipEditorUIComponent.c:237-244`) skips it. Fresh text on every new hover; frozen for the
   duration of one. That is fine for a tooltip and must not be "fixed" with a timer.
4. `#OVT-GMIcon_Tooltip_*` strings in `.st`, `Comment` filled in.
5. **No group or player tooltips** (§5 D6) — record the reason in `context.md` so the next planner does not
   re-derive it.

**Acceptance**

- `tools/compile-check.sh` → exit **0**.
- `git status` shows **no** change under `Configs/` and no new file under any base-game path.
- Hovering a town / base / tower icon shows the Overthrow line under the entity name; hovering a vanilla
  entity is unchanged.

---

### Phase 5 — Verification gate — **M — user-driven, no agent**

Run §6's Verification Method end to end. This is the only evidence that exists for icon visibility, chrome,
tooltip rendering, editor hygiene and multiplayer behaviour. ⚠️ **Warn the user before launching** — client
launches open a window on their desktop and can orphan.

---

### Phase 6 — `context.md` and epic bookkeeping — **S — `component-developer`**

1. Write `docs/features/gm/hud-icons/context.md`: the shipped flag/draw-distance table as built, the Phase 1
   Workbench findings, the icon-asset conventions **stated explicitly for `gm-map` to reuse**, the Phase 5
   results, and a "the GM sees no icons" triage section (editor mode is unlimited? → prefab block present
   (Init case)? → draw distance? → LOCAL/RplComponent error in the log? → then gm-state's own triage).
2. Update `docs/features/gm/epic-overview.md`: feature 3 status and task count. **Do not add a help-docs
   phase** — the consolidated pass belongs to epic end.
3. If Phase 5 discharged any of gm-state's still-owed log-based MP items, record that in
   `gm-state/context.md` too.

**Acceptance:** all files updated; the epic table refreshed.

---

## 5. Key Technical Decisions

### D1 — Ride the vanilla editable-entity system — **user decision, settled**

Add `SCR_EditableEntityComponent`-family components to Overthrow prefabs rather than building a parallel
icon layer. The whole vanilla pipeline comes with it: reprojection, hover ring, selection, tooltips, and —
critically for epic Phase 2 — a natural context-menu home. Clicks on the icon *and* clicks on the world
geometry underneath resolve through the same state filters, so a GM who clicks the flagpole gets the base.

**Accepted costs, named:** five prefab edits; draw-distance and icon tuning that only a human eye can settle;
towns/bases/towers appearing in the base-game editor's entity list. **Rejected:** a custom `SCR_InfoDisplay`
HUD layer (wrong lifetime, no selection, no tooltip, and Phase 2's actions would have nowhere to live).

### D2 — Read-only now, but do not foreclose Phase 2

Epic Phase 1 is strictly read-only, so this feature adds no context actions. But the mechanism chosen is
exactly the one vanilla context actions consume (`SCR_BaseContextAction` operates on the SELECTED filter and
the hovered entity), so Phase 2 adds an action class and changes nothing here. **No scaffolding is built for
it** — no empty action classes, no "future" hooks. YAGNI.

### D3 — `SYSTEM` type; flags `NON_DELETABLE | HAS_FACTION`, ~~plus `LOCAL` for towers only~~ (**as built: no LOCAL — towers inherit `RplComponent` via same-GUID delta; see §1 fact 4 correction**)

Three constraints collapse to one answer. The type enum is closed at 11 values
(`EEditableEntityType.c:5-18`) so a new type is impossible. COMMENT would give a wide text label but asserts
`IsInherited(SCR_EditableCommentComponent)` (`SCR_EditableEntityComponent.c:1992-1993`) and — worse — vanilla
queries `FindNearestEntity(pos, COMMENT, LOCAL)` for the player's location name in
`SCR_MapLocator.c:84`, `SCR_NotificationPlayerAndLocation.c:35` and `SCR_EditableDescriptorComponent.c:49`,
with `HasEntityFlag(flags)` as a **required-flags** filter (`SCR_EditableEntityCore.c:361-364`). LOCAL-flagged
Overthrow tower comments would therefore start winning that lookup and rename locations in notifications.
GENERIC is safe but 24 px. **SYSTEM is safe, 48 px, needs no particular base class, and Conflict's military
bases already use it** (`ConflictBase_Base.et:36-45`) — including the identical flag value 2052.

`LOCAL` on towers is forced, not chosen: `SCR_EditableEntityComponent.c:2237-2256` fails the component
outright if LOCAL and `RplComponent` disagree, and the towers have no `RplComponent` anywhere in their
inheritance chain. The alternative — adding `RplComponent` to every map-placed transmitter tower — buys
nothing and costs replication and JIP bandwidth for static scenery. LOCAL's only side effects are
skipped budget accounting (`SCR_EditableEntityCore.c:868`), non-serialization (`:222`), exclusion from ping
targets, and `IsReplicated() == false` — the last of which is a *feature* here (§1 fact 5).

`NON_INTERACTIVE` is explicitly **not** set: it would return null from
`SCR_HoverEditableEntityFilter.GetEntityUnderCursor` (`:73`) and remove selection, which is the feature.

### D4 — "Visible only to authorized GMs" costs zero script

The icon layer exists only inside editor modes whose layouts instantiate `EditableEntities.layout` — Edit,
Admin, Strategy and CampaignBuilding (verified by grep; Photo and Screenshot do not). Reaching an unlimited
editor is itself the authorization gate. And the panel that hosts the detail exists **only in EDIT mode**
(`overthrow-panel/implementation.md` §5 D1). So there is no role check, no visibility toggle and no
`IsLimited()` call anywhere in this feature — the same structural argument the panel made, reused.

**On leakage:** the icons carry no Overthrow data of their own. Detail is composed from data that is either
already replicated to every client (town, tower, player, base location/faction) or gated by gm-state's own
server-side authorization (base resources/upgrades, group origins). The gate lives where it already lives;
this feature adds no path around it.

### D5 — Tooltips via `GetDescription()` on a per-instance `SCR_UIInfo` — **no config fork**

The brief flagged the risk of forking `Configs/Editor/Tooltips/EntityTooltips.conf`, a base-game conf
referenced from `UI/layouts/Editor/Tooltips/Tooltip_Entity.layout:26`. It does not have to be forked, because
every entity type's detail list already contains a `SCR_DescriptionTooltipDetail` (ten of them; citations in
§1 fact 2), and that detail resolves its text through `entity.GetInfo().GetDescription()`. Since we must set
a per-instance info anyway for the entity *name* (§1 fact 3), the description costs one additional override.

**Rejected:** a same-GUID delta of `EntityTooltips.conf` (a delta over a nested array of ten containers, each
holding a further array — the shape most likely to behave unlike a hand-reading of the file; project memory
already records one wrong diagnosis caused by assuming delta semantics); forking `Tooltip_Entity.layout` (a
base-game layout, and a merge conflict on every Reforger update); a `modded class
SCR_EntityTooltipEditorUIComponent` overriding `SetTooltip` (possible and clean, but strictly more code and
more coupling than an override we need to write regardless).

### D6 — Tooltips cover towns, bases and towers; groups and players are click-only

Groups already own their info instance: `SCR_EditableGroupComponent` constructs a
`SCR_EditableGroupUIInfo`, holds it in `m_GroupInfo` and installs it via `SetInfoInstance` at `:324-329`, and
that class drives the dynamic group name off the military symbol (`SCR_EditableGroupUIInfo.c:35-54`).
Replacing it would clobber vanilla group naming; and `SCR_EditableGroupUIInfo` carries **no back-reference to
its entity**, so a `modded class` override of `GetDescription()` could not tell which group it was describing.
Players are `SCR_EditableCharacterComponent` on the vanilla character chain, with the same problem and no
Overthrow-authored prefab to hang an info on.

**Decision:** tooltip coverage stops at the three families whose prefabs Overthrow owns. Group and player
detail is click-only, in the panel — which is exactly what `requirements.md:15-16` asks for. The trade-off is
recorded here so it is not re-litigated. This directly discharges the brief's instruction to "prefer the
smallest safe integration and record the trade-off as a decision".

### D7 — Group icons are not modified — **user decision, settled**

`requirements.md:15` reads as if origin text belongs on the group icon. It does not: vanilla group icons are
military symbols rendered by `SCR_GroupEditableEntityUIComponent`, and Overthrow AI groups already inherit a
working chain from `Group_Base.et:80`. The epic constraint is explicit — prefer base-game group icons over
custom art. Origin and reason appear on click, in the panel. **This plan overrides that reading of
`requirements.md`.**

### D8 — Zero new networking; every value is already on the client

Two sources, both local: the gm-state client store (base and group records) and already-replicated Overthrow
managers (towns, radio towers, player money/level, base location/faction). gm-state's grep gate F-7 forbids
the already-replicated values from *its* wire precisely because they travel on the existing ones.

The DoD therefore carries a grep: **no `Rpc(`, no `[RplProp]`, no `RplRcver` in any file this feature adds or
edits**. A future contributor who "just needs one more number" adds it to gm-state's fan, not here.

### D9 — Movement and deletion are blocked by two independent mechanisms

`NON_DELETABLE` stops deletion at both the component (`SCR_EditableEntityComponent.c:851-853`) and the
context action (`SCR_DeleteSelectedContextAction.c:64`). `IsReplicated() → false` stops transformation
(`SCR_TransformingEditorComponent.c:88`, `:98`, `:121`), re-parenting
(`SCR_EntitiesManagerEditorComponent.c:140-141`) and layer moves (`SCR_LayersEditorManager.c:138`, `:259`) —
each of which simply skips the entity and, when nothing is left, cleans up and returns with no error.
`STATIC_POSITION` was considered and rejected: it is only consulted on the *placing* path
(`SCR_PlacingEditorComponent.c:988`, `SCR_PreviewEntityEditorComponent.c:932-948`), never when transforming
an entity that already exists.

The `IsReplicated()` override is a deliberate, documented deviation from the base class's contract. Its blast
radius was measured — a grep of every call site in the base tree returns exactly the five above plus
`SCR_EditableCharacterComponent.c:214` (a different class) and commented-out code inside
`SCR_EditableEntityComponent` itself. If Phase 5 finds an unexpected consequence, deleting the override
restores stock behaviour and costs only the movement lock.

### D10 — Fixed authored rows; `SetText` only; never `ClearDetail()`

Eight label/value rows plus a title, subtitle and note, all authored in the layout and found once at attach.
Selection changes flip row visibility (a state transition, not a per-frame cost) and write text. Nothing is
created or destroyed at runtime. This is `overthrow-panel`'s D8 applied to the surface hanging off its own
detail slot, and it keeps the ownership boundary clean: the panel owns the slot's existence, this feature
owns everything inside it, and **`ClearDetail()` would delete this feature's own widget tree** — so it is
never called.

---

## 6. Definition of Done

Criteria an independent evaluator with no implementation context can verify.

### Functional

- **F-1** With Game Master open, an icon is visible above **every town, every occupying-faction base and
  every radio tower** in the world.
- **F-2** ~~Those icons are still visible with the camera at 300 m altitude and 3 km away~~ **REVISED
  2026-08-16 (user):** icons fade out beyond ~2 km from the camera like vanilla group/vehicle icons —
  the original whole-map visibility (20000 m) made the UI too busy. New criterion: icons within ~2 km
  are visible; the full map is deliberately **not** covered.
- **F-3** Base and tower icons are tinted by their controlling faction, and a town icon retints when the town
  flips.
- **F-4** Clicking a **town** icon fills the Overthrow panel's detail section with its name, controlling
  faction, support, stability and population.
- **F-5** Clicking a **base** icon shows its faction, total resources, garrison group count and a per-upgrade
  breakdown (capped, with "+N more" when it overflows the authored rows).
- **F-6** Clicking a **radio tower** icon shows its faction and either "online" or the sabotage downtime
  remaining as a countdown.
- **F-7** Clicking an Overthrow-spawned **AI group** shows where it came from (base / town / tower / camp /
  FOB / QRF / deployment / job) and why (reason string).
- **F-8** Clicking a **player** shows their money and level.
- **F-9** **Hovering** a town, base or radio tower icon shows a one-or-two-line Overthrow summary in the
  vanilla tooltip, under the entity name.
- **F-10** Deselecting (clicking empty ground) hides the detail section; the panel returns to exactly its
  pre-selection height.

### Quality

- **Q-1 Empty states are honest.** A group with no gm-state record reads "no Overthrow record". Base rows
  before the first snapshot read "waiting for campaign data" while faction/location stay live. A non-player
  AI character shows no detail section at all. No screen of zeros anywhere.
- **Q-2 The GM cannot break the campaign.** Town, base and tower entities cannot be deleted (delete key and
  context menu both refuse) and cannot be dragged, rotated or re-parented. Verified by trying.
- **Q-3 No regression to vanilla editor behaviour.** Vanilla entity icons, tooltips, selection, the content
  browser, the entity list and the budget bar behave as they did before the mod change. Specifically: a
  vanilla character tooltip is unchanged, and the SYSTEMS budget readout is not obviously wrong (SYSTEM-typed
  entities are counted in `EEditableEntityBudget.SYSTEMS` — `SCR_EditableEntityCore.c:427-429`).
- **Q-4 No leaks to non-GM clients.** Nothing new is replicated (Q-6), the icons carry no Overthrow payload,
  and seam-sourced rows are empty for an unauthorized player. Grep proves no new field crosses a wire.
- **Q-5 Open/close hygiene.** Opening and closing the editor **five times** produces no script errors, no
  duplicated detail widgets and no leftover subscriptions — verified with a temporary `Print` pair in
  `HandlerAttached` / `HandlerDeattached` matching one-to-one, and a snapshot commit invoking nothing while
  the editor is shut.
- **Q-6 No new networking.** Grep over every file this feature adds or edits: zero `Rpc(`, zero `[RplProp]`,
  zero `RplRcver`.
- **Q-7 No `ClearDetail()` call.** Grep proves it; calling it would delete this feature's own widget tree.
- **Q-8 Null-safety.** Grep proves every `OVT_GMPanelUIComponent.GetInstance()`, `GetDetailSlot()`,
  `OVT_ControllerComponent<OVT_GMRequestComponent>.Get()`, `OVT_Global.GetTowns()`,
  `OVT_Global.GetEconomy()`, `OVT_Global.GetOccupyingFaction()` and `OVT_Global.GetPlayers()` call site is
  null-checked. All of them are legitimately null on some machine or at some moment.
- **Q-9 No error spam at world load.** The server log contains **no** "missing RplComponent" or "flagged as
  LOCAL, but contains RplComponent" line for any Overthrow prefab
  (`SCR_EditableEntityComponent.c:2243`, `:2252`) — the single most likely silent failure in Phase 1.

### Integration

- **I-1 Zero forks.** `git diff --stat` shows no file under any base-game path; no
  `Configs/Editor/*` and no `UI/layouts/Editor/*` exists in the mod. Exactly one `modded class` file is
  touched, and it is the one `overthrow-panel` already created.
- **I-2 The panel contract is honoured exactly.** Only `GetInstance()`, `GetDetailSlot()` and
  `ShowDetail(bool)` are used; the instance and the slot are re-fetched on every use and never cached;
  `ClearDetail()` is never called.
- **I-3 The gm-state contract is honoured.** The seam is resolved through
  `OVT_ControllerComponent<OVT_GMRequestComponent>.Get()` (never `OVT_Global`), only `GetState()`,
  `GetOnSnapshotUpdated()` and `GetOnStateCleared()` are used, no request is added, and **nothing already
  replicated is asked of the seam** — grep proves no town support/stability/population, player money/level,
  base location/faction or tower field is read from `OVT_GMCampaignState`.
- **I-4 No `OVT_Global` accessor was added** for any controller component (project rule,
  `OVT_ControllerComponent.c:10-14`).
- **I-5 Icon conventions are written down for `gm-map`** — the kind enum, the imageset image names and the
  detail row model are documented in `context.md` in enough detail that `gm-map` can match them without
  reading this feature's code.
- **I-6 No help/wiki content was added** — the consolidated pass belongs to epic end.

### Verification Method

⚠️ **Warn the user before launching anything.** Client launches open a window on their desktop and can orphan.
All user-driven checks are batched into **one** session (Phase 5), except Phase 1's two-minute smoke check.

**Step 1 — Workbench Play mode (host path).** Regenerate the localization exports first, or every label
renders as a raw key and the visual check is worthless.

1. Open Game Master. Confirm **F-1**: icons over the town, the base and the tower.
2. Climb to ~300 m and fly 3 km away. Confirm **F-2**. If icons vanish, `m_fMaxDrawDistance` did not take —
   check that the prefab value is non-zero (`SCR_EditableEntityCore.c:127-130` only fills in the type default
   when it is).
3. Hover each of the three. Confirm **F-9** and that the entity *name* is the specific one (town name, base
   index) and not a shared placeholder — a shared name means `SetInfoInstance` did not happen.
4. Click each of the three, then a group, then a player. Confirm **F-4 … F-8**. Click empty ground; confirm
   **F-10**.
5. Try to delete each icon's entity (delete key **and** right-click context menu) and try to drag one.
   Confirm **Q-2**.
6. Hover and select a few vanilla entities (a character, a vehicle, a waypoint). Confirm **Q-3**.
7. Open and close the editor five times. Confirm **Q-5**.
8. Read the world-load log for the two `SCR_EditableEntityComponent` error strings. Confirm **Q-9**.
9. Check the editor's budget readout before and after. Note anything odd (**Q-3**).

**Step 2 — the tower variants.** The test world only carries `TransmitterTower_01_small_base.et`. Find one
`_base` and one `_medium_base` tower in the campaign world (Eden) and confirm both carry an icon at the right
height. This is the only check that covers those two prefabs.

**Step 3 — multiplayer.** Reuse the path that worked for `overthrow-panel`: the user's own server, or
`tools/launch-server.sh` + `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent
-- -client 127.0.0.1:2001`. Note that a local `--mode dedicated` join wedged twice at Steam backend auth
during the panel's verification — prefer the user's own server or `--mode local`.

1. As a GM client, repeat Step 1's checks 1, 3, 4. Icons and detail must behave identically to the host path.
2. **Base and group rows are the seam test:** they must populate within one poll (~8 s) and refresh as the
   campaign changes. Town, tower and player rows must populate *immediately*, before any snapshot — that
   asymmetry is the proof that §3.3's local/seam split was implemented as designed.
3. **JIP:** with the campaign established, join a second client, make it a GM, and confirm icons and detail
   both work on first open. This is the project's most common regression class and no suite covers it.
4. **Negative path:** a second, non-authorized client. It must not be able to reach an unlimited editor at
   all; if it can, base and group rows must read "waiting for campaign data" and never populate.
5. **Record which auth path was used** (admin login / `-ovtGmDev` / GM role). This was *not* captured during
   the panel's verification and is still owed to the epic.

**Step 4 — grep gates.** Run Q-6, Q-7, Q-8, I-1, I-3 and paste the output into `context.md`. They are cheap
and they are the only check on properties no test can see.

---

## 7. Testing Strategy

**Say plainly what the suites can see: almost none of this.** Coverage in this project is a spine, not a
surface — **JIP/multiplayer, UI, performance and save/reload are uncovered**. This feature is a widget tree
inside the Game Master editor plus five prefab attribute blocks. No suite can open the editor.

### Init tier — `OVT_TEST_Init_GMIcons.c` (Phase 1) — **the important one**

A missing or malformed prefab component block produces no compile error, no runtime error and no log line.
This case is the only automated gate, and gm-state's `OVT_TEST_Init_GMRequestSeam` is the precedent for the
shape (it proved a prefab block, and was proven able to fail by removing it).

| Case | Asserts | Can-fail method |
|---|---|---|
| Town controller carries the component | `towns.m_TownControllers[0]` resolves; the entity carries `OVT_GMEditableCampaignComponent`; its `GetEntityType()` is `SYSTEM`; `HasEntityFlag(NON_DELETABLE)` | remove the block from `OVT_TownController.et` |
| Base controller carries the component | same via `GetOccupyingFaction().m_Bases[0].entId` | remove the block from `OVT_BaseController.et` |
| Every tower controller carries it, and there is at least one | world query mirroring `FilterTransmitterTowerEntities` (`OVT_OccupyingFactionManager.c:1069-1075`); **count ≥ 1** so the case cannot pass vacuously; every hit carries the component **and** `HasEntityFlag(LOCAL)` | remove the block from `TransmitterTower_01_small_base.et` |
| No editable component reports a replication error | for each of the above, `HasEntityFlag(LOCAL) == (FindComponent(RplComponent) == null)` — the exact invariant `SCR_EditableEntityComponent.c:2237-2256` enforces with an ERROR print | flip the LOCAL flag on any one prefab |

Test-world facts (verified): `Worlds/MP/OVT_Campaign_Test_Layers/default.layer` contains
`OVT_TownController.et` (`:298`), `OVT_BaseController.et` (`:216`) and
`TransmitterTower_01_small_base.et` (`:150`). The `_base` and `_medium_base` tower variants are **not** in the
test world — state that in the case header and cover them in Phase 5 Step 2.

### Logic tier — `OVT_TEST_Logic_GMIconFormat.c` (Phase 2)

World-free and pure — the entirety of the automatable formatting surface.

| Case | Asserts |
|---|---|
| Support percentage | `(248, 400)` → `"62%"`; **`(0, 0)` → `"0%"`, never a divide-by-zero** (mirrors `OVT_TownData.SupportPercentage()`'s own zero guard at `:41-42`) |
| Support at the extremes | `(0, 400)` → `"0%"`; `(400, 400)` → `"100%"` |
| Population | `(380, 400)` → `"380 / 400"` |
| Upgrade type readability | `"OVT_BaseUpgradeSniperPosition"` → `"Sniper Position"`; an unrecognised class name passes through unchanged rather than becoming empty |
| Origin, indexed | base-origin type + index `3` → a string containing both the type name and `3` |
| Origin, unindexed | index `-1` → the reason string alone, with no stray `-1` |

**Prove each case can fail** before shipping — invert the expectation, confirm the failure is a *named*
`SetFailure` and not a silent pass, revert, and record the method in `context.md`.

⚠️ Keep `GetGame()`, `OVT_Global` and every world reference out of both `OVT_GMIconFormat.c` and the test
file, **including comments** — the Logic-tier guard is a directory-wide grep that does not read prose.

### No Campaign or Persistence cases

This feature persists nothing and starts nothing. The Fast and All groups run unchanged as a regression net,
**by the orchestrator, after a phase completes — never by a planning or implementation agent**
(`.claude/test-policy.md`).

### What only a human can verify

Icon visibility and legibility at altitude, faction tint, tooltip rendering and wording, detail-row layout
and overflow, editor open/close hygiene, delete/drag refusal, absence of regressions to vanilla editor
behaviour, and every multiplayer and JIP property. All of it is §6's Verification Method.
`tools/compile-check.sh` compiles EnforceScript and **does not parse layouts, `.meta` files, `.et` prefabs or
the string table** — a malformed layout is a missing panel at runtime, a missing `.meta` is an unresolvable
resource, and a bad prefab block is silence.

---

## 8. Dependencies

### Internal — all built, all read-only

| System | What is used | Where |
|---|---|---|
| overthrow-panel detail seam | `GetInstance()`, `GetDetailSlot()`, `ShowDetail(bool)` | `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c:232`, `:247`, `:261` |
| overthrow-panel injection point | the existing `modded class` | `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c` |
| gm-state seam | `GetState()`, `GetOnSnapshotUpdated()`, `GetOnStateCleared()` | `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` |
| gm-state store | `HasData()`, `FindBase(int)`, `FindGroup(RplId)`, `m_aBases`, `m_aBaseUpgrades`, `m_aGroups` | `Scripts/Game/GameMode/GM/OVT_GMCampaignState.c:90` |
| Controller accessor | `OVT_ControllerComponent<T>.Get()` | `Scripts/Game/Components/Controller/OVT_ControllerComponent.c:36` |
| Towns | `GetNearestTown`, `GetTownID`, `GetTownName`, `OVT_TownData` | `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c:761`, `:377`, `:858`, `:19-59` |
| Occupying faction | `m_Bases`, `GetNearestBase`, `GetBaseIndex`, `GetNearestRadioTower`, `OVT_RadioTowerData` | `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c:149`, `:656`, `:812`, `:672`, `:54-84` |
| Economy | `GetPlayerMoney(string)` | `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c:1004` |
| Players | `OVT_PlayerData.Get(int)`, `GetLevel()` | `Scripts/Game/Data/OVT_PlayerData.c:168`, `:115` |
| Format helper | `OVT_GMPanelFormat.FormatCountdown` (reused for tower downtime) | `Scripts/Game/UI/GM/OVT_GMPanelFormat.c` |
| Icons | `UI/Imagesets/overthrow_mapicons.imageset` (`{C7691945DE01FB28}`) — images `town`, `village`, `city`, `tower` | existing asset; usage precedent `OVT_WantedInfo.c:230` |

### Base game — extended, never forked

`SCR_EditableSystemComponent`, `SCR_EditableEntityUIInfo`, `SCR_UIInfo` / `SCR_UIDescription`,
`SCR_ScriptedWidgetComponent`, the `SELECTED` entity filter, the tooltip description pipeline,
`EditableEntity_System.layout`, and `EditableEntity_System_Base.edds`.

**Two durable couplings this feature creates**, both cheap to detect and cheap to fix: the flag *values* in
`EEditableEntityFlag` (a renumbering in a future Reforger release would change behaviour silently — the Init
case's LOCAL/RplComponent invariant is the tripwire), and the presence of a `SCR_DescriptionTooltipDetail` in
the SYSTEM tooltip type (its removal would silently drop tooltips; the icons and click-detail would be
unaffected).

### Files modified outside this feature's own directories

Five prefabs (§3.4), `Language/localization_Overthrow.st`,
`Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c`, and — at Phase 6 —
`docs/features/gm/epic-overview.md`. **No** base-game file, no `.conf`, no persistence registration.

### Blocks / blocked by

- **Blocked by:** `gm-state` (built) and `overthrow-panel` (built, user-verified) — both at `b01782c3`.
- **Blocks:** `gm-map`, which reuses this feature's icon assets and detail conventions.
- **Parallel-safe with:** `waypoint-viz`. No shared file and no shared GUID series — `waypoint-viz` must pick
  a series other than `{6B09…}`.

---

## 9. Risks & Mitigation

**R1 — Draw-distance culling makes the icons invisible from exactly the camera position a GM uses.**
This is the single most likely surprise and it fails *silently*: `IsNear` culls on
`DistanceSq < GetMaxDrawDistanceSq() * m_fCameraDisCoef`
(`SCR_RenderedEditableEntityFilter.c:102-111`), and the coefficient shrinks to **0.1** below 15 m altitude
(`:9-19`). With SYSTEM's 1000 m default that is a 316 m radius — a GM at strategic altitude sees an empty map
and concludes the feature does not work.
**Mitigation:** `m_fMaxDrawDistance 20000` authored on all five prefabs (§3.4), yielding 6.3 km at ground
level and 20 km above 150 m. Phase 5 Step 1 check 2 is the specific test, and it is written to distinguish
"the value did not take" from "the value is too small". Note the value is squared at init
(`SCR_EditableEntityComponent.c:2258-2260`) and the type default only fills in when the prefab leaves it at
zero (`SCR_EditableEntityCore.c:127-130`) — so *any* non-zero prefab value survives.

**R2 — A prefab block is wrong and nothing says so.**
Wrong flag arithmetic, `WHEN_SPAWNED` instead of `ALWAYS` (which means map-placed entities are never
registered — `EEditableEntityRegister.c:12-14`), a LOCAL/RplComponent mismatch that nulls the component
(`SCR_EditableEntityComponent.c:2243`, `:2252`), or a mistyped GUID. None of these produce a compile error and
only the last two produce a log line.
**Mitigation:** the Init case is the gate and its four claims map one-to-one onto these failure modes; the
LOCAL/RplComponent invariant is asserted directly. Q-9 makes reading the world-load log an explicit DoD item.
Phase 1 is routed to an **advanced** agent for exactly this reason.

**R3 — `EEditableEntityType` is closed, so the type choice is permanent-ish and shapes the visuals.**
Eleven values, no extension point (`EEditableEntityType.c:5-18`), and the per-type icon slot size is authored
in a base-game layout (`EditableEntities.layout:14-46`) that this feature will not fork. SYSTEM gives 48 px;
if that reads too small at strategic altitude there is no in-scope lever.
**Mitigation:** SYSTEM is the largest type available without semantic abuse and matches Conflict's own
precedent. If Phase 5 finds 48 px illegible, the honest options are (a) accept it, (b) a bolder/higher-contrast
icon in the imageset, (c) escalate to a base-game layout fork as a *separate* decision with its own
merge-cost discussion. **Do not silently fork it.**

**R4 — The tooltip route depends on a base-game config staying the way it is.**
The whole no-fork tooltip design rests on the SYSTEM detail type containing a `SCR_DescriptionTooltipDetail`
(`EntityTooltips.conf:377`). A future Reforger release could drop it.
**Mitigation:** the failure mode is graceful — tooltips stop, icons and click-detail are untouched. §8 lists
it as a known coupling, and the fallback (a `modded class SCR_EntityTooltipEditorUIComponent.SetTooltip`
override, §5 D5's rejected option) is recorded so a future maintainer does not have to rediscover it.

**R5 — A GM interacts destructively with a controller entity anyway.**
Two mechanisms guard this (D9), but one of them — the `IsReplicated()` override — deliberately lies to the
base class, and the *other* editor paths that read it (re-parenting, layers) are also disabled as a
side effect. There may be a vanilla path not found by grep.
**Mitigation:** Q-2 tests deletion and dragging explicitly; the override is three lines and removing it
restores stock behaviour at the cost of the movement lock only. If Phase 5 finds a genuine consequence,
**record it and drop the override** rather than layering more special-casing.

**R6 — Layout and `.meta` problems ship silently.**
`compile-check.sh` parses none of it. A missing `.meta` makes the layout unresolvable; a duplicate widget GUID
produces undefined behaviour; `AlignableSlot` vertical padding is not measured and spills content past a
background (a real defect found in the panel's own Workbench round-trip).
**Mitigation:** `.meta` with all five console configurations and a clean duplicate-GUID run are Phase 2
acceptance items; the measure trap is called out inline in Phase 2 task 1; Phase 5 Step 1 is the only real
gate and it is scheduled, not hoped for.

**R7 — Detail overflow.** A base with a dozen non-empty upgrades has more to say than eight rows hold, and a
localized string can be 40% longer than English.
**Mitigation:** eight authored rows plus a "+N more" note; per-upgrade rows are the first thing truncated;
suppression/overflow copy is short. Judge it against a *real* populated campaign in Phase 5, not a fresh one.

**R8 — Towns and bases now appear in base-game editor entity lists.**
An accepted cost of D1, but it will surprise someone. They are not in the content browser (`PLACEABLE` is not
set) and they carry no budget cost (the info's budget arrays are empty), but they are SYSTEM-typed and
therefore counted in `EEditableEntityBudget.SYSTEMS` (`SCR_EditableEntityCore.c:427-429`).
**Mitigation:** Q-3 makes the budget readout an explicit observation item in Phase 5, and the cost is recorded
here and in `context.md` so it is not later reported as a bug.

**R9 — ~50 always-visible icons cost frame time in the GM view.**
`OnMenuUpdate` reprojects every RENDERED entity every frame (`SCR_EntitiesEditorUIComponent.c:181`), and R1's
mitigation deliberately keeps all Overthrow icons permanently RENDERED.
**Mitigation:** ~50 extra slots against a system that routinely reprojects hundreds of character icons.
Phase 5 Step 1 includes a subjective frame-rate observation with the editor open over a populated area. If it
is a real cost, the lever is a smaller `m_fMaxDrawDistance` per family (towers first — they are the most
numerous and the least strategically important).

**R10 — A parallel session changes the prefabs, the seam or the panel this feature builds on.**
This tree receives concurrent bugfix commits; every Overthrow-side line number here is a snapshot of
`b01782c3`.
**Mitigation:** re-check `git status` and re-confirm the cited lines at every phase boundary (a Phase 0
acceptance item, repeated). The cited *method and file names* are the durable anchor; line numbers are a
convenience. `waypoint-viz` may be building in parallel — do not collide on file names or the `{6B09…}`
series.

---

*Sibling features: `overthrow-panel` (hosts this feature's detail content), `waypoint-viz` (parallel),
`gm-map` (reuses these icon and detail conventions). Data spine: `docs/features/gm/gm-state/`.
Epic: `docs/features/gm/epic-overview.md`.*
