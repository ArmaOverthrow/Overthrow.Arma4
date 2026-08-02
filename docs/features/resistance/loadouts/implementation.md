# Resistance Loadouts - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** ~2025 (loadout persistence rebuilt 2026-08-02 during the vanilla-persistence migration)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02

---

## Executive Summary

The Loadouts system lets players snapshot their character's equipment as a named loadout and re-apply it later — to themselves or to their AI recruits — at any placed **Equipment Box**. A game-mode manager (`OVT_LoadoutManagerComponent`) owns the store: capture walks the character's inventory into a recursive `OVT_PlayerLoadout` data tree (weapons + attachments, clothing, nested container contents, quick slots, damaged-item health), and apply reverses it **by consuming matching items from the equipment box**, slot-exact where possible. Officers can additionally save "officer template" loadouts intended to be visible to everyone.

All mutation is server-side behind four `RpcAsk_*` endpoints on `OVT_PlayerCommsComponent`; clients hold a name-only index kept in sync by broadcast RPCs and JIP `RplSave/RplLoad`. Loadouts persist — index *and* contents — inside the manager's own record via `OVT_LoadoutManagerSerializer` (contents survive sessions only since the 2026-08-02 vanilla-persistence migration; under EPF they never came back).

Two things the shape of the code promises but doesn't deliver: officer templates are saved, flagged, persisted and JIP-shipped but **no listing path ever shows them to anyone but their author**, and the box-apply flow **spawns** the in-hands weapon (and nested attachments) from prefabs instead of consuming them from the box — a free-item source.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- One-interaction save ("what I'm wearing right now") and one-interaction re-equip at any equipment box, for the player and for nearby recruits (single or all-at-once).
- Fidelity: attachments, nested container contents (recursive), quick-slot assignments, exact slot placement, damaged-item health.
- Conservation: apply moves items **out of the box** and swaps replaced gear **into the box** — no free gear (see Known Issues for where this leaks).
- Multiplayer-correct: server-authoritative apply, client name-index via broadcast + JIP, per-player ownership.

### Success Criteria
- [x] Capture engine: equipped weapon + attachments, character root storage, recursive nested containers, quick slots, health property
- [x] Box-apply engine: recursive box search, slot-exact placement (priority/purpose/index), swap-into-box, container-emptying before transfer
- [x] Recruit handoff: apply to selected / all nearby recruits from the loadouts UI, with per-recruit notifications
- [x] Persistence: index + contents in one record, versioned, idempotent re-apply (v2, 2026-08-02)
- [x] JIP: full metadata list on join; save/delete broadcasts keep clients current
- [ ] Officer templates visible to non-officers (flag exists end-to-end; no listing path — inert)
- [ ] Strict conservation (equipped weapon and nested attachments are spawned, not consumed from the box)
- [ ] RPC validation (any client can save/load/delete as any player; the spawn-mode endpoint conjures items)

---

## Current Architecture

### Key Components
- `Scripts/Game/GameMode/Managers/OVT_LoadoutManagerComponent.c` (2103 L) — the heart. Singleton on `OVT_OverthrowGameMode` (`OVT_Global.GetLoadouts()`). Owns `m_mActiveLoadouts` (the store, keyed `"<persistentId>_<loadoutName>"`) and `m_mLoadoutIdMapping` (the name index shipped to clients). Capture (`ExtractEquipmentFromEntity` + helpers), apply (`ApplyEquipmentFromBox` box mode / `ApplyEquipmentToEntity` spawn mode), notifications, broadcasts, `RplSave/RplLoad`, and the persistence apply methods (`ApplyPersistedLoadoutIds`/`ApplyPersistedLoadouts`).
- `Scripts/Game/Data/OVT_PlayerLoadout.c` — plain data class (deliberately: clients construct these in `RplLoad`, and the old EPF base class reached for a server-only singleton). Name, owner, description, timestamps, template/officer-template flags, items, quick-slot prefab names, metadata. `GetDeterministicId()` derives the stable storage id from owner + sanitized name.
- `Scripts/Game/Data/OVT_LoadoutItem.c` — one item: prefab `ResourceName`, exact slot (`m_iSlotIndex`) + storage identity (`m_iStoragePriority`, `m_eStoragePurpose` — the EPF-style addressing), `m_bIsEquipped`, attachments (prefab strings), custom properties map (only `"health"` is ever written), recursive `m_aChildItems`.
- `Scripts/Game/Data/OVT_LoadoutMetadata.c` — category/version/favorite/tags exist but only `m_iUsageCount` is ever written (JIP and persistence deliberately carry just that).
- UserActions on the equipment box prefab (`Prefabs/Props/Military/AmmoBoxes/OVT_AmmoBox_Placed.et:38-64`): `OVT_SaveLoadoutAction.c` (name dialog → `comms.SaveLoadout`), `OVT_SaveOfficerLoadoutAction.c` (officer-gated, reuses the RENAME_RECRUIT dialog preset), `OVT_LoadLoadoutAction.c` (opens the UI context, local-only).
- `Scripts/Game/UI/Context/OVT_LoadoutsContext.c` (653 L) — loadout list (own loadouts only), apply/delete buttons, nearby-recruit discovery (5 m sphere query for characters whose `OVT_PlayerOwnerComponent` uid matches), apply-to-selected / apply-to-all. `Scripts/Game/UI/Components/OVT_LoadoutListEntryHandler.c` — list row (officer-template labelling is a TODO at :24).
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:1016-1158` — the RPC seam: `RpcAsk_SaveLoadout`, `RpcAsk_LoadLoadout` (spawn mode — **no in-repo caller of the client wrapper**), `RpcAsk_LoadLoadoutFromBox` (box + target by `RplId`), `RpcAsk_DeleteLoadout`.
- `Scripts/Game/Persistence/Serializers/Components/OVT_LoadoutManagerSerializer.c` (433 L) — versioned (v2) positional record: index as two parallel string arrays + `array<ref OVT_PersistedLoadout>` with full recursive contents. Registered in `Configs/Systems/Persistence/Overthrow.conf:42`.
- Adjacent but distinct: `Scripts/Game/Configuration/OVT_LoadoutConfig.c` (`OVT_LoadoutSlot`) is the authored **civilian spawn loadout** used by spawn logic and `OVT_Global.ApplyCivilianLoadout` — same word, different system (belongs to spawn/recruit flows).

### Data Flow
1. **Save:** box interaction → name dialog (1-32 chars, client-validated only) → `comms.SaveLoadout(playerId, name, "", isOfficer)` → server resolves the player's controlled entity and calls `SaveLoadout`. Capture order: equipped weapon in hands (+attachments) → quick slots (10 prefab-name strings) → each character root-storage slot (clothing etc.), recursing into containers and capturing per-item attachments/health. Store + index updated, `RpcDo_LoadoutSaved` broadcast (clients add a name-index placeholder).
2. **Apply to self:** UI → `comms.LoadLoadoutFromBox(playerId, name, boxRplId, targetRplId)` → server `LoadLoadout(...)` → `ApplyEquipmentFromBox`. Per item: recursive `FindItemInBox` by prefab name → if the found item is a container, empty its contents back into the box first → remove any current occupant of the exact target slot → move box item into `FindMatchingStorage(priority, purpose)` at `m_iSlotIndex` → swapped-out item goes into the box (deleted if the box is full) → restore the container's children from box stock only ("never spawn" in nested box mode — except attachments, see Known Issues). Quick slots re-linked by prefab match. Success/partial counts drive `LoadoutApplied`/`LoadoutAppliedPartial` notifications.
3. **Apply to recruits:** same RPC per recruit entity (target = recruit, discovered client-side within 5 m). Recruit identity comes from `OVT_PlayerOwnerComponent`/`OVT_RecruitData`; AI targets skip quick-slot application (`AIControlComponent` check). Notification variant `LoadoutAppliedToRecruit` includes the recruit's name. Recruit lifecycle itself belongs to `resistance/recruits`; this feature only equips the body it is handed.
4. **Spawn mode** (`ApplyLoadoutToEntity`): clears **all** equipment (deleting it) then spawns every item from its prefab. Only reachable through `RpcAsk_LoadLoadout`; no UI or script in the repo calls the client wrapper — it is a dormant, unvalidated item-printing endpoint.
5. **Delete:** UI → `RpcAsk_DeleteLoadout` → both maps drop the key → notification + `RpcDo_LoadoutDeleted` broadcast. (The UI refreshes its list immediately, before the broadcast lands — see Known Issues.)
6. **Persistence:** `Serialize` writes version, index arrays, then one `OVT_PersistedLoadout` per cached loadout (skipping ownerless ones). `Deserialize` is version-guarded (absent payload leaves the index alone; v1 payloads stop before contents) and idempotent — contents fill existing objects in place, and the index is repaired from the records so a record can never be invisible. Everything is synchronous; there are no queued/async apply mechanics anywhere in the system.
7. **JIP:** `RplSave` ships the whole index plus per-loadout metadata (no item trees); joining clients rebuild metadata-only `OVT_PlayerLoadout`s — enough for the UI, with contents living server-side only.

### Integration Points
- **resistance/building** (sibling): the Equipment Box is a placeable (`Configs/Resistance/placeables.conf` "Ammobox"/`#OVT-Place_EquipmentBox`, cost **$80**); placement rules/undo belong to building. Loadout application itself charges nothing — the box's stock is the cost.
- **resistance/recruits** (sibling): the handoff is `ApplyLoadoutToEntityFromBox(recruitEntity)`; recruit gear then persists with the recruit's stored body, not through this system. `OVT_RecruitData.GetRecruitDataFromEntity` is read for notification names only.
- **resistance/core** (sibling): `OVT_ResistanceFactionManager.IsOfficer` gates `SaveOfficerTemplate` (server) and the officer save action's visibility (client).
- **Economy epic:** no direct coupling — no price checks, no charge on apply. The free-spawn leaks (equipped weapon, nested attachments, spawn mode) are therefore economy bypasses, not economy bugs.
- **Notifications/Players:** `OVT_NotificationManagerComponent.SendTextNotification` with persistent→runtime id translation via `OVT_PlayerManagerComponent`; `OVT_PlayerOwnerComponent` resolves who owns an equipped entity.
- **Vanilla persistence:** one `ScriptedComponentSerializer` in the game mode's `ComponentSerializers` block; no entity rules, no self-spawn — loadouts are pure manager state.

---

## Implementation Details

### Phase 1: Manager + Data Model (COMPLETED)
Store/index maps, `OVT_PlayerLoadout`/`OVT_LoadoutItem`/`OVT_LoadoutMetadata`, deterministic ids, save/delete API, invokers.

### Phase 2: Capture/Apply Engine (COMPLETED)
EPF-style slot addressing (priority + purpose + index), recursive nested-container capture and restore, weapon attachments, quick slots, health property, box-conservation flow with swap-into-box and container-emptying.

### Phase 3: UI + Actions + Recruits (COMPLETED)
Three user actions on the box prefab, loadouts UI context with recruit discovery and apply-to-all, list entry handler (officer labelling still TODO), notifications.

### Phase 4: Multiplayer + Persistence (COMPLETED, REBUILT 2026-08-02)
RPC seam in `OVT_PlayerCommsComponent`, save/delete broadcasts, JIP metadata. Persistence originally rode the EPF scripted-state path whose repository was Print-stub placeholders — **loadout contents had never survived a session**. The vanilla-persistence migration replaced it with `OVT_LoadoutManagerSerializer` v2 (index + contents in one record, positional, versioned, idempotent).

---

## Key Technical Decisions

### Decision 1: The manager cache IS the store
**Context:** EPF-era loadouts split into an id index in the manager plus one database row per loadout, fetched by id — two halves that could disagree, and did (the "find a loadout whose id we lost" fallback was never written).
**Implementation:** `m_mActiveLoadouts` is authoritative; the serializer writes index and contents into the same game-mode record; `DeleteLoadout`/`SaveLoadout` just edit the two maps.
**Trade-offs:** Nothing can disagree with anything; whole store loads at session start (fine at this scale); every loadout of every player lives in memory on the server.

### Decision 2: Items addressed by (storage priority, storage purpose, slot index) — EPF-style
**Context:** Re-applying gear must land items in the *same* slots (uniform vs vest vs backpack), across different character states.
**Implementation:** Each `OVT_LoadoutItem` records the triple at capture; `FindMatchingStorage` re-resolves it at apply, with any-slot-in-same-storage fallback.
**Trade-offs:** Robust to entity churn and works on recruits; silently misplaces items if a different clothing item changes the storage layout; items are matched in the box **by prefab name only**, so a modded/attachment-differing variant is interchangeable with the saved one.

### Decision 3: Conservation via the equipment box
**Context:** Applying a loadout must not create gear the resistance doesn't own.
**Implementation:** Box mode consumes matching items from the box (recursively searched), swaps replaced gear into the box, and nested restore explicitly "never spawns new ones".
**Trade-offs:** The right economy shape — but the in-hands weapon path and the nested-attachment path bypass it by spawning from prefabs (Known Issues #1), and spawn mode bypasses it entirely.

### Decision 4: Clients hold a name-only index; contents never leave the server
**Context:** The UI needs to list loadouts; item trees are large and sensitive to cheating.
**Implementation:** `RplSave/RplLoad` ship index + metadata; `RpcDo_LoadoutSaved/Deleted` maintain it live; `GetAvailableLoadouts` parses owner from the map keys.
**Trade-offs:** Small JIP payload and no client-side item authority; the UI can never show item counts/contents (already a visible TODO), and key-string parsing assumes the persistent id contains no underscore.

### Decision 5: Persistence format mirrors, not reuses, the gameplay classes
**Context:** Binary save contexts are positional; pinning the save layout to a live gameplay class means any field addition silently corrupts old campaigns.
**Implementation:** `OVT_PersistedLoadoutItem`/`OVT_PersistedLoadout` hand-copy every field (`ReadFrom`/`Build`), flatten the properties map to parallel arrays, and version the record (v2 additive over v1).
**Trade-offs:** More code, but the payload is exactly what the file says; the fill-in-place apply keeps live references valid on re-apply.

---

## Current State

### What's Working
- Save → box-apply → recruit-apply round trip works and conserves most items; slot-exact placement with graceful fallback; nested containers and attachments captured recursively; quick slots restored on players (skipped on AI); partial-success notifications; JIP list sync; save/delete broadcasts; persistence of index *and* contents with idempotent re-apply (since 2026-08-02).

### Known Issues
- **Box apply spawns the equipped weapon from thin air** — `ApplyEquipmentFromBox` routes `m_bIsEquipped` items to `ApplyEquippedItem` (`OVT_LoadoutManagerComponent.c:470-473`), which `SpawnEntityPrefab`s the weapon (:1278) and its attachments (:1854-1895) instead of consuming them from the box; the box's copy stays put. Nested-container restore also spawns attachments onto box items (:1633-1636). Save a loadout holding an expensive rifle → repeated applies mint unlimited rifles. **The system's headline exploit.**
- **All four `RpcAsk_*` endpoints trust the client entirely** (`OVT_PlayerCommsComponent.c:1024-1157`): `playerId` is caller-supplied and never checked against the sending connection; no distance/ownership checks on box or target `RplId`s; `RpcAsk_DeleteLoadout` will delete any player's loadout; `RpcAsk_LoadLoadout` (the no-box spawn mode, zero legitimate callers) equips a full saved kit for free onto any player.
- **Officer templates are inert** — the save action, server-side `IsOfficer` gate, flag, broadcast, JIP field and persisted field all exist, but `GetAvailableLoadouts` (:991-1017) filters strictly `keyPlayerId == playerId` and is the *only* listing path; no other player can ever see a template. The list-row officer label is a TODO (`OVT_LoadoutListEntryHandler.c:24`).
- **Stowed weapons are never captured** — extraction reads the in-hands weapon (`ExtractEquippedItems`, :1020-1063 — and only via `weaponManager.GetCurrent()`) plus the character's clothing root storage (:361-411); `EquipedWeaponStorageComponent` is only ever touched at apply time. A slung rifle or holstered pistol silently vanishes from every saved loadout, and its quick-slot entry dangles.
- **Overwrite-save with an empty inventory destroys the old loadout** — `SaveLoadout` removes both map entries *before* extraction (:72-73) and returns on extraction failure (:81-85) without restoring or broadcasting; clients keep a stale index entry that no longer resolves.
- **Client delete refreshes too early** — `OVT_LoadoutsContext.DeleteLoadout` calls `Refresh()` (:344) immediately after firing the RPC, before `RpcDo_LoadoutDeleted` lands, so on remote clients the deleted loadout stays in the list until reopen.
- Save actions hint "saved successfully!" client-side before the server has done anything (extraction may fail); `LoadLoadout` cache-miss (3-arg, :137-144) fails silently with no log or notification.

### Technical Debt
- `m_iLastApplySuccessCount/TotalCount` are manager-level shared mutable state between `ApplyEquipmentFromBox` and the notification sender — apply-to-all-recruits works only because everything is synchronous on one thread.
- `OVT_LoadoutMetadata` (category/version/favorite/tags), `m_bIsTemplate` as distinct from officer template, `m_sDescription`, `RemoveItem`/`GetItem`/`InitLoadout` on `OVT_PlayerLoadout`, and `RemoveAttachment`/`ClearChildItems` on `OVT_LoadoutItem` are all write-only or dead.
- `GetAvailableLoadouts` re-derives owner by string-splitting map keys on `"_"` (safe for GUID-shaped uids, brittle by construction); the loadout name is recovered by substring arithmetic.
- Box search is naive: `FindItemInBox` re-walks the entire (recursive) box per item, and container restore re-searches per child — quadratic on big boxes; item matching ignores attachments/state, so "a" magazine is any magazine.
- The stale header comment at `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceSuite.c:57` still says loadouts "live in a separate scripted-state path whose repository methods are unimplemented" — that path was deleted in the migration.
- The equipment box prefab also carries `OVT_LockVehicleAction`/`OVT_UnlockVehicleAction` (:18-37) — vehicle actions on a box.

---

## Future Enhancements

### High Priority
- [ ] Close the free-item paths: source the equipped weapon and all attachments from the box (consume-or-fail), and remove or validate `RpcAsk_LoadLoadout` spawn mode.
- [ ] Validate the RPC seam: derive the acting player from the sending connection, check box/target proximity and recruit ownership server-side.
- [ ] Capture stowed weapons (iterate `BaseWeaponManagerComponent.GetWeaponsSlots` at extraction, restore per weapon-slot index).

### Medium Priority
- [ ] Surface officer templates: include `m_bIsOfficerTemplate` loadouts from *all* owners in the listing (or a dedicated `GetOfficerTemplates()`), label them in the row handler, and gate deletion to the author/officers.
- [ ] Make overwrite-save transactional (extract first, replace on success) and broadcast the failure case.
- [ ] Refresh the client list from the delete broadcast instead of immediately.

### Low Priority / Nice to Have
- [ ] Loadout details in the UI (item count, last used — data already JIP-shipped); officer-template labelling TODO.
- [ ] Match box items by more than prefab name (attachment set, condition) or document the fungibility as intended.
- [ ] Dead-metadata sweep (`OVT_LoadoutMetadata` fields, `m_bIsTemplate`, descriptions) — implement or delete.
- [ ] Apply-cost hook if the economy epic ever wants paid re-equips.

---

## Testing

### Current Coverage
One Init-tier assertion (`OVT_TEST_InitSuite.c:78`: `GetLoadouts()` resolves). The serializer and the capture/apply engine have **zero** automated coverage, and the Persistence-tier suite header still lists loadouts as deferred for a reason that no longer exists (:57).

### Testing Gaps
- Logic-tier candidates (pure/world-free):
  - `OVT_PersistedLoadoutItem.ReadFrom` ↔ `Build` round trip: recursive children, attachment list, properties-map flattening, mismatched parallel-array truncation (`Build` :101-111).
  - `OVT_PersistedLoadout.ReadFrom` field fidelity incl. metadata usage count and quick slots.
  - `ApplyPersistedLoadoutIds` parallel-array semantics: unequal lengths truncate, empty keys/ids skipped, clear-and-refill idempotency.
  - `ApplyPersistedLoadouts` fill-in-place identity (same object before/after) and index repair for a record missing from the index.
  - `OVT_PlayerLoadout.GetDeterministicId`/`SanitizeForId` (special chars → `_`, stability) and `GetLoadoutKey` ↔ `GetAvailableLoadouts` parsing round trip — including a loadout name containing underscores, and the (currently failing-by-design) uid-with-underscore case.
  - `SetAsOfficerTemplate(true)` implies `m_bIsTemplate`.
- Persistence-tier: a loadout manager record round trip through the save→dirty→re-apply suite (save two loadouts with nested items, wipe the maps, re-apply, assert contents and index) — the seam (`GetActiveLoadouts`/`ApplyPersistedLoadouts`) is public and synchronous.
- Not automatable: actual box item transfer, slot placement on live characters, recruit equip, JIP list — play-testing (two clients for the JIP/broadcast paths).

---

## Documentation

### Current Documentation
- This retrospective plan; extensive in-code documentation on the serializer and the manager's persistence methods (the migration left the "why" in the source).

### Documentation Needs
- Fix the stale deferral note in `OVT_TEST_PersistenceSuite.c:57`; document officer-template status wherever player-facing docs describe it.

---

## Dependencies

### External Dependencies
- Vanilla inventory stack: `InventoryStorageManagerComponent`, `SCR_CharacterInventoryStorageComponent` (quick slots), `UniversalInventoryStorageComponent`, `WeaponAttachmentsStorageComponent`, `EquipedWeaponStorageComponent`, `BaseWeaponManagerComponent`; `SCR_ConfigurableDialogUi` presets (`Configs/UI/Dialogs/DialogPresets_Campaign.conf`, `SAVE_LOADOUT` tag).

### Internal Dependencies
- `OVT_PlayerCommsComponent` (RPC seam), `OVT_PlayerManagerComponent` (persistent↔runtime id), `OVT_ResistanceFactionManager.IsOfficer`, `OVT_NotificationManagerComponent`, `OVT_RecruitData`/`OVT_PlayerOwnerComponent` (recruit identity), building system (equipment box placeable, $80), vanilla persistence config (`Overthrow.conf:42`).

---

## Notes

**Discovered Information:**
- Loadout **contents had never survived a session** before 2026-08-02: the EPF-era repository was Print-stub placeholders and the id-index/record split could not be reconciled. The serializer rebuild (v2) is the most-documented persistence code in the mod — the in-code comments narrate the whole failure history.
- Spawn mode (`RpcAsk_LoadLoadout` → `ApplyLoadoutToEntity` → `ClearEntityEquipment`) has no legitimate caller anywhere in the repo — it is simultaneously dead code and the most dangerous live endpoint.
- Applying a loadout is free by design; the only monetary cost anywhere in the feature is the $80 equipment box placeable.
- "Loadout" is overloaded in this codebase: `OVT_LoadoutConfig`/`OVT_LoadoutSlot`/`ApplyCivilianLoadout` are the civilian spawn kit (spawn logic), not this system.

**Retrospective Assessment:**
- The data model and the persistence rebuild are solid — versioned, idempotent, honestly documented. The capture/apply engine is feature-rich but finished to different depths: the box-conservation principle is clearly the design intent, and the equipped-weapon/attachment spawn paths look like the last unconverted corners.
- The security posture is the epic's weakest: every endpoint trusts client-supplied identity, which matters more here than elsewhere because the payoff is items.
- Officer templates are a complete vertical slice missing its final ten lines (the listing filter) — the cheapest headline win in the feature.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature resistance/loadouts` to begin making improvements.*
