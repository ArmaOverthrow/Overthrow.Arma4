# High Command - Task Checklist

**Last Updated:** 2026-08-22 (all 13 phases complete)
**Progress:** 77/77 tasks complete (100%) — ✅ **CLOSED 2026-08-22**, play-test green. T13.3's wiki half closed out, not done (deferred until 1.5 ships).

> **Epic:** `resistance` — reference as `resistance/high-command`.
> Phases **2, 3, 6, 7, 9, 11** are **ADVANCED** (`*-advanced` agents). Task numbering matches `implementation.md` §4.
> Every phase gate: `tools/compile-check.sh` exit 0 (agent) + `tools/run-tests.sh <SuiteClassName>` (orchestrator only, once per phase, per `.claude/test-policy.md`).

---

## Phase 1: Pure spine, manager skeleton, seam, serializer v1 (10/10) ✅ — `component-developer`

- [x] ✅ **T1.1** `OVT_HighCommandRecord.c` (+ `OVT_EHighCommandStance`), `OVT_HighCommandRules.c`, `OVT_HighCommandStatus.c`, `OVT_ItemSourcingRules.c` — §3.2
  - File(s): `Scripts/Game/Data/` · Estimate: 🟡
- [x] ✅ **T1.2** `OVT_HighCommandManagerComponent.c` skeleton — `s_Instance`, maps, getters, `AddRecord`/`RemoveRecord`, tuning `[Attribute]`s. No spawning/replication/ticks
  - File(s): `Scripts/Game/GameMode/Managers/` · Estimate: 🟡
- [x] ✅ **T1.3** `OVT_Global.GetHighCommand()` beside `GetRecruits()` (`OVT_Global.c:365`)
  - File(s): `Scripts/Game/OVT_Global.c` · Estimate: 🟢
- [x] ✅ **T1.4** Manager component onto `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 🟢
- [x] ✅ **T1.5** `OVT_HighCommandRequestComponent.c` — five ask stubs, `ResolveOwningPlayerId()`, `RpcDo_HCResult`; registered on `OVT_OverthrowController.et` before the trailing `RplComponent`
  - Estimate: 🟡
- [x] ✅ **T1.6** `OVT_TEST_Init_ControllerSeam.c` — one line in `FindFirstUnresolvedComponent()` + count 9 → 10
  - Estimate: 🟢
- [x] ✅ **T1.7** `OVT_HighCommandManagerSerializer.c` v1 + `Overthrow.conf` `ComponentSerializers` entry
  - Estimate: 🟡
- [x] ✅ **T1.8** `OVT_DifficultySettings.highCommandMemberCap` / `highCommandSupportersPerMember` + two server-only config fields/accessors
  - Estimate: 🟡
- [x] ✅ **T1.9** Logic cases: `OVT_TEST_Logic_HighCommandRules.c`, `..._HighCommandStatus.c`, `..._ItemSourcing.c`
  - Estimate: 🟡
- [x] ✅ **T1.10** `Init/OVT_TEST_Init_HighCommandSeam.c` — manager + request component resolve on own controller
  - Estimate: 🟡

## Phase 2: Group entity — spawn, observer, faction, waypoints, stances (9/9) ✅ ⚠️ ADVANCED — `component-developer-advanced`

- [x] ✅ **T2.1** `Prefabs/Groups/INDFOR/OVT_Group_HighCommand.et` + `.et.meta` — §3.5 (GUIDs `6B1C3D0000000003` / `…0004`; inherited `RplComponent` GUID copied) · 🟡
- [x] ✅ **T2.2** `OVT_HighCommandGroupComponent.c` — deferred observer install, unconditional `OnDelete` removal, owned-waypoint array (remove-then-delete), manager notify · 🔴
- [x] ✅ **T2.3** `OVT_VirtualizationManagerComponent.m_bHighCommandGroupsAreObservers` + accessor (own gate, NOT the recruit gate) · 🟢
- [x] ✅ **T2.4** Manager `SpawnGroup(entryKey, groupPrefab, vehiclePrefab, position, ownerPersistentId)` — composition via `SCR_AIGroupClass.GetMembers`, `AddAIEntityToGroup`, faction stamp, record create. **`SpawnGroupFromEntry` is Phase 3's resolver in front of it** · 🔴
- [x] ✅ **T2.5** Vehicle groups — spawn vehicle, seat members one per call-queue hop, hold vehicle id · 🔴
- [x] ✅ **T2.6** `ApplyStance(record)` + `ClearOwnedWaypoints()` — the three stance kits, terrain-clamped; patrol ring built in the manager so all nine waypoints are owned · 🟡
- [x] ✅ **T2.7** `DismissGroup(groupId)` — members → vehicle → group, delete-when-empty off first, record drop · 🟡
- [x] ✅ **T2.8** Manager `SyncGroupPositions()` — save-point sweep, called from `OVT_OverthrowGameMode.PreShutdownPersist` · 🟢
- [x] ✅ **T2.9** Extend `OVT_TEST_Init_HighCommandSeam.c` — case C: spawn/composition/faction/observer/dismiss (`HasEntityObserver` + `GetEntityObserverCount`, never an engine query) · 🟡

## Phase 3: Purchase server half — authored data, manifest, pricing, wire (8/8) ✅ ⚠️ ADVANCED — `component-developer-advanced`

- [x] ✅ **T3.1** `OVT_Faction` gains `OVT_HighCommandGroupEntry` + `m_aHighCommandGroups` + 4 read accessors — §3.3. Manager gains `SpawnGroupFromEntry` (Phase 2's carried-forward resolver) plus `GetEntryCount/ByIndex/ByKey`, `GetEntryMemberPrefabs/Count`, `QuoteEntry`, `ConsumeQuotedStock` · 🟡
- [x] ✅ **T3.2** `Configs/Factions/FIA_OverthrowData.conf` — 11 entries, ids `6B1C3D…0010`–`…001A`. 🔴 **One plan GUID was wrong**: `{16E32C3ABEAFC2C6}` is `Ural4320_FIA_transport.et`, not `…_transport_covered.et`; authored with the recovered `{B70E6D12A8EC2410}` · 🟡
- [x] ✅ **T3.3** `OVT_WarehouseStockUtils.c` — `CollectStores` / `CountAvailable` / `TakeUpTo`, per-call query objects, the record match delegated to `GetNearestWarehouse` · 🟡
- [x] ✅ **T3.4** `OVT_HighCommandManifest.c` (+ `OVT_HighCommandQuote`) — one-per-hop spawn-inspect with the deferred capture, `MANIFEST_CAPTURE_ATTEMPTS = 5`, double `UntrackTransient`, WARNING-on-empty, prices gated on `IsRegisteredResource` · 🔴
- [x] ✅ **T3.5** `RpcAsk_QuoteGroup` / `RpcAsk_PurchaseGroup` — the nine-step ladder in order, spawn→stock→supporters→money, `RpcDo_HCQuote` (arity 6) + `RpcDo_HCResult`; every refusal answers · 🔴
- [x] ✅ **T3.6** `OVT_BarracksComponent.c` + `OVT_BarracksQuery` — `BARRACKS_USE_RADIUS = 5`, one query object per call · 🟡
- [x] ✅ **T3.7** 9 presets in `Configs/overthrowBroadcastMessages.conf` (`6B1C3D…0020`–`…0031`) + their 9 `.st` master entries (`…00A0`–`…00A8`) · 🟢
- [x] ✅ **T3.8** 3 new Logic cases; all five `SplitCoverage` cases the task named already existed from Phase 1 · 🟡

## Phase 4: Barracks + purchase screen (6/6) ✅ — `ui-developer`

- [x] ✅ **T4.1** `Prefabs/Structures/Military/Houses/Barracks_01/OVT_Barracks.et` (already authored by the user; edited, not rewritten) + `OVT_BarracksComponent`, `OVT_StructureDestructionComponent` (bare `.xob` ruin model), `SCR_DestructibleBuildingComponent { Enabled 0 }`, `RplComponent`, desk child · 🟡
- [x] ✅ **T4.2** The three same-GUID vanilla `_base.et` deltas — line 1 + `ID` byte-for-byte from vanilla, `.et.meta` `Name` = the vanilla GUID (all three verified by inbound-reference grep) · 🟡
- [x] ✅ **T4.3** `Configs/Resistance/buildables.conf` — the Barracks entry, `m_SitePrefab` = the existing `Site_Barracks.et`, **with** `m_aResourceRequirements` (170/280/170/70), priced as **the most expensive buildable** (20000/60, per context.md's pre-brief override of D14) · 🟢
- [x] ✅ **T4.4** `OVT_ManageHighCommandAction.c` + the desk child block on all four hosts · 🟡
- [x] ✅ **T4.5** `OVT_HighCommandPurchaseContext.c` + `UI/Layouts/Menu/HighCommandMenu.layout` (+ `HighCommandMenu/HighCommandGroupRow.layout`) — list, server-quote details panel, Buy disabled until quote arrives, 250 ms debounce on `QuoteEntry` · 🔴
- [x] ✅ **T4.6** 41 new `.st` entries — the 22 `#OVT-HC_Group_*` title/description keys FIA_OverthrowData.conf already referenced, plus `OVT-Barracks` (the user's prefab referenced it unauthored), `OVT-Build_Barracks(_Description)`, `OVT-ManageHighCommand`, `OVT-HC_AtCap` and 15 purchase-screen keys. `Configs/Language/*.conf` untouched · 🟡

## Phase 5: Tent warehouse discount (4/4) ✅ — `component-developer` (all four; T5.4 is one arity change + one label)

- [x] ✅ **T5.1** `OVT_RecruitLoadoutPrice.m_aManifest` appended after `m_sUnpriceableResource`; `AddResource` (`:196`) fills it · 🟡
- [x] ✅ **T5.2** `OVT_RecruitCommandComponent.BuildQuote` (`:1014`) — `CollectStores` + `SplitCoverage`, charged subtotal into `TotalPrice` · 🟡
- [x] ✅ **T5.3** `RpcAsk_BuyEquippedRecruit` (`:837`) — `TakeUpTo` before `TakePlayerMoney` · 🟡
- [x] ✅ **T5.4** `RpcDo_RecruitQuote` arity 5 → 6 (`coveredValue`) at both `Rpc()` sites + handler; `OVT_LoadoutsContext.OnRecruitQuote` (`:384`) renders `#OVT-Recruit_QuoteCovered` · 🟡

## Phase 6: Replication — JIP, deltas, heartbeat, load walk (7/7) ✅ ⚠️ ADVANCED — `network-specialist-advanced`

- [x] **T6.1** `RplSave`/`RplLoad` — positional per-group blocks, member cap first, append-only rule in header comments · 🔴
- [x] **T6.2** The four broadcast deltas (#8–#11) + send helpers · 🟡
- [x] **T6.3** `SweepStatus()` on `STATUS_SYNC_INTERVAL_MS = 10000` — contact/ammo/mounted/moving flags, broadcast only on change · 🔴
- [x] **T6.4** Client-side record mirror — no client ever dereferences a group entity · 🟡
- [x] **T6.5** Extend `OVT_TEST_Init_HighCommandSeam.c` — members/flags/position populated within one heartbeat · 🟡
- [x] **T6.6** 🆕 **The §3.11 spawn-on-load walk** — the plan assigned this to no phase (orchestrator caught it after Phase 2). One record per call-queue hop → spawn `OVT_Group_HighCommand.et` → `RequestSpawn` each saved body UUID (`BODY_SPAWN_TIMEOUT_MS = 15000`) → fall back to `memberPrefabs[i]` → join → faction stamp → re-seat a vehicle group → `ApplyStance`. Without it F18/F19 cannot pass · 🔴
- [x] **T6.7** 🆕 🔴 **HC members must survive `UntrackTransient`** — see the Phase 2 findings in `context.md`. Register HC members with the HC manager **before** the group add and add the HC exclusion to the modded `SCR_AIGroup.AddAIEntityToGroup` (`:461`) / `OnAgentAdded` (`:489`), exactly as recruit bodies are excluded. **`m_aMemberBodyIds`, D8, F18 and Phase 9's "converted recruits keep their gear" are all dead without this** · 🔴

## Phase 7: Map command & control + gamepad context (7/7) ✅ ⚠️ ADVANCED — `ui-developer-advanced`

- [x] ✅ **T7.1** `OVT_MapHighCommandLayer.c` + `UI/Layouts/Map/MapHighCommandLocation.layout` (root `FrameWidgetClass`, `Alignment 0.5 0.5`, `Clipping False`) · 🔴
- [x] ✅ **T7.2** `Configs/Map/MapOverthrow.conf` registration block · 🟢
- [x] ✅ **T7.3** `OVT_MapLayersUI` third row — `KEY_HIGHCOMMAND`, `LABEL_HIGHCOMMAND`, `BuildHighCommandRow`, `ApplyHighCommandMarkerPreference`, `ApplyOne` branch before `ApplyCanvasLayer` · 🟡
- [x] ✅ **T7.4** Selection + hover magnet (`TickHoverMagnet` model), panel suppression, reassign-the-claim-first ordering · 🔴
- [x] ✅ **T7.5** `chimeraInputCommon.conf` — `OverthrowMapCommandContext` + `OverthrowHCOrder` + `OverthrowHCStance`, per-frame activation with one-frame arm delay. ⚠️ **Phase 4 already added `OverthrowHighCommandBuy` on `KC_B` / `gamepad0:x` and `OverthrowHighCommandContext` (Priority 50, Flags 4)** — the plan gives `OverthrowHCOrder` `gamepad0:x` and `OverthrowHCStance` `KC_B`, so re-run the checker and confirm the contexts genuinely never coexist · 🔴
- [x] ✅ **T7.6** Selected-group info panel + order/stance flow calling `RpcAsk_OrderGroup` · 🟡
- [x] ✅ **T7.7** `check-input-conflicts.py`, plain and `--warnings`, back at baseline · 🟢

## Phase 8: "Manage Groups" roster + main-menu item (3/3) — `ui-developer`

- [x] ✅ **T8.1** `OVT_HighCommandRosterContext.c` + `UI/Layouts/Menu/HighCommandRoster.layout` — flat entry list, `n / cap` line, status icons, Show on Map + Dismiss (confirm) · 🟡
- [x] ✅ **T8.2** `UI/Layouts/Menu/MainMenu.layout` — "Manage Groups" button copied from the Manage Recruits block · 🟢
- [x] ✅ **T8.3** `OVT_MainMenuContext` — `GetButtonText` + `m_OnClicked` pair and `ManageGroups()` · 🟢

## Phase 9: Recruit conversion + split roster (5/5) ✅ ⚠️ ADVANCED — `component-developer-advanced` then `ui-developer`

- [x] **T9.1** `RpcAsk_ConvertRecruitGroup(string anchorRecruitId)` — resolve caller/record/ownership, collect the inactive group · 🔴
- [x] **T9.2** Conversion order — HC record + group ownership, re-point observer, clear `m_sBodyPersistenceId` **before** dropping each recruit record via the existing remove path · 🔴
- [x] **T9.3** Cap interaction — `CONVERT_AT_CAP` refusal; converted recruits leave the recruit cap in the same operation · 🟡
- [x] ✅ **T9.4** `OVT_RecruitsContext` — inactive section split by group via `OVT_RecruitInactiveGrouping` through `BuildSection` (`:229`), flat selection preserved · 🟡
- [x] ✅ **T9.5** Per-group **Convert to High Command** button + one-way confirm dialog · 🟡

## Phase 10: Rearm and refuel ticks (3/3) ✅ — `component-developer`

- [x] ✅ **T10.1** Rearm tick — deficient-magazine derivation (`OVT_VehicleRearmUtils.AnyAmmoMissing` shape, per character), `OVT_HighCommandRules.AggregateResourceNeeds` (pure), `CollectStores` + `TakeUpTo`, deliver via `SetAmmoCount`. `NO_AMMO` clears on the next status read rather than a third `BroadcastGroupStatus` call site (documented deviation, see context.md) · 🟡
- [x] ✅ **T10.2** Refuel tick — `GetOwnFuelManager`→`GetRefuelableCapacity`→`FindBestFillSource`→`GetFuelCostPerLitre`→`ComputeFillPlan`, deliver, drain source, charge owner via `TakePlayerMoneyPersistentId` for what ARRIVED; `OVT_FuelChargeLedger.Accrue` keyed by group id, cleared in `RemoveRecord` · 🔴
- [x] ✅ **T10.3** Both ticks gated on `Replication.IsServer()` at the top; skip a group with no live entity or zero agents · 🟢

## Phase 11: Garrison retirement, serializer bumps, map rows, changelog (8/8) ✅ ⚠️ ADVANCED — `component-developer-advanced`

- [x] ✅ **T11.1** `OVT_ResistanceFactionManager` — nine garrison methods + `SpawnGarrisons` + `ApplyPersistedGarrison` + both call sites + the `CallLater` deleted; `OVT_CampData`/`OVT_FOBData` garrison fields gone. `PostGameStart()` kept as an empty lifecycle hook (the game mode calls it unconditionally) · 🔴
- [x] ✅ **T11.2** `OVT_OccupyingFactionManager` — the friendly-garrison `else` branch in `InitBaseControllers` and its two dead locals, `ApplyPersistedBaseGarrison` **and its call site** (not in the brief's list; `base.garrison` no longer exists), `OVT_BaseData` garrison fields · 🟡
- [x] ✅ **T11.3** `OVT_ResistanceRequestComponent` (`AddGarrison`, `RpcAsk_AddGarrison`, `IsGarrisonPrefabIndexValid`, the now-dead `BASE_MAX_DISTANCE`) + `OVT_FOBRequestComponent` (both asks, both handlers, `FindGarrisonPrefabIndex`, `IsGarrisonPrefabIndexValid`) · 🟡
- [x] ✅ **T11.4** `OVT_BaseMenuContext` + `OVT_FOBMenuContext` reduced to a close-button shell, `OVT_GroupUIInfo` deleted, both layouts' garrison rows removed (53/53 braces each). ⚠️ Both menus are now content-free — see context.md · 🟡
- [x] ✅ **T11.5** Map rows — `OVT_MapDataKeys.GARRISON_COUNT` + its "reads 0 on every client" warning, the writes and info rows in all three `OVT_MapLocation*` files, `#OVT-Garrison` and `#OVT-AddToGarrison` `.st` entries (braces 2302 → 2298, balanced) · 🟡
- [x] ✅ **T11.6** `BASE_GARRISON` / `CAMP_GARRISON` / `FOB_GARRISON` **removed**, not reserved — evidence in context.md. `RADIO_TOWER_GARRISON` is the occupying faction's and stays · 🔴
- [x] ✅ **T11.7** `OVT_ResistanceManagerSerializer` 1 → **2** and `OVT_OccupyingFactionManagerSerializer` 2 → **3**; every `garrison` field still declared and still read, written empty from the new version. Every `Read()` return now checked · 🔴
- [x] ✅ **T11.8** `docs/overview.md` §Changelog (the project's changelog of record — there is no `docs/CHANGELOG.md`) — v1.37 carries the explicit save warning · 🟢

## Phase 12: Objectives predicate, QRF verification, persistence round trip, review (4/4) ✅ — `component-developer` + main thread

- [x] ✅ **T12.1** `OVT_AssetStarvedObjectiveAbort.c:222` — `IsPlayerAtAsset()` → `OVT_ResistancePresence.IsGroundHeld(...)`; update the players-only comment · 🟢
- [x] ✅ **T12.2** Init case — HC member faction key == `m_sPlayerFaction`, alive and conscious; `OVT_QRFControllerComponent.c` unmodified · 🟢
- [x] ✅ **T12.3** Three `OVT_TEST_PersistenceRoundTripSuite` cases (group / order / member bodies), sorted after `…_Capability_…`, public API only · 🟡
- [x] ✅ **T12.4** Cross-phase review on the main thread — every phase's acceptance criteria + the §6 static gates · 🟡

## Phase 13: Localization, input check, help / Field Manual / wiki (3/3) ✅ (T13.3 partial — wiki owed) — main thread + `help-docs-sync`

- [x] ✅ **T13.1** `.st` audit — every runtime key present with a filled `Comment`, braces counted before/after, fresh GUIDs, Id order; ask the user to re-export · 🟡
- [x] ✅ **T13.2** `check-input-conflicts.py` plain and `--warnings`, back at baseline · 🟢
- [x] ⚠️ **T13.3** `help-docs-sync` — tutorials ✅ + Field Manual ✅ with a full file:line citation ledger. **WIKI NOT DONE — no `wikijs` MCP server attached to this session.** Closed out, not done · 🟡
