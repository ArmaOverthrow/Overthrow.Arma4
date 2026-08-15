# Recruit UX — Implementation Plan

**Status:** Phases 1-8 Ready for Review · **Phase 9 planned (post-ship extension)**
**Started:** 2026-08-14
**Completed (build):** 2026-08-14 (Phases 1-8)
**Last Updated:** 2026-08-14 — Phase 9 planned

---

## 1. Executive Summary

Recruits today are all-or-nothing: every one you own follows you in your slave group, forever, and the only things the roster screen tells you are name, level and distance. This feature adds the missing half of squad management.

**Inactive recruits** (the headline) let a player park a recruit where they stand — out of the group, still owned, still counted against the 16-recruit cap, still persisted and respawned — holding position with a defend waypoint. Nearby inactive recruits of the same owner cluster into one shared AI group, so a garrison of five does not become five one-man formations. Deactivation and reactivation are driven from a held character action on the body and from a button in the recruits screen.

Around that sit three supporting changes: a **map marker layer** showing every one of your recruits with an armed/ammo status tag, a **richer roster screen** split into Active and Inactive sections with the same status iconography and an `X / 16` capacity header, and a **loadout swap** action that exchanges the player's entire kit with a recruit's by moving the real item entities — nothing spawned, nothing deleted.

Two code-health fixes ride along in the first phase: BUG-107 (recruit XP gated on hardcoded `"US"`/`"USSR"` faction keys) and the `FindRecruitEntity` mid-iteration map removal.

---

## 2. Goals

### Primary

- **G1** A recruit can be made inactive and active again, from the body and from the roster, with the server as the only authority on the state.
- **G2** An inactive recruit holds position and defends it, in a group Overthrow owns from creation to destruction — no orphaned slave groups, no stranded AI, no leaked waypoints.
- **G3** Inactive state survives replication (JIP + live), persistence (serializer v3, backward-compatible with v1/v2 payloads), owner disconnect/reconnect, and quit/continue — landing the recruit back in an inactive group, not in the owner's squad.
- **G4** A map layer shows all of the player's own recruits — active at full opacity, inactive dimmed — with a status tag for unarmed / armed-with-ammo / armed-out-of-ammo, filterable from the existing layer panel with per-profile persistence.
- **G5** The roster screen shows Active and Inactive sections, per-recruit armed/ammo/wounded status, and `X / 16` capacity, and stays fully navigable on a gamepad.
- **G6** A held action on an active recruit swaps the player's whole loadout with theirs, moving real entities, with zero possibility of duplication or loss by construction.

### Secondary

- **G7** BUG-107 fixed: XP is awarded against the configured occupying faction.
- **G8** `FindRecruitEntity` no longer mutates `m_mEntityToRecruit` mid-iteration.
- **G9** New pure decision logic (clustering selection, status derivation, transferable filtering) lands in world-free classes the Logic tier can pin.

### Non-goals (explicitly out of scope)

- Reworking show-on-map (it keeps hijacking the job waypoint slot; the new marker layer complements it, it does not replace it).
- Opening a recruit's inventory from the roster screen.
- Any fix to the loadout engine's BUG-042 / BUG-043 / BUG-044 — this feature does **not** build on the capture/apply engine at all.
- Migrating the legacy recruit RPCs (`RpcAsk_RecruitCivilian`, `RpcAsk_RecruitFromTent`, `RpcAsk_DismissRecruit`, `RpcAsk_RenameRecruit`) off `OVT_PlayerCommsComponent`. See §10 R11 for one discovery worth filing.

---

## 3. Architecture Overview

### 3.1 What is new and what changes

| File | New/Changed | Purpose |
|---|---|---|
| `Scripts/Game/Data/OVT_RecruitData.c` | changed | `m_bInactive` field |
| `Scripts/Game/Data/OVT_RecruitStatus.c` | **new** | Pure status-flag derivation + tag-icon selection (Logic tier) |
| `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` | changed | inactive state API, group fork, status sweep, JIP field, quick-win fixes |
| `Scripts/Game/GameMode/Managers/OVT_RecruitInactiveGrouping.c` | **new** | Pure clustering-candidate selection (Logic tier) |
| `Scripts/Game/GameMode/Managers/OVT_GroupRecruitTransfer.c` | changed | inactive recruits are not transferable |
| `Scripts/Game/Components/OVT_InactiveRecruitGroupComponent.c` | **new** | Marks a group as an inactive-recruit group; owns its waypoint's lifetime |
| `Scripts/Game/Components/Controller/OVT_RecruitCommandComponent.c` | **new** | Client→server: set inactive/active, swap loadout. Server→owner: status push |
| `Scripts/Game/GameMode/Managers/OVT_LoadoutSwap.c` | **new** | Server-side direct entity-transfer swap routine |
| `Scripts/Game/Persistence/Serializers/Components/OVT_RecruitManagerSerializer.c` | changed | v3 = `inactive` appended |
| `Scripts/Game/UserActions/OVT_BaseRecruitUserAction.c` | **new** | Base: "this is a recruit *I* own", mirror of `OVT_BaseCivilianUserAction` |
| `Scripts/Game/UserActions/OVT_SetRecruitInactiveAction.c` | **new** | Hold action, shown on active owned recruits |
| `Scripts/Game/UserActions/OVT_SetRecruitActiveAction.c` | **new** | Hold action, shown on inactive owned recruits |
| `Scripts/Game/UserActions/OVT_SwapLoadoutWithRecruitAction.c` | **new** | Hold action, shown on active owned recruits |
| `Scripts/Game/UI/Context/OVT_RecruitsContext.c` | changed | Sections, capacity header, activate/deactivate button, status |
| `Scripts/Game/UI/Components/OVT_RecruitListEntryHandler.c` | changed | Status icons, inactive styling, `GetRecruitData()` |
| `Scripts/Game/UI/Map/Visualization/OVT_MapRecruitLocation.c` | **new** | The marker layer |
| `Scripts/Game/UI/Map/OVT_MapLayersUI.c` | changed | Second hand-built filter row |
| `Scripts/Game/Global/OVT_Global.c` | changed | `GetRecruitCommands()` accessor |
| `Scripts/Game/Components/Controller/OVT_TravelRequestComponent.c` | changed | Fast travel does not take inactive recruits |
| `Prefabs/Groups/INDFOR/OVT_Group_InactiveRecruits.et` | **new** | Empty, non-playable, delete-when-empty group |
| `Prefabs/GameMode/OVT_OverthrowController.et` | changed | + `OVT_RecruitCommandComponent` |
| `Prefabs/GameMode/OVT_OverthrowGameMode.et` | changed | + inactive-group prefab attribute on the recruit manager |
| `Prefabs/Characters/Factions/CIV/Character_CIV_Recruit.et` | changed | + three recruit actions |
| `Prefabs/Characters/Factions/CIV/Character_CIV.et` | changed | + three recruit actions |
| `Prefabs/Characters/Factions/INDFOR/FIA/Character_CIV.et` | changed | + three recruit actions |
| `UI/Layouts/Map/MapRecruitLocation.layout` | **new** | Marker: base image + tag image |
| `UI/Layouts/Menu/RecruitsMenu.layout` | changed | Two list sections, capacity text, fourth button |
| `UI/Layouts/Menu/RecruitsMenu/RecruitListItem.layout` | changed | Status icon row |
| `UI/Imagesets/overthrow_mapicons.imageset` | changed | 4 placeholder entries |
| `Configs/Map/MapOverthrow.conf` | changed | Register the layer |
| `Configs/System/chimeraInputCommon.conf` | changed | One new action + context ref |
| `Language/localization_Overthrow.st` | changed | New string items (**master only** — see D14) |

**Verified corrections to assumptions carried into this plan:**

- The only *live* recruit prefab is `Prefabs/Characters/Factions/CIV/Character_CIV_Recruit.et` (`OVT_OverthrowGameMode.et:161`, `m_sRecruitPrefab`). `Prefabs/Characters/Factions/INDFOR/FIA/Character_CIV_Recruit.et` and `Character_FIA_Recruit.et` have **zero references anywhere** — do not touch them. Directly-recruited civilians keep `Character_CIV.et`, which exists in **two** copies (`Factions/CIV/` and `Factions/INDFOR/FIA/`), both carrying the same `ActionsManagerComponent "{520EA1D2F659CE02}"` override. Three prefabs need the new actions.
- The controller components resolve the sender with **`ResolveOwningPlayerId()`** (five identical copies, e.g. `OVT_TowerSabotageComponent.c:59-80`), **not** `ResolveSenderPlayerId` — that one is the legacy `OVT_PlayerCommsComponent.c:1058` helper and only works because that component sits on the player's character.
- `OVT_MapLayerPrefsStore` exposes **`SetHidden(key, hidden)`**, not `SetVisible`; absent-from-the-set means visible.
- `Duration` is a native `BaseUserAction` config field set **per prefab instance**, not a script member (`BaseUserAction.c:68` exposes only `GetActionDuration()`).

### 3.2 The inactive flag, in four formats

`m_bInactive` on `OVT_RecruitData` is the single source of truth. It must be carried in exactly four places, and each has a different rule:

1. **Record** — `bool m_bInactive = false;` on `OVT_RecruitData`. Server-authoritative.
2. **JIP** — appended as the **last** write in the per-recruit block of `RplSave` (`OVT_RecruitManagerComponent.c:2038-2090`) and the last read in `RplLoad` (:2096-2174). Both sides ship together, so no version negotiation is needed; the positional order is the format.
3. **Live replication** — a **new, dedicated broadcast RPC**, *not* an extra parameter on `RpcDo_RecruitUpdated`. That handler already carries 8 parameters and the code notes 8 as the limit (:2317). Adding a 9th is a wire-level failure the compile check cannot see (BUG-090 class). New:
   ```
   void BroadcastRecruitActiveState(OVT_RecruitData recruit)      // server
   [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
   protected void RpcDo_RecruitActiveStateChanged(string recruitId, bool inactive)
   ```
   Guarded with `if (RplSession.Mode() != RplMode.Client) return;` like its siblings at :2374 and :2417.
4. **Persistence** — `bool inactive` appended as the **last** member of `OVT_PersistedRecruit`, version bumped to 3. Deserialize clears it for `version < 3` exactly the way `ClearBodyPersistenceIds` clears the v2 field: whatever the reader left in an unwritten field is not ours.

### 3.3 The inactive group — ownership and lifetime

This is the riskiest part of the feature, because the recruits epic already documents what goes wrong when group lifetime is not owned: an emptied master group is destroyed by vanilla next frame and orphaned slave groups strand AI (`OVT_RecruitManagerComponent.c:1892-1920`).

**The group entity IS the record.** There is no server-side registry of inactive groups — a registry would be a second truth that can desync from the world and can hold dangling pointers to deleted entities. Instead the group carries a marker component and everything is derived from the world:

```
OVT_Group_InactiveRecruits.et  (SCR_AIGroup)
├── m_aUnitPrefabSlots     EMPTY      — spawns nobody
├── m_bPlayable            0          — SCR_AIGroup.c:3255 only calls RegisterGroup when playable,
│                                       so this group never appears in anyone's group list
├── m_bDeleteWhenEmpty     1          — OnEmpty() -> CallLater(DeleteEntityAndChildren, 1)
│                                       (SCR_AIGroup.c:2442-2455) is what destroys it
└── OVT_InactiveRecruitGroupComponent — m_sOwnerPersistentId (server-only) + the defend waypoint,
                                        deleted in OnDelete so no waypoint outlives its group
```

**Lifecycle policy is deliberately left at the default `Manual`.** `SCR_AIGroup.EOnFrame` (:313-318) only runs `LifecycleTick` for `ProximityDriven` groups, so a Manual group is never proximity-despawned. **`SetLifecyclePolicy` must never be called on this group** — a ProximityDriven inactive group would delete the recruit bodies at 800 m and destroy the feature.

**`OVT_EntitySpawningAPI.CleanupGroup` / `CleanupEntity` must never be called on this group either** — it deletes every member soldier (`OVT_EntitySpawningAPI.c:379-400`).

**State machine**

```
                 ┌──────────── ACTIVE ────────────┐
                 │ agent in owner's SLAVE group   │
                 └───┬────────────────────────▲───┘
    Deactivate       │                        │      Reactivate
 (action / roster)   │                        │  (action / roster)
                     ▼                        │
  1. leave slave group (single-recruit exit,  │  1. leave inactive group
     the RemoveRecruitsFromGroup sequence)    │     (RemoveAgentFromControlledEntity)
  2. find cluster host within 50 m:           │  2. if that group is now empty, vanilla's
     an owned INACTIVE recruit whose parent   │     m_bDeleteWhenEmpty destroys it — we do
     group carries the marker component       │     NOT delete it ourselves (double delete)
  3. host found  -> AddAIEntityToGroup        │  3. AddRecruitToPlayerGroup(owner, entity)
     no host     -> spawn group prefab at the │  4. m_bInactive = false; broadcast
                    body, set faction, add    │
                    SpawnDefendWaypoint(pos), │
                    UntrackTransient(group),  │
                    then add the agent        │
  4. verify GetAgentsCount() >= 1, else       │
     delete the group we just made            │
  5. m_bInactive = true; broadcast            │
                     │                        │
                 ┌───▼──────── INACTIVE ──────┴───┐
                 │ agent in an OVT-owned AI group │
                 └────────────────────────────────┘
```

**Terminal transitions, and who cleans up:**

| Event | What happens to the recruit | What happens to the group |
|---|---|---|
| Inactive recruit **dies** | `OnCharacterKilled` drops the record as today | Agent removed by the engine; vanilla `OnEmpty` deletes the group when it was the last |
| Inactive recruit **dismissed** | `RemoveRecruit` as today | same |
| Owner **disconnects** | `ReserveRecruitBody` hides the body and calls `DeactivateAI()` | The group may empty and self-delete. **This is fine and expected** — the group is derived state, rebuilt on return |
| Owner **returns** / **save loaded** | `RespawnPlayerRecruits` → body attached | The clustering routine runs again and rebuilds groups from scratch |
| Group somehow ends up empty but alive | — | Only possible on the create-then-fail path, which step 4 above closes explicitly |

**The respawn fork is the load-bearing change.** Both places that put a body under command today call `AddRecruitToPlayerGroup` — `AttachRecruitBody` (:1516) and the already-in-world branch of `RespawnPlayerRecruits` (:1036). Both are replaced by one method:

```
protected void PlaceRecruitInWorld(string playerPersistentId, notnull OVT_RecruitData recruit, notnull IEntity recruitEntity)
{
    if (recruit.m_bInactive)  PlaceRecruitInInactiveGroup(recruit, recruitEntity);
    else                      AddRecruitToPlayerGroup(playerPersistentId, recruitEntity);
}
```

Bodies come back at their stored positions, so recruits that were within the cluster radius of each other when deactivated re-cluster into approximately the same groups. **Approximately, not identically** — the reconstructed grouping depends on the order bodies arrive (`RequestSpawn` is async). That is acceptable: the group has no identity a player can observe. It is documented here so nobody later treats it as a bug.

**Clustering selection is pure and testable.** `OVT_RecruitInactiveGrouping` (mirroring `OVT_GroupRecruitTransfer`) holds the world-free half:

```
static array<string> SelectClusterCandidates(notnull array<ref OVT_RecruitData> ownedRecruits,
                                             string excludeRecruitId, vector origin, float radius)
```
— records that are inactive, online, not the one being placed, and within `radius` of `origin`, in table order. The manager then walks that list, resolves entities, and takes the first whose parent group carries the marker component. Nothing in `OVT_RecruitInactiveGrouping.c` may ever dereference an entity or a manager.

### 3.4 Recruit status — one source, server-side

Armed / has-ammo / wounded is computed **on the server** and pushed to the owning client. The alternative — reading the streamed entity client-side — is wrong precisely where this feature needs it: an inactive recruit is by definition somewhere you are not, its entity is not streamed, and every distant recruit would render as unarmed. One source keeps the map tag and the roster row from disagreeing.

- **Derivation is pure.** `OVT_RecruitStatus` (new, `Scripts/Game/Data/`) holds `static const int` flags (`ARMED = 1`, `HAS_AMMO = 2`, `WOUNDED = 4`, `UNCONSCIOUS = 8`), a `static int Derive(bool armed, bool hasAmmo, bool wounded, bool unconscious)`, and `static string TagIcon(int flags)` returning `""` / `"recruit_ammo"` / `"recruit_ammo_empty"`. Logic tier pins it.
- **Reading is world-bound** and lives in the manager: weapons via `BaseWeaponManagerComponent.GetWeaponsSlots()` + `slot.GetWeaponEntity()` (the same enumeration `OVT_LoadoutManagerComponent.ExtractEquippedItems` :1075-1079 uses, which exists because reading only the in-hands weapon loses slung gear — BUG-044); wounded/unconscious via `SCR_CharacterDamageManagerComponent.GetState()` and `CharacterControllerComponent.IsUnconscious()`, exactly as `OVT_RecruitListEntryHandler.PopulateFromEntity` :87-100 already does.
- **Delivery is one owner-targeted RPC per recruit**, on a slow server sweep (`STATUS_SYNC_INTERVAL_MS = 10000`), only for owners who are online and only for recruits with a live body:
  ```
  [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
  void RpcDo_RecruitStatus(string recruitId, vector position, int statusFlags)
  ```
  Three scalar parameters of already-proven types — no array-over-RPC risk, ~1.6 RPCs/second/player at the cap. The sweep also writes `m_vLastKnownPosition` from the live entity, which improves save fidelity for free. **It must not call `SyncRecruitPositions()`** — that materialises persistence ids and writes to storage, which is a save-point operation, not a tick operation.
- The client keeps the flags in a small `map<string, int>` on `OVT_RecruitCommandComponent`, read by both the map layer and the roster.

### 3.5 Client→server seam

One new component on `OVT_OverthrowController`, following the five existing controller components exactly:

```
Scripts/Game/Components/Controller/OVT_RecruitCommandComponent.c   (: OVT_Component)

  // client → server
  void RequestSetInactive(string recruitId, bool inactive)   // listen-server short-circuit
  [RplRpc(..., RplRcver.Server)] protected void RpcAsk_SetRecruitInactive(string recruitId, bool inactive)
  void RequestSwapLoadout(RplId recruitEntityRplId)
  [RplRpc(..., RplRcver.Server)] protected void RpcAsk_SwapLoadout(RplId recruitEntityRplId)

  // server → owner
  void SendRecruitStatus(string recruitId, vector position, int flags)
  [RplRpc(..., RplRcver.Owner)] void RpcDo_RecruitStatus(string recruitId, vector position, int flags)
  void SendResult(int resultCode)
  [RplRpc(..., RplRcver.Owner)] void RpcDo_RecruitCommandResult(int resultCode)

  int GetStatusFlags(string recruitId)   // client-side cache read
```

Every `RpcAsk_` derives the actor with `ResolveOwningPlayerId()` (copy the verbatim helper from `OVT_TowerSabotageComponent.c:59-80`) and then **re-validates**: the recruit exists, its `m_sOwnerPersistentId` matches the sender's persistent id, it has a live body, and the requested state is not the current one. No identity, no target and no state ever comes from the payload except the recruit id, which is validated against ownership.

Both send helpers short-circuit on the listen server (`if (Replication.IsServer()) RpcAsk_...(...); else Rpc(RpcAsk_..., ...)`) — the engine never loops an RPC back to the sender, which is the BUG-164/BUG-090 failure mode every existing controller component guards against.

The recruit manager reaches the owning client the canonical way: `OVT_Global.GetPlayers().GetController(playerId)` → `FindComponent(OVT_RecruitCommandComponent)` → call the send helper (`OVT_OverthrowGameMode.c:1173-1185` is the reference three-step resolve).

### 3.6 Map layer

`OVT_MapRecruitLocation : SCR_MapUIBaseComponent`, modelled line-for-line on `OVT_MapPlayerLocation` (`Scripts/Game/UI/Map/Visualization/`):

- `OnMapOpen` — reset `m_bAvailableThisSession = false` **first**, resolve the local persistent id, build one widget per owned recruit into `map<string, ref Widget>` keyed by recruit id, then re-assert `SetMarkersVisible(m_bMarkersVisible)` on the fresh widget set. Availability is true only once markers actually exist, so a player with no recruits gets **no filter row** rather than a dead toggle — the same rule and the same reason as the players row.
- `Update(timeSlice)` — per marker: resolve the live entity through the manager (client path goes through `m_mRplIdToRecruit`); if it resolves, use `GetOrigin()` + `GetYawPitchRoll()[0]` for rotation; if it does not, fall back to `m_vLastKnownPosition` with no rotation. Then `WorldToScreen` → `DPIUnscale` → `FrameSlot.SetPos`, exactly as the player layer does.
- **Opacity is owned by `Update()`**: 1.0 for active, `INACTIVE_MARKER_OPACITY = 0.45` for inactive. Therefore `SetMarkersVisible` uses `SetVisible` and **never** `SetOpacity` — the identical contention the player layer documents at its :152-159.
- The tag image is a **sibling** of `"Image"` under the root frame, never a child of it: `Update()` rotates `"Image"` by heading and a nested tag would spin with it. Tag quad from `OVT_RecruitStatus.TagIcon(flags)`; empty string → `SetVisible(false)`.
- Roster churn while the map is open: subscribe to `m_OnRecruitAdded` / `m_OnRecruitRemoved` and rebuild the widget set. Inactive state and status flags are read **per frame**, never cached at build time, so a deactivation performed with the map open dims the marker immediately.
- Filter row: a second hand-built row in `OVT_MapLayersUI` beside `BuildPlayerRow` (:728-742) — `KEY_RECRUITS = "recruits"`, `LABEL_RECRUITS = "#OVT-Map_Layer_Recruits"`, plus `ApplyRecruitMarkerPreference()` next to `ApplyPlayerMarkerPreference()` (:939-949) and a branch in `ApplyOne` (:1013-1025). Preference storage is free: `OVT_MapLayerPrefsStore.LayerKey("recruits")` and the existing flush points.
- Registration: one sibling entry in the `m_aUIComponents` block of `Configs/Map/MapOverthrow.conf` (:56-78). **Only that config** — `MapOverthrow_GM.conf` (editor) and `MapRespawn.conf` (respawn screen) do not carry the player layer either.

Imageset: four new `ImageSetDefClass` entries in `UI/Imagesets/overthrow_mapicons.imageset`. Entries are `ImageSetDefClass <name> { Pos x y Size w h Flags 0 0 }` with no GUID, and **two entries may point at the same atlas region**, so placeholders cost no atlas edit at all:

| New entry | Placeholder alias (Pos) | Real-art target |
|---|---|---|
| `recruit` | `clothes` — `652 2` | row 4, `132 392` |
| `recruit_ammo` | `fob` — `262 132` | row 4, `262 392` |
| `recruit_ammo_empty` | `tower` — `132 262` | row 4, `392 392` |
| `recruit_wounded` | `pharmacy` — `522 2` | row 4, `522 392` |

Row 4 of the atlas (`y = 392`, `x = 132 … 652`) is empty space today — the user drops real art there and repoints `Pos`, with no code change. The placeholders being obviously wrong buildings is deliberate: missing art should be visible, not plausible.

### 3.7 Recruits screen

Two changes of substance to `OVT_RecruitsContext` (498 lines) and one to `OVT_RecruitListEntryHandler`.

**Sections.** `RecruitsScrollContainer` gets two labelled child containers, `ActiveList` and `InactiveList`, each preceded by its own header text; a section with no members hides its header and its container. Rows are built into whichever container matches.

**The selection model is replaced, not patched.** Today `SelectRecruitByIndex` walks widget siblings counting to `index` (:311-336), which silently assumes the Nth sibling of one container is the Nth recruit — an assumption two containers and two headers break. Refresh instead builds a flat, ordered `array<ref OVT_RecruitEntryRef>` of `{ recruit, widget }` in display order (actives then inactives), and every selection path indexes that array. `MenuUp`/`MenuDown` then walk one flat list across the section boundary with no special cases, which is what keeps gamepad navigation working.

**Capacity.** `SelectedName`'s pane header (or a new `CapacityText` beside `RecruitsListTitle`) shows `X / 16` from `GetRecruitCount(persId)` and `MAX_RECRUITS_PER_PLAYER`.

**Fourth action button.** `ToggleActiveButton` in the `ActionButtons` row, label switching between activate/deactivate for the selected recruit, disabled when the recruit has no live body. It must be added to the hardcoded `SetButtonsVisible` array at :340-344 or it will never hide. It needs a new input action (`OverthrowRecruitsToggleActive`) in `Configs/System/chimeraInputCommon.conf` **and** an `ActionRefs` entry in the `OverthrowRecruitsMenuContext` block (:1101-1115). That context already binds `gamepad0:x`, `y` and `shoulder_right`; `shoulder_left` is free there.

**Status icons.** `RecruitListItem.layout` gains a small horizontal image row between `Spacer` and `RecruitStatus`. There is no precedent in Overthrow for a script-driven icon in a menu list row, so combine the two halves that exist: `OVT_MapInfoRowHandler.c:111` (icon in a row handler) and `OVT_WantedInfo.c:213-231` (the `SetVisible` → `LoadImageFromSet` → `SetColor` order, with the layout also carrying an authored `Texture`/`Image` so the widget is never blank before script runs). Declare the imageset GUID as a named constant, following `OVT_MapShopPriceIndicator.c:56`, not as a raw GUID inside the handler.

### 3.8 Loadout swap

Server-side, entity-transfer only. The engine has exactly the primitive this needs:

- **`InventoryStorageManagerComponent.TrySwapItemStorages(IEntity itemA, IEntity itemB)`** (`generated/InventorySystem/InventoryStorageManagerComponent.c:37`) — "place itemA to itemB storage slot and itemB to itemA storage slot". For a slot occupied on both sides, this *is* the swap, and it is atomic from script's point of view.
- **`TryMoveItemToStorage(IEntity item, BaseInventoryStorageComponent to, int slotID)`** (:34) for a slot occupied on one side only. Proven to work across two different characters' storages by vanilla itself (`SCR_TourniquetStorageComponent.c:176` moves a tourniquet from the medic into the patient).

**The algorithm** (`OVT_LoadoutSwap.Swap(IEntity a, IEntity b)`, server-only, `Replication.IsServer()` guard with the `OVT_InventoryManagerComponent.c:130-136` message shape):

1. Enumerate both sides **into arrays first**, then mutate. Never mutate a storage while iterating it (`OVT_LoadoutManagerComponent.c:752-768` is the in-repo pattern).
   - Worn clothing per area typename via `EquipedLoadoutStorageComponent.GetClothFromArea(areaType)`; areas from the list at `OVT_InventoryManagerComponent.c:812-820`.
   - Weapons via `BaseWeaponManagerComponent.GetWeaponsSlots()` → `slot.GetWeaponEntity()`.
   - Loose container contents via the character's universal storages.
   - **Do not use `GetItems(..., PURPOSE_DEPOSIT)`** — it returns worn gear too (BUG-083, documented at `OVT_SellableItemScanner.c:40-52`), and `EStoragePurpose` has no explicit values so bitmask tests diverge from the engine's real flags. Use `OVT_SellableItemScanner.IsEquipped()` as the classifier instead of hand-rolling one.
2. Swap **matched pairs first**, slot class by slot class: both sides occupied → `TrySwapItemStorages`. This is the majority of a swap and the safest operation available.
3. Then move **unmatched singles**: `TryMoveItemToStorage` into the counterpart storage; on failure retry with `slotID = -1`, mirroring `OVT_InventoryManagerComponent.TransferItemWithFallback` :310-325.
4. **Failure never deletes.** Each step appends to a rollback journal of `(item, originalStorage, originalSlotId)`. A failed step rolls its own move back and the routine continues with the remaining items, returning a count of items that could not be exchanged. If even the rollback fails, the item is detached to the world at the character's feet (`TryRemoveItemFromStorage` with no new parent — `BaseInventoryStorageComponent.c:153`: "item drop in world"). **The worst possible outcome of this feature is an item on the ground; a missing or duplicated item is impossible because nothing spawns and nothing is deleted.**
5. Rebuild quickslots on both characters afterwards (`SCR_CharacterInventoryStorageComponent.StoreItemToQuickSlot` :378) — quickslot state is per-character and does not follow the item.

**Known hazards, to be closed in the phase, not discovered in play-test:** `IsAreaBlocked` / `GetBlockedSlots` (`SCR_CharacterInventoryStorageComponent.c:730,219`) mean a plate carrier can block a jacket slot, so *ordering matters* — clear-then-fill per area, outermost first. `SCR_InventoryStorageManagerComponent.CanMoveItem` gates on `IsAnimationReady()` and `IsInventoryLocked()`; prefer the engine-level `InventoryStorageManagerComponent` methods, which do not. `BaseWeaponManagerComponent.SelectWeapon` is explicitly **not** synchronized (`BaseWeaponManagerComponent.c:35`) — if the swap changes what is in hands, that must be broadcast, or simply avoided by leaving both characters with nothing selected.

---

## 4. Implementation Phases

Each phase ends with `tools/compile-check.sh` exit 0. **No phase runs `tools/run-tests.sh`** — the orchestrator runs it after a phase completes (`.claude/test-policy.md`).

GUID series reserved for this feature: **`{6B4C0000000000XX}`** — verified free (the only `6B4x` GUIDs in the tree are four unrelated singletons: `6B49118D`, `6B493772`, `6B49DA97`, `6B4F85E8`).

**Ledger:** Phases 1-8 consumed `01`-`6E` (re-verified 2026-08-14 — nothing at or above `6F` is used anywhere in the tree). **Phase 9 starts at `70`.**

Phases 1-8 are built and green. **Phase 9 is a post-ship extension** and the only outstanding phase.

---

### Phase 1 — Data model, replication, serializer v3, quick wins

**Agent:** `component-developer` · **Estimate:** 4-6 h
**Why first:** every later phase reads or writes this flag. It lands with no behaviour attached to it, so it can be reviewed as a format change.

**Tasks**

1. **T1.1** `OVT_RecruitData.c`: add `bool m_bInactive = false;` with a doc comment stating it means "owned but out of the owner's group", and that it is *not* the same as `m_bIsOnline` (which is about having a body).
2. **T1.2** `OVT_RecruitManagerComponent` JIP: append `writer.WriteBool(recruit.m_bInactive)` as the last write in the per-recruit block of `RplSave` (:2038-2090) and the matching last read in `RplLoad` (:2096-2174). Positional order is the format — append only.
3. **T1.3** Add `BroadcastRecruitActiveState(OVT_RecruitData)` + `[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)] RpcDo_RecruitActiveStateChanged(string recruitId, bool inactive)` with the `RplSession.Mode() != RplMode.Client` guard its siblings use. **Do not extend `RpcDo_RecruitUpdated`** — it is at the documented 8-parameter limit (:2317).
4. **T1.4** `OVT_PersistedRecruit`: append `bool inactive;` **last**, after `bodyPersistenceId`. Serializer writes `version 3`; `Deserialize` adds `if (version < 3) ClearInactiveFlags(records);` beside the existing `ClearBodyPersistenceIds` call, with the same "whatever the reader left there is not ours" comment. Extend the class-header VERSION HISTORY block.
5. **T1.5** `ApplyPersistedRecruits` adopts `inactive` onto the record, preserving the live-session idempotency contract already documented in the serializer header.
6. **T1.6** *(quick win, BUG-107)* `OnAIKilled` :713 — replace `if (factionKey != "US" && factionKey != "USSR")` with a comparison against `OVT_Global.GetConfig().m_sOccupyingFaction`, guarding the config lookup. Precedent: `SCR_CharacterDamageManagerComponent.c:91`.
7. **T1.7** *(quick win)* `FindRecruitEntity` :1648-1665 — stop removing from `m_mEntityToRecruit` inside the `foreach` over that same map. Collect stale ids and remove after the loop, matching `SyncRecruitPositions` :1546-1572, which documents exactly this hazard.
8. **T1.8** Public server API on the manager, state only, no group work yet: `bool IsRecruitInactive(string recruitId)`, `array<ref OVT_RecruitData> GetPlayerRecruitsByState(string persId, bool inactive)`.
9. **T1.9** Persistence tier: extend `OVT_TEST_Persistence_Recruits_RoundTrip` (`OVT_TEST_PersistenceSuite.c:551`) to set and read back `m_bInactive` through the manager API, and add an inactive assertion to `OVT_TEST_PersistenceRoundTrip_Recruits_SurvivesSaveAndReload` (`OVT_TEST_PersistenceRoundTripSuite.c:1022`) so the flag is proven to survive save → dirty → re-apply.
10. **T1.10** Logic tier, new file `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_RecruitStatus.c` — for now only the record-level default (`new OVT_RecruitData()` is active) and the `OVT_GroupRecruitTransfer` change from Phase 2 lands here later. ⚠️ The Logic directory is grepped for the manager-accessor and game-mode-getter identifiers; they may not appear anywhere under `TestSuites/Logic/`, **including in comments** (`OVT_TEST_LogicSuite.c` header, rule 2).

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- `grep -n "version" OVT_RecruitManagerSerializer.c` shows `3` written and a `< 3` guard on read.
- The new broadcast RPC exists and `RpcDo_RecruitUpdated` still has exactly 8 parameters.
- Each new test case has a recorded can-fail proof in a preamble comment. No `maxAttempts`.

---

### Phase 2 — Inactive group mechanics (server)

**Agent:** `component-developer-advanced` · **Estimate:** 10-14 h
**Advanced because:** it owns an entity lifetime across a group class that self-deletes under a held pointer, forks the respawn path that BUG-130/131 fixed, and touches the slave-group exit sequence the recruits feature warns must stay in step with vanilla.

**Tasks**

1. **T2.1** `Prefabs/Groups/INDFOR/OVT_Group_InactiveRecruits.et` — based on `Prefabs/AI/Groups/Group_Base.et` like `Group_Player.et` is: `m_aUnitPrefabSlots` **empty**, `m_bPlayable 0`, `m_bDeleteWhenEmpty 1`, no Persistence component, plus `OVT_InactiveRecruitGroupComponent`. GUID from the reserved series.
2. **T2.2** `Scripts/Game/Components/OVT_InactiveRecruitGroupComponent.c` — `string m_sOwnerPersistentId` (server-only, never replicated), `AIWaypoint m_Waypoint`, `SetOwner/GetOwner`, `SetWaypoint`, and `override void OnDelete(IEntity owner)` deleting the waypoint so none outlives its group.
3. **T2.3** Add `[Attribute()] ResourceName m_sInactiveGroupPrefab;` to the recruit manager and set it on `Prefabs/GameMode/OVT_OverthrowGameMode.et` beside `m_sRecruitPrefab` (:161).
4. **T2.4** `Scripts/Game/GameMode/Managers/OVT_RecruitInactiveGrouping.c` — `SelectClusterCandidates(...)` exactly as §3.3, plus `static const float DEFAULT_CLUSTER_RADIUS = 50.0;`. **Pure**: no manager lookup, no entity dereference, ever. Class header states that rule the way `OVT_GroupRecruitTransfer.c:1-19` does.
5. **T2.5** Manager: `protected void RemoveRecruitFromSlaveGroup(IEntity recruitEntity, SCR_AIGroup slaveGroup, SCR_GroupsManagerComponent groupsManager)` — the four-step exit currently inlined in `RemoveRecruitsFromGroup` :1959-1996 (parent-group check → deactivate the slave when it is losing its last AI → `RemoveAgentFromControlledEntity` → `AskRemoveAiMemberFromGroup`). **Refactor `RemoveRecruitsFromGroup` to call it** so there is one implementation of the exit, and keep the "keep in step with `SCR_PlayerControllerGroupComponent.RemoveAiFromSlaveGroup`" note on it.
6. **T2.6** `protected bool PlaceRecruitInInactiveGroup(notnull OVT_RecruitData recruit, notnull IEntity recruitEntity)` — clustering + host join, or spawn: `OVT_Global.SpawnEntityPrefab` at the body position, cast to `SCR_AIGroup`, set the group faction to `OVT_Global.GetConfig().m_sPlayerFaction` (see `OVT_EntitySpawningAPI.SetGroupFaction` :230), `AddWaypoint(OVT_Global.GetConfig().SpawnDefendWaypoint(pos))`, stash the waypoint on the marker component, `OVT_PersistenceManagerComponent.UntrackTransient(groupEntity)` (same session-scoped rule waypoints get in `OVT_Global.c:847-850`, BUG-118), then `AddAIEntityToGroup(recruitEntity)`. **Verify `GetAgentsCount() >= 1` afterwards and delete the group if not** — vanilla's `m_bDeleteWhenEmpty` explicitly does not delete a group that *starts* empty (`SCR_AIGroup.c:96`).
   - ⚠️ **Never call `SetLifecyclePolicy` on this group** and **never call `OVT_EntitySpawningAPI.CleanupGroup/CleanupEntity` on it** (deletes the members). Put both prohibitions in the method's doc comment.
7. **T2.7** `bool SetRecruitInactive(string recruitId, bool inactive)` — the one server entry point. Validates, performs the transition of §3.3, writes `m_bInactive`, calls `BroadcastRecruitActiveState`, returns success. Server-guarded.
8. **T2.8** `protected void PlaceRecruitInWorld(...)` fork (§3.3) and rewire **both** call sites: `AttachRecruitBody` :1516 and the already-in-world branch of `RespawnPlayerRecruits` :1036.
9. **T2.9** `OVT_GroupRecruitTransfer.SelectTransferable` — skip inactive recruits (an inactive recruit does not follow its owner into a friend's group), with a separate out-count so the log line can say why. Update the class header's "two rules" to three.
10. **T2.10** `GetPlayerRecruitEntitiesInRadius(persId, pos, radius, bool excludeInactive = false)`; pass `true` from `OVT_TravelRequestComponent.ResolveTravellingRecruits` :271-290. Fast travel must not drag a garrison along. The loadouts screen keeps the default (an inactive recruit standing next to you is still equippable).
11. **T2.11** Logic tier — extend `OVT_TEST_Logic_GroupRecruits.c` for the new `SelectTransferable` rule, and add `OVT_TEST_Logic_RecruitClustering.c` for `SelectClusterCandidates`: exclusion of self, of active recruits, of offline recruits, radius boundary behaviour (⚠️ `vector.Distance` is not correctly rounded at 1000 m/2000 m — pick test distances well clear of the boundary or assert the boundary indirectly), empty input, and table-order stability.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- Deactivating the last member of a group leaves no `SCR_AIGroup` and no `AIWaypoint` behind (checked in play-test with the entity count, see §6).
- `grep -rn "SetLifecyclePolicy\|CleanupGroup\|CleanupEntity" Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` is empty.
- Every new pure function is covered by a Logic case with a recorded can-fail proof.

---

### Phase 3 — Controller component and user actions

**Agent:** `network-specialist` · **Estimate:** 6-9 h

**Tasks**

1. **T3.1** `Scripts/Game/Components/Controller/OVT_RecruitCommandComponent.c` per §3.5 — for this phase, the inactive-toggle ask, the result reply, and `ResolveOwningPlayerId()` copied verbatim from `OVT_TowerSabotageComponent.c:59-80`. The status cache and `RpcDo_RecruitStatus` land in Phase 4; the swap ask lands in Phase 7.
2. **T3.2** Add the component to `Prefabs/GameMode/OVT_OverthrowController.et` with a reserved GUID. **A component that exists in script but not in this prefab is a silent no-op** — this trap has cost this project features before.
3. **T3.3** `OVT_Global.GetRecruitCommands()` following `GetTowerSabotage()` :157, including the "null until `RpcDo_NotifyOwnerAssignment` lands" guard note every caller must respect.
4. **T3.4** `Scripts/Game/UserActions/OVT_BaseRecruitUserAction.c` — the mirror image of `OVT_BaseCivilianUserAction`. `CanBeShownScript` requires: a resolvable recruit record for `GetOwner()` **and** `record.m_sOwnerPersistentId == OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user)`; `CanBePerformedScript` additionally requires `RplComponent.Cast(user.FindComponent(RplComponent)).IsOwner()` — the show/perform split `OVT_LockVehicleAction.c:15-39` uses. Ownership comes from the **recruit record**, not `OVT_PlayerOwnerComponent`, because the record is the one route present on every recruit body. Every dereference guarded: this runs on clients during JIP when the manager, the config and the replicated table may all be absent.
5. **T3.5** `OVT_SetRecruitInactiveAction` (shown when active), `OVT_SetRecruitActiveAction` (shown when inactive), both calling `OVT_Global.GetRecruitCommands().RequestSetInactive(...)`, both `HasLocalEffectOnlyScript() => true` like every other Overthrow action. Follow `OVT_SabotageTowerAction`'s UX rule: when an action cannot be performed but is relevant, keep it **visible** with a `SetCannotPerformReason`, rather than vanishing.
6. **T3.6** Prefab wiring — an `ActionsManagerComponent "{520EA1D2F659CE02}"` override with an `additionalActions` block containing both actions, `ParentContextList { "default" }`, per-instance `UIInfo` names and `Duration` (deactivate `3`, reactivate `2`), added to **three** prefabs: `Prefabs/Characters/Factions/CIV/Character_CIV_Recruit.et`, `Prefabs/Characters/Factions/CIV/Character_CIV.et`, `Prefabs/Characters/Factions/INDFOR/FIA/Character_CIV.et`. Reuse the existing `ActionsManagerComponent`/context GUIDs already present in the two `Character_CIV.et` files. **Do not touch** `INDFOR/FIA/Character_CIV_Recruit.et` or `Character_FIA_Recruit.et` — both are dead files with zero references.
7. **T3.7** Localization: add `OVT-Recruit_SetInactive`, `OVT-Recruit_SetActive` (+ any `SetCannotPerformReason` keys) to `Language/localization_Overthrow.st` **only**. Until the user regenerates the runtime exports in Workbench, the prefab `UIInfo` names must be literal English text (see D14).

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- Every `RpcAsk_` in the new component calls `ResolveOwningPlayerId()` and re-checks ownership before touching state; no handler trusts a player id from the payload.
- Both send helpers short-circuit when `Replication.IsServer()`.
- The actions appear on an owned recruit and are absent on someone else's recruit and on a plain civilian.

---

### Phase 4 — Status derivation and owner-targeted push

**Agent:** `component-developer` · **Estimate:** 4-6 h

**Tasks**

1. **T4.1** `Scripts/Game/Data/OVT_RecruitStatus.c` — flags, `Derive(...)`, `TagIcon(int flags)`, `IsArmed/HasAmmo/IsWounded/IsUnconscious(int flags)`. Pure; no world access.
2. **T4.2** Manager: `protected int ReadRecruitStatus(IEntity recruitEntity)` — weapon slots via `BaseWeaponManagerComponent.GetWeaponsSlots()`, ammo by inspecting the weapon's magazine/muzzle state, wounded via `SCR_CharacterDamageManagerComponent.GetState()`, unconscious via `CharacterControllerComponent.IsUnconscious()`. Reads only; feeds `OVT_RecruitStatus.Derive`.
3. **T4.3** Manager: a `CallLater` sweep at `STATUS_SYNC_INTERVAL_MS = 10000`, server-only, walking a **snapshot** of owner ids; for each online owner with a controller, per live recruit body: update `m_vLastKnownPosition`, read status, and send through `OVT_RecruitCommandComponent.SendRecruitStatus`. **Never call `SyncRecruitPositions()` from the sweep.**
4. **T4.4** Component: `RpcDo_RecruitStatus` handler writing a client-side `map<string,int>` plus `m_vLastKnownPosition` on the local replica; `GetStatusFlags(recruitId)` reader; entries dropped on `m_OnRecruitRemoved`.
5. **T4.5** Logic tier: extend `OVT_TEST_Logic_RecruitStatus.c` — every flag combination through `Derive`, `TagIcon` returning empty for unarmed and the two ammo quads otherwise, and predicate accessors on a zero mask.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- The sweep is server-guarded and skips offline owners; a single-player session with 16 recruits shows no measurable frame cost.
- Logic cases cover the full flag matrix with recorded can-fail proofs.

---

### Phase 5 — Recruits screen

**Agent:** `ui-developer` · **Estimate:** 8-11 h
**Note for the implementer:** T5.2 is the risky task — it replaces the selection model the gamepad depends on. Do it first and verify pad navigation before adding anything else.

**Tasks**

1. **T5.1** `RecruitsMenu.layout` — `ActiveList` + `InactiveList` containers with their own headers inside `RecruitsScrollContainer`; a capacity text beside `RecruitsListTitle`; `ToggleActiveButton` in `ActionButtons`. Fresh GUIDs (this file already contains duplicated GUIDs — `ActionButtons` reuses `SelectedHometown`'s), and **keep** `SCR_InputButtonComponent` GUID `{5D346C3DD81D95CD}` on the new button or it will be inert.
2. **T5.2** `OVT_RecruitsContext.Refresh()` — build the flat ordered entry array of §3.7; rewrite `SelectRecruitByIndex`, `SelectPreviousRecruit`, `SelectNextRecruit` against it; delete the sibling walk. Empty-section headers hidden. Add `"ToggleActiveButton"` to the `SetButtonsVisible` array (:340-344).
3. **T5.3** Toggle handler → `OVT_Global.GetRecruitCommands().RequestSetInactive(id, !inactive)`; button label and enabled state follow the selection; disabled with a reason when the recruit has no live body. **Do not** mutate the local replica optimistically — the broadcast is the confirmation. (The existing rename path writes locally *and* asks the server; do not copy that.)
4. **T5.4** Capacity header `X / 16` from `GetRecruitCount` + `MAX_RECRUITS_PER_PLAYER`.
5. **T5.5** `RecruitListItem.layout` + `OVT_RecruitListEntryHandler.Populate` — status icon row driven by `GetStatusFlags`, inactive rows visually distinguished (dimmed name/level, not hidden), `GetRecruitData()` accessor added. Null-guard the `character.GetCharacterController()` deref at :39-41 while in there.
6. **T5.6** `chimeraInputCommon.conf` — `Action OverthrowRecruitsToggleActive` (`keyboard:KC_G` or similar free key, `gamepad0:shoulder_left`) **and** an `ActionRefs` entry in `ActionContext OverthrowRecruitsMenuContext` (:1101-1115). Run the repo's input-conflict checker, remembering its blind spot: inline `ActionContext` actions are invisible to it, so cross-check the keyboard key by hand.
7. **T5.7** Localization items into `Language/localization_Overthrow.st` only: section headers, capacity format, button labels, cannot-perform reasons.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- Pad-only: open the roster, walk from the first active recruit through the section boundary to the last inactive one and back, toggle a recruit's state, and close — no mouse.
- A player with zero inactive recruits sees no empty Inactive section.
- No layout key renders as a raw `#OVT-...` string on screen (literal text until exports are regenerated).

---

### Phase 6 — Map marker layer

**Agent:** `ui-developer` · **Estimate:** 6-9 h

**Tasks**

1. **T6.1** Four `ImageSetDefClass` entries in `UI/Imagesets/overthrow_mapicons.imageset` per the §3.6 table, with a comment recording the real-art target row.
2. **T6.2** `UI/Layouts/Map/MapRecruitLocation.layout` — copy `MapPlayerLocation.layout` (root `FrameWidgetClass`, `Z Order 9`, `Clipping False`, 44×44 `Image` centred by `Alignment 0.5 0.5`) and add a **sibling** `TagImage` with its own `FrameWidgetSlot` and corner alignment. The badge precedent is `FastTravelIndicator` in `OVT_MapLocationElement.layout:142-153`.
3. **T6.3** `Scripts/Game/UI/Map/Visualization/OVT_MapRecruitLocation.c` per §3.6, including `SetMarkersVisible` / `AreMarkersVisible` / `IsAvailableThisSession` and the "`SetVisible`, never `SetOpacity`" comment explaining that `Update()` owns opacity for the active/inactive distinction.
4. **T6.4** Register in `Configs/Map/MapOverthrow.conf` `m_aUIComponents` (:56-78), with `m_Layout` and a faction-palette attribute mirroring the player layer.
5. **T6.5** `OVT_MapLayersUI` — `KEY_RECRUITS`/`LABEL_RECRUITS` constants, `BuildRecruitRow()` beside `BuildPlayerRow` (:728) and called from the same place, `ApplyRecruitMarkerPreference()` beside :939, a branch in `ApplyOne` (:1013). Row skipped entirely when the layer reports unavailable.
6. **T6.6** `OVT-Map_Layer_Recruits` into `Language/localization_Overthrow.st`.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- Markers track a moving active recruit smoothly and hold position for an inactive one, at visibly different opacities.
- The Recruits row appears in the layer panel, toggles the markers, and its state survives a game restart.
- A player with no recruits gets no Recruits row (not a dead one).

---

### Phase 7 — Loadout swap

**Agent:** `component-developer-advanced` · **Estimate:** 10-14 h
**Advanced because:** it moves live item entities between two replicated inventories with no transaction support, against an API whose failure modes (blocked areas, animation gates, unsynchronized weapon selection) are all silent.

**Tasks**

1. **T7.1** `Scripts/Game/GameMode/Managers/OVT_LoadoutSwap.c` — the routine of §3.8. Server guard first, with the `OVT_InventoryManagerComponent.c:130-136` message shape. Returns a small result struct: items exchanged, items that failed, items dropped to the world.
2. **T7.2** Enumeration helpers reusing what exists: `OVT_SellableItemScanner.IsEquipped()` as the classifier, `BaseWeaponManagerComponent.GetWeaponsSlots()` for weapons, the area typename list at `OVT_InventoryManagerComponent.c:812-820` for clothing. **No `GetItems(PURPOSE_DEPOSIT)`** (BUG-083). Collect into arrays, then mutate.
3. **T7.3** Pair-then-single execution, rollback journal, world-drop fallback, quickslot rebuild.
4. **T7.4** `RpcAsk_SwapLoadout(RplId recruitEntityRplId)` on `OVT_RecruitCommandComponent`: resolve the entity from the RplId, `ResolveOwningPlayerId()`, re-check that the target is a recruit owned by the sender, is **active** (the requirement restricts the swap to active recruits), is alive and conscious, and is within a sane distance of the actor. Reply with `RpcDo_RecruitCommandResult` so the client can hint success/partial/failure.
5. **T7.5** `OVT_SwapLoadoutWithRecruitAction` on the same three prefabs as Phase 3, `Duration 5`, shown only on active owned recruits.
6. **T7.6** Notifications/hints for the three outcomes, keys into `.st` only.
7. **T7.7** Log every failed move and every world-drop with the item's prefab name at `LogLevel.WARNING`. A silent partial swap is the one outcome nobody can diagnose later.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- `grep -n "SpawnEntityPrefab\|TryDeleteItem\|DeleteEntityAndChildren\|TrySpawnPrefabToStorage" Scripts/Game/GameMode/Managers/OVT_LoadoutSwap.c` is **empty**. This is the structural proof of "no duplication, no loss".
- The item-count invariant in §6 holds across every play-test swap.

---

### Phase 8 — Help and documentation sync

**Agent:** `help-docs-sync` · **Estimate:** 2-3 h
**Required:** this feature changes what players see and do — a new held action on recruits, a new roster section, a new map layer, a new swap action, and a cap display.

**Tasks**

1. **T8.1** Tutorial popups (`Configs/Tutorials/`) — a tip on making a recruit inactive, one on the loadout swap.
2. **T8.2** Field Manual (`Configs/FieldManual/`) — extend the recruits entry: inactive recruits count against the 16 cap, hold position, defend, and come back where you left them; the map layer and its filter row; the swap.
3. **T8.3** Public wiki via the wikijs MCP tools, matching shipped behaviour.
4. **T8.4** Every factual sentence cites a `file:line` in the review notes or is cut — two shipped tips have previously described mechanics that do not exist, and no gate catches a well-formed lie.

---

### Phase 9 — Buy equipped recruits at the tent

**Agent:** `component-developer-advanced` (T9.12 → `ui-developer`, T9.16 → `help-docs-sync`) · **Estimate:** 16-22 h
**Post-ship extension.** Phases 1-8 are built and green; nothing here changes their behaviour. It is numbered 9 because it extends the same seam (`OVT_RecruitCommandComponent`) and the same GUID series.

**Advanced because:** it opens a **second money path** into a system whose first one is legacy and unvalidated, and it re-introduces a network route to the loadout engine's *spawning* apply — the exact endpoint that was deliberately deleted as a free-item hole (`OVT_PlayerCommsComponent.c:1693-1695`, BUG-043). Getting the authority, the ordering and the refusal set right **is** the phase; the UI is the easy half.

#### P9.1 What the player does

1. Stands at a recruitment tent. A second held action, **Buy Equipped Recruit**, sits beside the existing **Recruit Civilian** action on the tent's `tabletop` context (`Prefabs/Structures/Military/FOB/OVT_RecruitmentTent.et:55-88`).
2. It opens the **existing** loadouts screen in a new *purchase mode* — same list, same selection model, same pad bindings (`Scripts/Game/UI/Context/OVT_LoadoutsContext.c`).
3. Selecting a loadout asks the server for a **quote**. The price lands on the Buy button and in the details pane. A loadout that cannot be priced shows the reason instead, with the button disabled.
4. Buy → the server re-validates everything, spawns the recruit at the tent, equips it from prefabs, takes a town supporter and the money, and replies with the outcome.

#### P9.2 Data flow

```
CLIENT                                          SERVER (the only authority)
------                                          --------------------------
OVT_BuyEquippedRecruitAction (tent tabletop)
  └ OVT_LoadoutsContext in purchase mode
      (holds the TENT entity, not a box)

  select a loadout
   └ GetRecruitCommands().RequestRecruitLoadoutQuote(tentRplId, name)
        ── RpcAsk_QuoteEquippedRecruit ─────►  ResolveOwningPlayerId()
                                               ValidateTentPurchase(...)  ─── refusal ──┐
                                               GetLoadouts().GetLoadout(persId, name)   │
                                               OVT_RecruitLoadoutPricing.Price(...)     │
        ◄── RpcDo_RecruitQuote(name, price, itemCount, refusalCode) ────────────────────┘
  button label = "Buy Recruit ($price)" | refusal sentence + button disabled

  press Buy
   └ RequestBuyEquippedRecruit(tentRplId, name)
        ── RpcAsk_BuyEquippedRecruit ──────►   1  ResolveOwningPlayerId()
                                               2  ValidateTentPurchase()  tent resolves, is a
                                                  RecruitmentTent, is in range, under the 16 cap,
                                                  town has a supporter, sender owns the named
                                                  loadout, every item is priceable
                                               3  quote = tentRecruitCost
                                                        + Round(gearSubtotal * feeMultiplier)
                                               4  PlayerHasMoney(persId, quote)?  no → refuse
                                               5  recruits.SpawnTentRecruit(tent, playerId)
                                               6  loadouts.ApplyLoadoutToEntity(loadout, body)
                                               7  towns.TakeSupportersFromNearestTown(pos, 1)
                                               8  economy.TakePlayerMoneyPersistentId(persId, charge)
        ◄── RpcDo_EquippedRecruitResult(code, charged, applied, total)
  hint
```

Steps 5-8 are in that order **because the shipped tent handler already is**: `OVT_PlayerCommsComponent.c:1517-1530` spawns, owns, and only then takes supporters and money — *"Costs are taken only after the recruit actually exists"* (:1528). Charging first and failing to spawn is the one ordering that can take a player's money for nothing.

#### P9.3 The spawn-based apply already exists — do not build one

The requirement is "spawned and charged for, never consumed from a box". That path is **already in the tree and shipped**, and it is a different implementation from the box path:

| | Box path (conservation) | Spawn path (Phase 9 uses this) |
|---|---|---|
| Entry | `ApplyLoadoutToEntityFromBox` :189 | `ApplyLoadoutToEntity` :168 |
| Per item | `ApplyLoadoutItemFromBox` :526 (finds a real entity in the box) | `ApplyLoadoutItem` :1504 (`SpawnEntityPrefab` :1520) |
| Equipped weapon | `ApplyEquippedItemFromBox` (BUG-042 note at :495-496) | `ApplyEquippedItem` :1306 (spawns, :1322) |
| Container contents | `ApplyNestedItems` :1577 | `ApplyNestedItemsSpawn` :1603 (recursive) |
| Attachments | `ApplyWeaponAttachmentsFromBox` :2035 | `ApplyWeaponAttachments` :2062 (spawns, :2085) |

So the answer to "can a spawn-based apply reuse the item-tree walking without the box-stock logic" is **yes, with no refactor** — the two walks are already separate. `ApplyLoadoutToEntity` is used today by the logout-gear restore (`OVT_SpawnLogic.c:457`) and is pinned end-to-end, nested containers included, by `OVT_TEST_InitSuite.c:1173-1224`.

Three properties of that path the phase must respect:

- **It clears first.** `ApplyEquipmentToEntity` :450 calls `ClearEntityEquipment` :1278-1303, which **deletes every item** on the target. On the body Phase 9 just spawned that is the free civilian loadout from `SpawnRecruit` :912, and deleting it is correct. **It must never be pointed at an existing recruit** — that would destroy gear the player paid for. The handler therefore only ever applies to the body it created two lines earlier, and never to a target named by a client.
- **Quickslots never apply to a recruit.** `ApplyQuickSlots` returns early for an entity with an activated `AIControlComponent` (:1933-1935). `m_aQuickSlotItems` names items already in the tree, so nothing extra spawns — and nothing extra may be priced.
- **Per-item success is known only at the top level.** `ApplyEquipmentToEntity` counts top-level successes (:457-471); `ApplyNestedItemsSpawn*` return `void` (:1603-1621). "How much of the kit arrived" is a top-level number, which is what D19 is built on.

#### P9.4 Pricing

Two classes, split the way this feature already splits everything (pure decision logic apart from world-bound reads — §3.3, §3.4):

```
Scripts/Game/Data/OVT_RecruitPurchaseRules.c              PURE (Logic tier)
  static const float DEFAULT_LOADOUT_FEE_MULTIPLIER = 1.5;
  static float ResolveFeeMultiplier(float configured)     // <= 0 -> DEFAULT. See the note below.
  static int   GearFee(int gearSubtotal, float multiplier)
  static int   TotalPrice(int tentRecruitCost, int gearSubtotal, float multiplier)
  static int   OutcomeFor(bool spawned, int appliedItems, int totalItems)

Scripts/Game/GameMode/Managers/OVT_RecruitLoadoutPricing.c    SERVER, world-bound
  static OVT_RecruitLoadoutPrice Price(OVT_PlayerLoadout loadout, vector pos, int playerId)
```

`Price` walks the record — every `OVT_LoadoutItem`, then its `m_aAttachments` (separate prefabs, and they *are* spawned, :2085), then `m_aChildItems` recursively — and for each resource:

1. **`economy.IsRegisteredResource(res)` first.** `GetInventoryId` is a bare map index, and an unregistered prefab silently resolves to **id 0, i.e. some other item's price** — the function's own doc comment says exactly that (`OVT_EconomyManagerComponent.c:1679-1691`). One unregistered resource makes the whole loadout unpriceable and names itself in the result.
2. `GetBuyPrice(GetInventoryId(res), tentPos, playerId)` (:582-594) — the buy side, so the fee is charged on top of what the same item would cost at a shop near that tent, and the player's own `priceMultiplier` perk applies for free.

The result carries `m_iSubtotal`, `m_iItemCount` and `m_sUnpriceableResource`. Nothing else in the phase computes a price.

⚠️ **`ResolveFeeMultiplier` treats `<= 0` as "unset" and returns the default — a zero fee is deliberately NOT a supported setting.** A float field that never resolves reads `0`, and a `0` multiplier means *free gear on a money path*, which is the failure this whole phase is guarding against. Making zero unreachable costs a server admin nothing (`0.01` is available and means the same thing in practice) and removes the possibility entirely. This matters because it is not settled from the code whether an `[Attribute(defvalue:)]` is applied to a field absent from an authored difficulty `.conf` — T9.2 writes the value into all four presets explicitly *and* this guard backs it up, so neither has to be trusted alone.

⚠️ **`GetPrice` cannot report "no price": it returns 500 for any id it has never heard of** (`OVT_EconomyManagerComponent.c:209-213`). Registration is the only real signal, which is why step 1 is the gate and a zero check is not.

#### P9.5 Where the price lives, and why nothing new is replicated

**A client physically cannot price a loadout.** `OVT_LoadoutManagerComponent.RplSave/RplLoad` replicate loadout **metadata only** — key, name, owner, description, officer flag, timestamps, usage count (:2153-2180, read back :2219-2250, *"Add to active loadouts cache (without item data…)"* :2248), and the class comment at :40 says so outright. `m_aItems` is empty on every client.

Three consequences, all good:

- The **server quotes**; the client displays a number it was handed. Preview and charge come from the same function on the same machine, so they cannot drift.
- The fee multiplier is **never read on a client**, so it does not enter the config JIP bitstream. `OVT_OverthrowConfigComponent.RplSave/RplLoad` (:560-694) stay untouched and `CONFIG_STREAM_VERSION` (:558) stays at 2 — BUG-078's failure mode is not reachable at all.
- The quote is **per-selection**, not per-list: one small RPC when the highlighted row changes, four scalars back. No array crosses the wire (D12's rule, kept).

#### P9.6 One spawn entry point, shared with the legacy handler

The tent-recruit internals live inside `RpcAsk_RecruitFromTent` on the deprecated comms component (`OVT_PlayerCommsComponent.c:1486-1531`). Phase 9 may not add an op there and may not duplicate it, so the internals move **onto the recruit manager** and both callers use them:

```
OVT_RecruitManagerComponent
  static const float TENT_MAX_DISTANCE = 20;                        // the magic 20 both sites use today
  int ValidateTentRecruit(vector tentPos, int playerId)             // cap, supporters, distance, identity
  SCR_ChimeraCharacter SpawnTentRecruit(IEntity tent, int playerId) // placement + spawn + own
```

`SpawnTentRecruit` is the body of :1517-1526 — `SpawnRecruit` at a position derived from the tent, `RecruitCivilian`, and the `SCR_EntityHelper.DeleteEntityAndChildren` *"never leave an unowned civilian standing at the tent"* branch — **plus the placement hardening of §P9.7**. Neither method touches money or supporters: the two callers keep their own, because their prices differ. `RpcAsk_RecruitFromTent` is then refactored to call both. That is a refactor of an existing handler, not a new op on it, and it is what makes any placement change land once.

#### P9.7 Placement — and why the reported "~100 m away" is *not* a placement bug

**The spawn position is already correct, and this is provable rather than arguable.** `RpcAsk_RecruitFromTent` refuses any `tentPos` more than 20 m from the requesting player (`OVT_PlayerCommsComponent.c:1503-1504`, added in `bf2693f6`), and then spawns at `tentPos + "2 0 2"` (:1518). `SpawnRecruit` hands that straight to `EntitySpawnParams.Transform[3]` with no adjustment of any kind (`OVT_RecruitManagerComponent.c:899-908` → `OVT_Global.c:936-951`). **So a recruit that exists at all was created within ~23 m of the player** — a body 100 m away did not spawn there.

Nothing then moves it. Verified absent across the whole chain: `OVT_Global.SpawnCharacterEntity` (:934-968), `OVT_Global.ApplyCivilianLoadout` (:1119-1160, inventory only), `RecruitCivilian` (`OVT_RecruitManagerComponent.c:939-985`), `AddRecruitToPlayerGroup` (:2013-2105, ending at `SCR_PlayerControllerGroupComponent.AddAIToSlaveGroup` :2103), and on the vanilla side `AddAIToSlaveGroup` (`SCR_PlayerControllerGroupComponent.c:1433-1458`) → `SCR_AIGroup.AddAgentFromControlledEntity` (`SCR_AIGroup.c:1126-1144`). `grep` for `SetOrigin|SetTransform|SetWorldTransform|Teleport` over `SCR_AIGroup.c` and all of `scripts/Game/Groups/` returns **zero hits**. The known navmesh-snap-and-terrain-clamp at `SCR_AIGroup.c:1725-1737` is **unreachable** here: it lives in `SpawnGroupMember` (:1658), where it is a *spawn parameter* written to `spawnParams.Transform[3]` (:1739) and consumed by `SpawnEntityPrefabEx` (:1742) — computed before any entity exists, never applied to one. Its only two call sites (:1642 and :2742) are both `m_aUnitPrefabSlots`-driven prefab spawns. (A group's own position is *derived from* its members — `AIGroup.GetCenterOfMass`, `generated/AI/AIGroup.c:43` — not imposed on them.)

Two more candidates are ruled out with citations: there is exactly **one** tent recruitment path (`"RecruitmentTent"` appears once in the whole repo, `OVT_RecruitmentTent.et:5`, and nothing consumes the string), and there is **no registry holding a tent position** — the position is read live from the entity at action time (`OVT_RecruitFromTentAction.c:38`). The child-entity worry is also dead: the action *is* authored on the table child (`OVT_RecruitmentTent.et:52-94`, local `coords -0.088 0.045 -2.3`), but if `GetOrigin()` were returning parent space the 20 m guard would refuse every request and **no recruit would appear at all**.

**The recruit walks.** With no waypoint (Overthrow adds waypoints only to *inactive*-recruit groups, `OVT_RecruitManagerComponent.c:2416-2424`) the group's activity tree is the stub `ActivityIdle.bt`, but each soldier independently runs `Idle.bt:60-96`, one of whose four random branches (:71) is `Idle_ReturnToFormation.bt` — `SetMovementSpeed RUN`, `AITaskGetFormationOffset`, `DecoTestDistanceToEntity DistanceThreshold 2 / NegativeCondition 1`, then `AITaskMove`. **An idle AI more than 2 m from its formation slot runs to it, at any distance, uncapped.** The slot is anchored on the slave group's own index-0 agent unless the group is "formation displaced" (`SCR_AIUtilityComponent.c:288`, :296-320) — and Overthrow never calls `SetFormationDisplacement`; the only caller in the whole vanilla tree is `SCR_FollowGroupCommand.c:56-57`. Adding a member also restarts the group activity and re-deals every slot (`SCR_AIGroupUtilityComponent.c:238-240`, flag set :461).

BI hit this exact failure and patched it in *their* command, with the comment still in the source (`SCR_FollowGroupCommand.c:38-42`):

> `// Hotfix: teleport group to leader, otherwise if group is very split up, bots will try to regroup somewhere in the middle, far away`

`AddAIToSlaveGroup` — the call Overthrow uses — **does not contain that hotfix**.

**Conclusion: the ~100 m is post-spawn AI regroup, not placement.** It is a different bug with a different fix (mirror BI's hotfix, or set formation displacement so the anchor is the player), it affects every recruit and not just tent ones, and it is **out of scope for Phase 9**. It has a falsifiable prediction the user can check in one minute: *it should only happen when the player already has at least one active recruit, and the arrival point should be on or near those recruits rather than at a fixed spot.* **File it as a bug** (next free BUG id, linked `resistance/recruits`), citing `Idle_ReturnToFormation.bt`, `SCR_AIUtilityComponent.c:288` and the hotfix comment above as the shape of the fix.

**What Phase 9 does do**, because the equipped purchase must land "at the tent, safe-position adjusted" and one variable in the current spawn genuinely is uncontrolled:

- `SpawnTentRecruit` takes the tent **entity**, never a client-supplied vector, and derives the position from the tent **root** (`GetWorldTransform`, not the action owner's `GetOrigin()`), offset along the tent's own forward axis instead of the fixed world-space `+2 0 +2` — so the recruit appears in front of the tent whichever way it was built.
- Ground-clamp the result (`BaseWorld.GetSurfaceY`) and pass it through `OVT_Global.FindSafeSpawnPosition(pos, "-0.5 0 -0.5", "0.5 2 0.5", true)`. ⚠️ **`skipSpawnPointSearch` must be `true`.** With the search enabled the function returns the closest `OVT_SpawnPointComponent` within **15 m** and returns *before the TraceBox is ever built*, ignoring the mins/maxs box entirely (`OVT_Global.c:406-440`). Tents are built at bases and FOBs (`Configs/Resistance/buildables.conf:16-27`, `m_bBuildAtBase`/`m_bBuildAtFOB`), and both `Prefabs/Controllers/OVT_BaseController.et` and `Prefabs/Vehicles/Wheeled/M923A1/OverthrowMobileFOBDeployed.et` carry an `OVT_SpawnPointComponent` — a tent built within 15 m of one would drop its recruits on the respawn marker instead of at the tent.
- Because it is shared (§P9.6), the plain Recruit Civilian action gets the same treatment for free.

#### P9.8 Client→server seam

Four new RPCs on `OVT_RecruitCommandComponent`, taking its ledger from five call sites to nine. Every parameter is a scalar, an `RplId` or a string; every send short-circuits on the listen server; and the class-header ledger (:24-35) is extended in the same commit (BUG-090 — a wrong arity compiles clean and dies silently at the wire).

```
// client -> server
void RequestRecruitLoadoutQuote(RplId tentRplId, string loadoutName)
[RplRpc(..., RplRcver.Server)] protected void RpcAsk_QuoteEquippedRecruit(RplId, string)      2 args
void RequestBuyEquippedRecruit(RplId tentRplId, string loadoutName)
[RplRpc(..., RplRcver.Server)] protected void RpcAsk_BuyEquippedRecruit(RplId, string)        2 args

// server -> owner
void SendRecruitQuote(string loadoutName, int price, int itemCount, int refusalCode)
[RplRpc(..., RplRcver.Owner)] protected void RpcDo_RecruitQuote(string, int, int, int)        4 args
void SendPurchaseResult(int resultCode, int charged, int applied, int total)
[RplRpc(..., RplRcver.Owner)] protected void RpcDo_EquippedRecruitResult(int, int, int, int)  4 args
```

**The tent is named by `RplId` and treated as hostile**, exactly as `RpcAsk_SwapLoadout` treats the recruit body (:355-370): the id must resolve, the entity must carry an `OVT_BuildableComponent` whose `GetBuildableType()` is `"RecruitmentTent"` (`Scripts/Game/Components/OVT_BuildableComponent.c:20-23`, set at `OVT_RecruitmentTent.et:4-6`), and the requester must be within `TENT_MAX_DISTANCE` of it. This is strictly stronger than the legacy handler, which trusts a bare `vector` and only checks that the player is within 20 m of the position *they claimed* — it never verifies a tent is there.

**The loadout is named by string and re-owned server-side.** `GetLoadout(persId, name)` is keyed by the persistent id this controller resolved (`OVT_LoadoutManagerComponent.c:155-165`, key format :868), so a client cannot reach another player's loadout by naming it — the same fix `ResolveSenderPersistentId` applied to the box path (`OVT_PlayerCommsComponent.c:1719-1720`).

**Result codes, appended 14-24** (never renumbered — the block's own rule, :95-97):

| Code | Meaning |
|---|---|
| `RESULT_BUY_OK` 14 | Recruit bought, whole kit arrived |
| `RESULT_BUY_PARTIAL` 15 | Recruit bought, some of the kit did not arrive (counts in the reply) |
| `RESULT_NO_TENT` 16 | The RplId is not a recruitment tent, or the buyer is not at it |
| `RESULT_NO_LOADOUT` 17 | No loadout of that name belongs to the sender |
| `RESULT_LOADOUT_EMPTY` 18 | The loadout records no items — nothing to sell |
| `RESULT_UNPRICEABLE` 19 | An item in the loadout has no catalog price (names it) |
| `RESULT_CANNOT_AFFORD` 20 | Quote exceeds the player's balance |
| `RESULT_AT_CAP` 21 | Already at `MAX_RECRUITS_PER_PLAYER` (:11, :206-208) |
| `RESULT_NO_SUPPORTERS` 22 | Nearest town has no supporter to give (`NearestTownHasSupporters` :1213-1220) |
| `RESULT_GEAR_FAILED` 23 | Recruit spawned but no gear landed — **the gear fee is not charged** |
| `RESULT_SPAWN_FAILED` 24 | Nothing spawned, nothing charged |

The three count-bearing outcomes (14, 15, 23) assemble their sentences in `RpcDo_EquippedRecruitResult`; `ReasonKeyFor` is never asked about them — the same split the swap codes already use (:644-645). The refusals get keys.

#### P9.9 UI — purchase mode on the existing loadouts screen

The smallest surface that meets the pad-first bar is the screen that **already lists the player's loadouts with a working selection model and pad bindings**. `OVT_LoadoutsContext` gains a mode, mirroring the box it already carries:

- `void SetRecruitmentTent(IEntity tent)` beside `SetEquipmentBox` (:326-329); purchase mode is "a tent was set and a box was not".
- The screen already hides button groups by name — `ApplyButton`/`DeleteButton` at :287-295, the two recruit buttons at :593-601. Purchase mode hides both groups and the whole `RecruitSection` (`LoadoutsMenu.layout:187`), and shows one new `PurchaseButton` in `ActionButtons` (:230-260).
- The price line goes in `SelectedStatus` (`LoadoutsMenu.layout:178`), already a per-selection text the context drives.
- Selection change → `RequestRecruitLoadoutQuote`. Until the reply lands the button reads "Checking price…" and is **disabled**, so a pad user can never fire an unpriced purchase.

One new input action, `OverthrowLoadoutsBuyRecruit`, plus an `ActionRefs` entry in `ActionContext OverthrowLoadoutsContext` (`Configs/System/chimeraInputCommon.conf:1134-1146`). That context uses `gamepad0:x`, `y`, `shoulder_right` and `right_trigger` (:629-745), so `gamepad0:left_trigger` is free there. ⚠️ **`shoulder_left` is VON and always live** — Phase 5 was caught by exactly this; keyboard keys must be cross-checked by hand because the conflict checker cannot see inline `ActionContext` actions.

**Tasks**

*Server core*

1. **T9.1** `Scripts/Game/Data/OVT_RecruitPurchaseRules.c` — pure, per §P9.4. Class header states the purity rule the way `OVT_ShopSellRules.c:1-11` does. ⚠️ Keep the arithmetic in floats and `Math.Round` once at the end; `new` applies no `[Attribute]` defvalues, so every value arrives as a parameter.
2. **T9.2** Config, four places, **not replicated** (D18): the field on `OVT_DifficultySettings` beside `baseRecruitCost` (:73); on `OVT_OverthrowConfigStruct` + `SetDefaults()` (`OVT_OverthrowConfigComponent.c:24-65`); one line in the `overrideDifficulty` block (`OVT_OverthrowGameMode.c:377-384`); and `recruitLoadoutFeeMultiplier 1.5` written explicitly into all four `Configs/Difficulty/*.conf` so the authored value is visible rather than depending on defvalue resolution. **Do not invent per-difficulty balance.** ❌ Do **not** touch `RplSave`/`RplLoad` or bump `CONFIG_STREAM_VERSION` (:558).
3. **T9.3** `OVT_RecruitManagerComponent`: `TENT_MAX_DISTANCE`, `ValidateTentRecruit`, `SpawnTentRecruit` per §P9.6, including the §P9.7 placement hardening. Doc-comment both: *no money, no supporters — the caller owns its own transaction*. Keep the "never leave an unowned civilian standing at the tent" cleanup (`OVT_PlayerCommsComponent.c:1522-1526`).
4. **T9.4** Refactor `RpcAsk_RecruitFromTent` (:1486-1531) onto T9.3. Behaviour unchanged except placement. **A refactor of a legacy handler, not a new op on it** — no new parameters, no new features, no "improved" validation.
5. **T9.5** `Scripts/Game/GameMode/Managers/OVT_RecruitLoadoutPricing.c` — the recursive walk + `OVT_RecruitLoadoutPrice` result class, per §P9.4. Same file shape as `OVT_LoadoutSwap.c` (static routine + result class, server guard first). `IsRegisteredResource` before every `GetInventoryId`. Recurse into `m_aAttachments` **and** `m_aChildItems`; never price `m_aQuickSlotItems`.
6. **T9.6** `OVT_LoadoutManagerComponent`: set `m_iLastApplySuccessCount`/`m_iLastApplyTotalCount` in `ApplyEquipmentToEntity` (:440-478) as the box path already does (:517-519), plus a public accessor. Symmetry fix — the counters exist and mean nothing in the spawn path today.

*Networking*

7. **T9.7** The eleven new `RESULT_` constants 14-24 of §P9.8, each with the one-line doc comment the existing block gives every code; `ReasonKeyFor` (:648-669) extended for the refusals only.
8. **T9.8** Quote ask + owner reply. The quote runs the **same** `ValidateTentPurchase` the purchase runs, so the picker can never offer something the server would refuse. A quote never mutates and never charges.
9. **T9.9** Purchase ask + the eight-step transaction of §P9.2. Explicit `PlayerHasMoney` **before** spawning — `DoTakePlayerMoney` clamps at zero (`OVT_EconomyManagerComponent.c:1206-1216`) and can never tell you the player could not pay. Charge with `TakePlayerMoneyPersistentId(persId, charge)` (:1168-1181), a **direct server-side call**; never `TakePlayerMoney(playerId, …)`, which forwards through `OVT_Global.GetServer()` off the server and is the BUG-161 shape (never `Rpc()` out of a server handler).
10. **T9.10** Update the class-header `Rpc()` ledger (:24-35) from five call sites to nine with arities, hand-checked against the handlers, and extend the listen-server note.

*UI*

11. **T9.11** `Scripts/Game/UserActions/OVT_BuyEquippedRecruitAction.c` + wiring into the tent's existing `additionalActions` block (`OVT_RecruitmentTent.et:65-87`), `ParentContextList { "tabletop" }`, `HasLocalEffectOnlyScript() => true`. It must resolve the tent **root** — the action is authored on the table child (:52-94), and the root is what carries the `RplComponent` and the `OVT_BuildableComponent`. Precedent for walking up: `OVT_DeployFOBAction.c:7`. GUIDs from `{6B4C00000000007X}`.
12. **T9.12** *(`ui-developer`)* Purchase mode on `OVT_LoadoutsContext` per §P9.9, plus `PurchaseButton` in `LoadoutsMenu.layout`, the new input action and its `ActionRefs` entry. A missing `ActionRefs` entry is a silently dead pad button. Run `check-input-conflicts.py` and cross-check the keyboard key by hand.
13. **T9.13** Localization into `Language/localization_Overthrow.st` **only** (D14): action name, buy-button label with price, price/item-count line, one sentence per refusal code. Layout and prefab text stay literal English until the user regenerates the exports.

*Tests*

14. **T9.14** Logic tier — `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_RecruitPurchase.c`: `GearFee` at 1.0 / 1.5, `ResolveFeeMultiplier` at 0 and negative (both → default, proving free gear is unreachable), rounding at a half, `TotalPrice` composition, `OutcomeFor` over the full grid, and a zero-item loadout. ⚠️ The Logic directory is grepped for the manager-accessor and game-mode-getter identifiers; neither may appear anywhere under `TestSuites/Logic/`, **including in comments**.
15. **T9.15** Init tier — beside the existing loadout round-trip case (`OVT_TEST_InitSuite.c:1173-1224`): `OVT_RecruitLoadoutPricing.Price` on a saved loadout returns a subtotal > 0 and the record's item count, and an unregistered resource reports unpriceable. This is the tier that has a world and an economy; the money path itself is not automatable.
16. **T9.16** *(`help-docs-sync`)* Tutorial popup for the tent purchase; Field Manual paragraph (the tent's two actions, how the price is made, the supporter cost, the 16-cap); wiki update. Every factual sentence cites a `file:line` or is cut.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- `grep -n "CONFIG_STREAM_VERSION" Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c` still shows `= 2`, and the diff touches neither `RplSave` nor `RplLoad` in that file.
- `grep -n "TakePlayerMoney(" Scripts/Game/Components/Controller/OVT_RecruitCommandComponent.c` is **empty** — the charge goes through `TakePlayerMoneyPersistentId`, server-side (BUG-161).
- `grep -n "ApplyLoadoutToEntity" Scripts/Game/Components/Controller/OVT_RecruitCommandComponent.c` shows **exactly one** call, and its target is the local variable the same handler just spawned (R13).
- `grep -n "SpawnRecruit" Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` is **empty** — the spawn lives in one place (T9.3/T9.4).
- The class-header `Rpc()` ledger lists nine call sites and matches nine handler signatures by hand.
- Every new Logic case has a recorded can-fail proof in a preamble comment. No `maxAttempts`.

**Definition of Done — Phase 9 addition to §6**

*Functional*

- [ ] **F13** A recruitment tent offers **two** actions: the existing Recruit Civilian, and Buy Equipped Recruit. The second opens the loadouts screen showing the player's own loadouts with no box-only buttons.
- [ ] **F14** Selecting a loadout shows a price: the loadout's items at shop buy price, times the fee multiplier, plus the normal tent recruit cost.
- [ ] **F15** Buying spawns a recruit **beside the tent** (a few metres, on the ground, not inside it), wearing and carrying the loadout, in the player's squad, counted against the 16 cap, with one supporter taken from the nearest town.
- [ ] **F16** The player's money drops by exactly the previewed price, and by nothing else.
- [ ] **F17** Buying with insufficient funds, at the cap, with no town supporters, away from a tent, or with an unpriceable loadout is refused with a sentence naming the reason, and **nothing spawns and no money moves**.
- [ ] **F18** The plain Recruit Civilian action also appears beside the tent (shared placement, §P9.6).

*Quality*

- [ ] **Q10** No free gear: every item that arrives was priced into a charge the player paid, and nothing spawns before the funds check passes.
- [ ] **Q11** No paid-for-nothing: a failed spawn charges nothing, and a kit that lands nothing charges only the base recruit cost.
- [ ] **Q12** The quote a client displays and the amount the server takes are produced by the same function on the same machine; a client never computes a price.
- [ ] **Q13** No new field enters the config JIP bitstream and `CONFIG_STREAM_VERSION` is unchanged.
- [ ] **Q14** Every new handler derives its actor from `ResolveOwningPlayerId()` and re-derives the tent, the loadout and the price server-side; the payload contributes one `RplId` and one string, both validated.

*Verification — added to §6's numbered play-test*

23. **Do this with ZERO active recruits.** (§P9.7: an idle recruit runs to its formation slot, which is anchored on your *other* recruits — with a squad already out, a correctly-placed recruit will still leave the tent, and the tester will misread that as a placement failure.) Save a loadout with a rifle, a vest holding a magazine, and a rucksack with something in it. Build a recruitment tent. Open Buy Equipped Recruit, select the loadout → a price appears. Note your cash.
24. Buy → a recruit appears **at the tent**, wearing the kit, in your squad; cash dropped by exactly the quoted amount; the roster capacity header goes up by one. Open its inventory: the rucksack still has its contents (the nested-apply path, `OVT_TEST_InitSuite.c:1173-1224`).
25. Repeat to the 16 cap, and again with less cash than the quote → each refusal names its reason, nothing spawns, cash unchanged.
26. Use the plain **Recruit Civilian** action, still with no other recruits out → it also appears at the tent (F18).
27. Now buy one **with two recruits standing 100 m away** → expect the new recruit to spawn at the tent and then run to them. That is the separate AI-regroup bug of §P9.7, not this feature; confirm it and file it.
28. **MP:** on `tools/launch-server.sh`, client 1 buys an equipped recruit — the price shown equals the money taken; client 2 can neither quote nor buy client 1's loadouts (the picker only ever lists its own — `OVT_LoadoutsContext.c:109`).
29. **Pad-only:** open the picker from the tent, walk the list, read the price, buy, close. No mouse.

---

## 5. Key Technical Decisions

| # | Decision | Rationale |
|---|---|---|
| **D1** | Inactive state is a field on `OVT_RecruitData`, not a separate table | It is a property of the recruit, travels free through the three collections that already exist, and cannot desync from the record it describes. *(User decision.)* |
| **D2** | A **new** broadcast RPC for the state change, not a 9th parameter on `RpcDo_RecruitUpdated` | That handler is at the documented 8-parameter limit. `Rpc()` has an untyped variadic prototype, so a wrong arity compiles clean and dies silently at the wire (BUG-090). |
| **D3** | Serializer v3 with `inactive` appended last and a `version < 3` clear on read | Binary contexts are positional; append-only is the only safe change. Clearing rather than trusting an unwritten field is the rule v2 already set. |
| **D4** | Client→server ops go on a new `OVT_RecruitCommandComponent` on `OVT_OverthrowController` | `OVT_PlayerCommsComponent` is deprecated and its recruit handlers are the epic's worst client-trust seam (`RpcAsk_DismissRecruit` validates nothing at all). *(User decision.)* |
| **D5** | Sender identity from `ResolveOwningPlayerId()`, never the payload | The controller entity the handler ran on *is* the identity. This is the established pattern across five components; the legacy `ResolveSenderPlayerId` only works on a component that sits on the player's character. |
| **D6** | The inactive group is a spawned, marked, non-playable, delete-when-empty `SCR_AIGroup`; there is **no registry** | The entity is the record. A registry is a second truth that can desync and can hold pointers to a group class that self-deletes. `m_bPlayable 0` keeps it out of everyone's group list (`SCR_AIGroup.c:3255`). |
| **D7** | Lifecycle policy stays `Manual`; the group is untracked from persistence | `ProximityDriven` would despawn the recruits at 800 m (`SCR_AIGroup.c:2989-3020`). The group is session-scoped by construction and rebuilt on the next boot — the same reasoning that untracks waypoints (BUG-118). |
| **D8** | Inactive groups are **reconstructed by clustering** on respawn, not persisted | The grouping has no player-observable identity; persisting it would add a format, an orphan class and a way for the save to disagree with the world, to reproduce something the clustering routine reproduces for free. |
| **D9** | Defend-in-place via `SpawnDefendWaypoint` on the group | Reuses the mod's own garrison precedent (`OVT_ResistanceFactionManager.c:1036-1049`) rather than inventing behaviour. *(User decision: defend in place.)* |
| **D10** | Cluster radius 50 m, first suitable host wins | Matches the fast-travel recruit radius already in the codebase, and "first suitable" keeps the selection deterministic and therefore reproducible in a failing play-test. *(User decision: cluster nearby inactives.)* |
| **D11** | Armed/ammo/wounded computed **server-side** and pushed to the owner every 10 s | Inactive recruits are by definition unstreamed; a client-side read would report every distant recruit as unarmed. One source keeps the map tag and the roster row consistent. |
| **D12** | Status delivery is one 3-scalar owner-targeted RPC per recruit, not a batched array RPC | Array parameters work in vanilla RPCs but are unproven in this codebase; three scalars of proven types cost ~1.6 RPCs/s/player at the cap and carry no new wire risk. |
| **D13** | The swap uses `TrySwapItemStorages` / `TryMoveItemToStorage` on real entities and never spawns or deletes | Duplication and loss become impossible by construction rather than by care. The loadout engine's capture/apply path is explicitly not the foundation (BUG-042/044 live there). *(User decision.)* |
| **D14** | Localization goes into `Language/localization_Overthrow.st` **only**; layouts and prefab `UIInfo` carry literal English until the user regenerates | The runtime `.conf` exports are Workbench-generated, their `Ids{}`/`Texts{}` blocks are not parallel, and hand-editing has silently corrupted six files before. |
| **D15** | Placeholder icons alias existing atlas regions and look obviously wrong | Two entries may share a `Pos`, so placeholders cost no atlas edit; wrong-looking art makes the missing-art state visible instead of plausible. *(User decision: plan for placeholder art.)* |
| **D16** | The roster's selection model is replaced with a flat ordered entry array | The existing sibling walk assumes one container and no headers. Patching it around two sections would leave gamepad navigation depending on widget-tree order — the thing the change breaks. |
| **D17** *(P9)* | The purchase and quote ops go on the **existing** `OVT_RecruitCommandComponent`, not a new controller component | It is a recruit command from a client, and that component already owns `ResolveOwningPlayerId()`, the `RESULT_` vocabulary, the reason-key mapping and both listen-server shapes. A seventh copy of the resolve helper is the thing worth avoiding, not four more RPCs. The cost — the `Rpc()` ledger grows from five call sites to nine — is paid by keeping every signature scalar/`RplId`/string and extending the header ledger in the same commit (BUG-090). |
| **D18** *(P9)* | **The server quotes; the client never prices anything**, and the fee multiplier is therefore never replicated | Loadout JIP carries metadata only — `m_aItems` is empty on every client (`OVT_LoadoutManagerComponent.c:2153-2180`, `:2248`, class comment `:40`). A client could not sum item prices if it wanted to. This turns a hazard into a non-event: no new field enters the hand-rolled config bitstream, `CONFIG_STREAM_VERSION` stays at 2 (`OVT_OverthrowConfigComponent.c:558`), and BUG-078's failure mode is not reachable. |
| **D19** *(P9)* | **Quote == charge**, with two exceptions: an unpriceable item refuses the purchase before anything spawns, and a kit that lands *nothing* is not charged for | One number shared by the preview, the funds check and the charge is what makes the feature testable and the preview trustworthy. Per-item refunds were rejected: `ApplyEquipmentToEntity` reports success only at the **top level** (`:457-471`) because `ApplyNestedItemsSpawn*` return `void` (`:1603-1621`), so a per-item refund would itself be wrong for container contents — and a wrong refund is worse than a price the player was shown before pressing the button. A partial apply is charged in full, logged per item (`:1516`, `:1523`, `:1542`, `:1561`) and reported with counts. |
| **D20** *(P9)* | An item with **no catalog price refuses the whole purchase**, naming the item — it is never priced at zero | `GetPrice` cannot say "unknown": it returns **500** for any id it has not heard of (`OVT_EconomyManagerComponent.c:209-213`), and `GetInventoryId` resolves an unregistered prefab to **id 0 — some other item's price** (its own doc comment, `:1679-1691`). The alternatives are "hand out free gear" and "charge a randomly wrong price", both of which are the failure class this feature exists to avoid (BUG-042). The refusal shows up in the **quote**, before the player commits, and it is actionable: re-save the loadout without that item. |
| **D21** *(P9)* | The picker is **purchase mode on the existing loadouts screen**, not a new context | That screen already lists the player's own loadouts with a selection model, pad bindings and a details pane, and it already hides button groups by name (`OVT_LoadoutsContext.c:287-295`, `:593-601`). Purchase mode is one setter mirroring `SetEquipmentBox` (`:326-329`) plus one button — a smaller and better-tested surface than a new layout, context class, prefab entry and input context. |
| **D22** *(P9)* | Tent-recruit **spawn internals move onto `OVT_RecruitManagerComponent`**, and the legacy handler is refactored onto them | They live inside `RpcAsk_RecruitFromTent` on the deprecated comms component (`OVT_PlayerCommsComponent.c:1486-1531`); the project rule forbids adding an op there, and duplicating them would mean the placement fix has to be made twice and stay made twice. Moving them is the only option that leaves one implementation — and the manager is where every other recruit spawn already lives (`SpawnRecruit` `:899`, `SpawnRecruitBody` `:1196`). |
| **D23** *(P9)* | The purchase pays the **normal tent recruit cost plus** `Round(gear subtotal × fee)`, and consumes a town supporter like any tent recruit | It is a tent recruit that happens to arrive dressed. Charging for gear alone would make the equipped purchase cheaper than the plain one at low kit values, and skipping the supporter would make it a way around the town-support economy. The fee multiplies the **gear only**, per the requirement. |

---

## 6. Definition of Done

### Functional

- [ ] **F1** Holding the deactivate action on an owned, active recruit removes it from the player's group. It stops taking orders, stops following, and stays where it was.
- [ ] **F2** The deactivated recruit holds position and returns fire when attacked; it does not wander.
- [ ] **F3** Deactivating a second recruit within 50 m of an inactive one puts them in the **same** AI group; deactivating one further away creates a new group.
- [ ] **F4** Holding the reactivate action puts the recruit back in the player's group and it resumes taking orders.
- [ ] **F5** The recruits screen shows an **Active** section and an **Inactive** section, each recruit in the correct one, with empty sections hidden, and a `X / 16` capacity header that counts inactive recruits.
- [ ] **F6** The roster's toggle button performs the same transition as the character action, for the selected recruit.
- [ ] **F7** Each roster row shows the recruit's armed/ammo status and, when wounded, a wounded indicator.
- [ ] **F8** An inactive recruit still counts toward the 16 cap: at 15 active + 1 inactive, recruiting a 17th is refused.
- [ ] **F9** The map shows a marker for every one of the player's own recruits: active at full opacity, inactive dimmed, with no tag when unarmed, an ammo tag when armed with ammo, and a crossed-out ammo tag when armed and dry.
- [ ] **F10** The map layer-filter panel has a **Recruits** row that hides and shows those markers, and the choice survives a restart.
- [ ] **F11** Holding the swap action on an active owned recruit exchanges the player's entire loadout with the recruit's — clothing, vest, backpack, weapons and carried items.
- [ ] **F12** Fast travel with recruits takes active recruits only; inactive ones stay put.

### Quality

- [ ] **Q1** No recruit is ever lost, duplicated or stranded by any transition. After a full deactivate/reactivate cycle the roster count is unchanged and every recruit has exactly one body.
- [ ] **Q2** No orphaned `SCR_AIGroup` and no orphaned `AIWaypoint` survives the destruction of an inactive group.
- [ ] **Q3** No item is created or destroyed by the swap. Count items on both characters before and after: `before(player) + before(recruit) == after(player) + after(recruit)`, and every individual item is on one of the two characters or on the ground beneath them.
- [ ] **Q4** Every new client→server RPC derives the actor from the controller and re-validates ownership; no handler acts on a player id supplied by the caller.
- [ ] **Q5** Every new client-side dereference of the manager, config, controller component or recruit record is null-guarded (JIP arrives before they exist).
- [ ] **Q6** Existing recruit persistence tests stay green: `OVT_TEST_Persistence_Recruits_RoundTrip` and `OVT_TEST_PersistenceRoundTrip_Recruits_SurvivesSaveAndReload`.
- [ ] **Q7** BUG-107 fixed and BUG-107's file closed; `FindRecruitEntity` no longer removes from the map it is iterating.
- [ ] **Q8** No `#OVT-...` key renders raw on screen; no runtime `localization_Overthrow.<lang>.conf` file is modified.
- [ ] **Q9** `grep` proofs pass: `SpawnEntityPrefab|TryDeleteItem|DeleteEntityAndChildren|TrySpawnPrefabToStorage` absent from `OVT_LoadoutSwap.c`; `SetLifecyclePolicy|CleanupGroup|CleanupEntity` absent from the recruit manager.

### Integration

- [ ] **I1** **JIP:** a client joining a session where recruits are already inactive sees them in the Inactive section and dimmed on the map, with correct status tags.
- [ ] **I2** **MP live:** a state change made by the owner is reflected on a second client's replica of the table (both clients see the same roster contents).
- [ ] **I3** **Owner offline/return:** an inactive recruit whose owner disconnects is reserved with the other bodies and, on return, comes back **inactive and in an inactive group** — not in the owner's squad.
- [ ] **I4** **Save/continue:** quit to menu and Continue; inactive recruits are still inactive, still where they were, still carrying their gear, and in an inactive group.
- [ ] **I5** **Backward load:** a save written before this feature (payload version 2) loads without error and every recruit comes back **active**.
- [ ] **I6** **Group sharing:** joining another player's group takes only active recruits; leaving it returns only active recruits. Inactive ones are untouched in both directions.
- [ ] **I7** The map layer's filter row coexists with the Players row and the location-type rows; toggling one does not affect another.

### Verification Method

**Automated (orchestrator, after each phase completes — never inside an agent):**

1. `tools/compile-check.sh` → exit 0.
2. `tools/run-tests.sh "{6A6E2A002F53A581}"` (All group — persistence state is touched) → exit 0. Artifacts in `.tmp/run-tests/`.
3. Grep proofs Q9.

**Single-player play-test (F1-F8, F11, F12, Q1-Q3):**

1. Start a campaign, recruit three civilians in one town.
2. Note the roster count and the capacity header.
3. Walk two of them 20 m apart; hold the deactivate action on each. → both leave the group; both hold position; the roster shows them under **Inactive**; the capacity header still reads `3 / 16`.
4. Open the group menu → there is no extra selectable group in the list. **(Q2 / D6.)**
5. Walk 200 m away, deactivate the third. → a separate group; it holds position there.
6. Reactivate the two clustered ones one at a time. → each rejoins the squad; after the second, nothing remains at that spot.
7. Fire at an inactive recruit from a distance → it returns fire and does not chase. **(F2.)**
8. Recruit up to the cap with one recruit inactive → the 17th is refused. **(F8.)**
9. Note the item count on the player and on an active recruit (open both inventories and count). Hold the swap action. → kits exchange; recount; totals match, nothing on the ground unaccounted for. **(F11 / Q3.)**
10. Fast travel with recruits from beside an inactive one → it does not travel. **(F12.)**
11. Open the map: active recruits follow the player, the inactive one sits still and dimmed; give a recruit a weapon with no magazines → its tag becomes the crossed-out ammo icon. **(F9.)**
12. Layer panel → toggle **Recruits** off and on; quit to menu, restart, reopen → the choice held. **(F10.)**
13. Pad-only pass over the roster: walk both sections, toggle a recruit, close. **(F5, F6.)**

**Persistence (I3, I4, I5):**

14. With two recruits inactive, quit to menu and Continue. → both still inactive, in place, with their gear, in an inactive group (verify by walking off and back: they are still there and still defending).
15. `git stash` the feature branch, start a campaign, recruit two, save, restore the branch, Continue. → loads clean, both recruits active. **(I5.)**

**Multiplayer (I1, I2, I6):**

16. `tools/launch-server.sh`
17. `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`
18. `tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001`
    (**Always pass a long `--timeout`** — the default 600 s kills the client mid-test.)
19. Client 1 recruits two and deactivates one. Client 2 opens its roster → sees client 1's recruits with the same active/inactive split. **(I2.)**
20. Client 2 disconnects and rejoins → same state on arrival. **(I1.)**
21. Client 1 joins client 2's group → only the active recruit follows; the inactive one stays. Client 1 leaves → only the active one returns. **(I6.)**
22. Client 1 disconnects for 11 minutes, then rejoins → the inactive recruit is back, still inactive, still in place. **(I3.)**

---

## 7. Quality Bar

This feature is half UI and half systems, and the two halves are judged differently.

**UI half — gamepad/console usability and visual clarity**

- Every new control is reachable and operable with a pad alone. The sectioned roster is the specific risk: the flat entry array of D16 exists so that `MenuUp`/`MenuDown` never depend on widget-tree order.
- The new roster button gets both a keyboard binding and a pad binding, registered in the action **and** in `OverthrowRecruitsMenuContext` — a missing `ActionRefs` entry is a silently dead pad button.
- Status icons must be readable at a glance and distinguishable from each other at map-marker size. Inactive is communicated by **opacity plus section membership**, never by opacity alone.
- No raw localization key ever reaches the screen; literal English is correct until the exports are regenerated.
- An unavailable feature shows **no control** rather than a dead one — the rule the players filter row already encodes.

**Systems half — authority, replication, and item safety**

- The server decides every state change. Clients ask; they never assert, and they never optimistically mutate their replica (the existing rename path does; do not copy it).
- Every RPC handler derives its actor from the controller entity and re-validates ownership, liveness and current state before acting.
- Group lifetime is owned end to end: created deliberately, destroyed deliberately or by a vanilla mechanism we have verified, with no path that leaves an empty group or a widowed waypoint.
- The swap's safety is **structural**, not procedural: because nothing spawns and nothing is deleted, the worst reachable outcome is an item on the ground. The grep proof in Q9 is what keeps it that way through future edits.
- New decision logic goes in world-free classes so the Logic tier can pin it. Nothing in `OVT_RecruitInactiveGrouping` or `OVT_RecruitStatus` may ever grow a manager lookup or an entity dereference.

---

## 8. Testing Strategy

### Logic tier (Fast) — `Scripts/Game/Tests/TestSuites/Logic/`

| File | Covers |
|---|---|
| `OVT_TEST_Logic_RecruitClustering.c` *(new)* | `OVT_RecruitInactiveGrouping.SelectClusterCandidates`: self excluded, active excluded, offline excluded, radius in/out, empty input, null holes in the table skipped without counting, table-order stability |
| `OVT_TEST_Logic_RecruitStatus.c` *(new)* | `OVT_RecruitStatus.Derive` over the full flag matrix; `TagIcon` empty for unarmed, `recruit_ammo` armed+ammo, `recruit_ammo_empty` armed+dry; predicates on a zero mask |
| `OVT_TEST_Logic_GroupRecruits.c` *(extend)* | `SelectTransferable` now excludes inactive recruits, with the skip counted separately from the offline skip |

⚠️ **Tier rule:** the Logic directory is grepped for Overthrow's static manager accessor and the engine's game-mode getter; neither identifier may appear anywhere under `TestSuites/Logic/`, *including in comments*. `new` applies no `[Attribute]` defvalues, so every field a case reads must be set explicitly. Floats compared with `OVT_TEST_LogicFixture.EPSILON`. ⚠️ `vector.Distance` is not correctly rounded — keep radius cases well clear of the exact boundary.

### Persistence tier (All)

| File | Covers |
|---|---|
| `OVT_TEST_PersistenceSuite.c` *(extend `OVT_TEST_Persistence_Recruits_RoundTrip` :551)* | `m_bInactive` set and read back through the public manager API |
| `OVT_TEST_PersistenceRoundTripSuite.c` *(extend `..._Recruits_SurvivesSaveAndReload` :1022)* | the inactive flag survives save → dirty → re-apply |

### Init tier (Fast)

Add to `OVT_TEST_InitSuite.c`: `OVT_Global.GetRecruitCommands()` resolves on a started session (this is the component-on-the-prefab trap's fail-proof — until `OVT_OverthrowController.et` carries the component the case fails, which is exactly what it is for).

### Every new case must be proven able to fail once

For each new case, break the subject deliberately (invert a comparison, drop the flag write, remove the exclusion), confirm the case fails, restore, confirm it passes, and record the method in a preamble comment. **No `maxAttempts`** — a case that needs retries is a bug in the case.

### Not automatable, and why

| Area | Why | Substitute |
|---|---|---|
| Group insertion/exit, defend behaviour | needs a live world, AI agents and vanilla group state | play-test steps 3-7 |
| JIP and MP replication of the flag | the test world has one machine | play-test steps 16-22 |
| Map marker rendering, opacity, tags | no headless widget assertions exist | play-test steps 11-12 |
| Roster layout and gamepad navigation | same | play-test step 13 |
| The swap | needs two live inventories and real items; `ScriptBitWriter` cannot round-trip from script, so the wire cannot be unit-tested either | play-test step 9 with the item-count invariant |
| The true quit-and-continue restart | `SaveGameManager.Load` restarts the autotest harness | play-test steps 14-15 |

---

## 9. Dependencies

### Internal

- **`resistance/recruits`** — the manager, record, JIP payload, broadcast RPCs, body reservation/respawn path, `OVT_GroupRecruitTransfer`. Every phase touches it; Phase 2 changes its respawn fork, which is the code BUG-130/131 fixed.
- **`core/player-groups`** — `MoveRecruitsToGroup` / `RemoveRecruitsFromGroup` are driven by its `OnGroupPlayerAdded/Removed` hooks; Phase 2 changes what those transfer.
- **`resistance/core`** — the garrison group + defend-waypoint precedent (`OVT_ResistanceFactionManager.c:1021-1052`) and `OVT_OverthrowConfigComponent.SpawnDefendWaypoint` :468.
- **`map/map-layers`** — the filter panel, prefs store and the `OVT_MapPlayerLocation` template. Phase 6 adds the panel's **second** hand-built row; the first one's comments are the spec.
- **`core/game-mode` / `OVT_OverthrowController`** — the controller prefab and the five existing controller components (pattern source).
- **`resistance/loadouts`** — only as a source of enumeration helpers (`ExtractEquippedItems`, `FindMatchingStorage`) and as the thing the swap deliberately does **not** build on.
- **`resistance/wanted-system`** — untouched, but note BUG-074 (listen-host loot events flag seen recruits): the swap must not route through loot paths.
- **Vanilla persistence** — serializer binding is already in `Configs/Systems/Persistence/Overthrow.conf:34`; a version bump needs no config change.

### External

None. No new mod dependency; EPF/EDF remain retired.

---

## 10. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **An emptied inactive group leaks**, or worse, deletes its members. | Medium | Stranded AI or lost recruits | Lifetime is vanilla's `m_bDeleteWhenEmpty` (verified `SCR_AIGroup.c:96`, `:2442-2455`) plus one explicit guard for the create-then-fail case vanilla does not cover. `CleanupGroup`/`CleanupEntity` are banned in writing (T2.6) because they delete members. Verified by play-test step 6 and Q2. |
| **R2** | **A ProximityDriven policy despawns inactive recruits** at 800 m if anyone ever calls `SetLifecyclePolicy`. | Low now, higher later | Recruits vanish when you walk away | Default is `Manual` and `EOnFrame` only ticks ProximityDriven groups (`SCR_AIGroup.c:313-318`). The prohibition is documented on the method and grep-checked in Phase 2's acceptance criteria. |
| **R3** | **The respawn fork regresses BUG-130/131.** The reservation flow is the most carefully engineered code in the feature. | Medium | Recruits lose gear or fail to return | The fork is one method (`PlaceRecruitInWorld`) inserted at exactly the two existing call sites; nothing in `ReserveRecruitBody`/`UnreserveRecruitBody`/`RequestPersistedRecruitBody` changes. Play-test steps 14 and 22 exercise both the save path and the 10-minute offline path. |
| **R4** | **Reconstructed clusters differ from the originals** after a load, because bodies arrive asynchronously. | High (by design) | Cosmetic: a garrison splits into two groups | Accepted and documented in §3.3. The grouping has no player-visible identity; positions and behaviour are unchanged. |
| **R5** | **Swap fails midway**, leaving a half-swapped pair. | Medium | Confusing state | Pair-first ordering means most of a swap is single atomic `TrySwapItemStorages` calls; a rollback journal restores any failed step; the last resort drops to the ground. Nothing is deleted, so nothing is unrecoverable. Every failure logged with the prefab name (T7.7). |
| **R6** | **Clothing does not fit** (blocked areas, body-type restrictions), so parts of the swap silently no-op. | Medium-High | Partial swap | `IsAreaBlocked`/`GetBlockedSlots` handled by outermost-first ordering; a garment that cannot be worn falls back to the recipient's container storage, then to the ground. The result reply distinguishes "complete" from "partial" so the player is told. |
| **R7** | **Gamepad navigation breaks** in the sectioned roster. | Medium | The screen becomes console-hostile | The selection model is replaced with a flat ordered array (D16) rather than patched; T5.2 is sequenced first and pad-verified before anything else is added; play-test step 13 is pad-only. |
| **R8** | **Status push cost** at 6 players × 16 recruits. | Low | Bandwidth | 3 scalars per recruit per 10 s, owner-targeted, skipped for offline owners and body-less recruits — ~1.6 RPCs/s/player at the cap. If it ever matters, the interval is one constant. |
| **R9** | **A stale marker or status entry** for a removed recruit. | Medium | Ghost marker on the map | Both the widget map and the status cache are keyed by recruit id and pruned from `m_OnRecruitRemoved`; the map layer rebuilds its widget set on roster change. |
| **R10** | **The new controller component is not added to the prefab**, so every request silently no-ops. | Medium | Feature appears broken with no error | T3.2 is an explicit task, and the Init-tier case in §8 fails until it is done. This exact trap is documented at two existing call sites. |
| **R11** | **Discovered, out of scope:** `RpcAsk_DismissRecruit` (`OVT_PlayerCommsComponent.c:2018-2059`) performs **no ownership validation at all** — any client can dismiss any recruit by id. | Certain (present today) | Grief vector | Not fixed here (scope). **File as a new bug** (next id after BUG-165) linked to `resistance/recruits`, referencing the rename handler at :1994 as the shape of the fix. |
| **R12** *(P9)* | **A new free-item endpoint.** Phase 9 re-opens a network route to the loadout engine's *spawning* apply — the endpoint deleted as unsafe (`OVT_PlayerCommsComponent.c:1693-1695`, BUG-043). | Medium | Economy-breaking exploit | Five independent server-side gates: identity from `ResolveOwningPlayerId()` and never the payload; the loadout is fetched by the **resolved** persistent id, so another player's kit is unreachable; the apply target is the body the same handler spawned two lines earlier and is never named by a client; the price is computed server-side from the same record; and the transaction consumes a town supporter and a slot against the 16 cap, so the campaign itself rate-limits it. Two acceptance greps pin the last two. |
| **R13** *(P9)* | **`ClearEntityEquipment` deletes the target's kit** (`OVT_LoadoutManagerComponent.c:450`, `:1278-1303`). Pointed at an existing recruit it destroys paid-for gear. | Low, catastrophic | Irrecoverable item loss | The apply target is a local variable produced by `SpawnTentRecruit` in the same handler; no code path lets a client name it. Written as a prohibition in the handler's doc comment and pinned by an acceptance grep (exactly one `ApplyLoadoutToEntity` call site in the component). |
| **R14** *(P9)* | **Price/charge drift** between what the picker shows and what the server takes. | Low | Player distrust | The client never computes a price (D18), and the quote runs the *same* validation and the *same* pricing function the purchase does. The only way they can differ is a world change between quote and buy (town stock, the player's price multiplier) — bounded, and the reply carries the amount actually charged, so a mismatch is visible rather than silent. |
| **R15** *(P9)* | **The `RpcAsk_RecruitFromTent` refactor regresses plain tent recruiting** — a shipped path with a supporter cost and an orphan-cleanup branch. | Medium | A core loop breaks | The move is mechanical: `ValidateTentRecruit` + `SpawnTentRecruit` are the existing lines, and money and supporters stay in the caller. Play-test step 26 exercises the plain action specifically, and an acceptance grep proves no `SpawnRecruit` call is left in the comms component. |
| **R16** *(P9)* | **Loadouts under-represent the kit** (BUG-044: stowed weapons are never captured), so a purchase can feel like it short-changed the player. | High (present today) | Confusion, not loss | Out of scope to fix. The price is computed from *the record*, so the player is charged for exactly what they receive, and the quote shows the item count so a suspiciously small loadout is visible before buying. T9.16's Field Manual text says the price covers what the loadout recorded. |
| **R17** *(P9)* | **The placement fix looks like it failed**, because a correctly-placed recruit still runs off to its formation slot (§P9.7). | High during play-test | A good change gets reverted | §P9.7 states the mechanism with citations and play-test step 23 mandates **zero active recruits** for the placement check; step 27 then reproduces the run deliberately so it is recorded as the separate bug it is. The AI-regroup bug is filed, not fixed, in this phase. |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Data model, replication, serializer v3, quick wins | `component-developer` | no |
| 2 — Inactive group mechanics (server) | `component-developer-advanced` | **yes** — entity lifetime across a self-deleting group class, respawn-path fork, slave-group exit refactor |
| 3 — Controller component and user actions | `network-specialist` | no |
| 4 — Status derivation and owner-targeted push | `component-developer` | no |
| 5 — Recruits screen | `ui-developer` | no (T5.2 sequenced first and pad-verified) |
| 6 — Map marker layer | `ui-developer` | no |
| 7 — Loadout swap | `component-developer-advanced` | **yes** — live item entities across two replicated inventories, no transactions, silent failure modes |
| 8 — Help and documentation sync | `help-docs-sync` | no |
| 9 — Buy equipped recruits at the tent *(post-ship)* | `component-developer-advanced` (T9.12 → `ui-developer`, T9.16 → `help-docs-sync`) | **yes** — a second money path into a system whose first one is legacy and unvalidated, re-opening a network route to the loadout engine's *spawning* apply (the deleted free-item endpoint, BUG-043), plus a refactor of the shipped tent-recruit handler |

**Skills to activate:** `enforcescript-patterns` (all phases), `overthrow-architecture` (1, 2, 3, 4, 7, 9), `overthrow-ui-patterns` (5, 6, 9), `workbench-workflow` (2, 5, 6, 7, 9).
