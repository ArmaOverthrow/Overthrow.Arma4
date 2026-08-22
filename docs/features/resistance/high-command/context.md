# High Command - Context & Decisions

**Last Updated:** 2026-08-22
**Current Phase:** ✅ CLOSED — all 13 phases, 77/77, play-test passed
**Status:** ✅ **DONE** — play-test green, merged, closed 2026-08-22

---

## Quick Status

**What's Done:**
- ✅ Planning complete (`implementation.md`, 13 phases)
- ✅ Dev docs scaffolded (`/autorun-feature resistance/high-command`, 2026-08-22)
- ✅ **Phase 1** (T1.1–T1.10) — the pure spine (`OVT_HighCommandRecord` + `OVT_EHighCommandStance`, `OVT_HighCommandRules`, `OVT_HighCommandStatus`, `OVT_ItemSourcingRules`), the manager skeleton, `OVT_Global.GetHighCommand()`, the `OVT_HighCommandRequestComponent` seam (5 asks + `RpcDo_HCResult`) on the controller prefab, serializer v1 + its `Overthrow.conf` entry, 2 difficulty knobs + 2 server-only config fields, 18 test cases across 3 Logic files + 1 Init file. **Compile-check exit 0.** Suite gate **DEFERRED** (see Blockers).
- ✅ **Phase 3** (T3.1–T3.8) — the purchase, server half: `OVT_HighCommandGroupEntry` + `m_aHighCommandGroups` + 4 read accessors on `OVT_Faction`, the 11-entry FIA catalog, `OVT_WarehouseStockUtils`, `OVT_HighCommandManifest` (+ `OVT_HighCommandQuote`), the nine-step gate ladder on `RpcAsk_QuoteGroup` / `RpcAsk_PurchaseGroup` + `RpcDo_HCQuote`, `OVT_BarracksComponent` + its sphere query, 9 notification presets + 9 `.st` entries, 3 new Logic cases. **Compile-check exit 0.** Suite gate owed with Phase 1's and Phase 2's.
- ✅ **Phase 2** (T2.1–T2.9) — the live group entity: `OVT_Group_HighCommand.et` (+ `.et.meta`), `OVT_HighCommandGroupComponent` (deferred observer install, unconditional `OnDelete` removal, owned waypoints), the HC-only observer gate on the virtualization manager, `SpawnGroup` / `OrderGroup` / `DismissGroup` / `ApplyStance` / `SyncGroupPositions` on the manager, the order + dismiss asks wired on the seam, 1 new Init case. **Compile-check exit 0.** Suite gate owed with Phase 1's.

- ✅ **Phase 4** (T4.1–T4.6) — the barracks and the purchase screen: `OVT_BarracksComponent` + `OVT_StructureDestructionComponent` + `RplComponent` + a desk child added to the user's already-authored `OVT_Barracks.et`, the three same-GUID vanilla `_base.et` deltas, the Barracks `buildables.conf` entry (20000/60, most expensive), `OVT_ManageHighCommandAction`, `OVT_HighCommandPurchaseContext` + `HighCommandMenu.layout` (+ `HighCommandGroupRow.layout`), the `RpcDo_HCQuote`/`RpcDo_HCResult` stub bodies filled with `m_OnHCQuote`/`m_OnHCResult` invokers, 41 new `.st` entries. **Compile-check exit 0. Conflict checker at baseline.** Suite gate owed with the rest.

- ✅ **Phase 5** (T5.1–T5.4) — `m_aManifest` on `OVT_RecruitLoadoutPrice`, warehouse coverage folded into the tent quote, stock consumed before money on a successful buy, `RpcDo_RecruitQuote` 5 → **6**. **Compile-check exit 0.** The shipped `//To-do: factoring in warehouse` is now fulfilled on both surfaces.

- ✅ **Phase 6** (T6.1–T6.7) — replication: `RplSave`/`RplLoad` (member cap first, per-group positional blocks, append-only), the four broadcast deltas + their send helpers, `SweepStatus()` on a 10 s heartbeat with a change filter, the client record mirror, the §3.11 spawn-on-load walk (**T6.6**), and the HC exclusion in both `SCR_AIGroup` untrack hooks so member bodies stay tracked (**T6.7**). 1 new Init case. **Compile-check exit 0.**

- ✅ **Phase 7** (T7.1–T7.7) — map command & control: `OVT_MapHighCommandLayer` + `MapHighCommandLocation.layout`/`MapHighCommandPanel.layout`, the `OVT_MapLayersUI` filter row, the cursor magnet + selection + info panel, `OverthrowMapCommandContext` (Priority 70, Flags 0x2e — exclusivity bit added over §3.9's literal spec) + `OverthrowHCOrder`/`OverthrowHCStance`. **Compile-check exit 0. Conflict checker at baseline plain and `--warnings`.**

- ✅ **Phase 8** (T8.1–T8.3) — "Manage Groups": `OVT_HighCommandRosterContext` + `HighCommandRoster.layout` (+ `HighCommandRosterRow.layout`/`OVT_HighCommandRosterRowHandler`), the flat owner-groups list with a `n / cap` line and per-group status icons, Show on Map (raises the map gadget then calls `OVT_MapHighCommandLayer.SelectGroup`, now public) and Dismiss (confirm dialog → `RpcAsk_DismissGroup`, server-confirmed, nothing client-side deletes anything), the "Manage Groups" `MainMenu.layout` button + `OVT_MainMenuContext.ManageGroups()`. **Compile-check exit 0. Conflict checker at baseline plain and `--warnings`.**

- ✅ **Phase 11** (T11.1–T11.8) — garrison retirement: nine methods + `SpawnGarrisons` + `ApplyPersistedGarrison` off `OVT_ResistanceFactionManager`, `ApplyPersistedBaseGarrison` + the `InitBaseControllers` restore block off `OVT_OccupyingFactionManager`, both request seams' asks and helpers, both menu contexts reduced to a Close button, the map's Garrison row and `GARRISON_COUNT`, three `OVT_EGroupOrigin` members, `OVT_ResistanceManagerSerializer` **1 → 2** and `OVT_OccupyingFactionManagerSerializer` **2 → 3** (read-and-discard, no field deleted), and the v1.37 changelog warning. **Compile-check exit 0, gate proven able to fail.**

**What's Next:**
- Phase 12 (objectives predicate, QRF verification, persistence round trip, cross-phase review).

**Blockers:**
- ⏸️ **Phase 1 + 2 + 3 + 4 suite gate deferred** (user's call, 2026-08-22): Workbench PID 73260 is live — the user is authoring barracks prefabs, and the suites both steal focus and read unreliably against a live Workbench session. **Owed: `OVT_TEST_LogicSuite` then `OVT_TEST_InitSuite`, by class name, one at a time**, when the user says the coast is clear.

---

## Baseline (taken 2026-08-22, immediately before Phase 1)

- Branch: `v1.5`, working tree **clean** at `6db17fef`.
- Suite baseline: recorded below on the first orchestrator run (per `.claude/test-policy.md`, no planning-time run).
- **GUID series sweep discharged (2026-08-22, orchestrator):** `grep -rl -e 6B1C3D -e 6B0E7A8` over `/mnt/n/Projects/Arma 4/ArmaReforger` → **0 hits**, exit 0. Both reserved series are safe to author from (the plan's §3.13 outstanding pre-authoring check).

---

## Key Files

### Core Implementation
- `Scripts/Game/GameMode/Managers/OVT_HighCommandManagerComponent.c` — records, JIP, deltas, heartbeat, ticks
- `Scripts/Game/Components/OVT_HighCommandGroupComponent.c` — observer, owned waypoints, stance, owner id
- `Scripts/Game/Components/Controller/OVT_HighCommandRequestComponent.c` — the only client→server seam
- `Scripts/Game/Data/OVT_HighCommandRecord.c` / `OVT_HighCommandRules.c` / `OVT_HighCommandStatus.c` / `OVT_ItemSourcingRules.c`
- `Scripts/Game/Persistence/Serializers/Components/OVT_HighCommandManagerSerializer.c`
- `docs/features/resistance/high-command/implementation.md` — §3 architecture, §4 phases, §5 decisions, §6 DoD

### Untouchable (Definition of Done I1–I9)
- `OVT_RealEstateManagerComponent.c`, `OVT_QRFControllerComponent.c`, `OVT_ObjectivePhaseRules.c`, `OVT_RecruitPurchaseRules.c`
- every `logistics/storage` and `economy/fuel` file
- `OVT_MainMenuContextOverrideComponent`

---

## Decisions Made During Implementation

### Phase 1 (component-developer, 2026-08-22)

- **Manager collection visibility is package, not `protected`** — mirrors `OVT_RecruitManagerComponent`, whose serializer reads the fields directly. The generic "protected + getters" guidance is superseded by this project's own manager/serializer precedent.
- **`ApplyPersistedGroups` writes the maps directly, not through `AddRecord()`** — `AddRecord()` gates on `Replication.IsServer()` and deserialization runs before that is meaningful. The `ApplyPersistedRecruits` precedent.
- **The five ask stubs each answer honestly rather than dropping.** Order/dismiss run the real `GetGroup()` lookup (always null today); quote/purchase/convert answer `RESULT_BAD_ENTRY` / `RESULT_NO_RECRUIT_GROUP` because `OVT_Faction.m_aHighCommandGroups` does not exist until Phase 3. Every ask still always answers `RpcDo_HCResult`.
- **`m_iRearmIntervalMs` / `m_iRefuelIntervalMs` declared now, unused until Phase 10** — §3.12 places them on the manager.
- Tag quad names `hc_contact` / `hc_no_ammo` are **placeholders**; the imageset entries are Phase 7's job.

**GUIDs minted:** `6B1C3D0000000001` (manager block), `6B1C3D0000000002` (request block), `6B0E7A8100000001` (serializer entry). Each verified to appear exactly once in this tree and zero times in the vanilla tree.

**Verified:** no `OVT_Global`/`GetGameMode` identifier under the new Logic files; `CONFIG_STREAM_VERSION` still 6; no `maxAttempts`; no ternaries; no new `.et`/`.conf` created so no `.meta` owed.

⚠️ **Carried into Phase 2 (status after Phase 2):** `ApplyPersistedGroups()` still rebuilds records only — the spawn-on-load walk of §3.11 is **still owed (Phase 6)**. The `RpcAsk_OrderGroup`/`RpcAsk_DismissGroup` tails are **discharged** — both now run the ownership check and call the manager.

### Phase 2 (component-developer-advanced, 2026-08-22)

- **`SCR_AIGroup.GetMembers` does not exist — the static is on `SCR_AIGroupClass`** (`SCR_AIGroupClass.c:20`). The plan's citation is wrong; `SCR_AIGroupClass.GetMembers(source, prefabs, offsets)` compiles. It also returns 0 when the prefab's `AIFormationComponent.DefaultFormation` cannot be resolved (no `AIWorld`) *without reading the slots at all*, so `ReadComposition` falls back to `source.Get("m_aUnitPrefabSlots", …)` and synthesises ring offsets. **Disclosed fallback.**
- **Members are joined with `AddAIEntityToGroup`, NOT `SCR_PlayerControllerGroupComponent.AddAIToSlaveGroup`.** That method early-returns on `group.GetSlave()` (`SCR_PlayerControllerGroupComponent.c:1439`), and a non-playable HC group has no slave — it would have been a silent no-op. `AddAIEntityToGroup` + an agent-hierarchy confirmation is the `AddRecruitAgentToGroup` shape (`OVT_RecruitManagerComponent.c:2871`).
- **The patrol ring is built in the manager, not through `OVT_OverthrowConfigComponent.GivePatrolWaypoints`.** That helper adds its cycle to the group itself and hands nothing back, so its nine waypoints could never reach `AddOwnedWaypoint` — for a group a player re-orders that is a nine-waypoint leak per order, whose only symptom is a slow server. Same shape (road-snapped `[patrol → wait]` ×4 in a cycle with `SetRerunCounter(-1)`), every waypoint owned. `OVT_OverthrowConfigComponent` is **not** modified.
- **`DismissGroup` order: `SetDeleteWhenEmpty(false)` → members → vehicle → group entity.** Emptying the group first would arm vanilla's `OnEmpty` → `CallLater(DeleteEntityAndChildren, 1)` and the explicit delete would then be a double delete of an entity vanilla still holds a pointer to; deleting the group first would mean deleting a *non-empty* group. Members before the vehicle so no vehicle is ever deleted over an occupant (`OVT_InsertionSpawningDeploymentModule.c:2140-2148`).
- **The group entity's own origin never follows its members**, so `SyncGroupPositions` reads `GetLeaderEntity()` (then the first member, then the entity) — `GetOrigin()` on the group would freeze every marker at the barracks.
- **HC vehicles are `UntrackTransient`'d at spawn.** The whole HC footprint is rebuild-on-boot: the modded `SCR_AIGroup.EOnInit`/`AddAIEntityToGroup` already untrack the group and its members. A tracked vehicle would come back on load *alongside* the one the Phase 6 load walk spawns.
- 🔴 **`m_aMemberBodyIds` cannot be populated by simply spawning.** The modded `SCR_AIGroup.AddAIEntityToGroup` and `OnAgentAdded` (`Scripts/Game/Modded/SCR_AIGroup.c:461`, `:489`) call `UntrackTransient` on every agent that joins any group, excluding only player bodies and **recruit-manager-registered** recruits. So D8's body-UUID path needs HC members registered *before* the group add, or its own exclusion in that hook — **Phase 6/Phase 9 must solve this**, or a converted recruit's gear will not survive a reload.
- **`ApplyPersistedGroups` counted members from `memberBodyIds` only** — which Phase 2 makes reachably wrong, since a spawned group writes prefabs and no body ids. Now `Math.Max(bodyIds, prefabs)`, and `m_iAliveMembers` is seeded with it.
- **Order/dismiss wired on the seam** (the carried-forward note above, discharged): both asks now run the ownership check via `ResolveCallerPersistentId(playerId)` and call the manager. No new RPC, no broadcast — that is Phase 6.
- **Group ids are minted before anything spawns**, from a monotonic salt walked against the live table with a pigeonhole bound (`m_mGroups.Count() + 1`), not a retry budget. The recruit manager's `maxAttempts` shape is deliberately not copied.

**GUIDs minted:** `6B1C3D0000000003` (`OVT_Group_HighCommand.et.meta` Name), `6B1C3D0000000004` (the group component block). Both verified: exactly one occurrence each in this tree, zero in the vanilla tree. The prefab's `RplComponent "{524EC5D51F101B32}"` is the **inherited** GUID from `Group_Base.et`, copied not minted.

✅ **DISCHARGED in Phase 3:** `SpawnGroupFromEntry(entryKey, position, ownerPersistentId)` now exists on the manager, resolves the entry by key and delegates to `SpawnGroup`. Original note: `SpawnGroupFromEntry(entryKey, position, ownerPersistentId)` does not exist yet — Phase 2 shipped `SpawnGroup(entryKey, groupPrefab, vehiclePrefab, position, ownerPersistentId)`. T3.1 must add the entry-key resolver (read `m_sGroupPrefab` / `m_sVehiclePrefab` off the authored `OVT_HighCommandGroupEntry`) and delegate; `entryKey` is already stored verbatim on the record and nothing else changes.

---

### Phase 2 (component-developer-advanced, 2026-08-22)

**Three plan citations were wrong or actively harmful. All caught by building against the real code:**

1. 🔴 **`SCR_AIGroup.GetMembers` does not exist** — the static is on **`SCR_AIGroupClass`** (`SCR_AIGroupClass.c:20`). The plan's §3.4 citation is wrong. Compile-check caught it.
2. 🔴 **`AddAIToSlaveGroup` would have been a silent no-op.** It early-returns on `group.GetSlave()` (`SCR_PlayerControllerGroupComponent.c:1439`), and a `m_bPlayable 0` HC group has no slave. Replaced with `AddAIEntityToGroup` + an agent-hierarchy confirmation (the `AddRecruitAgentToGroup` shape, `OVT_RecruitManagerComponent.c:2871`). **The plan named this call in T2.4 and it would have produced empty groups with no error.**
3. 🔴 **`config.GivePatrolWaypoints` would have leaked 9 waypoints per re-order.** It adds its own cycle and returns nothing, so its waypoints can never reach `AddOwnedWaypoint` — precisely the "only symptom is a slow server" failure this phase was flagged ADVANCED for. The ring is now built in the manager, same shape, all 9 owned. `OVT_OverthrowConfigComponent` is untouched.

**Other decisions:**
- **`DismissGroup` order is `SetDeleteWhenEmpty(false)` → members → vehicle → group.** Emptying first arms vanilla's `OnEmpty` → `CallLater(DeleteEntityAndChildren, 1)`, making the explicit delete a double-delete on a pointer vanilla still holds. Members before the vehicle so nothing is deleted over an occupant.
- `ReadComposition` has a disclosed fallback: `GetMembers` returns 0 without reading the slots when the prefab's formation cannot resolve; the fallback reads `m_aUnitPrefabSlots` directly and synthesises ring offsets.
- `PATROL_RADIUS = 150.0` added to `OVT_HighCommandRules` — §3.12 places it there and Phase 1 omitted it.
- Two Phase-1 corrections made reachable: `ApplyPersistedGroups` now counts members as `Math.Max(bodyIds, prefabs)` and seeds `m_iAliveMembers`; HC vehicles get `UntrackTransient` so a save cannot resurrect one alongside the load walk's.
- T2.9(d) asserts through `FindEntityByID` (null after deletion → safe `false`), never a pointer captured before the delete; the load-bearing half is `GetEntityObserverCount()` back at its pre-spawn baseline.

**GUIDs minted:** `6B1C3D0000000003` (group prefab), `…0004`. `RplComponent "{524EC5D51F101B32}"` is the **inherited** GUID copied from `Group_Base.et`, not minted.

### Phase 3 (component-developer-advanced, 2026-08-22)

🔴 **ONE PLAN GUID FAILED VERIFICATION.** §3.3's `truck_squad` row pairs `{16E32C3ABEAFC2C6}` with
`Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_FIA_transport_covered.et`. Recovered from inbound
references in the vanilla tree, `{16E32C3ABEAFC2C6}` is **`Ural4320_FIA_transport.et`** — the
UNCOVERED variant — and the covered one is **`{B70E6D12A8EC2410}`**. The path is what the plan
actually wanted (a covered troop truck), so the entry is authored with the corrected GUID. The other
ten pairs (8 groups, 2 UAZ variants) all verified exactly, as did every member count in the table
(2/2/2/2/4/4/5/7). Ten distinct character prefabs across the whole catalog — inside §3.4's 10–15
estimate.

**Decisions:**

- **The manifest caches COUNTS, never prices.** `GetBuyPrice` takes a position and a player id, so a
  cached price would be one town's price shown to everybody. `m_mCharacterLines` and `m_mEntryLines`
  hold `OVT_ItemSourcingLine`s with `m_iUnitPrice` 0; `BuildEntryManifest` stamps prices into FRESH
  line objects per call, so no caller can write a price into another caller's manifest.
- **An unregistered resource is DROPPED from the manifest, not carried at price 0.** A zero-price
  line would still be handed to `TakeUpTo` and would consume warehouse stock the player was never
  charged for. `OVT_ItemSourcingRules`' zero-price reporting is still exercised by its Logic case; it
  is just never reached from this caller.
- **The consumption clamp lives in the ledger.** `ConsumeQuotedStock` asks `TakeUpTo` for each line's
  FULL need and the ledger answers with what it had — the same `min(need, available)` the coverage
  split used a moment earlier, expressed once. `SplitCoverage` has no per-line out param and does not
  need one.
- **`IsRegisteredWarehouse` delegates to `realEstate.GetNearestWarehouse(pos, WAREHOUSE_MATCH_RANGE)`**
  rather than walking `m_aWarehouses` itself, because that is exactly what `PlayerMayUseWarehouse`
  does internally (`<`, not `<=`) — two copies would disagree at the boundary. `m_aWarehouses` is
  still read (the null guard). `OVT_RealEstateManagerComponent.c` is **unmodified**.
- **The scratch position is a world-bounding-box corner, not "the campaign start position".** The
  plan's phrase has no shipped accessor behind it, and the acceptance criterion is "outside every
  base" — which a corner can be CHECKED for. `ResolveScratchPosition` insets 200 m from each of the
  four corners and takes the first whose nearest base is further than `baseRange`; it falls back to
  the first corner when no base registry exists yet.
- **The quote ask exits through `ReplyQuote`, which answers `RpcDo_HCQuote` AND, on a refusal,
  `RpcDo_HCResult`.** The screen draws the quote row (keyed to the entry it asked about); the generic
  reason handler reads the result. No notification fires from the quote path — only the purchase
  notifies, or auto-quoting a list would spam the HUD.
- **The purchase spawns at the CALLER'S position, not the barracks origin.** The player is standing
  within `BARRACKS_USE_RADIUS` on ground they walked to; a building's origin is inside the building.
  Same position feeds the shop price and the warehouse query, so all three agree.
- **An authored entry with an empty `m_sKey` is `BAD_ENTRY` at gate 6.** `SpawnGroupFromEntry`
  resolves BY key, so without this an unkeyed entry would pass all nine gates and then answer
  `SPAWN_FAILED` — safe (nothing charged) but a lie about the cause.
- **A supporter rate of 0 never refuses.** `NearestTownHasSupporters` answers false when there is no
  town at all, which for a draw-down of nothing would refuse a free purchase.
  `NearestTownCanSupport(pos, 0)` short-circuits to true.
- **`PostGameStart()` on the HC manager is called from `DoStartGame()`** through `GetInstance()`, the
  way the save-point sweep already reaches it — no new cached member on the game mode. It only queues
  the manifest build, so its position in the chain is not load-bearing. ⚠️ Init-tier test worlds never
  run `PostGameStart`, so the manifest does not build there; nothing in the Init tier depends on it.
- **`.st` master entries were added for the nine notification keys** even though §4 assigns `.st`
  work to T4.6. A preset pointing at a missing key renders the raw key on screen, so T3.7 would have
  shipped visibly defective without them. `Language/*.conf` exports are untouched — **a Workbench
  re-export is owed**.
- **`ReplyQuote` and `ReplyPurchase` wrap `Rpc()`**, following Phase 1's shipped `ReplyResult`. Each
  helper contains exactly one `Rpc()` call at a fixed literal arity, which is not the generic
  variadic wrapper DoD **Q5** is aimed at — but Phase 6 owns Q5 and should rule on it explicitly.

**GUIDs minted:** `6B1C3D0000000010`–`…001A` (11 FIA catalog entries), `6B1C3D0000000020`–`…0031`
(9 notification presets + their UI infos), `6B1C3D00000000A0`–`…00A8` (9 `.st` entries). Each verified
to appear exactly once in this tree; `grep -rl 6B1C3D` over the vanilla tree → 0 hits (re-run, not
inherited from the planning sweep).

**Known limits, for Phase 4/6 play-test:**
- The gear walk reads `InventoryStorageManagerComponent.GetItems()` then recurses into every
  `BaseInventoryStorageComponent` on each item, de-duplicated by `EntityID`. A magazine already
  loaded in a weapon's muzzle may not appear — a small undercount, never an overcount.
- `MANIFEST_CAPTURE_ATTEMPTS = 5` is the plan's number, not a measured one. A
  `read no gear at all off '<prefab>'` WARNING in the log means the budget is too tight, not that the
  prefab is empty.
- `QuoteEntry` runs a 150 m `QueryEntitiesBySphere` per quote. If the purchase screen auto-quotes on
  every selection change, Phase 4 should debounce it.

---

### Phase 3 (component-developer-advanced, 2026-08-22)

🔴 **A fourth wrong plan citation — an authored GUID this time.** §3.3 pairs `{16E32C3ABEAFC2C6}` with `Ural4320_FIA_transport_covered.et`. **Verified by the orchestrator** against `/mnt/n/Projects/Arma 4/ArmaReforger/Configs/EntityCatalog/FIA/Vehicles_EntityCatalog_FIA.conf`:

| GUID | Real prefab |
|---|---|
| `{16E32C3ABEAFC2C6}` | `Ural4320_FIA_transport.et` (**uncovered**) — `:52` |
| `{B70E6D12A8EC2410}` | `Ural4320_FIA_transport_covered.et` — `:63` |

Authored with the corrected `{B70E6D12A8EC2410}`; the plan's *path* was the intent. The other ten group/vehicle pairs and all eight member counts verified exactly. **Running total: four plan citations wrong in three phases** — keep verifying every cited GUID and every named engine call before use.

**Decisions:**
- **The manifest caches counts, never prices.** `GetBuyPrice` takes a position and a player id, so a cached price is one town's price shown to everyone. Priced lines are `new`-ed per call.
- **An unregistered resource is dropped, not carried at price 0** — a zero-price line would still reach `TakeUpTo` and consume warehouse stock nobody was charged for.
- **The consumption clamp lives in the ledger.** `ConsumeQuotedStock` asks for each line's full need and `Take()` answers with what it had — the same `min(need, available)` the split used, expressed once.
- **`IsRegisteredWarehouse` delegates to `GetNearestWarehouse(pos, WAREHOUSE_MATCH_RANGE)`** — what `PlayerMayUseWarehouse` does internally (`<`, not `<=`). A hand-rolled loop would disagree at the boundary.
- **Purchase spawns at the caller's position, not the barracks origin** — a building origin is inside the building. The same position feeds price, warehouse query and spawn.
- **Empty `m_sKey` → `BAD_ENTRY` at gate 6**, so an unkeyed entry cannot pass nine gates and then answer `SPAWN_FAILED` — safe, but a lie about the cause.
- Scratch manifest position is a world bounding-box corner inset 200 m, first of four whose nearest base is beyond `baseRange` ("campaign start position" has no shipped accessor).
- **9 `.st` entries added with the presets** (nominally T4.6) — a preset pointing at a missing key renders the raw key, so T3.7 would otherwise have shipped visibly defective. **Orchestrator verified `.st` braces balanced at 2120/2120** and `Configs/Language/` untouched.

⚠️ **Correction (Phase 6, 2026-08-22):** Phase 3's report claimed `RpcDo_HCQuote` was missing from §3.6's table and the orchestrator repeated it. **It is wrong** — §3.6 lists `RpcDo_HCQuote` as row **#6**. The genuinely missing twelfth row is `RpcDo_RecruitQuote`, whose 5 → 6 change Phase 5 owed to this table. The audit table below is correct.

**⚖️ Orchestrator ruling on DoD Q5 ("no `Rpc()` call wrapped in a helper") — the helpers STAND.** Q5 exists because of BUG-090: `Rpc()` is untyped variadic, so wrong arity compiles clean and dies at the wire. `ReplyResult` / `ReplyQuote` each contain **exactly one** `Rpc()` at a fixed literal arity, sitting directly beside the `ShouldRespondLocally` branch that calls the same handler with the identical argument list — so an arity mismatch is visible in two adjacent lines, which is *safer* than inlining, not looser. The rule targets generic dispatchers that hide which RPC and how many args; these are not that. **Condition: Phase 6's audit table must carry a "sent via" column naming the helper for each RPC, so the 1:1 mapping stays auditable.**

---

### Phase 4 (ui-developer, 2026-08-22)

**Orchestrator-verified independently:** all three same-GUID deltas match their vanilla file's **line 1 and root `ID` byte-for-byte**, and each `.et.meta` `Name` carries the correct vanilla GUID (`{5F97E54397247954}` / `{2CB4D91249389DFD}` / `{60A21DFAFF77773D}`) with the identical path. `.st` braces balanced (2204/2204). `Configs/Language/` untouched. **No economy call anywhere in the purchase context** — it renders the server quote and nothing else.

**Decisions:**
- **The screen reuses Phase 3's 9 `#OVT-Msg-HCPurchase*` sentences as in-screen refusal text** rather than authoring parallel copies — one wording per refusal, whatever surface shows it.
- **`#OVT-Barracks` was added even though it predates this phase** — the user's prefab referenced it with no `.st` backing, so the map POI would have rendered a raw key.
- `m_tPreview` omitted from the Barracks buildable entry — **no `.edds` preview art exists**. The build card will render without a thumbnail until art is made.
- `RpcDo_HCQuote`/`RpcDo_HCResult` gained `m_OnHCQuote`/`m_OnHCResult` `ScriptInvoker`s so the screen can subscribe. **No arity change, no new RPC** — Phases 6–9 should subscribe the same way rather than re-plumbing the stubs.
- The debounce pattern (`m_iQuotedEntryIndex == m_iSelectedIndex`) is the template for any future quote-gated Buy button here.

🔴 **Phase 4 defect, found by the user in Workbench and fixed same-day (2026-08-22): `OVT_Barracks.et` rendered the desk and nothing else.**

Cause: the agent re-declared the inherited `SCR_DestructibleBuildingComponent` with a **freshly minted** GUID (`{6B1C3D0000000043}`) instead of the inherited one. A minted GUID does not override the parent's component — it **adds a second one**, so the entity carried two destruction managers and the building mesh never rendered.

Fix: copy the parent's own GUID. `Barracks_01_Base.et:` declares `SCR_DestructibleBuildingComponent "{5D7E937B62062DE5}"`, so the delta must reuse **`{5D7E937B62062DE5}`** with `Enabled 0`. This is exactly what the shipped precedent does — `OVT_Warehouse.et` re-declares `"{5D7AD0958CE7D71F}"`, which is `Warehouse_01_Base.et:29`'s own GUID.

**The standing rule — this feature alone hit it THREE times (the building mesh, `OVT_PlaceableComponent` on the radio, then the radio's `Hierarchy`), so treat it as the default suspicion whenever a prefab renders wrong:** re-declaring an *inherited* component to change one field means **copying that component's GUID**. Minting one adds a duplicate. Only genuinely new components and new child entities get fresh GUIDs. (`OVT_PlaceableComponent "{65CE9A2ECEC30E10}"` on the desk child is correct — that *is* the inherited GUID from Overthrow's `FurnitureMilitary_base.et` override.)

Verified after the fix: compile-check exit 0; the three same-GUID vanilla deltas declare **no** destruction component at all, so vanilla barracks keep vanilla destruction — only the buildable gets Overthrow's phase destruction.

⚠️ **Input note for Phase 7:** this phase authored `OverthrowHighCommandBuy` (`KC_B` / `gamepad0:x`) and `OverthrowHighCommandContext` (Priority 50, Flags 4). §3.9 gives `OverthrowHCOrder` `gamepad0:x` and `OverthrowHCStance` `KC_B`. The checker is at baseline **today** because the contexts do not coexist — Phase 7 must confirm that still holds once the map context is added, not assume it.

### Phase 5 (component-developer, 2026-08-22)

- **Routing deviation (orchestrator's call):** one `component-developer` did all four tasks. The plan splits T5.4 to a `ui-developer`, but T5.4 is an RPC arity change plus one rendered label — component work with a one-line UI touch.
- 🔴 **A fifth drifted plan citation.** §3.8 names `BuildQuote` at `OVT_RecruitCommandComponent.c:1014`; the real method is **`ValidateTentPurchase`**. The plan's own preamble warns citations drift on this tree — they do.
- **Manifest lines are aggregated by resource, not one per occurrence.** Load-bearing: `SplitCoverage` checks each line's stock independently, so two lines for the same resource would each see the whole warehouse total and **double-count coverage**.
- **Orchestrator-verified arity (BUG-090 is the reason this gets checked by hand):** all three sites at 6 — `:1148` local branch, `:1152` `Rpc(...)`, `:1168` handler — plus `OVT_LoadoutsContext.OnRecruitQuote` `:385` at 6. The file's own audit comment block was updated 5 → 6.
- **⚖️ Orchestrator ruling — the `RESULT_GEAR_FAILED` gate STANDS.** The agent additionally refused to consume stock when the gear apply failed wholesale. Not asked for, but it mirrors `ChargeFor`'s own D19 exception: a total gear failure charges no gear fee, so consuming warehouse stock for gear that never reached the body would be a silent resource leak with neither charge nor benefit. Correct, and consistent with the codebase's money-safety pattern.
- **No-warehouse regression proved by construction, not just by test:** with nothing in range `CollectStores` returns empty → `CountAvailable` is 0 per line → `covered = min(needed, 0) = 0` → `chargedSubtotal = Σ(needed × unitPrice)`, which is exactly `m_iSubtotal` as aggregated by the same per-resource walk. Pinned by `..._TentDiscountNoStockMatchesRawSubtotal`.
- `.st` braces 2204 → **2206**, balanced (orchestrator-verified). `Configs/Language/` untouched.

> ℹ️ `Scripts/Game/Utilities/OVT_StorageUtils.c` shows as modified in the working tree. That is **not this feature** — it is the user's own concurrent work. Left alone.

---

### Phase 6 (network-specialist-advanced, 2026-08-22)

**Two places the plan's literal wording could not be followed. Both are disclosed, both are safer than the
literal reading, and neither changes what a player sees.**

1. 🔴 **§3.11's load walk, taken literally, empties the group.** The plan reads "spawn
   `OVT_Group_HighCommand.et` → for each member, `RequestSpawn` filtered by the saved UUID … falling back to
   a fresh spawn of `memberPrefabs[i]` on timeout or failure". `RequestSpawn` is ASYNCHRONOUS, and
   `m_bDeleteWhenEmpty 1` deletes a group one frame after its last member leaves — an empty group waiting on
   an answer would be deleted before any body arrived, and the 15 s timeout would then fire against nothing.
   **The fallback is therefore PRE-EMPTIVE, not reactive:** the saved roster's prefabs are spawned and joined
   first, so the group is never empty and never a man short of what the player owned, and each stored body
   that does come back JOINS FIRST and then retires one stand-in — `n → n+1 → n`, never zero. A timeout is
   then simply "the stand-in keeps the slot", which is the outcome the plan asked for.
2. 🔴 **A restored group is rebuilt from the SAVED roster, not the catalog entry.** `SpawnGroupFromEntry`
   reads the entry's composition, which would bring a squad that lost two men back at full strength — and
   Phase 12's `…_HighCommandMemberBodies_RoundTrip` ("an altered roster reloads altered") could never pass.
   `SpawnGroup` gained one optional trailing parameter, `OVT_HighCommandRecord restoring = null`; when it is
   present the composition is `record.m_aMemberPrefabs`, the group id is the persisted one, and the record's
   stance, destination and stored body ids are kept. Every existing call site is unchanged. **The brief's
   instruction to call `SpawnGroupFromEntry(record.m_sEntryKey, …)` is followed literally** — the parameter
   is passed through it.

**Decisions:**

- **The change filter compares against a dedicated echo, not against the record.** `m_mStatusEcho` holds
  what was LAST SENT per group, and it is written in exactly one place — `ApplyGroupStatus`, which only runs
  on an actual send (and on `ApplyGroupCreated`, which seeds it so the first heartbeat after a purchase is
  already silent). Comparing against the record instead would have been subtly wrong: the record is also
  written by the save-point sweep and by the load walk, so a save would have suppressed a heartbeat that was
  never sent and left every client's marker stale until the group moved again.
- **The record is ALWAYS written; only the wire is filtered.** `SweepStatus` re-measures every live group and
  writes position, flags and alive count onto the record every tick regardless, because the record is the
  save's input and the listen host's own map source. `HasStatusChanged` gates the broadcast and nothing else.
- **`RplSave` takes its count from a snapshot, not from `m_mGroups.Count()`.** The recruit manager's
  `RplSave` writes a count and then `continue`s past a null element mid-loop, which desynchronises the stream
  for every field after it with no way for the reader to notice. The HC payload builds the array of writable
  records first and writes THAT count.
- **The member cap is the payload's first field** (§3.12), and `GetMemberCap()` prefers it on a client —
  difficulty settings are server-only, so without it a client's roster could not render `n / cap`.
  `CONFIG_STREAM_VERSION` is unchanged at 6. `GetMemberCap()` also gained a `m_Difficulty` null guard:
  `OVT_OverthrowConfigComponent.GetHighCommandMemberCap()` dereferences it unguarded, and on a client it can
  be null.
- **The four broadcasts take a LOCAL-APPLY branch first, which is the broadcast analogue of
  `ShouldRespondLocally`.** That helper is an owner-RPC device on `OVT_ControllerRequestComponent` and has
  no meaning for a `RplRcver.Broadcast` RPC sent from the authority: the engine never loops a broadcast back
  to its sender, so on a listen host the host's OWN listeners would never fire. Each helper therefore reads
  `Apply…(args); Rpc(RpcDo_…, args);` with the identical argument list on adjacent lines. Rows 6, 7 and 12 of
  the audit table use the literal `ShouldRespondLocally` form, unchanged.
- **`RemoveRecord` is the single broadcast point for #9.** Every removal route reaches it — dismissal, a wipe
  through the group component's `OnDelete`, and a save record that could not be rebuilt — and a second call
  for the same id returns before broadcasting because the record has already gone.
- **`m_aMemberBodyIds` and `m_aMemberPrefabs` are rebuilt at the SAVE POINT, not on the heartbeat.**
  `CaptureMemberRoster` materialises a persistence id per member (`OVT_PersistenceTracking.Save` when the
  lazy registration has not landed), which is a real storage write — the recruit sweep's explicit rule is
  that this is save-point work and must never be put on a timer. The two arrays are INDEX-ALIGNED and a
  member with no id yet contributes an empty string rather than being skipped, so the alignment survives.
- **T6.7 registers member bodies in the manager BEFORE the group add, on both flows** — `SpawnMembers` and
  the load walk's `AdoptRestoredBody` — and adds the HC exclusion to **both** hooks in
  `Scripts/Game/Modded/SCR_AIGroup.c`: in `AddAIEntityToGroup` (`:461`) it sits immediately before the
  unconditional `UntrackTransient`, and in `OnAgentAdded` (`:489`) immediately after the recruit exclusion.
  Both hooks are needed because HC joins through `AddAIEntityToGroup` (whose own untrack is unconditional and
  would fire even though `OnAgentAdded` excused the body a moment earlier). `RegisterMemberBody` also calls
  `OVT_PersistenceManagerComponent.CancelUntrackTransient` — the BUG-131 remedy for a body being promoted
  into a category that must stay tracked, which is what Phase 9's converted recruits will be.
- **`PruneMemberRegistry()` runs on the heartbeat.** Without it the member registry grows for the length of a
  campaign, and an entity id the engine hands out again would be treated as a member of a group it never
  joined. Collected-then-removed, because removing inside the iteration invalidates it.
- **`ApplyPersistedGroups` now refuses to replace a record whose group is standing in the world.** Saved data
  can be re-applied to a running session (`ReapplyLatestSaveData`), and replacing a live record would orphan
  the entity — nothing would ever point at it again. ⚠️ **Known limit:** a re-apply that introduces a record
  with no live group will NOT rebuild it, because `DoStartGame` is not re-entrant and the restore walk is
  driven from `PostGameStart`. That is a debug/recovery path, not the load path; flagged for Phase 12.
- **`ApplyPersistedGroups` also stamps `m_iStatusFlags = 0` and both entity ids explicitly** — the "`new`
  sets every field" rule, and the INVALID entity id is what the restore walk identifies a saved group by.
- **The restore walk drops an unreadable record with an ERROR naming its id**, in three cases: an empty or
  unpublished `m_sEntryKey`, no saved position AND no saved destination, and a spawn that failed. Silently
  skipping would leave its owner carrying a group against their cap that nobody could ever see.
- **A restored vehicle group re-seats** by re-running `SeatNextMember(groupId, 0)` once nothing is in flight
  for it; `SeatAgent` already refuses a member that `IsInCompartment()`, so only the bodies that replaced a
  seated stand-in are picked up.
- **Three `ScriptInvoker`s added on the manager** — `m_OnHCGroupAdded` / `m_OnHCGroupUpdated` /
  `m_OnHCGroupRemoved`, each carrying the `OVT_HighCommandRecord`. Phases 7 and 8 should subscribe to these
  rather than polling, and must NOT re-plumb `m_OnHCQuote` / `m_OnHCResult`, which Phase 4 wired.

**How the parked-group silence was guaranteed (an acceptance criterion):**

1. `RpcDo_HCStatus` has **exactly one** send site — `BroadcastGroupStatus` (grep: `Rpc(RpcDo_HCStatus` → 1 hit).
2. `BroadcastGroupStatus` has **exactly two** callers (grep → `:1712` and `:2341`): `SweepStatus`, which
   reaches it only when `HasStatusChanged(...)` returned true, and `FinishRestoreIfSettled`, which runs once
   per group at load and is also what seeds that group's echo.
3. `HasStatusChanged` returns false when the status mask AND the alive count are identical to the echo AND
   the position moved no further than `STATUS_POSITION_THRESHOLD` (5 m, compared as a SQUARED distance —
   `vector.Distance` is not correctly rounded, so an exact-boundary decision would be a coin flip).
4. The echo is seeded the moment a group is created (`ApplyGroupCreated` → `SeedStatusEcho`), so a group that
   has not moved since it was bought is silent from its very first heartbeat, not from its second.

**Verified by hand this phase:**

- `RplSave` and `RplLoad` read side by side: `int` cap, `int` count, then per group
  `string / string / string / int / vector / vector / int / int / int` — identical order, identical types,
  `m_iTotalMembers` appended last in both with the append rule stated in both docblocks and in the class
  header. Every `reader.Read*` return is checked; the first failure returns false.
- `OVT_HighCommandManagerSerializer` untouched this phase — `Serialize` and `Deserialize` both still name the
  local `records`, and its single `context.Read(records)` return is checked. No `array<bool>` anywhere.
- `CONFIG_STREAM_VERSION` still `6`. No `maxAttempts`, no ternary in any file this phase touched.
- **No client path dereferences a group entity.** `GetGroupEntity` / `GetVehicleEntity` have 12 call sites,
  all inside the manager on server-gated paths, plus one in the Init suite (a listen host). No UI file, map
  file or context calls either. Both accessors now say so in their docblocks.
- I1–I6 clean: `git diff --exit-code` over `OVT_RealEstateManagerComponent.c`, `OVT_QRFControllerComponent.c`,
  `OVT_ObjectivePhaseRules.c`, `OVT_RecruitPurchaseRules.c`, `OVT_StorageComponent.c`, `OVT_StorageLedger.c`,
  `OVT_FuelUtils.c`, `OVT_FuelPricing.c`. (`OVT_StorageUtils.c` is the user's concurrent work and was left
  alone, as instructed.)
- No `.st`, no `Configs/Language/*.conf` and no `.conf` of any kind touched — this phase adds no localized
  string, only `Print()` diagnostics. No new `.et`, so no `.meta` owed and no GUID minted.

**Known limits, for Phase 7/8 and the play-test:**

- **`RplSave`/`RplLoad` cannot be unit-tested.** `ScriptBitWriter` constructs fine from script and hard-crashes
  on first use (project memory), so a hand-rolled JIP payload is only ever provable by a real JIP join —
  play-test step 42.
- **`STATUS_POSITION_THRESHOLD = 5.0` is a chosen number, not a measured one.** A defending group's leader
  shuffles for cover; if the map marker jitters, raise it. If a walking group's marker looks laggy, lower it
  or shorten `STATUS_SYNC_INTERVAL_MS`.
- **The heartbeat's contact read is `SCR_AICombatComponent.GetCurrentTarget()` on each member.** That is the
  same question vanilla's own `SCR_AIDecoCombatEnemy` asks, but whether it flickers on and off between two
  ticks in a real firefight (and therefore how much the `CONTACT` badge blinks) is play-test territory.
- **A member killed in combat is only noticed on the next heartbeat** — up to 10 s of a stale alive count.
  Deliberate: the alternative is a death hook per member, which is Phase 10 territory if it is wanted at all.
- **The load walk has never been run against a real save**, because no save yet contains an HC group with
  body UUIDs — `m_aMemberBodyIds` only starts being written now that T6.7 keeps the bodies tracked. Play-test
  step 29 is what proves it, and the tell for a broken body path is a restored squad in the right place with
  the wrong gear (the stand-ins kept every slot).

### Phase 7 (ui-developer-advanced, 2026-08-22)

🔴 **The binding collision was far worse than Phase 4 flagged, and the fix is structural.** Both §3.9 verbs collide with **vanilla `MapContext` itself**, not just with Phase 4's buy action:

| Input | Also owned by |
|---|---|
| `gamepad0:x` | `MapContextualMenu` (opens `SCR_MapRadialUI`) **and** `OverthrowHighCommandBuy` |
| `keyboard:KC_B` | `MapToolProtractor`, `OverthrowHighCommandBuy`, `OverthrowLoadoutsBuyRecruit` |
| `gamepad0:y` | `OverthrowToggleRecruits`, `HintDismiss` |
| `keyboard:KC_G` | `HintDismiss` |

The agent **proved `SCR_MapRadialUI` is live on Overthrow's map** rather than assuming: `MapOverthrow.conf` inherits `MapFullscreen.conf`, derived `SCR_MapConfig` arrays **merge by GUID rather than replace** (`MapFullscreenConflict.conf` re-declares two components using the parent's GUIDs), and decisively — `MapOverthrow.conf` never lists `SCR_MapCursorModule`, yet `SCR_MapCursorInfo` (written only by that module) is what the shipped hover magnet reads and it works.

**Two independent locks, both required:**
1. `OverthrowMapCommandContext` is `Priority 70, Flags 0x2e 0` — §3.9's `0x26` **plus bit `0x8`**, the runtime-proven exclusivity bit (BUG-156). An exclusive claim at 70 suppresses every priority-50 claimant, so a double-fire is *structurally* impossible.
2. `TickCommandContext()` refuses to renew the lease while `OVT_UIManagerComponent.IsAnyContextBlocking()` is true. **Phase 4's scepticism was justified** — Overthrow menus do not disable the character tier, so a player *can* raise the map gadget over the purchase screen. Exclusivity alone was not allowed to protect a money path.

**⚖️ Orchestrator ruling — omitting `Menu*` from the map context STANDS, against §3.9's literal `ActionRefs`.** The context lists only the two verbs. Adding `Menu*` to an **exclusive** context would claim `gamepad0:a`, both sticks and the whole d-pad away from `MapContext` — which *is* the "ships controller-dead" failure the requirement names. Selection instead rides vanilla `MapSelect` (LMB / `a`) and deselect rides the existing `OverthrowCloseInfoPanel` (`KC_C` / `b`). **Zero new navigation bindings.** The rule's purpose (a pad can move inside the screen) is satisfied by `MapContext`, which already works.

**✅ Orchestrator-verified: the input-conflict gate was NOT weakened.** The agent added two `ACKNOWLEDGED` entries to `check-input-conflicts.py`. Running the **committed, unmodified** checker against the current tree gives a byte-identical verdict (`0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged`, exit 0), so the entries are genuinely inert documentation and the baseline is green on its own merits. `--warnings` likewise.

**Other decisions:**
- **No new imageset entries** (context.md had assigned them here). An `ImageSetDefClass` needs atlas art in `overthrow_mapicons_atlas.edds`; a def over unused pixels renders garbage. `TAG_CONTACT`/`TAG_NO_AMMO` are untouched (Logic cases stay green) and the layer maps them onto shipped quads — contact → `ID_D:Hostile_Land_Bcg` tinted red, no-ammo → `overthrow_mapicons:recruit_ammo_empty`. **Two constants to change when art arrives.**
- `m_OnHCGroupUpdated` drives the panel only, never a marker rebuild — `Update()` re-reads records per frame, so rebuilding per heartbeat would churn the widget set.
- Hit the reserved-keyword trap again: a local named `owned` fails to parse. Renamed `isOwner`.
- `.st` braces 2206 → **2254**, balanced (orchestrator-verified). `Configs/Language/` clean.

⚠️ **Untested — UI has no automated coverage.** Chiefly: that `Flags 0x2e` actually suppresses `MapContextualMenu`. **If `X` opens the radial menu *and* orders, the one-line fallback is to move `OverthrowHCOrder`'s pad source to `gamepad0:thumb_left`** — the only pad input confirmed free on this map. Panel anchor and 36 px marker size are eyeballed; the `ID_D`/`ICO_Land` composite has never been rendered.

---

### Phase 8 (ui-developer, 2026-08-22)

3/3 tasks. Compile-check exit 0 (6311 files). Input checker at the shipped baseline plain **and** `--warnings`, byte-identical to the recorded baseline.

**T8.1 decisions:**
- **`OVT_MapHighCommandLayer.SelectGroup` promoted `protected` → public**, per the plan's explicit instruction — no second selection mechanism was written. "Show on Map" raises the fullscreen map gadget (`OVT_MapContext.ShowMap()`) and, once it finishes opening, calls the layer's own `SelectGroup(groupId)` through `mapEntity.GetMapUIComponent(OVT_MapHighCommandLayer)`.
- **The map-open-complete subscription deliberately survives `OnClose()`.** `ShowMap()`'s raise animation is NOT synchronous (`SCR_MapGadgetComponent.SetMapMode` queues its own `CallLater` at `m_fActivationDelay`, default 300 ms), so the layer's markers do not exist the instant `ShowOnMap()` returns. The selection is deferred to `SCR_MapEntity.GetOnMapOpenComplete()` — but `CloseLayout()`, called at the end of `ShowOnMap()`, runs `OnClose()` in the SAME call frame. Tearing the subscription down in `OnClose()` would cancel the very request `ShowOnMap()` just made. The subscription is self-removing on the one answer it is waiting for instead (`OnMapReadyForSelection`), the `OVT_MainMenuContext.Save()`/`OnSaveResult` shape — documented at both ends (the method header and `OnClose()`'s own comment) so it does not read as a missed cleanup.
  - **Known edge case, accepted:** if `ShowMap()` succeeds but the map somehow never finishes opening (or something else opens a map first during the ~300 ms window), the pending selection is only ever consumed by the next `GetOnMapOpenComplete()` firing, whatever caused it. Checked with `mapContext.ShowMap()`'s own bool return before subscribing, so a *failed* raise (no map item — the shipped `MustHaveMap` notification) never leaves a stale subscription.
- **The roster has no active/inactive split** — every High Command group is always live, so the "flat model" is simply: selection indexes `m_aEntries`, never widget-tree position, the same rule `OVT_RecruitEntryRef`'s header states, applied to one section instead of two.
- **Dismiss is server-confirmed, not optimistic.** Unlike `OVT_RecruitsContext.OnConfirmDismiss` (which hints immediately), this screen waits for `RpcDo_HCResult` before showing `#OVT-HC_Roster_Dismissed` — an in-flight guard (`m_bDismissInFlight` + matching `groupId`) ignores any other answer this shared controller component receives (e.g. a map order). The row itself is removed by `m_OnHCGroupRemoved` → `Refresh()`, already subscribed — **nothing client-side deletes anything**.
- **`MenuSelect` is deliberately unwired**, the `OVT_HighCommandPurchaseContext` rule: a pad's confirm button must not be able to dismiss a group while the player is just browsing the list.
- **Status label/colour/stance-label helpers live as `static` methods on `OVT_HighCommandRosterRowHandler`**, called by both the row and the context's details panel, rather than duplicated — but the contact/no-ammo badge quad mapping (`Hostile_Land_Bcg` / `recruit_ammo_empty`) IS duplicated from `OVT_MapHighCommandLayer`, on purpose: Phase 7 already established "two constants to change when art arrives" as the pattern for this exact no-art gap, and centralising it now would mean one shared accessor two unrelated screens depend on for a placeholder.
- **`DISMISS_HC_GROUP` reuses `CreateFromPreset`'s exact resource-GUID literal** already used by `DISMISS_RECRUIT` (`{26C9263913A8D1BD}Configs/UI/Dialogs/DialogPresets_Campaign.conf`) rather than minting a fresh one — three different GUIDs already label this same `.conf` path across the codebase (`OVT_CampMenuContext`, `OVT_RecruitsContext` ×2), so copying a proven-working one over inventing a fourth was the safer call.
- **The main menu title and button reuse ONE key** (`#OVT-MainMenu_ManageGroups`) for both the `MainMenu.layout` button text and the roster's own header — the `OVT-MainMenu_ManageRecruits` precedent (`RecruitsMenu.layout:84` uses the exact same key as its menu button).

**GUIDs minted:** `6B1C3D0000005000` (`HighCommandRoster.layout` resource), `…5001`–`…5021` (its widget tree), `…5040` (`HighCommandRosterRow.layout` resource), `…5041`–`…504C` (its widget tree + handler component), `…5090` (`OVT_HighCommandRosterContext` instance on `Character_Player.et`), `…5091` (the MainMenu "Manage Groups" button widget), `…50A0`–`…50A9` (2 new `Action` blocks), `…50B0`–`…50B2` (the `DISMISS_HC_GROUP` dialog preset), `…5100`–`…510B` (12 `.st` entries). Every GUID verified to appear exactly once in this tree (except `…5000`/`…5040`, which legitimately recur three times each as a `.layout.meta` `Name` cross-referenced by the `[Attribute]` default and the `Character_Player.et` registration — the Phase 4 precedent) and zero times in the vanilla tree.

**`.st` braces: 2254/2254 before this phase's edit, 2278/2278 after** (12 entries × 2 braces = +24). `Configs/Language/` untouched.

**What Phase 9 must know:**
- **The roster is a plain `OVT_RecruitsContext`-shaped flat list with NO group-of-groups concept** — Phase 9's converted-recruit groups will appear here exactly like a purchased one, needing no roster change, PROVIDED conversion produces a normal HC record (which the plan already requires).
- **`m_OnHCGroupAdded` fires the moment `SpawnGroup`/conversion calls `AddRecord`**, so a freshly converted group will appear in this roster on its own, with no additional wiring — the same three invokers (`Added`/`Removed`/`Updated`) this screen is subscribed to are exactly what Phase 9's conversion path also needs to fire, and it already will if it goes through the manager's existing `AddRecord`/`BroadcastGroupCreated`.
- **The dismiss flow is the reference for any future "ask, wait for the matching `RpcDo_HCResult`, then react" UI** in this feature (in-flight guard keyed on `groupId`, not just a boolean) — Phase 9's Convert button, if it wants a result-driven success/failure hint rather than an optimistic one, can copy this shape directly.

### Phase 8 (ui-developer, 2026-08-22)

- **"Show on Map" reuses Phase 7's selection, it does not duplicate it.** `SelectGroup` was promoted `protected` → public (as Phase 7 asked). The flow raises the map with `OVT_MapContext.ShowMap()` then defers the selection to `SCR_MapEntity.GetOnMapOpenComplete()`, because the raise is **animated, not synchronous** (`SCR_MapGadgetComponent.SetMapMode` queues its own `CallLater`).
- ⚠️ **That one subscription deliberately outlives `OnClose()`** — `CloseLayout()` runs `OnClose()` synchronously inside `ShowOnMap()`, so tearing it down there would cancel the request just made. It self-removes on the single answer it waits for (the `OVT_MainMenuContext.Save()` precedent). Documented at both ends. This is the *only* exception to the "OnClose removes exactly what OnShow inserted" rule in this feature.
- **Dismiss is server-confirmed, not optimistic** (it waits for `RpcDo_HCResult`), unlike `OVT_RecruitsContext`'s dismiss. A deliberate strengthening — it keeps "nothing client-side deletes anything" true end to end.
- One `.st` key serves both the menu button and the screen title (the `OVT-MainMenu_ManageRecruits` precedent) rather than a redundant `_Title` key.
- New input actions: `OverthrowHighCommandRosterShowOnMap` (`KC_M` / `gamepad0:shoulder_right`), `OverthrowHighCommandRosterDismiss` (`KC_X` / `gamepad0:y`), in a Priority 50 / Flags 4 menu context. `gamepad0:shoulder_left` remains untouched (VON, BUG-092).
- `.st` braces 2254 → **2278**, balanced (orchestrator-verified). `Configs/Language/` clean. **I9 verified: `OVT_MainMenuContextOverrideComponent` untouched.**
- ✅ **Checker verified against the COMMITTED script again** — green on its own merits with Phase 8's three new bindings; the agent did not edit it this phase.

---

## ✅ CLOSED ARCHITECTURAL GAP — found in Phase 2, closed in Phase 6 (T6.6 / T6.7)

> **Both halves are closed.** T6.6 shipped `QueueRestoreWalk` / `RestoreNextGroup` / `RestoreGroup` on the
> manager, driven from `PostGameStart()`. T6.7 shipped `RegisterMemberBody` / `IsMemberBody` on the manager
> and the matching exclusion in **both** `Scripts/Game/Modded/SCR_AIGroup.c` hooks. See the Phase 6 section
> below for the two places the plan's literal wording could not be followed and what was done instead.

**1. The §3.11 spawn-on-load walk was assigned to no phase.** Phase 1 built serializer v1, Phase 2 built `SyncGroupPositions`, Phase 6 is replication, Phase 12 only writes the round-trip *cases*. Nothing actually re-creates groups on load. **Added as T6.7's sibling T6.6.** Without it F18 and F19 cannot pass and Phase 12's round-trip cases will fail.

**2. HC member bodies are untracked the instant they join the group, so `m_aMemberBodyIds` can never be populated.** Verified by the orchestrator at `Scripts/Game/Modded/SCR_AIGroup.c`:

- `AddAIEntityToGroup` (`:461`) calls `OVT_PersistenceManagerComponent.UntrackTransient(entity)` on **every** entity it adds, unconditionally.
- `OnAgentAdded` (`:489`) is the catch-all for AI reaching a group by any other route, and excludes exactly two categories: **player bodies** and **recruit bodies registered in the recruit manager *before* the group add**.

HC members are neither, so every member is untracked and D8's body-UUID persistence is dead. **This also kills Phase 9's core promise** — "converted recruits keep their bodies and therefore their equipment" — because the conversion moves bodies into an HC group, which untracks them.

**Fix (T6.7), mirroring the shipped recruit precedent exactly:** register HC members with the HC manager **before** the group add, and add the HC exclusion to both hooks. Do **not** invent a second mechanism — the recruit exclusion is the pattern, and its comment at `:483-488` already documents why the ordering is load-bearing.

---

## RPC Arity Audit (Q4 — filled in Phase 6)

**Twelve rows.** §3.6's table has eleven and DOES list `RpcDo_HCQuote` at #6 (the brief's note that it omits
it is incorrect — verified by reading §3.6). The twelfth row is **`RpcDo_RecruitQuote`**, which this feature
changed from arity 5 to 6 in Phase 5 and whose audit entry Phase 5's own acceptance criteria explicitly owed
to this table. Every row below was checked by reading the send site and the handler declaration side by side;
the `Sent via` column is the Q5 ruling's condition.

| # | RPC | Arity | Sent via | Handler | Checked |
|---|-----|-------|----------|---------|---------|
| 1 | `RpcAsk_QuoteGroup(int entryIndex)` | 1 | `RequestQuoteGroup` — direct `:78` / `Rpc()` `:80` | `OVT_HighCommandRequestComponent:136` | ✅ |
| 2 | `RpcAsk_PurchaseGroup(int entryIndex)` | 1 | `RequestPurchaseGroup` — `:89` / `:91` | `OVT_HighCommandRequestComponent:228` | ✅ |
| 3 | `RpcAsk_OrderGroup(string, int, vector)` | 3 | `RequestOrderGroup` — `:102` / `:104` | `OVT_HighCommandRequestComponent:343` | ✅ |
| 4 | `RpcAsk_ConvertRecruitGroup(string)` | 1 | `RequestConvertRecruitGroup` — `:113` / `:115` | `OVT_HighCommandRequestComponent:400` | ✅ |
| 5 | `RpcAsk_DismissGroup(string)` | 1 | `RequestDismissGroup` — `:124` / `:126` | `OVT_HighCommandRequestComponent:419` | ✅ |
| 6 | `RpcDo_HCQuote(int, int, int, int, int, int)` | 6 | `ReplyQuote` — `ShouldRespondLocally` `:517` / `Rpc()` `:519` | `OVT_HighCommandRequestComponent:644` | ✅ consumer `OVT_HighCommandPurchaseContext.OnQuote:335` also 6 |
| 7 | `RpcDo_HCResult(int, int, string)` | 3 | `ReplyResult` — `ShouldRespondLocally` `:615` / `Rpc()` `:619` | `OVT_HighCommandRequestComponent:628` | ✅ consumer `OVT_HighCommandPurchaseContext.OnResult:488` also 3 |
| 8 | `RpcDo_HCCreated(string, string, string, vector, int)` | 5 | `BroadcastGroupCreated` — local apply `:1456` / `Rpc()` `:1457` | `OVT_HighCommandManagerComponent:1508` | ✅ |
| 9 | `RpcDo_HCRemoved(string)` | 1 | `BroadcastGroupRemoved` — local apply `:1468` / `Rpc()` `:1469` | `OVT_HighCommandManagerComponent:1517` | ✅ |
| 10 | `RpcDo_HCOrdered(string, int, vector)` | 3 | `BroadcastGroupOrdered` — local apply `:1480` / `Rpc()` `:1481` | `OVT_HighCommandManagerComponent:1528` | ✅ |
| 11 | `RpcDo_HCStatus(string, vector, int, int)` | 4 | `BroadcastGroupStatus` — local apply `:1496` / `Rpc()` `:1497` | `OVT_HighCommandManagerComponent:1544` | ✅ **the heartbeat, its own RPC** |
| 12 | `RpcDo_RecruitQuote(string, int, int, int, string, int)` | 6 | `SendRecruitQuote` — direct `:1148` / `Rpc()` `:1152` | `OVT_RecruitCommandComponent:1168` | ✅ 5→6 in Phase 5; consumer `OVT_LoadoutsContext.OnRecruitQuote:385` also 6 |

**Properties this table is the evidence for:**

- **No `array<…>` on any RPC** — grep of every `[RplRpc]` declaration + the following two lines: 0 hits.
- **No `RplId` anywhere in the feature** — `grep -rn RplId` over the manager, the seam, the group component
  and the four `Data/` classes: 0 hits. Nothing goes through a `ScriptInvoker` but ints, strings, vectors and
  `OVT_HighCommandRecord`.
- **No identity argument on any ask** — rows 1–5 carry entry indexes and group ids only.
- **No RPC exceeds 6**, and the heartbeat (#11) is its own RPC at 4.
- **Each send helper contains exactly one `Rpc()`** at a fixed literal arity, on the line immediately after
  the direct call with the identical argument list — the shape the Q5 ruling stands on.

---

## Test-Case Mutation Ledger (Q2 — every Logic case proven able to fail once)

| Case | Mutation | Resulting failure message |
|---|---|---|
| `HighCommandRules_CapAdmitsExactFit` | `<=`→`<` in `FitsUnderCap` | exact fit (40+8=48) refused |
| `HighCommandRules_CapZeroIsUnlimited` | removed the `cap <= 0` branch | cap of 0 refused a huge purchase |
| `HighCommandRules_RemainingCapacityNeverNegative` | removed the `Math.Max(0, …)` floor | 60 members vs 48 cap reported −12 |
| `HighCommandRules_StanceValidationRejectsOutOfRange` | `>=`→`>` lower bound | DEFEND did not validate |
| `HighCommandRules_MovingIsSquaredDistance` | `DistanceSq`→`Distance` vs a squared radius | 25.1 m from a 25 m radius did not count as moving |
| `HighCommandRules_SupportersScaleWithMembers` | removed the `<=0` floor guard | a negative rate required −21 |
| `HighCommandRules_GroupIdIsStableAndUnique` | hardcoded the salt suffix | salts 1 and 2 minted the same id |
| `HighCommandRules_DestinationLegality` | return `true` unconditionally | the zero vector validated as legal |
| `HighCommandStatus_DeriveSetsEachBitIndependently` | `contact` also sets `MOVING` | contact alone derived mask 5, expected 1 |
| `HighCommandStatus_TagIconPrefersContact` | swapped the `NO_AMMO`/`CONTACT` order | contact+no-ammo showed the ammo tag |
| `ItemSourcing_CoverageExactlyMeetsNeed` | `Math.Min(needed,available) − 1` | covered 9/180 instead of 10/200 |
| `ItemSourcing_CoveragePartial` | `charged = needed` | charged 10/200 instead of 6/120 |
| `ItemSourcing_CoverageSurplus` | `covered = available`, uncapped | covered 50/1000 instead of 10/200 |
| `ItemSourcing_CoverageAcrossManyLines` | `break` after the first line | summed 10/200 instead of 15/445 |
| `ItemSourcing_CoverageIgnoresZeroPriceLine` | `continue` on `unitPrice<=0` | a zero-price line's units went unreported |
| `ItemSourcing_CoverageNeverNegative` | removed the `covered<0` clamp | produced covered −5/−100 |
| `HighCommandRules_CapRefusesWhenAlreadyOver` | `currentMembers + incoming <= cap` → `incoming <= cap` | an owner 12 over a 48 cap bought one more — **designed, not yet run** |
| `HighCommandRules_SupportersAtDefaultRate` | `memberCount * rate` → `memberCount - 1` | 7 members at rate 1 required 6 — **designed, not yet run** |
| `ItemSourcing_CoverageFeedsTheGearFee` | feed `SplitCoverage`'s RAW subtotal to `TotalPrice` instead of the charged one | stock in range did not lower the price at all (900 vs 900) — **designed, not yet run** |
| `Init_HighCommandSeam_AManagerResolves` | removed the manager block from the game-mode prefab | `GetHighCommand()` still null after 300 frames |
| `Init_HighCommandSeam_BRequestComponentResolves` | removed the request block from the controller prefab | `OVT_ControllerComponent<…>.Get()` returned null |
| `Init_HighCommandSeam_CGroupSpawnsWithObserver` (a) | drop one member prefab from the composition before spawning | group holds n−1 agents against n authored slots — **designed, not yet run** |
| `Init_HighCommandSeam_CGroupSpawnsWithObserver` (b) | skip `StampMemberFaction` | a member reports faction `CIV` against the configured `FIA` — **designed, not yet run** |
| `Init_HighCommandSeam_CGroupSpawnsWithObserver` (c) | make `InstallObserver` read the RECRUIT gate (D3's rejected option) with that gate off | still not an observer after 30 frames — **designed, not yet run** |
| `Init_HighCommandSeam_CGroupSpawnsWithObserver` (d) | make the `OnDelete` observer removal conditional on `GetHighCommandGroupsAreObservers()` and turn the gate off between add and dismiss | observer count above its pre-spawn baseline after dismissal — **designed, not yet run** |

| `HighCommandRules_VehicleIsChargedOnTopOfTheCrew` | `total += vehicleCost` → `total = total` in `PurchaseTotal` | `The same crew WITH a 27500 vehicle was priced at 900, expected 28400 - the vehicle must be added to the total at its full shop buy price, not folded into the gear fee and not dropped` — **mutation applied, COMPILE-VERIFIED (exit 0), the mutated pure function executed against the case body, then reverted** |
| `HighCommandRules_StatusReadCadenceIsAdaptive` | removed the `if (IsMoving(position, destination)) return true;` branch from `IsStatusReadDue` | `A group 400 m from its destination was NOT due a status read on sweep tick 1 - a travelling group must be read on every tick or its map marker crawls` — **mutation applied, COMPILE-VERIFIED (exit 0), the mutated pure function executed against the case body, then reverted** |

| `Init_HighCommandSeam_DHeartbeatFillsTheRecord` | delete the `CallLater(SweepStatus, STATUS_SYNC_INTERVAL_MS, true)` line from `OVT_HighCommandManagerComponent.OnPostInit` | `The status heartbeat had not run 30000 ms after the group was spawned. OVT_HighCommandManagerComponent.OnPostInit() is what installs it (CallLater at STATUS_SYNC_INTERVAL_MS), so without it every High Command marker freezes where the group was bought…` — **mutation applied and COMPILE-VERIFIED (exit 0) then reverted; not executed, see the note below** |
| `HighCommandSupply_AggregateSumsDuplicates` | `needed.Set(resource, needed.Get(resource) + 1)`/`Set(resource, 1)` branch collapsed to an unconditional `needed.Set(resource, 1)` | `Three deficient 'mag_556' magazines aggregated to 1 distinct resource(s) needing 1 - expected 1 resource needing 3` — **mutation applied, COMPILE-VERIFIED (exit 0), then reverted; not executed (agents may not run suites)** |
| `HighCommandSupply_AggregateSeparatesResources` | same mutation as above | `Five deficient magazines across two resources aggregated to 2 distinct resource(s) (mag_556=1, mag_762=1) - expected 2 resources needing 2 and 3` — **mutation applied, COMPILE-VERIFIED (exit 0), then reverted; not executed** |
| `HighCommandSupply_AggregateSkipsEmptyResource` | removed the `if (resource == "") continue;` guard in `AggregateResourceNeeds` | `Two empty resource names among two real ones aggregated to 2 distinct resource(s) needing 2, with an empty key present=true - expected exactly 1 resource ('mag_556') needing 2 and no empty key` — **mutation applied, COMPILE-VERIFIED (exit 0), then reverted; not executed** |

⚠️ The four Phase 2 rows are **designed mutations, not executed ones** — the suite gate is the orchestrator's and Phase 1's is still deferred (Workbench live). Run `OVT_TEST_InitSuite` before treating them as discharged. The Phase 6 and Phase 10 rows above are in the
same state, for the same reason: the mutation was applied to the tree and compiled clean before being reverted, but no agent
may run the suites and the gate is still deferred while Workbench is live.

---

## 🔧 PLAY-TEST RIDERS (user, 2026-08-22) — first live test of Phases 1–8

**Fixed immediately (map-side only, compile-check exit 0):**

1. ✅ **Hovered/selected markers rendered transparent.** Cause: `Friend_Focus_Land` / `Friend_Select_Land` are **hollow overlay rings**, not alternative fills — `UpdateSymbolQuad` was swapping the *base* image to them, so the filled green field was replaced by an outline. Fix: base `Friend_Land_Bcg` is now permanent and set once at creation; a new `SelectRing` `ImageWidget` (`{6B1C3D0000004006}`, Z-order 2) carries the focus/select quad and is shown/hidden. Selection remains a channel distinct from opacity and visibility.
2. ✅ **Destination line.** New `OVT_MapHighCommandLineLayer : OVT_MapCanvasLayer` (`m_iDrawOrder 250`, id `highcommandlines`), registered in `MapOverthrow.conf` `{6B1C3D0000004050}`. Dashed `LineDrawCommand` segments (12 px dash / 8 px gap, screen-space so the rhythm survives zoom), resistance green via `GetArgbForRole` (**never `Color.PackToInt`** — it bakes the palette's own alpha), dimmed to 115 for other owners to match `OTHER_OWNER_OPACITY`. Drawn only while `OVT_HighCommandRules.IsMoving(...)` — **the same arrival test the status flag uses**, so the line and the "Moving" label cannot disagree. ⚠️ It is a *canvas* layer, so `ApplyHighCommandMarkers` and the restore-on-open path both had to learn to toggle it — the HC row returns before `ApplyCanvasLayer` ever runs.
3. ✅ **Selected-group panel moved +20 px right** (`PositionX 24 → 44`).

8. ✅ **Spawn-point + parking components on all four barracks hosts** (user's suggestion, 2026-08-22) — the gunner very likely spawns *inside* the building and has to path out, because the purchase spawns at the **caller's** position (Phase 3 decision: "a building origin is inside the building; the player is within 5 m on ground they walked to"). That is fine for infantry and wrong for a vehicle crew.

   Both components already ship and are Workbench-authorable with a debug draw — **nothing new was written**:
   - `OVT_SpawnPointComponent` — `m_aPoints` array, `GetSpawnPoint()` picks a random point, applies the building's rotation and **clamps to `GetSurfaceY` + 0.5** (`:21-50`).
   - `OVT_ParkingComponent` — `m_aParkingSpots` of `OVT_ParkingPointInfo` (offset, angles, `m_Type`), and `GetParkingSpot()` already does an **obstruction check** against reserved vehicles (`:20`).

   Added to `OVT_Barracks.et` and the three vanilla building overrides with **placeholder offsets for the user to author** (3 spawn points ~14 m out, 2 `PARKING_CAR` spots beside them). GUID blocks `6B1C3D…6000/6100/6200/6300`.

   ⏳ **Still to wire (queued with the rest):** the purchase path must prefer `OVT_SpawnPointComponent.GetSpawnPoint()` for members and `OVT_ParkingComponent.GetParkingSpot()` for the vehicle, falling back to the caller's position when a host has neither.

**Fixed 2026-08-22 (component-developer-advanced, compile-check exit 0):**

4. ✅ **R4 — adaptive heartbeat.** `STATUS_SYNC_INTERVAL_MS` is now **2000** — the sweep's own tick AND the cadence a *travelling* group reports at — with `STATUS_IDLE_SWEEP_TICKS = 5` so a parked group is still only read every 10 s. The per-group decision is a pure static, `OVT_HighCommandRules.IsStatusReadDue(position, destination, tickIndex, idleEveryTicks)`, measured from the record's LAST measured position so an ordered group reads as moving on the very next tick and an arrived one drops straight back to the cheap cadence. **The parked-group silence is untouched**: the read gate is upstream of `HasStatusChanged`, which still gates every send, and a parked group's read cadence is bit-for-bit what it was. `PruneMemberRegistry()` was moved onto the idle tick so it did not become 5× more expensive. `STATUS_POSITION_THRESHOLD` is deliberately **still 5.0** — with a 2 s tick a vehicle sends every tick and infantry roughly every 4 s; lower it only if infantry still looks steppy.

5. ✅ **R5 — the send site was never wrong.** `OVT_HighCommandRequestComponent.ReplyPurchase()` has always passed `detail` (`quote.m_iSupportersRequired` at gate 8, `m_iMemberCount` at gate 7, `m_iTotal` at gate 9) and `SendTextNotification` forwards it; the HUD renders with `SetTextFormat`. The literal `%1` came from **the barracks screen**: `OVT_HighCommandPurchaseContext.OnQuote()` passed `ReasonKeyFor(refusalCode)` straight to `ShowQuoteState()`, which does `statusText.SetText(key)` — a raw key with a placeholder renders the placeholder. New `ReasonTextFor(refusalCode, memberCount, supportersNeeded, total)` translates the three parameterized keys (`HCPurchaseAtCap`, `HCPurchaseNoSupporters`, `HCPurchaseCannotAfford`) and returns the bare key for the other six. **Audit of every other parameterized HC key (13 of them) came back clean** — all already go through `SetTextFormat`/`WidgetManager.Translate`.

6. ✅ **R6 — the gunner.** What was actually found, statically: the composition is **not** the problem (`Group_FIA_SentryTeam.et:85-87` authors two `Character_FIA_Rifleman.et` slots, so both men spawn and both join). The two real facts are (a) **the turret is not on the vehicle** — `UAZ469_PKM_FIA.et`'s own compartment manager declares only `PilotCompartment` and `Passenger_r01`; the `TurretCompartmentSlot` lives on the `Weapon` slot's child prefab `UAZ469_FIA_weapon_mount_6T5_PKM.et`, so `MoveInVehicle(vehicle, TURRET)` depended entirely on vanilla's child-scan fallback; and (b) **the one-member-per-hop seating raced a lock it did not own** — `MoveInVehicle` locks the slot it hands out with `CallLater(SetCompartmentAccessible, 1ms)` while the seating hop was `CallLater(SeatNextMember, 0ms)`, and the occupancy that would otherwise separate two men is not established by the call at all (it arrives with the `MoveInVehicleOwner` RPC the call sends). Whichever call-queue entry ran first decided whether man 2 was offered the driver's seat again — and when he was, he landed nowhere. The shipped `OVT_VehicleSpawningDeploymentModule.EnsureTurretsManned()` is the standing evidence that the type-based form is unreliable on this codebase, and its explicit-slot call is the shape now used. **Fix:** `SeatNextMember`/`SeatAgent` are replaced by `SeatCrew(groupId)` + `CollectFreeSeats` + `CollectCompartments` + `AppendFreeSeatsOfType`. The seat plan is built by walking the vehicle **and its children** (`SEAT_SEARCH_DEPTH = 3`, de-duplicated by slot), ordered driver → turrets → cargo, and each man is handed an **explicit, distinct** `BaseCompartmentSlot` — so the lock, the ordering and the RPC latency stop mattering and the whole crew is seated in one pass. A man already aboard is skipped and does not consume a seat (the restore re-seat path). A `seated N of M crew into K free compartments` WARNING is the new play-test tell.

7. ✅ **R7 — the vehicle is priced.** `OVT_HighCommandManagerComponent.PriceVehicle()` uses the shipped economy path with its shipped guard — `IsRegisteredResource(res)` **before** `GetInventoryId`, then `GetBuyPrice(id, pos, playerId)` — exactly as `OVT_HighCommandManifest.BuildEntryManifest` does for gear. It lands on `OVT_HighCommandQuote.m_iVehicleCost` and is folded in by a new pure static, `OVT_HighCommandRules.PurchaseTotal(crewTotal, vehicleCost)`. **Both asks are covered by one change**: `QuoteEntry()` is the only place a HC price is made and both `RpcAsk_QuoteGroup` and `RpcAsk_PurchaseGroup` call it, so the quote shown, the funds check and `TakePlayerMoneyPersistentId` are all the same number. The vehicle is **not** fee-multiplied and **not** warehouse-covered — full shop buy price, the user's decision. An unpriceable vehicle costs 0 and prints a WARNING naming it, **once per prefab per session** (`m_aUnpriceableVehicles`) — a quote runs on every selection change, so an unconditional print would bury the log. Reference numbers: `UAZ469_PKM` is `cost 25000` in `Configs/Pricing/vehiclePrices.conf:72-76` and `GetSellPriceAtOffset` short-circuits for vehicles (`m_aAllVehicles.Contains(id)`), so the charge is the flat price plus the shop margin and the player's own multiplier. **The ladder order is unchanged and the funds check is still last.**

8. ✅ **R8 — authored spawn points.** `SpawnGroupFromEntry` / `SpawnGroup` gained one optional trailing `OVT_SpawnPointComponent spawnPoints = null`; `RpcAsk_PurchaseGroup` resolves it from the barracks entity **the gate ladder already found** (no second query) and passes it. `ResolveSpawnAnchor()` makes the authored point the group's anchor — group entity, `m_vDestination` and `m_vLastKnownPosition` all agree, so nothing reads as spuriously MOVING — members each take their own `GetSpawnPoint()`, and `AttachVehicle` prefers `GetVehicleSpawnPoint(position, angles)` behind `HasVehicleSpawnPoints()` before falling back to `OVT_WorldUtils.FindVehicleSpawnNear`. **`HasSpawnPoints()` was added to `OVT_SpawnPointComponent`** and is the gate rather than a null check on the component: `GetSpawnPoint()` falls back to the holder's own ORIGIN when nothing is authored, which for a building is a point inside it — exactly the failure this rider exists to stop. The restore walk passes null and still rebuilds at the saved position. **`OVT_ParkingComponent` is not referenced anywhere in this path.** No prefab was edited.

   ⚠️ **Two known limits of R8, for the play-test:** members pick a random point *each*, so a squad larger than the authored point count will double up on a point (author more points if it looks bad — the three barracks families carry 3 each today); and R8 does **not** interact with R6, because `MoveInVehicle` force-teleports (`GetInVehicle(..., forceTeleport: true)`), so how far a member spawns from the vehicle never affected seating.

### Phase 9 (component-developer-advanced + ui-developer, 2026-08-22)

**T9.2's shape: a NEW group, bodies moved in — and stamping the existing entity was IMPOSSIBLE, not merely worse.** There is no runtime API to add a scripted component to a live entity (`WorldEditorAPI.CreateComponent` is Workbench-only). It would have been wrong anyway: the entity already carries `OVT_InactiveRecruitGroupComponent`, whose `OnDelete` removes the observer **unconditionally** — two observer-owning components on one entity is exactly the double-install/orphan hazard the acceptance criterion names. Moving the bodies makes the deciding constraint *disappear* rather than have to be managed: the emptied recruit group self-deletes a frame later and takes its own hold waypoints and observer with it, **none of which the HC component ever owned**.

**The ordering, quoted** (`OVT_HighCommandManagerComponent.c:2300-2306`): `recruit.m_sBodyPersistenceId = "";` then `recruits.RemoveRecruit(...)`. Clear, then drop — the `OnCharacterKilled` precedent, for the same reason: a dropped record still naming the body could ask persistence to spawn that character a second time. Records are dropped **only** for bodies that actually arrived.

**A reserved entry key.** `RestoreGroup` drops any record whose `m_sEntryKey` the catalog does not publish, so a converted group names `hc_converted` and `GetEntryByKey` answers it **last** (an authored entry always wins) with a synthesized entry — title, `Infantry_Friend` icon, **no prefabs**. It cannot be purchased: the buy list enumerates the faction array **by index**, which this entry is not in.

**UI:** one shared Convert button keyed to the current selection, not one per cluster — `SCR_InputButtonComponent`'s action listener is **global and ignores focus**, so two live instances bound to one action both fire on a single press. Matches the shipped `ToggleActiveButton` / Phase 8 roster shape.
⚠️ **The client's split is an approximation** — it clusters by 50 m of *record* position while the server converts by *actual group membership*. They normally agree. The confirm dialog therefore says "this group" rather than promising a count.
The button stays **enabled** at the HC cap (it cannot be checked client-side; `CONVERT_AT_CAP` is authoritative) and is **disabled with a reason** for an active or bodiless selection — never hidden (BUG-102).

---

### Phase 10 (component-developer, 2026-08-22)

**T10.1 — rearm.** Eligibility and mutation are split the way `OVT_VehicleRearmUtils` splits them, applied per character instead of per vehicle: `CollectDeficientMagazines` (read-only) walks every member's weapon slots, takes the **current muzzle's** loaded magazine (the `CanFireOrReload`/`AnyAmmoMissing` precedent — a weapon with no magazine loaded at all is not counted, because like `OVT_VehicleRearmUtils.PerformRearm` this tops up magazines that exist rather than conjuring a new one into an empty weapon), and reports every one below its own max ammo alongside `BaseMuzzleComponent.GetDefaultMagazineOrProjectileName()` as the warehouse resource. The new pure `OVT_HighCommandRules.AggregateResourceNeeds` sums that list into a per-resource count. `RearmGroup` then runs `OVT_WarehouseStockUtils.CollectStores` + `TakeUpTo` per resource and spends the budget one magazine at a time via `BaseMagazineComponent.SetAmmoCount(GetMaxAmmoCount())`, in the order the deficiency was found.

🔴 **Deviation, disclosed:** the `NO_AMMO` bit is **not** cleared inline in `RearmGroup`. `BroadcastGroupStatus`'s docblock (Phase 6) states it has **exactly two callers** and that invariant is what the Phase 6 "how the parked-group silence was guaranteed" proof rests on; adding a third call site here would make that proof wrong without anyone re-checking it. `SweepStatus` already re-measures every live group's ammo from scratch on its own cadence — at worst `STATUS_IDLE_SWEEP_TICKS x STATUS_SYNC_INTERVAL_MS` (10 s) for a parked group — and is what actually clears the bit and feeds the map badge. Since `REARM_INTERVAL_MS` defaults to 60 s, the badge is stale for at most a few seconds after a successful rearm, never indefinitely. This is a timing gap, not a correctness gap, and it is the safer reading of "the same pass": the same *tick cadence*, not a duplicated write to the same field from two places.

**T10.2 — refuel.** `RefuelGroup` is `OVT_FuelRequestComponent.RpcAsk_FillFuel`'s exact call shape, minus the client-supplied tank id (an HC vehicle only ever has one manager on its own entity — `GetOwnFuelManager`, the "one fill touches one manager" scoping rule, A2): `GetRefuelableCapacity` → `FindBestFillSource` → `GetStationFuelManager`/`GetProvidableFuel` → `GetFuelCostPerLitre` → `economy.GetPlayerMoney` → `OVT_FuelPricing.ComputeFillPlan`. `AddFuelToManager`/`DrainFuelFromManager` are **duplicated**, not shared, from `OVT_FuelRequestComponent` — that method's own header says it is not exposed for reuse, and I6 forbids touching the file it lives on to change that. Both are simple node walks with no pricing in them, so duplicating them does not create a second pricing implementation.

**Charged for what ARRIVED, never for what was planned** — the `OVT_FuelRequestComponent` rule, restated here because it is the reason `m_FuelChargeLedger.Accrue` is called with `delivered` (the actual return of `AddFuelToManager`) and not `plan.m_fLitres`. The two agree in every case the node arithmetic can produce (no await between measuring and transferring), but a target whose nodes moved between the capacity read and the transfer must not be billed for the difference.

**The ledger is keyed by GROUP id, not by owner persistent id.** Reusing the owner's key would let two groups the same player owns share one fractional-dollar pot — a technical topping up 0.3 L every tick and a truck topping up 0.4 L every tick would round to a whole dollar together, at neither vehicle's true rate. `m_FuelChargeLedger` lives on the manager (one instance for the whole table, `[NonSerialized()]`, matching every other server-only collection on this class) and is cleared in `RemoveRecord` — the single choke point dismissal, a wipe (`OnGroupEntityDeleted`) and a save record that could not be rebuilt all already pass through, so no new removal path was needed to reach it.

**Both ticks are `Replication.IsServer()`-gated at the top** (`RearmTick`/`RefuelTick`/`RearmGroup`/`RefuelGroup` all open with it, belt-and-braces since only the tick functions are ever called), started **unconditionally** in `OnPostInit` alongside `SweepStatus` (the recruit-sweep rule: a timer that starts and then refuses costs one branch per interval; a timer that never started because a guard read false too early costs the whole feature with no symptom), and skip a group with no live entity (`GetGroupEntity` null — unspawned, still queued for restore, or gone) or zero agents (`GetAgentsCount() <= 0` — a dead group vanilla has not yet cleaned up).

**No hand-rolled pricing anywhere in this phase** — `RefuelGroup` calls `OVT_FuelUtils.GetFuelCostPerLitre` and `OVT_FuelPricing.ComputeFillPlan` and does no `litres * price` arithmetic of its own; a free source (`OVT_FuelSourceComponent.IsFree()`, read through `IsFreeFuelSource`/`GetFuelCostPerLitre`) resolves `price = 0`, `ComputeFillPlan` returns the full affordable litres at `m_iCost = 0` without ever consulting the balance, and `Accrue` on a price of 0 returns 0 immediately (`OVT_FuelPricing.ComputeCost` short-circuits at `cost <= 0`) — so a free source costs nothing through the same call path as a paid one, with no free/paid branch needed in HC's own code.

**No new Logic case for the fuel-charge accrual itself.** `OVT_FuelChargeLedger.Accrue` and `OVT_FuelPricing.ComputeFillPlan` are already Logic-tested (`OVT_TEST_Logic_Fuel_ChargeLedger`, `..._FillPlan`) and are I6-protected — a case proving them "able to fail" would have to mutate code this feature may not touch. HC's only new contribution to that path is the group-id key and the call site, both of which are `Replication.IsServer()`-gated and world-touching (a live vehicle, a live fuel station), so they are Init/manual-test territory, not Logic. The genuinely new pure logic this phase ships is `OVT_HighCommandRules.AggregateResourceNeeds`, Logic-tested below.

**I5/I6 verified:** `git diff --exit-code` on `OVT_StorageComponent.c`, `OVT_StorageLedger.c`, `OVT_FuelUtils.c`, `OVT_FuelPricing.c`, `OVT_FuelChargeLedger.c`, `OVT_FuelSourceComponent.c`, `OVT_FuelRequestComponent.c` — all clean. `OVT_StorageUtils.c` still carries the pre-existing unrelated diff flagged in Phase 5/6 — left exactly as it is, nothing added.

No `.st` touched (2302/2302 braces, unchanged from baseline — this phase adds no player-facing string). No new `.et`/`.conf`, no GUID minted, no new RPC, no arity change, `CONFIG_STREAM_VERSION` still 6.

**What Phase 11/12 must know:**
- `RearmTick`/`RefuelTick` walk `m_mGroups` with the same id-snapshot-first pattern as `SweepStatus`, so removing a group mid-tick (garrison retirement touches none of this, but a future dismiss-during-tick race would not) is already safe.
- `m_FuelChargeLedger` has exactly one clear site (`RemoveRecord`). If Phase 11/12 add a second way for a group's record to disappear, route it through `RemoveRecord` rather than adding a second clear call.
- The `NO_AMMO`-clears-on-next-heartbeat deviation means Play-test step 23 ("the badge clears and the men have ammo") may show a few seconds of lag between the ammo actually landing and the badge updating — expected, not a bug, unless it visibly exceeds `STATUS_IDLE_SWEEP_TICKS x STATUS_SYNC_INTERVAL_MS`.

### Phase 10 (component-developer, 2026-08-22)

- **Refuel reuses the player-facing fill's exact call shape** — `GetRefuelableCapacity` → `FindBestFillSource` → `GetFuelCostPerLitre` → `ComputeFillPlan`. Orchestrator grep for a hand-rolled `litres × price` anywhere in the manager: **none**.
- **A free source needs no special case** — `GetFuelCostPerLitre` resolves 0 for `IsFree()` and `Accrue` short-circuits at `cost <= 0`.
- **The owner is charged for fuel that ARRIVED, not fuel that was planned.**
- **`OVT_FuelChargeLedger` has exactly ONE clear site** — `RemoveRecord` (`:654`), the choke point every removal route already passes through (dismiss and wipe both). Any future removal path must route through it rather than add a second clear.
- ⚠️ **Disclosed deviation: `NO_AMMO` is NOT cleared inline by the rearm pass.** The plan asked for it, but `BroadcastGroupStatus` has a documented "exactly two callers" invariant that is **load-bearing for Phase 6's parked-group-silence proof**, and a third call site would have weakened it. The bit clears on the next `SweepStatus` read instead — ≤10 s for a parked group, well inside the 60 s rearm interval. **Accepted:** the invariant is worth more than ≤10 s of badge lag. Expected behaviour, not a bug, unless the lag visibly exceeds `STATUS_IDLE_SWEEP_TICKS × STATUS_SYNC_INTERVAL_MS`.
- Both ticks snapshot `m_mGroups` ids first, the `SweepStatus` pattern.

---

### Phase 11 (component-developer-advanced, 2026-08-22) — garrison retirement

**T11.6 — the three enum members were REMOVED, not reserved. The evidence, gathered before deleting:**

| Question | Command | Answer |
|---|---|---|
| Does any serializer name the enum or an origin field? | `grep -rn "OVT_EGroupOrigin\|m_iOriginType\|originType" Scripts/Game/Persistence/ Scripts/Game/Modded/` | **0 hits** |
| Is it authored anywhere data outlives a build? | `grep -rn "EGroupOrigin" Configs/ Prefabs/ Worlds/` | **0 hits** |
| Does an `RplProp` carry it? | `grep -n "RplProp" OVT_GMGroupRegistry.c OVT_GMRecords.c` | **0 hits** |
| Who reads `m_iType`? | `grep -rn "m_iType"` | 4 sites, all GM-side, all same-process or same-build |

The ordinal **does** cross the wire — `OVT_GMSnapshotBuilder` copies it onto `OVT_GMGroupRecord.m_iOriginType` and
`OVT_GMRequestComponent.RpcDo_Group` sends it as an `int` — but only between a server and clients running the same
addon build, which is what the enum's own header already says ("the enum is not persisted and not replicated, so
reordering costs nothing today"). Reserving three dead values to protect a wire both ends of which are compiled from
the same file would have been cargo cult. **`RADIO_TOWER_GARRISON` is NOT one of the three** — it is the occupying
faction's tower garrison, it is out of T11.6's scope, and it stays (it has had no `Tag()` call site since the
base-defense migration, which is a separate pre-existing dead value, not this phase's).

**T11.7 — neither serializer deletes a field, and the write side is the shipped precedent.**

`OVT_PersistedBase.upgrades` was already being *written empty and never removed* after the base-defense migration,
for exactly this reason, and its comment says so at `OVT_OccupyingFactionManagerSerializer.c:260`. `garrison` gets
the identical treatment in all three record classes:

| Class | Field | v1/v2 | New version |
|---|---|---|---|
| `OVT_PersistedCamp` | `garrison` | written from live groups | **declared, written empty** |
| `OVT_PersistedFOB` | `garrison` | written from live groups | **declared, written empty** |
| `OVT_PersistedBase` | `garrison` | written from live groups | **declared, written empty** |

So the on-disk record shape is **byte-identical** before and after: an old payload's list is read into the record and
nothing consumes it; a new payload's list is read back as the empty array it was written as. Dropping the field would
have changed each record's length, and because the records are read as one array, every record after the first would
have been parsed out of its neighbour's bytes. `WriteGarrison` (resistance) and the `base.garrisonEntities` walk (OF)
are gone, and each file's now-unreferenced `GetPrefabResourceName` went with them.

🔴 **Deviation, disclosed: every `Read()` return in both `Deserialize` bodies is now checked, which they were not
before.** §3.11's mandatory rule and the vanilla idiom (`SCR_ArsenalComponentSerializer.Deserialize` gates every
apply on `if (context.Read(x))`) both require it, and the shipped `OVT_HighCommandManagerSerializer` already does it.
On a failure each now logs an ERROR naming the field and returns **without applying anything**, because a positional
stream that has lost one field is reading another field's bytes for every field after it — and half-read camps at
junk positions would be written straight back out by the next save. This is a behaviour change on the load path:
before, a partial read applied whatever it had. **It cannot make a well-formed save load worse** — the shape read is
identical to the shape written, at v1 and at the new version alike — but it has not been exercised against a real
save file, and that is the one claim in this phase that only the Persistence round-trip suite and a real Continue can
settle.

**What a save does on load, stated precisely:**

- **Written before this phase (v1 resistance / v1–2 OF).** Version gate passes (`version < 1` is still the only
  rejection). Camps, FOBs, bases, towers, resources and threat all come back. Each record's `garrison` list is read
  into the record and then read by nobody: `ApplyPersistedGarrison`, `ApplyPersistedBaseGarrison`, `SpawnGarrisons`
  and the `InitBaseControllers` `else` branch that replayed it are all deleted. **No error, no garrison.**
- **Written after this phase (v2 / v3).** Identical shape with empty garrison arrays. Loads identically.
- **Not proven from code alone:** that the engine's `Read()` returns true for these payloads in practice. Everything
  above follows from the shape being unchanged; it is not a measurement.

**T11.2 went one item past the brief.** `OVT_OccupyingFactionManager.ApplyPersistedBaseGarrison()` and its call site
in `ApplyPersistedOccupyingFaction()` are not in §4's list, but they dereference `base.garrison`, which T11.2 deletes.
Also removed: the two locals at the top of `InitBaseControllers` (`rf`, `resistance`), both of which were **already
dead** before this phase — the deleted block called `OVT_Global.GetResistanceFaction()` directly.

⚠️ **T11.4 leaves two content-free menus, and that is what the plan asked for.** `OVT_BaseMenuContext` and
`OVT_FOBMenuContext` were 100 % garrison; with the garrison half gone each is a title and a Close button, still
opened by `OVT_ManageBaseAction` from a base flag or a FOB. §4 scopes T11.4 to "the garrison halves" and "both
layouts' garrison rows", and play-test step 30 reads "the base and FOB menus have no garrison controls" — so the
shells were intended. They were **not** deleted because the script classes are referenced by GUID from
`Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et:75` and `:109`; removing them means a prefab edit plus
removing `OVT_ManageBaseAction`, which is a scope and a Workbench check this phase does not own. **Recommend a
follow-up** that either deletes the action + both contexts + both layouts + the two prefab blocks, or gives the base
menu real content. Note that **camps are unaffected** — `OVT_ManageCampAction` opens `OVT_CampMenuContext` (privacy,
delete), a different and still-useful screen; `OVT_FOBMenuContext.m_Camp` was already set by nobody before this phase.

**Also cleaned, because they made a factual claim about deleted code** (none are in §4's list): the
`ChargeForGarrison()` citations in `OVT_ResistanceFactionManager.RepairStructure`'s docblock and in
`OVT_BaseRepairBehaviorDeploymentModule.c:187`; `OVT_GMGroupRegistry`'s "camp/FOB removal never touches
garrisonEntities" and its "two client-local throwaway preview groups" note (the preview spawn was the thing T11.4
deleted); `OVT_BaseControllerComponent`'s "read … by the resistance's FOB garrison path"; and the garrison
exclusions in four test-suite headers plus two seam-test failure messages that named `add-garrison`.

**Left alone, deliberately:** ~570 remaining case-insensitive `garrison` hits under `Scripts/ UI/ Configs/`, every
one of them the **occupying faction's** garrison domain (`Deployment_BaseGarrisonPatrol.conf`,
`Deployment_TowerGarrison.conf`, `Deployment_ObjectiveFOBGarrison.conf`,
`OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG`, `OVT_EObjectiveConcurrencyLimit.FORWARD_BASE_GARRISON`,
`objectiveFOBGarrisonMax` on all five difficulty presets) or ordinary English prose about a squad standing somewhere.
§4's acceptance line ("returns only the serializers' read-and-discard branches") is unachievable as literally
written — "garrison" is load-bearing vocabulary for a system this phase does not touch.

⚠️ **`OVT_BaseControllerComponent.m_AllCloseSlots` now has no reader at all** — `AddPatrolWaypoints` was its only
one. Left in place: it is populated by the base init world query and is part of a documented slot registry, and
removing it is an `occupying` change, not a `resistance/high-command` one.

**Verified this phase:** compile-check exit 0, and the gate proven able to fail (a deliberate syntax error in
`OVT_BaseMenuContext.c` produced `:13: error: Broken expression`, exit 1; reverted, green again). `.st` braces
**2302 → 2298**, balanced, two whole `CustomStringTableItem` blocks removed (each carries two `{` and two `}`,
counting the GUID's). No `Language/*.conf` export touched. `CONFIG_STREAM_VERSION` still 6. No ternary, no
`maxAttempts`, no `array<bool>`, no local named `owned` introduced. Neither JIP payload ever carried a garrison
(`RplSave`/`RplLoad` on both managers write persistentId/name/location/owner/flags only), so replication is
unchanged. Serialize/Deserialize locals still spelled identically in both files — resistance
`playerFactionKey` / `camps` / `fobs`, occupying `occupyingFactionKey` / `resources` / `threat` / `bases` /
`towers`. **No Persistence-tier case added** — Phase 12's T12.3 owns the round-trip cases and a garrison case would
have to assert on a field this phase just made unreadable from the public API.

### Phase 11 (component-developer-advanced, 2026-08-22)

**The record shape is byte-identical before and after — that is the whole safety argument.** `garrison` is **declared, still read, written empty** in all three persisted record classes, exactly as `OVT_PersistedBase.upgrades` already was after the base-defense migration. Dropping the field would change each record's length and every record after the first would parse out of its neighbour's bytes.

**Acceptance line corrected by the agent, and it was right.** §4 asks that `grep -rin "garrison"` return *only* the serializers' branches. **That is unachievable** — "garrison" is load-bearing vocabulary for the **occupying** faction's deployment/objective system, which this phase deliberately does not touch (`objectiveFOBGarrisonMax`, `Deployment_TowerGarrison.conf`, `FORWARD_BASE_GARRISON`, etc.). 778 → 569 hits, of which **16** are the serializers and the rest are occupying-side identifiers or prose. The meaningful grep is the **dangling-caller** one, which the orchestrator re-ran independently: **zero hits, zero stubs.**

**T11.6 — removed, not reserved.** Evidence: `OVT_EGroupOrigin` has **0 hits** under `Scripts/Game/Persistence/` and `Scripts/Game/Modded/`, **0** in `Configs/`/`Prefabs/`/`Worlds/`, and no `RplProp` carries it. The ordinal does cross the wire (`OVT_GMSnapshotBuilder.c:322`) but only between a server and clients built from the same addon — which the enum's own header already states. `RADIO_TOWER_GARRISON` is **not** one of the three and stays.

**⚖️ Orchestrator ruling — checking every `Read()` return STANDS.** The agent added return checks to both `Deserialize` bodies, which previously had none, and flagged it as a load-path behaviour change needing a ruling. **Approved:** §3.11 mandates it ("every `Read()` return is checked"), the shipped `OVT_HighCommandManagerSerializer` and vanilla `SCR_ArsenalComponentSerializer` both do it, and the failure direction is strictly safer — a positional stream that loses one field reads garbage for every field after it, and half-read camps at junk positions would be written straight back out by the next save. Aborting without applying beats half-applying. **Gate evidence: `OVT_TEST_PersistenceRoundTripSuite` 40/40 and `OVT_TEST_PersistenceSuite` 13/13, both exit 0.**

✅ **RESOLVED (user, 2026-08-22): both contexts REMOVED entirely.** Deleted `OVT_BaseMenuContext.c`, `OVT_FOBMenuContext.c`, `BaseMenu.layout`(+meta), `FOBMenu.layout`(+meta), `OVT_ManageBaseAction.c`, its four prefab declarations (`OVT_BaseController.et`, `BaseFlag.et`, `BaseFlag_FIA.et`, `FOB_V1_FIA.et`) and both context registrations on `Character_Player.et`. The action did nothing but open those two menus, so it went with them. A stale comment in `OVT_FOBRequestComponent.c:54` that cited it was corrected. Compile-check exit 0, zero dangling references. Camps unaffected. *(Original write-up below, for the record.)*

🔴 **WAS OPEN — a product decision, NOT a defect:** `OVT_BaseMenuContext` and `OVT_FOBMenuContext` were **100% garrison** and are now a title and a Close button. `OVT_ManageBaseAction` still opens them from a base flag / FOB, so a player presses the action and gets an empty screen. They were **not deleted** because both classes are referenced by GUID from `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et:75` and `:109`. §4 scoped this phase to "the garrison halves", and play-test step 30 only asks that the menus have no garrison controls — so the shells are as specified, but the result is a dead-end action. **Camps are unaffected** (`OVT_ManageCampAction` opens the separate, still-useful `OVT_CampMenuContext`). Recommend a follow-up: either give those menus real content or remove the actions.

⚠️ `OVT_BaseControllerComponent.m_AllCloseSlots` now has **no reader** (`AddPatrolWaypoints` was its only one). Left in place deliberately — it is filled by the base init world query and is part of a documented slot registry; removing it is an `occupying` change, not this feature's.

⚠️ `.st` 2302 → **2298**, balanced. `#OVT-Garrison` and `#OVT-AddToGarrison` removed. **`#OVT_Group`, `#OVT-SoldierCost`, `#OVT-EquipmentCost`, `#OVT-Total` are now orphaned** — Phase 13's `.st` audit owns them.

---

## ✅ T12.4 — CROSS-PHASE REVIEW (orchestrator, main thread, 2026-08-22)

**Every §6 static gate run and passed:**

| Gate | Result |
|---|---|
| I1–I4 (`RealEstate`, `QRF`, `ObjectivePhaseRules`, `RecruitPurchaseRules`) | ✅ `git diff --exit-code` clean |
| I5/I6 (`logistics/storage`, `economy/fuel`) | ✅ clean |
| Q8 — `Configs/Language/` untouched | ✅ |
| Q6 — no `SetLifecyclePolicy` / `Cleanup*` | ✅ (2 hits, both "never do this" comments) |
| Logic-tier purity — no `OVT_Global`/`GetGameMode` under `TestSuites/Logic/` | ✅ zero hits |
| Q11 — `CONFIG_STREAM_VERSION` | ✅ still 6 |
| Q5 — no `array<>`/`RplId` on any RPC | ✅ |
| Q4 — RPC count vs the audit table | ✅ 11 `[RplRpc]` here (7 request + 4 manager) + `RpcDo_RecruitQuote` = **exactly 12** |
| Serializer bumps | ✅ versions 2 and 3, each with a `version <` read branch |
| Q10 — GUID series | ✅ 377 distinct `6B1C3D…` in Overthrow, **0** in the vanilla tree |
| F20 — garrison paths gone | ✅ zero `AddGarrison`/`SpawnGarrison`/`garrisonCount`/`ChargeForGarrison` |
| Every `.et` this feature touched | ✅ brace-balanced |
| No `maxAttempts` | ✅ every hit is prose asserting its absence — the project convention |

**The substantive cross-phase question — does a CONVERTED group survive the paths built by other phases?** A converted group carries the reserved `hc_converted` key and a *synthesized* entry with **no prefabs**, which did not exist when Phases 3, 6, 7, 8 and 10 were written. Traced every consumer:

- `OVT_MapHighCommandLayer` (`:1277`), `OVT_HighCommandRosterContext` (`:427`) and `RestoreGroup` (`:2631`) **all** resolve through `GetEntryByKey`, which answers the converted key with the synthesized entry — so a converted group renders, lists **and restores**.
- The purchase asks resolve by **`GetEntryByIndex`** into the authored array, which the synthesized entry is not in — so it is **structurally unbuyable**, not merely guarded.
- The rearm/refuel ticks read the record's members and vehicle, never the entry — unaffected.

**The seam holds in both directions.** No defect found here; the one real defect this review caught was the orchestrator's own prefab damage, below.

**Pre-existing, not introduced:** `Configs/UI/Dialogs/DialogPresets_Campaign.conf` has no `.meta` and never did (absent from HEAD too).

---

## 🔴 INCIDENT #2 — the orchestrator broke four prefabs with an off-by-one delete (2026-08-22, caught by the Phase 12 gate)

Removing the `OVT_ManageBaseAction` block from four prefabs, the orchestrator deleted **5 lines** from a **6-line** block, leaving the block's closing `}` orphaned in every file. Each prefab went one brace unbalanced, and the stray brace closed the wrong scope — swallowing the components declared after it.

**Not caught by compile-check** (prefabs do not affect script compilation) and **not caught by the input checker**. It was caught by `OVT_TEST_InitSuite`, three cases at once, all naming `OVT_BaseController.et`:
- `GMIcons_BaseControllerIsEditable` — *"reports type GENERIC, expected SYSTEM"*
- `GMIcons_InfoHasNameAndDescription` — *"has no UI info at all"*
- `GMIcons_LocalFlagMatchesReplication` — *"is NOT flagged LOCAL and has no RplComponent"*

Three different symptoms, one cause: the components after the stray brace were no longer parsed as part of the entity.

**Fixed** by removing the single orphan `}` from each of the four files; every diff is now exactly the intended 6-line block and every file's brace count balances. **Init 209/209 green afterwards.**

⚠️ **THE RULE, same root cause as incident #1: never delete a structured block by line count.** Both `.st` entries and `.et` component blocks must be removed by **matching braces**, not by assuming a fixed length. Verify with a brace-balance count before and after — for `.et` files, compare against `git show HEAD:<file>`.

**This is the cross-phase review earning its place for the fourth feature running** — the defect was invisible to every automated gate except the suite that actually loads the prefab.

---

## 🔴 INCIDENT — the orchestrator destroyed 969 `.st` entries and rebuilt them (2026-08-22)

**Cause.** Removing four orphaned keys, the orchestrator matched each `CustomStringTableItem` with a regex of the shape `CustomStringTableItem \{ ... Id "<key>" ... \}`. The leading `CustomStringTableItem` anchored to a **much earlier block**, so each match swallowed everything from there to the target. Two keys → two large spans deleted: **1149 → 169 entries, braces 2298 → 340.**

**⚠️ THE RULE: never regex a multi-line block in a `.st`. Parse by brace depth.** The file is one `StringTable { Items { … } }` with flat `CustomStringTableItem` children; walk the children counting `{` and `}`. A regex cannot know where a block ends.

**Recovery, with no git write** (`git show HEAD:…` is a read):
1. Rebuilt from the **committed** file with a brace-depth parser — 1049 blocks.
2. Dropped the 4 keys that genuinely should go: `OVT-Garrison`, `OVT-AddToGarrison`, `OVT-ManageBase`, `OVT_Group`.
3. Re-attached the **89 session-added blocks that survived**, extracted verbatim from the damaged file.
4. Re-authored the **11 genuinely lost** session keys — `OVT-Recruit_QuoteCovered` (Phase 5) and the ten `OVT-Recruits_Convert*` / `OVT-Recruits_GroupHeader` (Phase 9 UI) — wording derived from the call sites that render them, GUIDs `6B1C3D…7100`–`…710A`.

**Verified after recovery:** 1145 entries (= 1149 − 4, exact); braces **2294/2294**; every committed key present bar the 4 intended; **no duplicate Ids**; the 4 duplicate GUIDs are **pre-existing** (identical counts in the committed file); compile-check exit 0. The 15 still-unresolved `#OVT-` references are string-concatenation fragments and test fixtures that were **already** absent from the committed file.

⚠️ **The file was rewritten wholesale — a Workbench open is owed before trusting it**, on top of the re-export already owed.

### Phase 13 (main thread + help-docs-sync, 2026-08-22)

**T13.1 `.st` audit and T13.2 input check — verified by the orchestrator, both clean.** Every High Command key has a filled `Comment`; no HC key is referenced-but-absent; 1166 entries, **all Ids unique**, braces **2336/2336**. The 15 remaining unresolved `#OVT-` references are string-concatenation fragments and test fixtures **already absent before this feature began**. Input checker at the shipped baseline **with the committed script**, exit 0.

**T13.3 — tutorials and Field Manual done, with a full `file:line` citation ledger for every claim** (in the agent's report). A new **High Command** Field Manual entry (16 pieces) plus a *Handing Recruits to High Command* section on the Recruits page; a `highcommand-first-open` tutorial popup registered on the game-mode prefab (**a tutorial is invisible unless listed there**). Three existing paragraphs were de-garrisoned rather than left lying: `FieldManual_BaseCapture_Text4`, `FieldManual_FOBs_Text3`, `FieldManual_Recruits_Text9`.

**Deliberately NOT claimed in player text** (the anti-invention discipline — two tips have previously shipped inventing mechanics): exact per-group member counts, the literal `48` cap (server-configurable, so the text says "a cap"), rearm/refuel intervals (configurable), and keybinds (rebindable, and the buttons self-label).

🔴 **WIKI NOT DONE — no `wikijs` MCP server is attached to this session.** Not faked, not skipped silently. Pages owed: resistance, base/base-capture, FOB/camp (garrison removal), map (the HC layer + ordering), recruits (conversion + tent coverage), and a new `high-command` page. ⚠️ Known wikijs hazards for whoever does it: **search returns wrong pageIds — verify by content**; update needs `tags`; a failed update can report success while leaving the render stale.

⚠️ **Stale translations, deliberately not touched:** the German and Ukrainian targets on `FieldManual_BaseCapture_Text4` and `FieldManual_FOBs_Text3` still describe buying a garrison. Deleting translator work was judged not the agent's call — those two paragraphs are **wrong in de/uk** until re-translated.

⚠️ **Framework gap, not hacked around:** `OVT_TutorialEvent` has **no** High Command event (no "group bought", no "group converted"), so the popup is bound to `MENU_OPENED` on the purchase screen. **A player who only ever acquires groups by converting recruits will not see it** until they open a barracks screen. Adding the event belongs to the tutorial system, not here.

ℹ️ `OVT-GMIcon_Tooltip_Base` still says "Garrison Groups" — it counts **occupying-side** groups in the GM tool, not the retired resistance garrison, so it was correctly left alone. Occupying-faction garrison wording generally (radio towers, base defences, difficulty text) is untouched and still accurate.

---

## 🔀 MERGE NOTE — `Scripts/Game/Modded/SCR_AIGroup.c` will conflict with main

**The recruit-untracking defect Phase 9 found is being fixed by the user on `main`** (2026-08-22 — it had already been reported by a player the same day, independently). **Not fixed here, deliberately.**

The defect: `PlaceRecruitInInactiveGroup` → `AddRecruitAgentToGroup` → `AddAIEntityToGroup`, whose `UntrackTransient` is unconditional and carries only the **HC** exclusion this feature added in T6.7 — the *recruit* exclusion lives in `OnAgentAdded`, the other hook. So a parked recruit loses its persistence record and its gear cannot survive a save. The class header at `:20-22` still asserts recruits "never" reach that method, which stopped being true when `recruit-ux` shipped inactive recruits.

✅ **DONE — merged 2026-08-22 as `d99bb57a`**, at the user's explicit instruction and after they committed the feature as `cb6c9945`. (This workflow performs no git write on its own; this was a direct request.)

**How the one conflict was resolved — main's shape won, as predicted.** `main`'s fix refactors *both* untrack hooks onto a shared `EntityMustStayTracked()` (player bodies + recruit bodies); v1.5 had added a **separate** HC exclusion to each hook. Resolution: **adopt main's structure and fold the HC check into the predicate**, so all three protected categories are named in one place and `OnAgentAdded`'s now-duplicate check is deleted. Also **kept v1.5's `NotifyMemberSpawned` call**, which main does not have — it is the virtualization member→slot reverse map and would have been silently lost by taking main's side wholesale. Class header updated from "those two" to "those three".

**Gates after the merge:** compile-check 0 · **Logic 282 · Init 209 · PersistenceRoundTrip 43**.

*(Original pre-merge analysis kept below, for the record.)*

**~~RESOLVED — the user merges `main` themselves~~ (superseded).** No git write is performed by this workflow. Blocker that forced the decision: `main`'s fix (`6d79dd1e`) touches `Scripts/Game/Modded/SCR_AIGroup.c`, which is **locally modified and uncommitted** by T6.7, and the tree carries ~102 changed files — `git merge` refuses over uncommitted local changes.

**Main's fix is the better shape and should win the merge.** It refactors *both* hooks onto a shared `EntityMustStayTracked(entity)` (player bodies + recruit bodies) instead of two parallel exclusion blocks. **The resolution is therefore ONE line, not "keep both sides": add the HC-member check into `EntityMustStayTracked` alongside the other two.** Adopt main's structure and fold HC into it.

⚠️ **Both fixes edit the same few lines of `AddAIEntityToGroup`.** When main merges into v1.5, expect a conflict there. **The resolution is to keep BOTH exclusions** — HC members *and* recruits — not to take one side. High Command's conversion path is immune either way (`RegisterMemberBody` re-tracks), but plain HC members still need their exclusion.

---

## Needs Human Verification

**Blocking-ish (do these first):**

1. 📤 **Workbench localization re-export.** ~130 new/changed `.st` keys across Phases 3, 4, 5, 7, 8, 9 and 13 render as **raw keys** until this runs. The `.st` is verified balanced (2336/2336, 1166 unique Ids) but it was **rebuilt wholesale after an incident** — open it in Workbench and sanity-check before trusting it.
2. 🔧 **Position the radio** in all four barracks hosts + the furniture overrides. Current `coords` are blind guesses; `0.045` was a desk-on-floor height and a radio meant to sit on furniture needs a real surface height.
3. 🎮 **The single most important play-test check:** with a group selected on the map, press **`X`** on a gamepad. The vanilla radial contextual menu **must not open**. If it opens *and* orders, the one-line fix is moving `OverthrowHCOrder`'s pad source to `gamepad0:thumb_left` — the only pad input confirmed free on that screen.
4. 🌐 **The wiki pass** (Phase 13 T13.3) — no `wikijs` MCP server was attached. See the Phase 13 section for the page list and the known wikijs hazards.

**Play-test checklists:** `implementation.md` §6 carries the full A (single-player), B (gamepad-only) and C (dedicated-server + JIP) lists, 45 numbered steps. **None of C has ever been exercised** — and `ScriptBitWriter` hard-crashes from script, so the JIP payload can never be unit-tested. MP is the only proof `RplSave`/`RplLoad` are correct.

**Known gaps, all deliberate:**

- 🎨 No art: the Barracks build card has no `m_tPreview`; the HC map badges borrow shipped quads (two constants in `OVT_MapHighCommandLayer` swap over when atlas art exists); the Field Manual tile and tutorial pages have no image.
- 🌍 German/Ukrainian translations of two Field Manual paragraphs still describe garrisons.
- 🔔 A player who only ever converts recruits won't see the tutorial popup (no HC tutorial event exists).
- ⏱️ The ammo badge clears on the next status read (≤10 s), not inline — a deliberate trade to protect the parked-group-silence invariant.
- 💰 **Vehicle pricing is at full shop price by the user's instruction** — a technical lands ~27–28k against a ~900 crew. Tunable in one place (`PurchaseTotal`).
- 🚧 Plain `Barracks_E_02.et` has no furniture prefab, so it gets no radio.

_(running list — play-test and Workbench items)_

- **Phase 6, play-test only (MP):** the whole of §6's play-test C, steps 38–45. The suites run one machine and
  cannot reach JIP, a second client, or an owner disconnecting. Step 42 (a third client joining late) is the
  only proof `RplSave`/`RplLoad` are correct at all — `ScriptBitWriter` cannot be exercised from a test.
- **Phase 6, play-test only:** step 29 (save → quit → Continue). Watch for a squad that comes back in the
  right place with the wrong gear — that is every stored body failing and the prefab stand-ins keeping their
  slots, which is the designed-safe outcome but should not be the normal one. A
  `No answer to High Command group … stored body request` WARNING per member is the tell.
- **Phase 6, play-test tunable:** `STATUS_POSITION_THRESHOLD = 5.0` m and `STATUS_SYNC_INTERVAL_MS = 10000`.
  Marker jitter on a defending group means the threshold is too low; a laggy marker on a moving group means
  the interval is too long.
- **Phase 6, known limit:** `ReapplyLatestSaveData` on a running session will not rebuild an HC group that has
  no live entity (`DoStartGame` is not re-entrant, and the restore walk hangs off `PostGameStart`). Records for
  live groups are protected. Worth a line in Phase 12's review.

- ✅ **RESOLVED 2026-08-22 — furniture added, NO re-parent needed.** The first read of this ("re-parent to a furnished leaf") was over-corrective. Re-reading the chain:

- `Barracks_01_Base.et` carries `SlotBoneMappings` that **auto-fill** `socket_win_150x72`, `socket_win_130x142`, `socket_door_`, `socket_doorframe_ext`, `socket_doorframe_int` and `socket_entry` — with **exactly the same prefabs** a furnished leaf instantiates as explicit children. Doors, windows and entries therefore arrive through inheritance already; the leaf's long child list is Workbench baking out those socket instances plus a glass-material override.
- The **only** thing a furnished leaf genuinely adds beyond the socket set is `Barracks_01_military_furniture_01.et` `{A7E02C0008C6455E}` (39 prop groups — lockers, bunks).

So the fix is **one child block**, not thirty, and not a re-parent: that child is now on `OVT_Barracks.et` (ID `6B1C3D000000004B`). Compile-check exit 0.

**Consequently `OVT_BarracksComponent "{6B1C3D0000000040}"` and the desk child STAY** — the earlier "delete these two blocks" instruction only applied under a re-parent, which is not happening. `OVT_Barracks.et` still inherits `Barracks_01_Base.et` directly, so it inherits nothing from the `_military_base` override and there is no duplication.

*(Leaf GUIDs were confirmed genuinely unrecoverable from the CLI: zero inbound references anywhere in the vanilla tree, and the local `resourceDatabase.rdb` indexes only this addon's own files.)*

🔄 **The action host MOVED AGAIN — from the building overrides into 7 same-GUID FURNITURE overrides (user's call, 2026-08-22).** Reason: Workbench will not open a prefab the addon does not own, so the radio could not be positioned against furniture that only exists on leaf variants. Overriding the furniture prefabs solves it — and unlike the leaves, **every furniture GUID is recoverable** from the leaves' own child references.

| Family | Furniture override (same-GUID) | Reached by |
|---|---|---|
| `Barracks_01_military_base` | `{A7E02C0008C6455E}` `Barracks_01_military_furniture_01.et` | camo, white, yellow — **and the buildable** |
| | `{46C6B41A7FB91C09}` `Barracks_01_military_furniture_02.et` | camo_v1 |
| | `{17055976255BBA78}` `Barracks_01_military_furniture_tutorial_01.et` | white_tutorial |
| `Barracks_USSR_01_military_base` | `{7BEA948DEA839B2A}` `Furniture/Barracks_USSR_01_military_furniture_01.et` | USSR military |
| | `{C5B7F128F06E18A7}` `Furniture/Barracks_USSR_01_BARS_furniture_01.et` | USSR BARS |
| `Barracks_E_02_base` | `{C58FB6ADECE48F6F}` `Furniture/Barracks_E_02_furniture_01.et` | E_02 camo |
| | `{B31404BAD83FD5D5}` `Furniture/Barracks_USSR_02_furniture_01.et` | USSR_02 |

Each override copies the vanilla line 1 and root `ID` byte-for-byte and adds the radio as a child at `coords 0 0 0` with the `hcradio` context and `OVT_ManageHighCommandAction`. GUIDs minted from `6B1C3D…3000` upward.

**The radio was REMOVED from all three building overrides and from `OVT_Barracks.et`** — otherwise a furnished leaf would carry two. The building overrides now hold **only** `OVT_BarracksComponent`, which is correct: that marker is the sphere-query target and belongs on the building. `OVT_Barracks.et` keeps its furniture child (`{A7E02C0008C6455E}`), so the buildable picks the radio up through the furniture override automatically.

⚠️ **Known coverage gap of this approach:** plain `Barracks_E_02.et` has **no furniture prefab at all**, so it gets no radio. The previous building-level placement did cover it. Accepted trade for authoring comfort; revisit if that variant matters.

✅ **Ruin gate verified safe either way:** `OVT_StructureDamage.IsUsable` walks `GetRootParent()` (`:87`), so it resolves to the *building* whether the radio hangs off the building or off the furniture.

🔄 **(Superseded) Action host changed from a desk to a radio (user request, 2026-08-22).** All four hosts now carry `GenericEntity : "{03B44EA7652D0D17}Prefabs/Props/Military/Radios/RadioStation_R123M_01.et"` — the same radio the recruitment tent uses (`OVT_RecruitmentTent.et:81`) — instead of `TableMilitary_US_01`. Rationale: the user wants to place the radio **on** a desk rather than have the desk itself be the interaction host. `ContextName` `hcdesk` → `hcradio` (both the context and the action's `ParentContextList`), and the action point offset dropped `0 0.9253 0` → `0 0.25 0` since 0.9253 was desk-surface height and the radio's own origin already sits on whatever it rests on.

⚠️ **The radio initially did not render — a SECOND minted-vs-inherited GUID collision, found by the user (2026-08-22).** The desk block carried `Hierarchy "{6B1C3D…004A}"`, an empty declaration with a **minted** GUID. `RadioStation_R123M_01.et:10` already declares `Hierarchy "{51D4F0B3132BBDEE}"`, so the radio ended up with **two Hierarchy components** and did not render. Removed the minted block from all four hosts — it was empty, so there was nothing to preserve and the inherited one is correct.

**Confirmed safe by inspection of the radio's full chain** (`RadioStation_R123M_01.et` → `DestructibleMultiPhase_Props_Base.et`): it carries `MeshObject`, `RigidBody`, `Hierarchy`, and from the base `SCR_DestructionMultiPhaseComponent` + `RplComponent`. There is **no `ActionsManagerComponent`** anywhere in the chain, so minting one for the action host is correct.

⚠️ **Removed with it: the stray `OVT_PlaceableComponent "{65CE9A2ECEC30E10}" { Enabled 0 }`.** That block existed to *disable* a component the desk inherited from Overthrow's `FurnitureMilitary_base.et` override. The radio's ancestry is `DestructibleMultiPhase_Props_Base.et`, which carries no such component — so on the radio that same block would have **added** one, which is exactly the minted-vs-inherited GUID defect that broke the building mesh earlier in this phase. Verified: zero `OVT_PlaceableComponent` anywhere in the radio's chain, and Overthrow overrides nothing in it.

🔧 **Workbench, Phase 4 — the radio `coords` in all four barracks hosts are a blind guess** (`coords 1 0.045 1`, `angles 0 0 0`). No interior floor plan is reachable from the CLI. Open `OVT_Barracks.et`, the three `_base.et` deltas **and one leaf variant of each** (e.g. `Barracks_01_military_camo.et`), confirm no dropped-attribute warnings, confirm the radio appears, and place it on a desk. Note `0.045` was a desk-on-floor height — a radio sitting on furniture needs a real surface height.
- 🎨 **Art, Phase 4:** the Barracks build card has no `m_tPreview` — no `.edds` exists for it.
- 🎨 **Art, Phase 7:** no HC imageset entries. Contact/no-ammo badges borrow shipped quads; two constants in `OVT_MapHighCommandLayer` swap over when atlas art exists.
- 🎮 **Play-test, Phase 7 — the single most important check:** with a group selected on the map, pressing `X` on a pad must **NOT** open the vanilla radial contextual menu. That is the exclusivity test. Full 10-step gamepad walkthrough in the Phase 7 session note.
- **Phase 2, play-test only:** a vehicle group is given its DEFEND waypoint in the same frame boarding starts — whether the AI stays mounted or dismounts at the destination is unverified. D10's fallback (`[move → wait]` cycle) is one line away if the behaviour is wrong.
- **Phase 2, pre-existing hazard (shared with QRF):** `SpawnDefendWaypoint` null-derefs if its waypoint prefab fails to spawn. Not introduced here; not fixed here.

- Workbench: desk `coords` inside each of the three vanilla barracks families; confirm each same-GUID override resolves (Phase 4).

- **Phase 3, Workbench:** open `Configs/Factions/FIA_OverthrowData.conf` and `Prefabs/GameMode/OVT_OverthrowFactionManager.et` and confirm the 11 `OVT_HighCommandGroupEntry` rows load with no dropped-attribute warnings, and that every group/vehicle picker resolves to a real prefab (11 pairs; the `truck_squad` GUID was corrected from the plan's).
- **Phase 3, `.st` re-export owed:** 9 new `OVT-Msg-HC*` keys are in `Language/localization_Overthrow.st` only. Until the Workbench export runs, those notifications render their raw key.
- **Phase 3, play-test only:** whether `MANIFEST_CAPTURE_ATTEMPTS = 5` frames is enough for `InitialInventoryItems` to land. A `read no gear at all off '<prefab>'` WARNING at campaign start is the tell; raising the budget is a one-line change.
- **Phase 3, play-test only:** the manifest spawns ~10 characters at a map corner in the first ~60 frames of a campaign. Confirm nothing visible happens there and no AI reacts to them.

---

## Phase 4 pre-brief — the Barracks prefab and its site ALREADY EXIST (user, 2026-08-22)

The user authored both by hand before the build reached Phase 4. **T4.1 and T4.3 consume these; they do not create new ones.**

| Asset | GUID | Path |
|---|---|---|
| Barracks buildable | `{048EA1F9675A05E6}` | `Prefabs/Structures/Military/Houses/Barracks_01/OVT_Barracks.et` |
| Construction site | `{E91657A942F4C8DC}` | `Prefabs/Sites/Site_Barracks.et` |

Deltas from `implementation.md` §3.10(a) — the plan is superseded on each of these points:

- **Path.** The plan wrote `Prefabs/Structures/Military/Barracks/OVT_Barracks.et`. The real path is `Prefabs/Structures/Military/Houses/Barracks_01/OVT_Barracks.et` (beside the vanilla family). Use the real one.
- **`m_sBuildableType` is `"barracks"`, lowercase.** Anything that matches on the type string must use that spelling.
- **Already on the prefab:** `OVT_BuildableComponent`, `OVT_MapMarkerComponent` (POI, `#OVT-Barracks`, `house` icon, `m_bMustOwnBase 1`), `OVT_PlayerOwnerComponent`.
- **Still to add in T4.1:** `OVT_BarracksComponent`, `OVT_StructureDestructionComponent` (bare `.xob` phase model), `RplComponent { Enabled 1 }`, and the desk child carrying `OVT_ManageHighCommandAction`.
- **T4.3 buildables.conf** follows the shipped Warehouse entry (`:134-164`): `m_aPrefabs` → the Barracks GUID above, `m_SitePrefab "{E91657A942F4C8DC}Prefabs/Sites/Site_Barracks.et"`, `m_bBuildAtBase 1` only.
- 🔴 **D14 is OVERRIDDEN by the user (2026-08-22) twice over: the Barracks costs materials, AND it is THE MOST EXPENSIVE BUILDABLE.** The plan's "no `m_aResourceRequirements`" line and its `m_iCost 6000` are both dead. The Barracks must top the shipped ladder, whose current peak is the Warehouse (`m_iCost 12000`, timber 100 / cement 200 / steel 100 / hardware 40).

  | field | value | vs. Warehouse (the old peak) |
  |---|---|---|
  | `m_iCost` | **20000** | 12000 |
  | `m_iRewardXP` | **60** | 50 |
  | `timber` | **170** | 100 |
  | `cement` | **280** | 200 |
  | `steel` | **170** | 100 |
  | `hardware` | **70** | 40 |

  **Quantities are sized to the haul, at the user's instruction (2026-08-22): "two almost full truck loads."** Volume is integer litres (`OVT_ResourceStoreComponent`, m³ × 1000, converted once); litres per unit come from `Configs/Resistance/resources.conf` `m_fCubicMetresPerUnit` — timber 100, cement 50, steel 40, hardware 20. The haulers are `Ural4320_transport.et` and `M923A1_transport.et` at `m_fCargoVolume 20` = **20 000 L**; the generic `Prefabs/Vehicles/Core/Wheeled_Truck_Base.et` is 15 m³.

  | Load | Litres | Trips in a 20 m³ transport |
  |---|---|---|
  | Warehouse (old peak) | 24 800 | 1.24 |
  | **Barracks** | **39 200** | **1.96 — one full truck + a second at 96 %** |

  That is the "two almost full truck loads" target hit at 98 % of two full loads, without spilling into a third trip. On the smaller 15 m³ truck base it is 2.61 loads. **If either the resource litre-per-unit figures or the transport `m_fCargoVolume` change, re-derive these quantities — the requirement is the haul, not the numbers.**

  Each `OVT_BuildableResourceRequirement` needs a fresh nested id from the `6B1C3D…` series. **If any buildable is added or repriced above these before Phase 4 lands, re-check that the Barracks is still the dearest** — "most expensive" is the requirement, the numbers are just today's expression of it. Play-test tunable — flag in the final report.

Known coverage gap, accepted: the three overrides miss the `Barracks_01_medical_base` family and the non-military `Barracks_USSR_01_base` leaves. That is the plan's curation, not an oversight.

---

### Phase 4 (ui-developer, 2026-08-22)

- **Verified all three T4.2 GUIDs.** `{5F97E54397247954}` / `{2CB4D91249389DFD}` / `{60A21DFAFF77773D}` each appear as the parent reference on exactly the leaf files the plan named (`Barracks_01_military_camo*.et`, `Barracks_USSR_01_military*.et`, `Barracks_E_02*.et`), confirming they are each file's own vanilla resource GUID and the delta targets are correct. No GUID failed verification this phase.
- **`OVT_Barracks.et` already existed** (user-authored, `{048EA1F9675A05E6}`, inherits `Barracks_01_Base.et` `{80D7D3EAABADBD44}`) with `OVT_BuildableComponent`, `OVT_MapMarkerComponent` and `OVT_PlayerOwnerComponent` already on it. Edited in place — `OVT_BarracksComponent`, `OVT_StructureDestructionComponent`, `SCR_DestructibleBuildingComponent { Enabled 0 }`, `RplComponent "{50A4E7C9B5728062}"` (the literal GUID Warehouse/RecruitmentTent/ShopHouse all reuse) and the desk child were appended; nothing pre-existing was touched.
- **Ruin phase model** is `{23A4F25400F52064}Assets/Structures/Military/Houses/Barracks_01/Barracks_01_Ruin.xob` — recovered from `Barracks_01_Ruin_base.et`'s own `MeshObject`, a bare `.xob` as D15 requires. Health/threshold (100000/50000) and `BREAK_METAL` copied from the Warehouse retrofit, since Barracks is now the more expensive of the two structures.
- **T4.2 deltas add nothing but a `components{}` block and a children block** — no attempt to restate the vanilla file's own content. `same-guid-prefabs-are-deltas` (project memory) confirms the merge is element-wise by GUID: our `OVT_BarracksComponent` and desk entity are pure appends alongside whatever each vanilla leaf's own base already declares, so nothing is lost.
- **The purchase screen never computes a price** (grepped clean for any economy call) and reuses the nine `Msg-HCPurchase*` sentences already authored in Phase 3 as its in-screen refusal text, rather than authoring a second copy of the same reasons.
- **`RpcDo_HCQuote`/`RpcDo_HCResult` were empty stubs** (Phase 3's honest placeholder) — filled with `m_OnHCQuote`/`m_OnHCResult` `ScriptInvoker`s, the `OVT_RecruitCommandComponent.m_OnRecruitQuote` shape. This is a Phase 3 file touched by this phase because the client entry points named in the task brief required it; no new RPC, no arity change.
- **Quote debounce is 250 ms**, cancelled and re-armed on every selection change (`GetGame().GetCallqueue().Remove` before `CallLater`), and the debounced call re-checks `index == m_iSelectedIndex` before firing — a pad holding `MenuDown` cannot fire one `QuoteEntry` sphere query per frame.
- **Buy is gated on `m_iQuotedEntryIndex == m_iSelectedIndex`**, set only by an `RESULT_OK` quote for that exact row and cleared on every new selection/debounce — the `OVT_LoadoutsContext.GetLastQuoteLoadout()` re-check shape, ported into the context itself since `OVT_HighCommandRequestComponent` does not cache a "last quote" the way the recruit command component does.
- **List navigation is explicit `MenuUp`/`MenuDown` handling in `OnActiveFrame`**, not focus-driven, and `MenuSelect` is deliberately unwired — the `OVT_LoadoutsContext` purchase-mode shape (a pad's confirm button must not be able to spend money while browsing).
- **`m_tPreview` omitted** on the Barracks buildable entry — no `.edds` preview asset exists for it in the extracted reference tree (checked). Cosmetic only; a follow-up asset drop is a one-line `.conf` edit.
- **Desk `coords 1 0.045 1`, `angles 0 0 0`, on all four hosts** — a blind guess (no interior floor plan available from the CLI), explicitly Workbench-gated per the plan. Every host uses the same table prefab (`{7762F50A860DD074}TableMilitary_US_01.et`) and the same offset, so tuning one is likely representative but each of the four barracks families should be opened and checked.
- **`OVT-Barracks` (the map marker name) was authored even though it predates this phase** — the user's already-existing `OVT_Barracks.et` referenced it via `OVT_MapMarkerComponent.m_UiInfo.Name` with no `.st` entry backing it, which would have rendered the raw key on every barracks POI. Low-risk, in-scope fix.
- **GUID series used:** `6B1C3D0000000040`–`…004A` (`OVT_Barracks.et`), `…0050`–`…0057` / `…0060`–`…0067` / `…0070`–`…0077` (the three T4.2 deltas), `…0080`–`…0084` (buildables.conf entry + 4 resource requirements), `…0085`–`…0089` (the `OverthrowHighCommandBuy` action), `…0090`/`…0092` (layout + context registration on `Character_Player.et`; `…0091` was reserved but not used — the row layout ended up keyed on `…2000` instead, see below), `…2000`–`…200A` (`HighCommandGroupRow.layout` and its handler component), `…1000`–`…1021` (`HighCommandMenu.layout` widgets), `…00A9`–`…00D1` (41 `.st` entries). All verified to appear exactly once in this tree (except the deliberate 2–3 legitimate cross-references: a `.layout.meta` `Name` GUID is expected to recur in the `.et`/`.c` files that reference that resource) and zero times in the vanilla tree.
- **`.st` braces: 2120/2120 before this phase's edit, 2202/2202 after** (41 entries × 2 braces = +82). Every new `Id` occurs exactly once.

**Known limits, for later phases / play-test:**
- Desk placement inside all four barracks hosts needs Workbench hand-tuning (above).
- No preview thumbnail for the Barracks build card.
- The purchase screen has no "n / cap" roster line (Phase 8's job); a cap refusal still shows via the reused `Msg-HCPurchaseAtCap` text once a quote is asked.

## Session Notes

### 2026-08-22 — Phase 6 complete

7/7 tasks, compile-check exit 0 (6308 files). The ADVANCED tier earned itself twice, both on the load walk:
§3.11's literal sequence spawns an EMPTY group and waits on an asynchronous answer, which `m_bDeleteWhenEmpty`
deletes a frame later — so the prefab fallback had to become pre-emptive rather than reactive; and rebuilding
from the catalog entry rather than the saved roster would have brought a squad that lost two men back at full
strength, quietly making Phase 12's `…MemberBodies_RoundTrip` case unpassable. Both are written up above.

Twelve-row RPC arity audit filled in, each row read against its handler by hand. The twelfth row is
`RpcDo_RecruitQuote` (Phase 5's 5→6 change), not — as the brief supposed — `RpcDo_HCQuote`, which §3.6's table
does list at #6. The parked-group silence guarantee is a three-link chain: one send site, one gated caller,
and an echo that only an actual send can write.

T6.7 closes the half of the architectural gap that was quietly fatal: HC member bodies were being untracked
the instant they joined their group, so D8's body UUIDs could never exist and Phase 9's "converted recruits
keep their gear" was already dead. The fix mirrors the recruit precedent exactly — register before the group
add, exclude in both hooks — and needed no second mechanism.

### 2026-08-22 — Phase 11 gate: FOUR suites run, one real red (NOT ours)

| Suite | Result |
|---|---|
| `OVT_TEST_PersistenceRoundTripSuite` | ✅ **40 tests**, exit 0 |
| `OVT_TEST_PersistenceSuite` | ✅ **13 tests**, exit 0 |
| `OVT_TEST_CampaignSuite` | ⚠️ **2 of 18 failed** |

Both Campaign failures were `TestResultTimeout` after 500 ms, not assertions. Re-run **individually** (the documented way to settle a specific red, and cheaper than a group):

- `OVT_TEST_Campaign_GMWaypointWalk` → ✅ **passes alone**. A load artifact, and it **clears the T11.6 enum-reordinal concern** — the GM waypoint path was the one thing that removal could plausibly have broken.
- `OVT_TEST_Campaign_ResourcePort_PurchaseMovesMoneyAndResources` → 🔴 **fails alone.** Real, and **not this feature's**: the assertion is a movement precondition (*"the caller never arrived at the port: after 301 poll(s) the character is 71.076 m from …"*), and the working tree carries **uncommitted concurrent-session changes to exactly that surface** — `OVT_PortContext.c`, `OVT_ResourceUtils.c`, `OVT_StorageUtils.c`, `OVT_ResourceStoreComponent.c`, plus a new untracked `OVT_ResourceCargoBedComponent.c`. High Command modified none of them. **Belongs to `logistics/resources`; flagged to the user, not filed.**

### 2026-08-22 — Suite gate for Phases 7–9 + riders DISCHARGED

| Suite | Result |
|---|---|
| `OVT_TEST_LogicSuite` | ✅ **OK — 278 tests**, 10 s, exit 0 |
| `OVT_TEST_InitSuite` | ✅ **OK — 208 tests**, 57 s, exit 0 |

🟢 **The pre-existing Init failure is GONE.** `OVT_TEST_Init_StructureDamage_FOnlyTheFuelDepotFallsToRifleFire` (`A vehicle bump ruined 'Warehouse'`) failed at the Phase 1–6 gate and was proven not-ours by removing the Barracks buildable entry and re-running. It now passes — fixed by the user's concurrent `core/damage` / resources work. **The Init suite is fully green for the first time this feature.**

### 2026-08-22 — Phase 9 complete + 5 play-test riders
5/5 tasks. Compile-check exit 0; checker at baseline (re-verified with the committed script); `.st` 2302/2302; I1–I6, I8 clean. 59/77 (77%).

Riders R4–R8 fixed in one pass — two of them corrected the orchestrator's own diagnosis (the `%1` was a raw key on the *screen*, not a missing notification parameter; the missing gunner was chiefly that `UAZ469_PKM_FIA.et` has **no turret compartment**, only pilot + cargo, so a turret miss fell silently through to the back seat). Orchestrator independently confirmed the compartment claim against the vanilla prefab.

### 2026-08-22 — Phase 8 complete
3/3 tasks. Compile-check exit 0 (6311 files). Input checker at baseline, re-verified with the committed script. `.st` 2278/2278 balanced.

### 2026-08-22 — Phase 7 complete
7/7 tasks. Compile-check exit 0 (6309 files). Input checker at the shipped baseline plain **and** `--warnings` — and the orchestrator confirmed that verdict holds with the **committed** checker, so the two `ACKNOWLEDGED` entries the agent added did not buy the green. Layer reads records only (the single grep hit is a header comment). No `ALWAYS_TOP` in either layout.

### 2026-08-22 — Suite gate for Phases 1–6 DISCHARGED

Run by class name, one at a time (the suites are not deterministic run back-to-back under load).

| Suite | Result |
|---|---|
| `OVT_TEST_LogicSuite` | ✅ **OK — 276 tests**, 11 s, exit 0 |
| `OVT_TEST_InitSuite` | ⚠️ **1 of 206 failed** — `OVT_TEST_Init_StructureDamage_FOnlyTheFuelDepotFallsToRifleFire` |
| `OVT_TEST_PersistenceRoundTripSuite` | ✅ **OK — 40 tests**, 22 s, exit 0 |

🔵 **The Init failure is PRE-EXISTING, not High Command's.** Failure text: `A vehicle bump ruined 'Warehouse' - vehicles park beside every buildable`. The subject is the **Warehouse**, whose prefab and config entry this feature never touched (`git status` on `Prefabs/Structures/Industrial/Houses/Warehouse_01/` is clean).

**Proved, not assumed:** the case reads the *live* buildables config (`OVT_TEST_Init_StructureDamage.c:897` — "a ninth buildable joins the contract the moment it is listed"), so adding the Barracks was a genuine suspect. The orchestrator removed the Barracks entry from `buildables.conf` and re-ran the single case: **it failed identically, same message, 1 of 1.** The entry was restored immediately (10 `m_sName` entries, Barracks back at `:166`).

This belongs to `core/damage` — its post-close "single bullets ruined buildables" fix (2026-08-21) still has a play-test owed, and the Warehouse's bump threshold looks to be the residue. **Not filed as a bug from here** (per the standing rule, and it is a shipped-code defect on a branch with concurrent sessions) — flag it to the user for `core/damage`.

### 2026-08-22 — Phase 5 complete
4/4 tasks, compile-check exit 0. Fifth drifted plan citation found (`BuildQuote` → `ValidateTentPurchase`). Verified the 5→6 arity change by hand at all four sites — `Rpc()` is untyped variadic, so a missed site compiles clean and dies at the wire.

Also fixed the Phase 4 rendering defect the user found in Workbench (minted vs inherited `SCR_DestructibleBuildingComponent` GUID) — see the Phase 4 section.

### 2026-08-22 — Phase 4 complete
6/6 tasks, compile-check exit 0 (6308 files), conflict checker unchanged at baseline (`0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged`). All three T4.2 GUIDs verified against the vanilla tree by inbound reference (none wrong this time). The barracks buildable and its site were pre-authored by the user per the pre-brief; this phase only added components to it. Filled the two Phase-3 RPC stubs (`RpcDo_HCQuote`/`RpcDo_HCResult`) the client screen needed. Full GUID sweep (component + layout + `.st` ranges) confirmed unique in both trees.

### 2026-08-22 — Phase 11 gate: FOUR suites run, one real red (NOT ours)

| Suite | Result |
|---|---|
| `OVT_TEST_PersistenceRoundTripSuite` | ✅ **40 tests**, exit 0 |
| `OVT_TEST_PersistenceSuite` | ✅ **13 tests**, exit 0 |
| `OVT_TEST_CampaignSuite` | ⚠️ **2 of 18 failed** |

Both Campaign failures were `TestResultTimeout` after 500 ms, not assertions. Re-run **individually** (the documented way to settle a specific red, and cheaper than a group):

- `OVT_TEST_Campaign_GMWaypointWalk` → ✅ **passes alone**. A load artifact, and it **clears the T11.6 enum-reordinal concern** — the GM waypoint path was the one thing that removal could plausibly have broken.
- `OVT_TEST_Campaign_ResourcePort_PurchaseMovesMoneyAndResources` → 🔴 **fails alone.** Real, and **not this feature's**: the assertion is a movement precondition (*"the caller never arrived at the port: after 301 poll(s) the character is 71.076 m from …"*), and the working tree carries **uncommitted concurrent-session changes to exactly that surface** — `OVT_PortContext.c`, `OVT_ResourceUtils.c`, `OVT_StorageUtils.c`, `OVT_ResourceStoreComponent.c`, plus a new untracked `OVT_ResourceCargoBedComponent.c`. High Command modified none of them. **Belongs to `logistics/resources`; flagged to the user, not filed.**

### 2026-08-22 — Suite gate for Phases 7–9 + riders DISCHARGED

| Suite | Result |
|---|---|
| `OVT_TEST_LogicSuite` | ✅ **OK — 278 tests**, 10 s, exit 0 |
| `OVT_TEST_InitSuite` | ✅ **OK — 208 tests**, 57 s, exit 0 |

🟢 **The pre-existing Init failure is GONE.** `OVT_TEST_Init_StructureDamage_FOnlyTheFuelDepotFallsToRifleFire` (`A vehicle bump ruined 'Warehouse'`) failed at the Phase 1–6 gate and was proven not-ours by removing the Barracks buildable entry and re-running. It now passes — fixed by the user's concurrent `core/damage` / resources work. **The Init suite is fully green for the first time this feature.**

### 2026-08-22 — Phase 9 complete + 5 play-test riders
5/5 tasks. Compile-check exit 0; checker at baseline (re-verified with the committed script); `.st` 2302/2302; I1–I6, I8 clean. 59/77 (77%).

Riders R4–R8 fixed in one pass — two of them corrected the orchestrator's own diagnosis (the `%1` was a raw key on the *screen*, not a missing notification parameter; the missing gunner was chiefly that `UAZ469_PKM_FIA.et` has **no turret compartment**, only pilot + cargo, so a turret miss fell silently through to the back seat). Orchestrator independently confirmed the compartment claim against the vanilla prefab.

### 2026-08-22 — Phase 8 complete
3/3 tasks. Compile-check exit 0 (6311 files). Input checker at baseline, re-verified with the committed script. `.st` 2278/2278 balanced.

### 2026-08-22 — Phase 7 complete
7/7 tasks. Compile-check exit 0 (6309 files). Input checker at the shipped baseline plain **and** `--warnings` — and the orchestrator confirmed that verdict holds with the **committed** checker, so the two `ACKNOWLEDGED` entries the agent added did not buy the green. Layer reads records only (the single grep hit is a header comment). No `ALWAYS_TOP` in either layout.

### 2026-08-22 — Suite gate for Phases 1–6 DISCHARGED

Run by class name, one at a time (the suites are not deterministic run back-to-back under load).

| Suite | Result |
|---|---|
| `OVT_TEST_LogicSuite` | ✅ **OK — 276 tests**, 11 s, exit 0 |
| `OVT_TEST_InitSuite` | ⚠️ **1 of 206 failed** — `OVT_TEST_Init_StructureDamage_FOnlyTheFuelDepotFallsToRifleFire` |
| `OVT_TEST_PersistenceRoundTripSuite` | ✅ **OK — 40 tests**, 22 s, exit 0 |

🔵 **The Init failure is PRE-EXISTING, not High Command's.** Failure text: `A vehicle bump ruined 'Warehouse' - vehicles park beside every buildable`. The subject is the **Warehouse**, whose prefab and config entry this feature never touched (`git status` on `Prefabs/Structures/Industrial/Houses/Warehouse_01/` is clean).

**Proved, not assumed:** the case reads the *live* buildables config (`OVT_TEST_Init_StructureDamage.c:897` — "a ninth buildable joins the contract the moment it is listed"), so adding the Barracks was a genuine suspect. The orchestrator removed the Barracks entry from `buildables.conf` and re-ran the single case: **it failed identically, same message, 1 of 1.** The entry was restored immediately (10 `m_sName` entries, Barracks back at `:166`).

This belongs to `core/damage` — its post-close "single bullets ruined buildables" fix (2026-08-21) still has a play-test owed, and the Warehouse's bump threshold looks to be the residue. **Not filed as a bug from here** (per the standing rule, and it is a shipped-code defect on a branch with concurrent sessions) — flag it to the user for `core/damage`.

### 2026-08-22 — Phase 5 complete
4/4 tasks, compile-check exit 0. Fifth drifted plan citation found (`BuildQuote` → `ValidateTentPurchase`). Verified the 5→6 arity change by hand at all four sites — `Rpc()` is untyped variadic, so a missed site compiles clean and dies at the wire.

Also fixed the Phase 4 rendering defect the user found in Workbench (minted vs inherited `SCR_DestructibleBuildingComponent` GUID) — see the Phase 4 section.

### 2026-08-22 — Phase 4 complete
6/6 tasks. Compile-check exit 0 (6308 files); input-conflict checker `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged` — the shipped baseline, unchanged. First phase where **no plan citation was wrong**: all three vanilla barracks GUIDs verified clean.

### 2026-08-22 — Phase 3 complete
8/8 tasks, compile-check exit 0 (6305 files). The ADVANCED tier earned itself once, on the money path:
the plan's `truck_squad` vehicle GUID is the UNCOVERED Ural, and authoring it blind would have shipped a
"covered troop truck" that arrives open-topped — the exact failure §3.3 warned about and asked to be
re-verified. Every other GUID and every member count in the table checked out against the vanilla tree.

Two design corrections the plan did not anticipate, both money-shaped: the manifest may not cache
prices (they are per-position and per-player), and an unregistered resource must be dropped rather than
carried at price 0 (a zero-price line would consume warehouse stock nobody paid for). One duplication
removed before it could drift — the warehouse record match delegates to the shipped
`GetNearestWarehouse` instead of re-walking `m_aWarehouses` with `<=` against its `<`.

T3.8 turned out to be mostly already done: all five `SplitCoverage` cases the task named were written
in Phase 1. Added the three that were genuinely missing, of which the load-bearing one is the
coverage→fee composition — the join between two classes that are each already correct on their own.

### 2026-08-22 — Phase 11 gate: FOUR suites run, one real red (NOT ours)

| Suite | Result |
|---|---|
| `OVT_TEST_PersistenceRoundTripSuite` | ✅ **40 tests**, exit 0 |
| `OVT_TEST_PersistenceSuite` | ✅ **13 tests**, exit 0 |
| `OVT_TEST_CampaignSuite` | ⚠️ **2 of 18 failed** |

Both Campaign failures were `TestResultTimeout` after 500 ms, not assertions. Re-run **individually** (the documented way to settle a specific red, and cheaper than a group):

- `OVT_TEST_Campaign_GMWaypointWalk` → ✅ **passes alone**. A load artifact, and it **clears the T11.6 enum-reordinal concern** — the GM waypoint path was the one thing that removal could plausibly have broken.
- `OVT_TEST_Campaign_ResourcePort_PurchaseMovesMoneyAndResources` → 🔴 **fails alone.** Real, and **not this feature's**: the assertion is a movement precondition (*"the caller never arrived at the port: after 301 poll(s) the character is 71.076 m from …"*), and the working tree carries **uncommitted concurrent-session changes to exactly that surface** — `OVT_PortContext.c`, `OVT_ResourceUtils.c`, `OVT_StorageUtils.c`, `OVT_ResourceStoreComponent.c`, plus a new untracked `OVT_ResourceCargoBedComponent.c`. High Command modified none of them. **Belongs to `logistics/resources`; flagged to the user, not filed.**

### 2026-08-22 — Suite gate for Phases 7–9 + riders DISCHARGED

| Suite | Result |
|---|---|
| `OVT_TEST_LogicSuite` | ✅ **OK — 278 tests**, 10 s, exit 0 |
| `OVT_TEST_InitSuite` | ✅ **OK — 208 tests**, 57 s, exit 0 |

🟢 **The pre-existing Init failure is GONE.** `OVT_TEST_Init_StructureDamage_FOnlyTheFuelDepotFallsToRifleFire` (`A vehicle bump ruined 'Warehouse'`) failed at the Phase 1–6 gate and was proven not-ours by removing the Barracks buildable entry and re-running. It now passes — fixed by the user's concurrent `core/damage` / resources work. **The Init suite is fully green for the first time this feature.**

### 2026-08-22 — Phase 9 complete + 5 play-test riders
5/5 tasks. Compile-check exit 0; checker at baseline (re-verified with the committed script); `.st` 2302/2302; I1–I6, I8 clean. 59/77 (77%).

Riders R4–R8 fixed in one pass — two of them corrected the orchestrator's own diagnosis (the `%1` was a raw key on the *screen*, not a missing notification parameter; the missing gunner was chiefly that `UAZ469_PKM_FIA.et` has **no turret compartment**, only pilot + cargo, so a turret miss fell silently through to the back seat). Orchestrator independently confirmed the compartment claim against the vanilla prefab.

### 2026-08-22 — Phase 8 complete
3/3 tasks. Compile-check exit 0 (6311 files). Input checker at baseline, re-verified with the committed script. `.st` 2278/2278 balanced.

### 2026-08-22 — Phase 7 complete
7/7 tasks. Compile-check exit 0 (6309 files). Input checker at the shipped baseline plain **and** `--warnings` — and the orchestrator confirmed that verdict holds with the **committed** checker, so the two `ACKNOWLEDGED` entries the agent added did not buy the green. Layer reads records only (the single grep hit is a header comment). No `ALWAYS_TOP` in either layout.

### 2026-08-22 — Suite gate for Phases 1–6 DISCHARGED

Run by class name, one at a time (the suites are not deterministic run back-to-back under load).

| Suite | Result |
|---|---|
| `OVT_TEST_LogicSuite` | ✅ **OK — 276 tests**, 11 s, exit 0 |
| `OVT_TEST_InitSuite` | ⚠️ **1 of 206 failed** — `OVT_TEST_Init_StructureDamage_FOnlyTheFuelDepotFallsToRifleFire` |
| `OVT_TEST_PersistenceRoundTripSuite` | ✅ **OK — 40 tests**, 22 s, exit 0 |

🔵 **The Init failure is PRE-EXISTING, not High Command's.** Failure text: `A vehicle bump ruined 'Warehouse' - vehicles park beside every buildable`. The subject is the **Warehouse**, whose prefab and config entry this feature never touched (`git status` on `Prefabs/Structures/Industrial/Houses/Warehouse_01/` is clean).

**Proved, not assumed:** the case reads the *live* buildables config (`OVT_TEST_Init_StructureDamage.c:897` — "a ninth buildable joins the contract the moment it is listed"), so adding the Barracks was a genuine suspect. The orchestrator removed the Barracks entry from `buildables.conf` and re-ran the single case: **it failed identically, same message, 1 of 1.** The entry was restored immediately (10 `m_sName` entries, Barracks back at `:166`).

This belongs to `core/damage` — its post-close "single bullets ruined buildables" fix (2026-08-21) still has a play-test owed, and the Warehouse's bump threshold looks to be the residue. **Not filed as a bug from here** (per the standing rule, and it is a shipped-code defect on a branch with concurrent sessions) — flag it to the user for `core/damage`.

### 2026-08-22 — Phase 5 complete
4/4 tasks, compile-check exit 0. Fifth drifted plan citation found (`BuildQuote` → `ValidateTentPurchase`). Verified the 5→6 arity change by hand at all four sites — `Rpc()` is untyped variadic, so a missed site compiles clean and dies at the wire.

Also fixed the Phase 4 rendering defect the user found in Workbench (minted vs inherited `SCR_DestructibleBuildingComponent` GUID) — see the Phase 4 section.

### 2026-08-22 — Phase 4 complete
6/6 tasks. Compile-check exit 0 (6308 files); input-conflict checker `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged` — the shipped baseline, unchanged. First phase where **no plan citation was wrong**: all three vanilla barracks GUIDs verified clean.

### 2026-08-22 — Phase 3 complete
8/8 tasks, compile-check exit 0 (6305 files). Caught the plan's fourth bad citation (the `truck_squad` vehicle GUID), which the orchestrator independently confirmed against the vanilla entity catalog. Ruled on DoD Q5 in the agent's favour with a condition on Phase 6's audit table. Verified `.st` braces balanced (2120/2120) — the failure mode there is silent data loss on the next Workbench save, so it gets checked every time the file is touched.

⚠️ **Localization re-export owed** — 9 new `.st` entries render as raw keys until the user re-exports from Workbench.

### 2026-08-22 — Phase 2 complete
9/9 tasks, compile-check exit 0 (6302 files). The ADVANCED tier paid for itself three times over: two wrong plan citations (`SCR_AIGroup.GetMembers`, `AddAIToSlaveGroup`) that would have produced silently empty groups, and one waypoint-ownership trap that would have leaked 9 waypoints per re-order. Orchestrator independently re-ran the load-bearing acceptance greps — all 6 `Spawn*Waypoint` sites own their waypoints through `GiveWaypoint`, `AddEntityObserver` has exactly one gated call site, `RemoveEntityObserver` is unconditional and ordered after `Remove(InstallObserver)`.

Also opened the two-part architectural gap above (load walk unassigned; HC bodies untracked), scheduled into Phase 6 as T6.6/T6.7. Task total 75 → 77.

### 2026-08-22 — Phase 1 complete
10/10 tasks, compile-check exit 0. Suite gate deferred at the user's instruction (Workbench live). Every prefab/config mutation in the ledger was confirmed **silent at compile-check** — the project's established "written, compiled, never wired" failure class, which is exactly why the Init cases exist.

### 2026-08-22 — Scaffolded
Docs created from `implementation.md`: 75 tasks across 13 phases (2, 3, 6, 7, 9, 11 ADVANCED). Baseline clean on `v1.5`.
