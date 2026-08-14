# Recruit UX - Task Checklist

**Last Updated:** 2026-08-14 22:07
**Progress:** 57/57 tasks complete (100%)

> Phases 2 and 7 are **ADVANCED** (`component-developer-advanced`). Task numbering matches `implementation.md` §4.

---

## Phase 1: Data model, replication, serializer v3, quick wins (10/10 complete) ✅ — `component-developer`

- [x] ✅ **T1.1 `m_bInactive` field on OVT_RecruitData**
  - Description: `bool m_bInactive = false;` + doc comment distinguishing it from `m_bIsOnline`
  - File(s): `Scripts/Game/Data/OVT_RecruitData.c`
  - Estimate: 🟢

- [x] ✅ **T1.2 JIP append in RplSave/RplLoad**
  - Description: `WriteBool(m_bInactive)` as last write in the per-recruit block (:2038-2090), matching last read (:2096-2174). Append-only.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c`
  - Estimate: 🟢

- [x] ✅ **T1.3 New broadcast RPC for state change**
  - Description: `BroadcastRecruitActiveState` + `RpcDo_RecruitActiveStateChanged(string, bool)` with the client-mode guard. Do NOT extend `RpcDo_RecruitUpdated` (8-param limit).
  - File(s): `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c`
  - Estimate: 🟡

- [x] ✅ **T1.4 Serializer v3**
  - Description: `bool inactive;` appended last on `OVT_PersistedRecruit`; write version 3; `if (version < 3) ClearInactiveFlags(records);` on read; VERSION HISTORY block extended
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_RecruitManagerSerializer.c`
  - Estimate: 🟡

- [x] ✅ **T1.5 ApplyPersistedRecruits adopts `inactive`**
  - Description: adopt onto record, preserving live-session idempotency contract
  - File(s): `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` / serializer
  - Estimate: 🟢

- [x] ✅ **T1.6 BUG-107: XP faction key from config**
  - Description: `OnAIKilled` :713 — compare against `OVT_Global.GetConfig().m_sOccupyingFaction`, guarded
  - File(s): `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c`
  - Estimate: 🟢

- [x] ✅ **T1.7 FindRecruitEntity mid-iteration removal fix**
  - Description: collect stale ids, remove after the loop (match `SyncRecruitPositions` :1546-1572)
  - File(s): `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c`
  - Estimate: 🟢

- [x] ✅ **T1.8 State-only public API**
  - Description: `IsRecruitInactive(string)`, `GetPlayerRecruitsByState(string, bool)`
  - File(s): `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c`
  - Estimate: 🟢

- [x] ✅ **T1.9 Persistence-tier test extensions**
  - Description: extend `OVT_TEST_Persistence_Recruits_RoundTrip` (:551) + `..._SurvivesSaveAndReload` (:1022) with inactive assertions; can-fail proof recorded
  - File(s): `Scripts/Game/Tests/TestSuites/.../OVT_TEST_PersistenceSuite.c`, `OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 🟡

- [x] ✅ **T1.10 Logic-tier seed file**
  - Description: `OVT_TEST_Logic_RecruitStatus.c` — record-level default (new record is active). No manager-accessor identifiers anywhere, incl. comments.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_RecruitStatus.c`
  - Estimate: 🟢

---

## Phase 2: Inactive group mechanics — server (11/11 complete) ✅ — `component-developer-advanced` ⚠️ ADVANCED

- [x] ✅ **T2.1 Inactive group prefab**
  - Description: `OVT_Group_InactiveRecruits.et` — empty slots, `m_bPlayable 0`, `m_bDeleteWhenEmpty 1`, no persistence, marker component. Reserved GUID series `{6B4C0000000000XX}`.
  - File(s): `Prefabs/Groups/INDFOR/OVT_Group_InactiveRecruits.et`
  - Estimate: 🟡

- [x] ✅ **T2.2 OVT_InactiveRecruitGroupComponent**
  - Description: owner persistent id (server-only), waypoint ref, `OnDelete` deletes waypoint
  - File(s): `Scripts/Game/Components/OVT_InactiveRecruitGroupComponent.c`
  - Estimate: 🟡

- [x] ✅ **T2.3 Manager prefab attribute**
  - Description: `[Attribute] ResourceName m_sInactiveGroupPrefab` + set on game mode prefab beside `m_sRecruitPrefab` (:161)
  - File(s): `OVT_RecruitManagerComponent.c`, `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 🟢

- [x] ✅ **T2.4 Pure clustering selection**
  - Description: `OVT_RecruitInactiveGrouping.SelectClusterCandidates(...)` + `DEFAULT_CLUSTER_RADIUS = 50.0`. Pure — no manager/entity access ever.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_RecruitInactiveGrouping.c`
  - Estimate: 🟡

- [x] ✅ **T2.5 Extract RemoveRecruitFromSlaveGroup**
  - Description: four-step exit extracted from `RemoveRecruitsFromGroup` :1959-1996; refactor caller to use it; keep the vanilla-sync note
  - File(s): `OVT_RecruitManagerComponent.c`
  - Estimate: 🟡

- [x] ✅ **T2.6 PlaceRecruitInInactiveGroup**
  - Description: cluster-host join or spawn group prefab + faction + defend waypoint + `UntrackTransient` + add agent; verify `GetAgentsCount() >= 1` else delete. Doc-comment prohibitions: no `SetLifecyclePolicy`, no `CleanupGroup`/`CleanupEntity`.
  - File(s): `OVT_RecruitManagerComponent.c`
  - Estimate: 🔴

- [x] ✅ **T2.7 SetRecruitInactive server entry point**
  - Description: validate → transition per §3.3 → write flag → broadcast → return success. Server-guarded.
  - File(s): `OVT_RecruitManagerComponent.c`
  - Estimate: 🔴

- [x] ✅ **T2.8 PlaceRecruitInWorld fork**
  - Description: fork on `m_bInactive`; rewire `AttachRecruitBody` :1516 and `RespawnPlayerRecruits` :1036 (the BUG-130/131 path — nothing else changes)
  - File(s): `OVT_RecruitManagerComponent.c`
  - Estimate: 🟡

- [x] ✅ **T2.9 SelectTransferable excludes inactive**
  - Description: skip inactive with its own out-count; class header "two rules" → three
  - File(s): `Scripts/Game/GameMode/Managers/OVT_GroupRecruitTransfer.c`
  - Estimate: 🟢

- [x] ✅ **T2.10 Fast travel excludes inactive**
  - Description: `GetPlayerRecruitEntitiesInRadius(..., bool excludeInactive = false)`; pass `true` from `ResolveTravellingRecruits` :271-290
  - File(s): `OVT_RecruitManagerComponent.c`, `Scripts/Game/Components/Controller/OVT_TravelRequestComponent.c`
  - Estimate: 🟢

- [x] ✅ **T2.11 Logic-tier tests**
  - Description: extend `OVT_TEST_Logic_GroupRecruits.c` (new transferable rule); new `OVT_TEST_Logic_RecruitClustering.c` (self/active/offline exclusion, radius behaviour clear of boundary, empty input, order stability). Can-fail proofs.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/`
  - Estimate: 🟡

---

## Phase 3: Controller component and user actions (7/7 complete) ✅ — `network-specialist`

- [x] ✅ **T3.1 OVT_RecruitCommandComponent (inactive toggle + result)**
  - Description: `RequestSetInactive` + `RpcAsk_SetRecruitInactive` + `RpcDo_RecruitCommandResult`; `ResolveOwningPlayerId()` verbatim from `OVT_TowerSabotageComponent.c:59-80`; listen-server short-circuit
  - File(s): `Scripts/Game/Components/Controller/OVT_RecruitCommandComponent.c`
  - Estimate: 🔴

- [x] ✅ **T3.2 Controller prefab wiring**
  - Description: add component to `OVT_OverthrowController.et` (reserved GUID) — silent no-op without this
  - File(s): `Prefabs/GameMode/OVT_OverthrowController.et`
  - Estimate: 🟢

- [x] ✅ **T3.3 OVT_Global.GetRecruitCommands()**
  - Description: follow `GetTowerSabotage()` :157 incl. null-until-owner-assignment note
  - File(s): `Scripts/Game/Global/OVT_Global.c`
  - Estimate: 🟢

- [x] ✅ **T3.4 OVT_BaseRecruitUserAction**
  - Description: ownership via recruit record; show/perform split per `OVT_LockVehicleAction.c:15-39`; every deref JIP-guarded
  - File(s): `Scripts/Game/UserActions/OVT_BaseRecruitUserAction.c`
  - Estimate: 🟡

- [x] ✅ **T3.5 Set inactive/active actions**
  - Description: `OVT_SetRecruitInactiveAction` (shown when active), `OVT_SetRecruitActiveAction` (shown when inactive); visible-with-reason UX per `OVT_SabotageTowerAction`
  - File(s): `Scripts/Game/UserActions/OVT_SetRecruitInactiveAction.c`, `OVT_SetRecruitActiveAction.c`
  - Estimate: 🟡

- [x] ✅ **T3.6 Prefab action wiring (3 prefabs)**
  - Description: `additionalActions` on `CIV/Character_CIV_Recruit.et`, `CIV/Character_CIV.et`, `INDFOR/FIA/Character_CIV.et` (Duration 3 / 2). Do NOT touch the two dead FIA recruit prefabs.
  - File(s): three character prefabs
  - Estimate: 🟡

- [x] ✅ **T3.7 Localization (.st only)**
  - Description: `OVT-Recruit_SetInactive` / `OVT-Recruit_SetActive` + reasons; prefab UIInfo stays literal English until export regen (D14)
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢

---

## Phase 4: Status derivation and owner-targeted push (5/5 complete) ✅ — `component-developer`

- [x] ✅ **T4.1 OVT_RecruitStatus pure class**
  - Description: flags, `Derive`, `TagIcon`, predicates. No world access.
  - File(s): `Scripts/Game/Data/OVT_RecruitStatus.c`
  - Estimate: 🟢

- [x] ✅ **T4.2 ReadRecruitStatus (world-bound reads)**
  - Description: weapon slots, ammo, wounded (`GetState()`), unconscious — reads only
  - File(s): `OVT_RecruitManagerComponent.c`
  - Estimate: 🟡

- [x] ✅ **T4.3 10 s status sweep**
  - Description: server-only CallLater sweep over owner-id snapshot; updates `m_vLastKnownPosition`; sends per-recruit status. NEVER calls `SyncRecruitPositions()`.
  - File(s): `OVT_RecruitManagerComponent.c`
  - Estimate: 🟡

- [x] ✅ **T4.4 Client status cache**
  - Description: `RpcDo_RecruitStatus` → `map<string,int>` + last-known position on local replica; `GetStatusFlags`; pruned on `m_OnRecruitRemoved`
  - File(s): `Scripts/Game/Components/Controller/OVT_RecruitCommandComponent.c`
  - Estimate: 🟡

- [x] ✅ **T4.5 Logic-tier flag matrix**
  - Description: full `Derive` matrix, `TagIcon` three outcomes, predicates on zero mask. Can-fail proofs.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_RecruitStatus.c`
  - Estimate: 🟡

---

## Phase 5: Recruits screen (7/7 complete) ✅ — `ui-developer`

- [x] ✅ **T5.1 RecruitsMenu.layout sections + capacity + button**
  - Description: `ActiveList`/`InactiveList` + headers in scroll container; capacity text; `ToggleActiveButton` (keep `SCR_InputButtonComponent` GUID `{5D346C3DD81D95CD}`); fresh GUIDs
  - File(s): `UI/Layouts/Menu/RecruitsMenu.layout`
  - Estimate: 🟡

- [x] ✅ **T5.2 Flat selection model (DO FIRST, pad-verify)**
  - Description: ordered entry array replaces sibling walk; rewrite `SelectRecruitByIndex`/`SelectPreviousRecruit`/`SelectNextRecruit`; `ToggleActiveButton` added to `SetButtonsVisible` (:340-344)
  - File(s): `Scripts/Game/UI/Context/OVT_RecruitsContext.c`
  - Estimate: 🔴

- [x] ✅ **T5.3 Toggle handler**
  - Description: `RequestSetInactive(id, !inactive)`; label/enabled follow selection; NO optimistic local mutation (broadcast is the confirmation)
  - File(s): `OVT_RecruitsContext.c`
  - Estimate: 🟡

- [x] ✅ **T5.4 Capacity header**
  - Description: `X / 16` from `GetRecruitCount` + `MAX_RECRUITS_PER_PLAYER`
  - File(s): `OVT_RecruitsContext.c`
  - Estimate: 🟢

- [x] ✅ **T5.5 Row status icons + inactive styling**
  - Description: icon row in `RecruitListItem.layout`; handler drives from `GetStatusFlags`; dimmed inactive rows; `GetRecruitData()`; null-guard :39-41
  - File(s): `UI/Layouts/Menu/RecruitsMenu/RecruitListItem.layout`, `Scripts/Game/UI/Components/OVT_RecruitListEntryHandler.c`
  - Estimate: 🟡

- [x] ✅ **T5.6 Input action + context binding**
  - Description: `OverthrowRecruitsToggleActive` (kb + `gamepad0:shoulder_left`) + `ActionRefs` in `OverthrowRecruitsMenuContext` (:1101-1115); run conflict checker + manual kb cross-check (checker blind spot)
  - File(s): `Configs/System/chimeraInputCommon.conf`
  - Estimate: 🟡

- [x] ✅ **T5.7 Localization (.st only)**
  - Description: section headers, capacity format, button labels, reasons
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢

---

## Phase 6: Map marker layer (6/6 complete) ✅ — `ui-developer`

- [x] ✅ **T6.1 Imageset placeholder entries**
  - Description: `recruit`, `recruit_ammo`, `recruit_ammo_empty`, `recruit_wounded` aliasing existing atlas regions per §3.6 table; comment records real-art row 4 targets
  - File(s): `UI/Imagesets/overthrow_mapicons.imageset`
  - Estimate: 🟢

- [x] ✅ **T6.2 MapRecruitLocation.layout**
  - Description: copy `MapPlayerLocation.layout`; `TagImage` as SIBLING of `Image` (rotation!) with corner alignment
  - File(s): `UI/Layouts/Map/MapRecruitLocation.layout`
  - Estimate: 🟡

- [x] ✅ **T6.3 OVT_MapRecruitLocation component**
  - Description: per §3.6 — availability rule, per-frame entity resolve w/ last-known fallback, opacity owned by `Update()` (`SetVisible` never `SetOpacity`), roster-churn rebuild
  - File(s): `Scripts/Game/UI/Map/Visualization/OVT_MapRecruitLocation.c`
  - Estimate: 🔴

- [x] ✅ **T6.4 Map config registration**
  - Description: sibling entry in `MapOverthrow.conf` `m_aUIComponents` (:56-78) ONLY (not GM/Respawn confs)
  - File(s): `Configs/Map/MapOverthrow.conf`
  - Estimate: 🟢

- [x] ✅ **T6.5 Layer panel filter row**
  - Description: `KEY_RECRUITS`/`LABEL_RECRUITS`, `BuildRecruitRow()`, `ApplyRecruitMarkerPreference()`, `ApplyOne` branch; row skipped when layer unavailable
  - File(s): `Scripts/Game/UI/Map/OVT_MapLayersUI.c`
  - Estimate: 🟡

- [x] ✅ **T6.6 Localization (.st only)**
  - Description: `OVT-Map_Layer_Recruits`
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢

---

## Phase 7: Loadout swap (7/7 complete) ✅ — `component-developer-advanced` ⚠️ ADVANCED

- [x] ✅ **T7.1 OVT_LoadoutSwap routine**
  - Description: server guard first; result struct (exchanged / failed / dropped)
  - File(s): `Scripts/Game/GameMode/Managers/OVT_LoadoutSwap.c`
  - Estimate: 🔴

- [x] ✅ **T7.2 Enumeration helpers**
  - Description: `IsEquipped()` classifier, weapon slots, clothing area list; NO `GetItems(PURPOSE_DEPOSIT)` (BUG-083); collect-then-mutate
  - File(s): `OVT_LoadoutSwap.c`
  - Estimate: 🟡

- [x] ✅ **T7.3 Pair-then-single execution + rollback**
  - Description: `TrySwapItemStorages` pairs first, `TryMoveItemToStorage` singles w/ -1 fallback, rollback journal, world-drop last resort, quickslot rebuild both sides
  - File(s): `OVT_LoadoutSwap.c`
  - Estimate: 🔴

- [x] ✅ **T7.4 RpcAsk_SwapLoadout**
  - Description: resolve RplId → `ResolveOwningPlayerId()` → re-check owned+active+alive+conscious+distance; reply via `RpcDo_RecruitCommandResult`
  - File(s): `Scripts/Game/Components/Controller/OVT_RecruitCommandComponent.c`
  - Estimate: 🟡

- [x] ✅ **T7.5 Swap user action**
  - Description: `OVT_SwapLoadoutWithRecruitAction`, Duration 5, active owned recruits only; same 3 prefabs as Phase 3
  - File(s): `Scripts/Game/UserActions/OVT_SwapLoadoutWithRecruitAction.c` + prefabs
  - Estimate: 🟡

- [x] ✅ **T7.6 Outcome notifications**
  - Description: complete / partial / failed hints; keys into `.st` only
  - File(s): UI hint path + `Language/localization_Overthrow.st`
  - Estimate: 🟢

- [x] ✅ **T7.7 Failure logging**
  - Description: every failed move + world-drop logged WARNING with prefab name
  - File(s): `OVT_LoadoutSwap.c`
  - Estimate: 🟢

---

## Phase 8: Help and documentation sync (4/4 complete) ✅ — `help-docs-sync`

- [x] ✅ **T8.1 Tutorial popups**
  - Description: inactive-recruit tip + loadout-swap tip
  - File(s): `Configs/Tutorials/`
  - Estimate: 🟢

- [x] ✅ **T8.2 Field Manual**
  - Description: extend recruits entry — inactive semantics, cap, map layer, swap
  - File(s): `Configs/FieldManual/`
  - Estimate: 🟡

- [x] ✅ **T8.3 Public wiki**
  - Description: wikijs MCP update matching shipped behaviour
  - File(s): wiki
  - Estimate: 🟢

- [x] ✅ **T8.4 Fact-check gate**
  - Description: every factual sentence cites file:line in review notes or is cut
  - File(s): review notes
  - Estimate: 🟢

---

## Bugs & Issues

**Active Bugs:**
- (none yet)

**Rides along:**
- BUG-107 (recruit XP hardcodes US/USSR) — fixed by T1.6
- `FindRecruitEntity` mid-iteration mutation — fixed by T1.7

**To file (R11, out of scope):** `RpcAsk_DismissRecruit` has zero ownership validation — file next BUG id, linked to `resistance/recruits`.

---

## Technical Debt

- (none yet)

---

## Progress Tracking

### Completed This Session (2026-08-14)
- ✅ All 8 phases (T1.1–T8.4), single /autorun-feature session. Gates: All 143/143 (P1), All 148/148 (P2), Fast 108/108 (P3); P4–P7 compile-clean with one Fast/All run **deferred** (user in game — pending); P5/P6/P8 are UI/docs (suites cover nothing).
- ✅ BUG-107 fixed (T1.6) and closed in tracker; BUG-166 filed (R11: unvalidated `RpcAsk_DismissRecruit`); BUG-167 filed (group-join dialog counts parked recruits).

### Discovered New Tasks
- [x] ✅ **Deferred regression gate** — All group run 2026-08-14 after the user's session ended: **159/159 green, exit 0**.
- [ ] **Workbench localization re-export** (user) — 24 new `.st` items; tutorials/Field Manual/reasons render raw `#OVT-…` keys until then.
- [ ] **Manual play-test pass** — per-phase checklists in context.md session notes + implementation.md §6 steps 1–22 (groups, roster pad-nav, map layer, swap invariant, MP/JIP, save/continue).
- [ ] *(follow-up, optional)* Live status-icon refresh in roster rows (invoker on `OVT_RecruitCommandComponent`); BUG-167 dialog count fix.

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
