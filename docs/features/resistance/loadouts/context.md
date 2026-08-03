# Resistance Loadouts - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code; capture/apply engine, recruit handoff, UI, JIP, persistence rebuilt 2026-08-02)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements — the free-item paths (equipped weapon + attachments spawned instead of consumed from the box) and the unvalidated RPC seam lead; officer-template listing is the cheapest win

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/OVT_LoadoutManagerComponent.c` (2103 L) — manager on the game mode: store + index maps, capture engine, box-apply / spawn-apply engines, broadcasts, JIP, persistence apply methods
- `Scripts/Game/Data/OVT_PlayerLoadout.c` / `OVT_LoadoutItem.c` / `OVT_LoadoutMetadata.c` — plain data model (recursive item tree; EPF-style slot addressing: priority + purpose + slot index)
- `Scripts/Game/UserActions/OVT_SaveLoadoutAction.c`, `OVT_SaveOfficerLoadoutAction.c`, `OVT_LoadLoadoutAction.c` — on `Prefabs/Props/Military/AmmoBoxes/OVT_AmmoBox_Placed.et` (the $80 "Equipment Box" placeable, `Configs/Resistance/placeables.conf`)
- `Scripts/Game/UI/Context/OVT_LoadoutsContext.c` + `Scripts/Game/UI/Components/OVT_LoadoutListEntryHandler.c` — loadout list, apply/delete, nearby-recruit (5 m) apply-to-one/all
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:1016-1158` — the four `RpcAsk_*` endpoints
- `Scripts/Game/Persistence/Serializers/Components/OVT_LoadoutManagerSerializer.c` — v2 record: index + full contents together; registered `Configs/Systems/Persistence/Overthrow.conf:42`

---

## Important Decisions

- **The manager cache IS the store:** `m_mActiveLoadouts` is authoritative; serializer writes index and contents into one game-mode record so they can never disagree (the EPF-era split of id-index vs. per-loadout rows is *why* contents never used to survive a session).
- **EPF-style slot addressing** (storage priority + purpose + slot index) re-lands items in the same slots on any character; falls back to any-slot-in-same-storage.
- **Conservation via the equipment box:** apply consumes matching items from the box and swaps replaced gear back in — no charge, no spawn (in intent; see Gotchas).
- **Clients get names only:** JIP ships index + metadata, never item trees; `RpcDo_LoadoutSaved/Deleted` maintain it live; contents stay server-side.
- **Persistence format mirrors gameplay classes** (`OVT_PersistedLoadout*` hand-copies fields, versioned, positional) so adding a gameplay field can't corrupt old saves; apply is idempotent and fills existing objects in place.

---

## Gotchas & Learnings

- **The box-apply flow spawns the in-hands weapon** (`ApplyEquipmentFromBox` → `ApplyEquippedItem`, `OVT_LoadoutManagerComponent.c:470-473`, :1278) and all weapon attachments (:1854-1895, also on nested box items :1633-1636) from prefabs — free-item duplication, the feature's headline exploit.
- **All four `RpcAsk_*` endpoints trust client-supplied `playerId`** and never check the sender, distance, or ownership (`OVT_PlayerCommsComponent.c:1024-1157`); `RpcAsk_LoadLoadout` (spawn mode, zero legitimate callers) conjures a full kit for free.
- **Officer templates are inert:** flag/gate/broadcast/persistence all exist, but `GetAvailableLoadouts` (:991-1017) is the only listing path and filters strictly by owner — nobody else ever sees a template.
- **Stowed weapons are never captured** — extraction reads only the in-hands weapon and the clothing root storage; `EquipedWeaponStorageComponent` is never enumerated at save time.
- **Overwrite-save with empty inventory destroys the old loadout** (maps cleared at :72-73 before extraction can fail at :81-85) with no broadcast — clients keep a stale entry.
- Client delete refreshes the list before the delete broadcast lands (`OVT_LoadoutsContext.c:344`); save actions show "saved successfully!" before the server has acted.
- Box item matching is **by prefab name only** (any magazine is "the" magazine) and `FindItemInBox` re-walks the whole box per item — quadratic on big boxes.
- Stale test doc: `OVT_TEST_PersistenceSuite.c:57` still lists loadouts as unpersistable — untrue since the 2026-08-02 migration; serializer has zero automated coverage.
- Naming trap: `OVT_LoadoutConfig`/`ApplyCivilianLoadout` are the civilian **spawn kit**, not this system.

---

*This context file was created retrospectively by analyzing existing code.*
