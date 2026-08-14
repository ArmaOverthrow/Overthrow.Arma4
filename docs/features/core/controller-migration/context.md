# Controller Migration - Context & Decisions

**Last Updated:** 2026-08-14
**Current Phase:** Complete
**Status:** ✅ **COMPLETE — MP play-test green 2026-08-14.** All 21 §6 steps passed, Workbench loads all three text-edited prefabs clean, listen-host save → quit → Continue green (F15). BUG-161/162 runtime-confirmed on the migrated path.

**Epic:** `core` (feature #5 — `docs/features/core/epic-overview.md`)
**Plan:** `implementation.md` (authoritative — §4 phase tables are the RPC-by-RPC checklist)
**Requirements:** `requirements.md`

---

## Quick Status

**What's Done:**
- ✅ Plan approved (2026-08-14): foundation-first, 10 phases, one final MP play-test in P10
- ✅ Baselines measured 2026-08-14: compile clean, Fast **101**, All **142**, both exit 0
- ✅ **Phase 1 — Seam hardening (2026-08-14, advanced agent):** `OVT_ControllerComponent<T>.Get()` live, six `OVT_Global` getters deleted (getter count 23→17), `LootBattlefield` inlined, `GetUI`/`GetDifficulty` null-safe, `OVT_ControllerRequestComponent` base created, controller cache + world-transition clear, idempotent chat-command registration, Continue-controller Campaign case + controller-seam Init case (both proven able to fail). Gates: Fast **102**, All **144**, GetServer still 60 (correct — no RPC moved yet)

- ✅ **Phase 2 — Vehicles (2026-08-14, network-specialist):** `OVT_VehicleRequestComponent` live with all 6 handlers (lock, claim, upgrade, repair, import, buy-vehicle); upgrade/repair gained the ownership/proximity/affordability validation they never had; §3.6(a) fixed in BuyVehicle (direct `DoTakePlayerMoney` + direct stock decrement); `PlayerMayUseVehicle` hoisted to the shared base; six handlers + wrappers deleted from the monolith (2,001 → 1,733 lines). Gates: compile 0, Fast **103**, All **145**, GetServer **53** (see P2-1 — the plan's 52 is an off-by-one)

- ✅ **Phase 3 — Real estate + warehouses (2026-08-14, component-developer):** `OVT_RealEstateRequestComponent` live with all 8 handlers (set-home, buy/sell/rent/stop-renting, add/take/take-to-vehicle warehouse); the three warehouse handlers gained the validation they never had (positive count, warehouse id in range, **registered resource**, stock available, vehicle proximity + `PlayerMayUseVehicle`); `RpcAsk_SetBuildingHome` **deleted** per §3.7/D6 with the disposition recorded in `economy/real-estate/context.md`; `OVT_Global.TransferToWarehouse` / `TakeFromWarehouseToVehicle` moved onto `OVT_RealEstateManagerComponent`; eight handlers + wrappers deleted from the monolith (1,733 → 1,515 lines). Gates: compile 0, Fast **104**, All **146**, GetServer **43**

- ✅ **Phase 4 — Economy, money and shop purchase (2026-08-14, network-specialist):** `OVT_EconomyRequestComponent` live with all 7 handlers + the `RpcDo_DoneTakingMoney` owner response (sell-drugs, donate, send-resistance-funds, send-money, take-player-money, set-tax, buy-skill); `OVT_ShopTransactionComponent` gained `RpcAsk_BuyItems` (+ quantity bound, + `IsValidResourceId`); `RpcAsk_AddToInventory`/`RpcAsk_TakeFromInventory` deleted and `OVT_ShopComponent.AddToInventory`/`TakeFromInventory` became plain server-side mutations. **Both §3.6 latent bugs confirmed live from the code path and filed: BUG-161 (item buying was FREE for clients and a total no-op for a listen host) and BUG-162 (three economy notifications never fired).** Eleven handlers + wrappers deleted from the monolith (1,515 → 1,122 lines). Gates: compile 0, Fast **105**, All **147**, GetServer **33** grep lines / **31** live call sites (see P4-1)

- ✅ **Phase 5 — Resistance operations and FOBs (2026-08-14, component-developer):** `OVT_ResistanceRequestComponent` (place, remove-placed, build, add-officer, add-garrison, convert-supporter + its owner response with the BUG-063 checks carried verbatim) and `OVT_FOBRequestComponent` (camp/FOB garrison, deploy, undeploy, set-priority, camp privacy, delete-camp) both live; **the two zero-validation camp endpoints are closed** (any client could previously delete any camp — and, via the RplId, any entity — or flip any camp's privacy from anywhere); set-priority gained an officer gate, deploy/undeploy gained proximity + `PlayerMayUseVehicle`, place/build gained the manager's own position radii at the seam; `OVT_UndeployFOBAction_New.c` deleted; thirteen handlers + wrappers deleted from the monolith (1,122 → 826 lines). Gates: compile 0, Fast **107**, All **149**, GetServer **18** grep lines / **17** live calls (see P5-1)

- ✅ **Phase 6 — Recruits (2026-08-14, component-developer):** `OVT_RecruitRequestComponent` live with all 4 handlers (recruit-civilian, recruit-from-tent, rename, dismiss); **dismiss gained the ownership check it never had** — any client could previously delete any player's recruit permanently, using ids that are broadcast to every client; rename's "unresolved sender is trusted" escape hatch is gone; both recruit paths keep their carried-verbatim faction/possession/proximity/affordability chain and their charge-after-the-recruit-exists ordering; four handlers + wrappers deleted from the monolith (826 → **642** lines). Gates: compile 0, Fast **108**, All **150**, GetServer **14** grep lines / **13** live calls (see P6-1)

- ✅ **Phase 7 — Loadouts, possession and inventory (2026-08-14, component-developer-advanced):** `OVT_LoadoutRequestComponent` (save, apply-from-box, delete) and `OVT_PossessionRequestComponent` (possess-and-open + its owner response + the whole BUG-147 client lifecycle) both live; **all three loadout handlers lost their client-supplied `string playerId`** so BUG-043's laundering is now structural, and the deliberate absence of a no-box `LoadLoadout` endpoint is preserved and documented; **possession gained the ownership, life-state and proximity checks it never had** — the endpoint previously took any claimed playerId and any RplId and handed out real control of the named character; `RpcDo_OpenInventory` went **Broadcast → Owner** and its `localPlayerId != playerId` filter, the `playerControllerId` plumbing and the server-side belt-and-braces direct call are all gone; `RpcAsk_RestorePossessedEntity` **deleted** per §3.7/D6; the possess request now originates on the **commanding player's own machine** (see P7-1); four handlers + wrappers + the dead `ResolveSenderPersistentId` deleted from the monolith (642 → **315** lines). Gates: compile 0, Fast **110**, All **152**, GetServer **8** grep lines / **7** live calls (see P7-5)

- ✅ **Phase 8 — Jobs and campaign actions (2026-08-14, component-developer):** `OVT_JobRequestComponent` (accept, decline) and `OVT_CampaignRequestComponent` (start-base-capture, deliver-medical-supplies, loot-wanted-check, request-save + its `RpcDo_SaveResult` owner response) both live; **`OVT_PlayerCommsComponent` now contains ZERO RPCs** — it is a bare shell awaiting P10's prefab strip (315 → **23** lines); `StartBaseCapture` lost its client-supplied position entirely (BUG-025's payload half is now inexpressible); the loot check re-derives the character from the caller instead of assuming `GetOwner()` is one; medical supplies gained proximity + `PlayerMayUseVehicle`; save gained a server-side officer gate while keeping the BUG-006 invoker contract byte-for-byte; the `SendNotification`/`RpcAsk_SendNotification` pair and the `RpcAsk_InstantCaptureBase` debug endpoint are **deleted** (the cheat is now a plain `#ifdef WORKBENCH` method on the game mode). Gates: compile 0, Fast **112**, All **154**, **`GetServer()` = 0 grep lines / 0 live calls**

- ✅ **Phase 9 — `OVT_Global` utility split (2026-08-14, component-developer):** `OVT_WorldUtils` / `OVT_PrefabUtils` / `OVT_LoadoutUtils` created in `Scripts/Game/Utilities/`, 21 helpers moved byte-identical, two zero-caller helpers deleted, three thin forwarders kept for the high-traffic three; `OVT_Global.c` **902 → 302** lines with 63 call sites re-pointed across 36 files. Gates: compile 0, Fast **112**, All **154** — dead level with P8, as a pure move must be

- ✅ **Phase 10 — Delete the monolith + final sweep (2026-08-14, network-specialist-advanced):** `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` is **DELETED**, `OVT_Global.GetServer()` is **DELETED**, and the component block is stripped from **both** prefabs (`OVT_OverthrowGameMode.et:150-151`, `Character_Player.et:15-16` — two lines each, nothing else touched). `grep -rn "OVT_PlayerCommsComponent\|GetServer()" Scripts/ Prefabs/` returns **0**, which took 63 doc-comment rewordings (P10-4) plus four `.st` fact-check comments re-pointed to their new homes (P10-5 — every fact still held; nine *co-cited* line numbers had drifted and were corrected). **The full Q10 arity audit is clean: 75 marshalled `Rpc()` calls, 75 twins, zero arity defects** — but it caught one **routing** defect in a component this feature never migrated: `OVT_TowerSabotageComponent` had no `Replication.IsServer()` branch, so radio-tower sabotage was a silent no-op for a listen host (P10-3, fixed). Gates: compile **0**, Fast **112** exit 0 (36 s), All **154** exit 0 (41 s), no flakes; `OVT_Global.c` **292** lines; `git diff --stat Language/` = only `localization_Overthrow.st`; persistence untouched

**What's Next:**
- 🔴 **USER: the §6 21-step MP play-test** (`tools/launch-server.sh` + `tools/launch-game.sh`), plus opening `OVT_OverthrowGameMode.et`, `Character_Player.et` and `OVT_OverthrowController.et` in Workbench to confirm clean loads. Nothing else is owed by the agents.
- 🟡 Then: the docs/wiki pass that P4-4 made **required** (BUG-161 means item buying was free — a player-facing economy change).

**Blockers:**
- None. All scheduling dependencies discharged (PR #152 and #154 merged).

---

## Key Files

### The seam being replaced
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` — 2,001-line monolith, 55 RpcAsk / 4 RpcDo; **deleted in P10**
- `Scripts/Game/Global/OVT_Global.c` — locator; loses `GetServer()`, 6 controller getters, `LootBattlefield`, 2 warehouse helpers, ~470 utility lines
- `Scripts/Game/Entities/OVT_OverthrowController.c` — the per-player owned seam entity
- `Scripts/Game/GameMode/Managers/OVT_PlayerManagerComponent.c` — controller spawn/ownership lifecycle (P1 edits `SetupPlayer` / `AssignControllerOwnership` / `CheckDisconnectedPlayers`)

### New home (all in `Scripts/Game/Components/Controller/`)
- `OVT_ControllerComponent.c` — the generic accessor (P1, epic-level API; name is load-bearing)
- Ten new `OVT_<Domain>RequestComponent` files, P2–P8 (see implementation.md §3.3 roster)
- Pattern references: `OVT_ShopTransactionComponent.c` (validation order, ResolveOwningPlayerId, listen-server owner response), `OVT_ContainerTransferComponent.c` (progress base)

### Prefabs (USER wires in Workbench)
- `Prefabs/GameMode/OVT_OverthrowController.et` — gains the ten components (per-phase)
- `Prefabs/GameMode/OVT_OverthrowGameMode.et` + `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` — lose `OVT_PlayerCommsComponent` in P10

---

## Important Decisions

Plan-time decisions D1–D11 live in `implementation.md` §5 — not repeated here. Build-time decisions get logged below as they happen.

### P1-1: Controller cache gets a world-transition clear (2026-08-14)
`s_LocalController` is a static and outlives a world rebuild; `IsDeleted()` on a handle from a destroyed world is not safe validation. `OVT_PlayerManagerComponent.Init()` calls `OVT_Global.SetLocalController(null)` — a fresh player manager is the proof of a fresh session. D9's fallback untouched.

### P1-2: Cache write guarded on identity; map write is not
`RegisterControllerForPlayer` is keyed by playerId and self-corrects on wrong-owner delivery; the cache has no key, so a mis-cache would poison every local request. Guard: `localPlayerId <= 0 || localPlayerId == playerId` (unknown local id still caches; genuine mismatch is skipped).

### P1-3: Owner-response helper is a predicate, not a wrapper
`ShouldRespondLocally(int)` returns bool; the `Rpc()` call stays at each call site, because `Rpc()` is an untyped variadic prototype and wrapping it would hide the arity defect class (BUG-090).

### P1-4: T1.6 re-pointed only the two named duplicates
`ResolveOwningPlayerId()` actually exists in **five** components. `OVT_TravelRequestComponent`, `OVT_RespawnRequestComponent`, `OVT_TowerSabotageComponent` keep their private copies for now — changing a prefab-wired component's base class is play-test surface. Tracked in tasks.md Discovered Tasks; fold into P10 or a follow-up.

### P1-5: T1.5 trap verdicts
- **(a) real but latent:** `SetupPlayer`'s already-mapped branch tested the raw map; one stale entry would silently deny a player a controller forever. Fixed via `GetController(playerId)` (which `IsDeleted()`-revalidates).
- **(b) real:** raw `delete controller` skips replication removal (engine contract, `RplComponent.c:19-32` in the reference tree). Now `RplComponent.DeleteRplEntity(controller, false)` — first use in the tree.

### P2-1: The plan's "GetServer ≤ 52" ceiling is off by one — 53 is correct (2026-08-14)
P2's re-point list contains **7** call sites, not 8: `OVT_ShopContext` is counted `×2` under P4 *and* once under P2, but only one of its two `GetServer()` lines (`:1352` BuyVehicle) is a P2 line — `:1357` is P4's item buy. 60 − 7 = **53**. Verified against HEAD (`git grep` per file). Every later phase's ceiling inherits the same +1; treat P3 as ≤ 44, P4 as ≤ 32, etc., or re-derive.

### P2-2: `UpgradeVehicle`'s charge moved server-side, because it had to
`OVT_VehicleManagerComponent.UpgradeVehicle` does **not** charge. `OVT_ManageVehicleContext.Upgrade()` did, via `economy.TakeLocalPlayerMoney()` — a *second, independent* client→server request sent just before the upgrade request. Adding the plan's affordability check to the handler while leaving that in place would have made the two race: the debit lands first, then the upgrade is refused for insufficient funds. So the menu's debit line is deleted (its `LocalPlayerHasMoney` check stays as advisory UX per §3.4) and `economy.DoTakePlayerMoney()` now runs inside `RpcAsk_UpgradeVehicle`. This does not touch P4's `RpcAsk_TakePlayerMoney`, which keeps its other callers.

### P2-3: `RepairVehicle` gets no affordability check — repair is free
Neither the manager nor the menu ever charged for a repair. There is no price to check against, and inventing one would be a gameplay change (G6). It gets the ownership + proximity pair only.

### P2-4: Upgrade authorisation is a config lookup, not `IsValidResourceId`
`ResolveUpgradeCost()` finds the requested prefab in the `OVT_VehicleUpgrades` entry whose `m_pBasePrefab` matches **this vehicle**, and returns −1 when it is absent. That is both the price the menu displayed and a stronger gate than the plan's "valid resource id" — it also refuses fitting a truck's upgrade to a motorcycle.

### P2-5: All three re-pointed user actions are `HasLocalEffectOnlyScript() == true`
`OVT_LockVehicleAction`, `OVT_UnlockVehicleAction` and (via `SCR_CompartmentUserAction`) `SCR_GetInUserAction` run `PerformAction` **only on the performing player's machine**, so `OVT_ControllerComponent<T>.Get()` is always the right seam and no "other clients also run this" filtering is needed. It also means the legacy path was broken for a **listen-server host**: `GetServer()` returned the game-mode copy and `Rpc()` from the authority went nowhere, so the host could never lock their own car. The new `if(Replication.IsServer()) direct else Rpc` branch fixes that — the first user-visible improvement of the migration outside §3.6.

### P2-6: `PlayerMayUseVehicle()` hoisted to `OVT_ControllerRequestComponent`
It was `protected` on `OVT_ShopTransactionComponent` and the vehicle component needs the identical rule for upgrade/repair. One definition means the trunk-sell path and the upgrade path can never disagree about who owns a car. Note it is deliberately **weaker** than the lock/claim rule: an *unlocked* vehicle may be repaired by a passer-by, but never locked or claimed by one (BUG-087).

### P3-1: `IsRegisteredResource(ResourceName)` is the real API — the plan named it correctly (2026-08-14)
`OVT_EconomyManagerComponent.IsRegisteredResource(ResourceName)` (`:1687-1691`) exists and is the right
gate: it is a `m_aResourceIndex.Contains()` test, i.e. exactly "will `GetInventoryId(res)` return this
resource's own id rather than silently resolving to id 0". Note the *other* one, `IsValidResourceId(int)`,
is the integer-id gate the vehicle/shop seams use — warehouses are keyed by raw `ResourceName` **string**,
never by id, so they need the string form. Warehouse inventories are `map<string,int>` and are persisted
verbatim, and the take-to-vehicle path feeds the key straight into `TrySpawnPrefabToStorage`: an
unregistered string was therefore both a permanent junk row in the save and an arbitrary client-chosen
prefab spawn. That is the exploit this phase closed.

### P3-2: The warehouse checks are duplicated at the seam on purpose
`DoAddToWarehouse` / `DoTakeFromWarehouse` already test `count <= 0` and the id range. The handlers test
them **again** before delegating, because a rejection that happens inside the manager returns silently and
is indistinguishable from a dropped packet (Q9). At the seam it can be logged with a reason and a caller.
This is the same rule the vehicle component follows and it costs two comparisons.

### P3-3: `TakeFromWarehouseToVehicle` gets a proximity + `PlayerMayUseVehicle` pair, not a quantity bound
The plan asked for "caller near the vehicle, caller may use the vehicle" and both are in. Quantity is
deliberately **not** bounded beyond stock: the manager clamps `qty` to what the warehouse holds and then
pays only for what actually fitted in the cargo (spawn-then-debit), so a huge `qty` cannot mint items or
delete stock into nowhere — unlike the import path, where BUG-033's bound exists because every unit is
spawned unconditionally. Adding an arbitrary cap here would refuse legitimate "take 500 rounds" requests
the menu already offers.

### P3-4: The two moved `OVT_Global` helpers gained null guards, and that is not a behaviour change
Both did `RplComponent.Cast(Replication.FindItem(from)).GetEntity()` with no guard on the cast — a VME
waiting for the first RplId that no longer resolves (a vehicle deleted between the click and the packet).
`TakeFromWarehouseToVehicle` also indexed `m_aWarehouses[warehouseId]` on the caller's promise that the
range had been checked. Both now guard. The only behavioural difference is "returns instead of throwing",
which is what every other rejection in this seam already does.

### P3-5: `RpcAsk_SetBuildingHome` is gone; real estate has been told exactly what to re-add
Deleted per §3.7/D6 (zero callers, and as an endpoint it let any client set any player's home to any
replicated entity on the map). `docs/features/economy/real-estate/context.md` now carries a disposition
note under Gotchas naming the file, the component and the three checks a validated re-add needs
(`ResolveOwningPlayerId`, server-side/position-checked building, `IsOwner`). `TeleportHome` is a separate
zero-caller method on the manager and was **not** touched — it is not a network endpoint.

### P4-1: The P4 `GetServer()` ceiling is 33 grep lines, not 32 — the P2-1 miscount lands twice (2026-08-14)
P2-1 found that `OVT_ShopContext` is counted `×2` under P4 *and* once under P2, and concluded "every
later phase's ceiling inherits the same +1". It inherits **+2** at P4 specifically, because the same
double-count also inflates **P4's own row**: P4 owns exactly **one** `OVT_ShopContext` line (`:1362`,
the item buy), not two. P4's real re-point list is **10** lines — ShopContext ×1, ResistanceMenuContext
×3, SellDrugsAction ×1, CharacterSheetContext ×1, ShopComponent ×2, EconomyManagerComponent ×2 — so
43 − 10 = **33**, and the plan's 31 (+1 = 32) is unreachable by construction, not by an omission.
Also worth knowing before the later ceilings are judged: **two of the 33 grep lines are not calls** —
`OVT_MainMenuContext.c:14` is a doc comment naming the API, and `OVT_UndeployFOBAction_New.c:11` is a
commented-out line in the file P5 deletes. Live call sites are **31**. Recommend re-deriving P5–P8
ceilings from `git grep` per file rather than from the plan's table.

### P4-2: §3.6(a) VERDICT — item buying was free on live, and worse than the plan expected
Read the pre-fix path end to end. `RpcAsk_Buy` is an `RplRcver.Server` handler, so it runs on the
authority, and it paid with `Rpc(RpcAsk_TakePlayerMoney, ...)` / `Rpc(RpcAsk_TakeFromInventory, ...)` —
`Rpc()` to `RplRcver.Server` handlers, marshalled *by* the authority, i.e. delivered to nobody
(BUG-045/052/088 family). The spawn/insert loop immediately above them ran normally. So:
- **Remote client on a dedicated server: items delivered, balance untouched, stock untouched.** An
  unbounded free-item endpoint, gated only by a client-side `PlayerHasMoney` that never stops passing
  because nothing is ever taken. Combined with selling (which *does* credit correctly), a money printer.
- **Listen host / SP: the Buy button did nothing at all.** One level up, `OVT_ShopContext.Buy()` reached
  the monolith through `OVT_Global.GetServer()`, which on a server returns the **game-mode** copy, and
  then sent an unconditional `Rpc(RpcAsk_Buy, ...)` — again an `RplRcver.Server` request from the
  authority, so the handler never ran. Same class as P2-5.
- `RpcAsk_BuyVehicle` called the identical two handlers **directly**: somebody fixed vehicles and left
  items behind, which is why vehicle buying always charged.
Filed as **`docs/bugs/BUG-161.md`** (open, high, code-derived; runtime confirmation is §6 step 2).

### P4-3: §3.6(b) VERDICT — all three notifications were dropped packets, for the same reason
`SendNotification()` is an unconditional `Rpc(RpcAsk_SendNotification, ...)`, and its only three callers
(`PlayerDonated`, `PlayerSentFunds`, `PlayerSentMoney`) were inside server-side handlers. Confirmed the
same way as (a). All three now call `OVT_Global.GetNotify().SendTextNotification(...)` directly. Filed
as **`docs/bugs/BUG-162.md`** (open, low). The `SendNotification` / `RpcAsk_SendNotification` pair is now
**caller-less** and is left on the monolith for P8 to delete per the plan's phase split; it carries a
loud "dead, do not give it callers" doc comment in the meantime.

### P4-4: The §3.6(a) verdict makes the P10 docs/wiki pass REQUIRED, not optional
implementation.md:709 asked P4 to decide this. Decision: **required.** Items that were free now cost
money and shops can now sell out — that is a player-visible economy change, which is exactly the
condition the plan attached to re-instating `help-docs-sync`. Only §3.6(a) triggers it; §3.6(b) is
purely additive (a message that never appeared now appears) and needs no doc change of its own.

### P4-5: `RpcAsk_SellDrugs` keeps its one-item-per-action shape; NO `qty` parameter
The phase brief named the signature `RpcAsk_SellDrugs(int qty, RplId dealerId)`. That `int` is the
monolith's `int playerId` — the identity parameter this migration deletes (G3/D3) — not a quantity: the
handler has always sold exactly one stack and `break`s after the first `DrugsWeed_01` match, and the
hold action's repeat cadence is balanced around that. Adding a real quantity would be a gameplay change,
which G6 forbids. Shipped as `RpcAsk_SellDrugs(RplId dealerId)`, matching the plan's §4 row ("carry
DEALER_MAX_DISTANCE 10 m + the `DrugsWeed_01` filter" — no quantity mentioned).

### P4-6: `AddToInventory` / `TakeFromInventory` became real methods, so the two open-coded copies collapse
§3.7 asked for the RPCs to become server-side methods on `OVT_ShopComponent`. Once they were,
`OVT_ShopTransactionComponent.RestockShop` and `OVT_VehicleRequestComponent.TakeFromShopStock` — both of
which open-coded the `m_aInventory` mutation *with a comment explaining that the shop's own method was
unusable from server code* — now delegate to them. That is a strict reduction: three definitions of
"mutate stock and stream the row" become one, and P4's buy path uses the same one.

### P4-7: `RpcAsk_SetResistanceTax` loses the "senderId == -1 means server-initiated" escape hatch
The monolith allowed an unresolved sender through as trusted, because its game-mode copy carried
server-side calls. A controller component only ever exists on a player's controller, so an unresolvable
caller is now a rejection. Server-side code that wants to set the tax calls
`OVT_EconomyManagerComponent.DoSetResistanceTax()` directly, which is what the `Replication.IsServer()`
branch of `SetResistanceTax()` already does.

### P5-1: The GetServer ceiling re-derived from HEAD is 18 grep lines / 17 live calls (2026-08-14)
P4-1 said to stop trusting the plan's table and re-derive per phase, so: HEAD carried **33** grep lines /
**31** live calls. P5's re-point list is **14** live lines — PlaceContext ×2 (`:551`, `:796`), BuildContext
×2 (`:545`, `:764`), ResistanceMenuContext `:208`, BaseMenuContext `:101`, FOBMenuContext ×2 (`:89`,
`:91`), CampMenuContext ×2 (`:69`, `:85`), ConvertSupporterAction `:10`, DeployFOBAction `:7`,
UndeployFOBAction `:7`, SetPriorityFOBAction `:7` — plus **1** non-call line inside the deleted
`OVT_UndeployFOBAction_New.c`. So 33 − 15 = **18** grep lines and 31 − 14 = **17** live calls, and the
remaining single non-call is still `OVT_MainMenuContext.c:14`'s doc comment. The plan's ≤16 is unreachable
for the same reason P4's ≤31 was: it is downstream of the P2-1 double-count, not of a missed re-point.
Measured after the phase: **18**, and zero resistance/FOB-shaped calls remain anywhere (the survivors are
P6 recruits ×4, P7 loadouts/possession ×6, P8 jobs/campaign ×6, and the doc comment).

### P5-2: `RemovePlacedItem` and `AddGarrison` were already validated manager-side — so the seam did NOT duplicate them
The phase brief asked to confirm this rather than assume it, and the answer changed what got written:
- **`OVT_ResistanceFactionManager.RemovePlacedItem` (`:863-891`) DOES check ownership.** It resolves the
  caller's persistent id, reads the recorded owner off the `OVT_PlaceableComponent`/`OVT_BuildableComponent`
  and refuses unless it matches or the caller is an officer. Nothing was added at the seam; the handler is
  four lines and passes the *resolved* caller, which is exactly the thing the deleted `playerId` argument
  used to get wrong.
- **`AddGarrison` (`:924-957`) validates base index, prefab index (via `GetGarrisonPrefab`), the town's
  supporter stock (BUG-064) and affordability (via `ChargeForGarrison`, `:908-922`).** The affordability
  rule is **(baseRecruitCost + 300) × the group's unit count**, and the unit count is only knowable *after*
  `SpawnGarrison()` has produced the group — so the manager is the only place that check can physically be
  made, and re-deriving it at the seam is impossible, not merely redundant. The seam therefore adds what
  the manager cannot see: **who** asked, and whether they are standing at the base.
The two index-range re-tests that *were* added exist for the P3-2 reason only (a manager rejection returns
silently and is indistinguishable from a dropped packet; at the seam it gets a caller, a reason and a
`LogLevel.WARNING` — Q9).

### P5-3: The placement radii are the manager's own numbers, copied not invented
Place = **50 m**, build = **250 m**, taken verbatim from `PlaceItem` (`:687`) and `BuildItem` (`:801`).
There is no separate "placement range the client UI uses" to bind to — `OVT_PlaceContext` clamps a camera
trace and `OVT_BuildContext` a free-flying build camera, neither of which exposes a number the server could
share. Copying the manager's radii means the seam can never refuse a request the manager would have
accepted (which would be a gameplay change, G6) while still naming the rejection. The two differ because
the build camera detaches much further from the player than the place trace does; that asymmetry is
pre-existing and was not touched.

### P5-4: `SetCampPrivacy`/`DeleteCamp` were the worst holes in the phase, and delete was worse than privacy
Both were bare forwards with **zero** validation of any kind. Privacy meant any client could flip any camp
public from anywhere. Delete was materially worse: `RemoveCamp(RplId, vector)` **deletes whatever entity the
RplId names**, and matched the record by position separately — so an arbitrary RplId paired with any valid
camp position was an **arbitrary-entity delete** for any modified client. The handler therefore checks that
the named entity is actually at the resolved camp (`REGISTRY_MATCH_DISTANCE`) *before* the ownership and
proximity gates. Ownership is "owner **or** officer": `OVT_ManageCampAction.CanBeShownScript` is owner-only
client-side, but refusing an officer server-side would make an abandoned camp permanently undeletable.
One incidental correctness win: both handlers now pass **`camp.location`** to the manager rather than the
client's `pos`. `SetCampPrivacy`/`RemoveCamp` match records by exact vector equality, so a client float that
was merely *near* the record used to broadcast a change that updated nothing.

### P5-5: `IsGarrisonPrefabIndexValid` is duplicated in both new components on purpose
Four lines of config lookup with no policy in it. Hoisting it to `OVT_ControllerRequestComponent` would put
domain knowledge ("what groups can this faction field") on a base whose stated contract is that it has
none. The P2-6 hoist of `PlayerMayUseVehicle` was justified by two *policies* that could disagree; this one
cannot disagree with itself.

### P6-1: The recruit ceiling lands at 14 grep lines / 13 live calls, not the plan's 12 (2026-08-14)
Same arithmetic as P2-1/P4-1, re-derived from HEAD rather than taken from the table: HEAD carried **18**
grep lines / **17** live calls. P6's re-point list is exactly **4** live lines — `OVT_RecruitCivilianAction:35`,
`OVT_RecruitFromTentAction:55`, `OVT_RecruitsContext:384` (dismiss) and `OVT_RecruitsContext:452` (rename) —
so 18 − 4 = **14** lines and 17 − 4 = **13** live calls. The one remaining non-call is still
`OVT_MainMenuContext.c:14`'s doc comment. Survivors: P7 loadouts/possession ×6, P8 jobs/campaign ×6, the
comment, and the `OpenInventoryCommand`/`PlayerWantedComponent` pair that P7/P8 own.

### P6-2: Dismiss ownership is owner-only, with NO officer override — deliberately unlike camps
P5-4 gave `DeleteCamp` an "owner **or** officer" rule because refusing an officer would make an abandoned
camp permanently undeletable world clutter. A recruit has no equivalent failure mode: it despawns on its
own once its owner has been offline for `OFFLINE_DESPAWN_TIME`, so there is nothing for an officer to clean
up, and a recruit is the one asset a player levels up individually (`AddRecruitXP`). An officer able to
dismiss another player's veteran recruit would be a *new* power, which G6 forbids. Rename follows the same
rule for the same reason.

### P6-3: Rename's "senderId > 0" branch became unconditional — the escape hatch is gone
The monolith let an **unresolved** sender through as trusted (`if (senderId > 0) { ...check... }`), because
its game-mode copy fielded server-side calls with no player behind them. A controller component only ever
exists on a player's controller, so an unresolvable caller is now a rejection. Identical to the
simplification P4-7 made for `SetResistanceTax`; server-side code that wants to rename a recruit calls
`OVT_RecruitManagerComponent.RenameRecruit()` directly, which is already what the manager exposes.

### P6-4: The handlers charge via `DoTakePlayerMoney`, not `TakePlayerMoney`
The monolith called `economy.TakePlayerMoney(playerId, cost)`, which branches on `Replication.IsServer()`
and — since P4 — falls through to `OVT_EconomyRequestComponent` on a client. Inside a handler that has
already returned on `!Replication.IsServer()`, that branch is dead weight whose client half would now be a
seam calling a seam. The direct `DoTakePlayerMoney()` is what the branch resolves to on a server, so this
is not a behaviour change; it is the same call the vehicle and shop seams make (P2-2).

### P6-5: `MarkAsPerformed()` moved BELOW the accessor null-guard in `OVT_RecruitCivilianAction`
The action used to mark itself performed and *then* send. If the controller seam is unavailable (dedicated
client before owner assignment), marking first would consume the hold action and produce nothing at all —
the dead-button symptom BUG-102 was filed for. Guard first, mark second: an unavailable seam now leaves the
action repeatable. `OVT_RecruitFromTentAction` has no equivalent latch, but its success hint likewise moved
below the guard.

### P6-6: The rename menu keeps its optimistic local rename
`OVT_RecruitsContext.OnRenameConfirmed` renames the local replica immediately after asking the server, and
that was left in place: the server's `BroadcastRecruitUpdate` is the confirmation, and the local write only
shortens the visible latency. It is now unreachable when the seam is null (early return), which is the
correct outcome — previously a null `GetServer()` would have VME'd on the unguarded `.RenameRecruit(...)`
call at `:452`, one of the few genuinely unguarded `GetServer()` dereferences left in the tree.

### P7-1: The possess request had to MOVE MACHINES, and that is the whole difficulty of the phase (2026-08-14)
`OVT_OpenInventoryCommand.Execute()` is not client code. `SCR_CommandingManagerComponent.RPC_DoExecuteCommand`
is **`RplRcver.Broadcast`** and calls `Execute()` on every machine with `isClient = rplComp.IsProxy()`; the
command's first line was `if (isClient) return true;`, so the entire body ran **only on the authority**. That
is incompatible with a controller seam in two independent ways: `OVT_ControllerComponent<T>.Get()` resolves the
**local** player's controller and is null forever on a dedicated server, and `ResolveOwningPlayerId()` answers
"who asked" from the controller entity the request **arrived on** — which is only the right answer if the
requester's own machine sent it.
The gate is therefore now **`playerID != SCR_PlayerController.GetLocalPlayerId()` → return**, replacing the
`isClient` early-return. That predicate is true on exactly one machine in every topology: on a dedicated server
the commander's client (the server's own local player id is 0, so its authority pass never fires); on a listen
host the host itself, on the authority pass (its client pass never runs, because the host is the master). It is
one condition, not a branch per topology.
Two consequences worth stating plainly:
- **This command could not have worked on a dedicated server before.** The old path reached the monolith's
  game-mode copy and sent `Rpc(RpcAsk_…)` to an `RplRcver.Server` handler *from the authority* — the pattern
  P2-5/BUG-161/BUG-162 are about. BUG-147's fix was verified in play on a host, where the same call is at worst
  a local loopback. The new path does not depend on the answer to that question at all: host → direct call,
  client → a genuine client→server RPC.
- **`Execute()`'s return value changes on two machines** (it drives `PlayCommanderSound` and the AI response in
  `RPC_DoExecuteCommand`). A dedicated server now returns `true` unconditionally instead of the result of a body
  it no longer runs; the commanding **client** now returns the real result instead of an unconditional `true`,
  so no commander sound plays when the request could not be sent. Both are cosmetic and both are strictly more
  honest than before.

### P7-2: Possession ownership is owner-only recruits + 20 m, and proximity is measured from `GetMainEntity()`
The endpoint had **zero** validation: `RpcAsk_SetPossessedEntityAndOpenInventory(int playerId, RplId)` took a
claimed id and any replicated entity and called `SetPossessedEntity()` on the pair. That is not an "inventory
peek" hole — possession is *control*, and nothing obliges the client to ever open (or close) the inventory, so
it was "any modified client may drive any replicated character". It now requires: the target resolves to an
`SCR_ChimeraCharacter`; `OVT_RecruitData.GetRecruitDataFromEntity(target).m_sOwnerPersistentId` equals the
**caller's** persistent id (owner-only, no officer override — same reasoning as P6-2, a recruit is private
property); the recruit is `ECharacterLifeState.ALIVE` (carried up from the command's client-side gate); and the
caller is within **`POSSESS_MAX_DISTANCE` 20 m**.
20 m is the number every other "manage this recruit of mine" interaction in the tree already uses
(`RECRUIT_MAX_DISTANCE`, `LOADOUT_BOX_MAX_DISTANCE`), and there is no existing client-side range to bind to —
the commanding menu's own gate is a cursor trace with an engine-defined length (`CameraBase.GetCursorTarget()`
is a proto). Since there was no server gate at all before, any number is a tightening; if F11 ever shows a
legitimate open being refused, **raise the constant, do not remove the check**.
Proximity is measured against `SCR_PlayerController.GetMainEntity()`, not the controlled entity, because a
caller who is *already* possessing something controls that thing — the controlled entity would be the recruit
they are currently wearing and the distance would be measured from the wrong body (`SCR_PlayerController.c:446`
is explicit that `GetMainEntity()` is the pre-possession body).

### P7-3: The BUG-147 lifecycle was moved, not rewritten — including the part that looks like a bug
All three pieces are on `OVT_PossessionRequestComponent` verbatim: `m_PossessedInventoryManager`; the
**one-shot** `m_OnInventoryOpenInvoker` subscription (`Insert` on open, `Remove` inside the close handler); and
the **300 ms `CallLater`** before `SCR_PlayerController.RequestRestorePossession()`. The deferral is the fix,
not a smell — the inventory close event fires at menu-teardown *start*, and unpossessing under a live menu is
what pins the recruit's facing forever; two earlier iterations failed in play for exactly that reason.
Two things were **deliberately not** "improved", because the brief said carry verbatim and both would be
behaviour changes dressed as tidying: (a) `Insert()` is not preceded by a defensive `Remove()`, even though that
is the project's idiom elsewhere (`GetOnSaveFinished()`); (b) the pending `CallLater` is not cancelled in a
destructor. Both are worth revisiting **after** F11 confirms the moved lifecycle still behaves — not before.
One thing did improve for free: the lifecycle now lives on the **controller**, which outlives the character it
used to sit on, so a death or respawn between "inventory closed" and the 300 ms restore can no longer take the
subscriber with it.

### P7-4: `RpcDo_OpenInventory`'s Broadcast→Owner change deletes a filter AND a server-side misfire
The monolith broadcast a per-player UI command to every client in the session and threw it away again at the
top of the handler on all but one (`localPlayerId != playerId`) — correct only because of a line a future edit
could drop. On the controller the entity is genuinely owned by one player, so Owner routing is real and the
filter and its `playerControllerId` argument are both gone.
The `RpcDo_OpenInventory(...)` direct call that sat *next to* the broadcast (the "belt-and-braces" pair) is not
merely redundant on a dedicated server, it is wrong: it ran the **client half** — including `OpenInventory()` —
on the server. It is now the `ShouldRespondLocally(playerId)` branch, so it fires on a listen host (where the
Owner RPC would not be delivered to the host itself) and nowhere else.

### P7-5: The GetServer ceiling lands at 8 grep lines / 7 live calls, and the last 8 are all P8's
Re-derived from HEAD per P4-1's instruction rather than taken from the plan's ≤5: HEAD carried **14** grep lines
/ **13** live calls. P7's re-point list is exactly **6** live lines — `OVT_OpenInventoryCommand:76`,
`OVT_LoadoutsContext:311/:337/:640`, `OVT_SaveLoadoutAction:87`, `OVT_SaveOfficerLoadoutAction:103` — so
14 − 6 = **8** lines and 13 − 6 = **7** live calls. What remains is P8's whole list and nothing else: jobs ×2,
campaign ×4 (`OVT_PlayerWantedComponent`, `OVT_OverthrowGameMode` DiagMenu, `OVT_CaptureBaseAction`,
`OVT_DeliverMedicalSuppliesAction`), `OVT_MainMenuContext:295` (save), plus `OVT_MainMenuContext.c:14`'s doc
comment. **P8's "GetServer = 0" is therefore reachable exactly on the arithmetic**, unlike every intermediate
ceiling since P2-1.
Note for P8/P10: a doc comment that spells `OVT_Global.GetServer()` literally counts in this grep. One was
written during this phase and reworded for that reason — prose about the seam should name it descriptively.

### P7-6: Loadout name bounding is on SAVE only, on purpose
`OVT_SaveLoadoutAction` and `OVT_SaveOfficerLoadoutAction` both enforce 1-32 client-side, and
`OVT_LoadoutManagerComponent` enforces nothing at all, so an unbounded name from a modified client became an
unbounded **persisted map key**. `LOADOUT_NAME_MAX_LENGTH = 32` matches the dialogs exactly, so it can never
refuse a name the UI would have offered. Apply and delete take names that must already **exist** to do anything,
so bounding them buys nothing and would risk orphaning a record written by an earlier build — they get the
non-empty check only. The officer gate on templates is likewise **not** duplicated at the seam:
`SaveOfficerTemplate()` re-derives it and logs its own refusal (same call as P4's BuySkill reasoning).

### P8-1: Decline iterates a SNAPSHOT of the matching jobs, because the manager mutates the list it was reading (2026-08-14)
`OVT_JobManagerComponent.DeclineJob()` removes the record from `m_aJobs` for a **private** job, and the
monolith called it from inside `foreach(OVT_Job job : jobs.m_aJobs)` — mutating the array under its own
iterator. It survived only because a private job matches exactly once and the removal happens on the last
useful pass. The migrated handler collects matches first and declines afterwards, which is behaviour-neutral
for one match and correct for any number. Accept did not need it (`AcceptJob` only writes fields).
Also added at the seam: **`jobIndex` is bounds-checked**. `GetConfig(int)` is a bare `m_aJobConfigs[index]`,
so an out-of-range index from a modified client was an array-out-of-bounds on the *server*, not a rejected
request — the cheapest denial-of-service in the phase.

### P8-2: `InstantCaptureBase` went to the GAME MODE, not to the campaign component
The plan allowed either home and asked for whichever keeps the DiagMenu call a direct server-side call. The
game mode wins on three counts: its only caller is six lines above it in `EOnFrame`, sitting with the other
DiagMenu cheats (add money, max support, flip town) which all call managers directly; putting it on a
*controller* component would make a debug cheat depend on the controller seam being spawned and owned, which
is precisely the thing that is null early in a session; and a component whose every other member is an RPC
handler would then carry one method that must never become one. Both the method **and its call site** are
`#ifdef WORKBENCH` — the call site had to be guarded too, since the method no longer exists in release.
The network endpoint is gone outright: it was `RplRcver.Server` gated only by a client-side `DiagMenu.GetValue`,
i.e. any modified client could flip any base, and the in-handler `#ifdef` was all that kept that out of retail.

### P8-3: The save invoker contract survived intact, and the host path it rides was BROKEN before
Everything BUG-006 depends on is preserved verbatim on the new component: `GetOnSaveResult()` exists with the
same signature, `OVT_MainMenuContext` still subscribes **before** calling (`Remove`-then-`Insert`), still caches
the component it subscribed to and still one-shot-removes itself in `OnSaveResult`, and the server still does
`Remove`-then-`Insert` on `GetOnSaveFinished()` and answers from the completion invoker rather than on send.
Two things are new. (a) The reply is routed through `ShouldRespondLocally()`, because an `RplRcver.Owner` RPC
from a listen host to itself is never delivered — and the request path had the mirror-image defect: the
monolith's `RequestSave()` was an unconditional `Rpc()` to an `RplRcver.Server` handler, so **on a listen host
"Save" sent a packet to nobody and the menu waited forever for a save that was never started** (P2-5 class).
(b) The handler stores the resolved caller in `m_iPendingSavePlayerId` rather than relying on "this component
instance belongs to the requester" — it still does, but the completion invoker fires later and carries no id,
and an implicit assumption that a future edit could break is not a contract.
The officer gate added server-side **replies false** rather than returning silently: the menu shows nothing at
all until the invoker fires, so a silent rejection would be a dead button (Q9/BUG-102 class).

### P8-4: `RpcAsk_DeliverMedicalSupplies` was an "empty any vehicle from anywhere" endpoint
It took an `RplId` and no caller checks at all: the named vehicle's entire drug-shop-sellable cargo was
deleted and converted into town support/stability modifiers, with no proximity and no ownership test. Any
modified client could therefore empty another player's **locked** truck from across the map. It now requires a
resolvable caller, a live controlled entity, `DELIVERY_MAX_DISTANCE` 30 m and `PlayerMayUseVehicle` (the P2-6
shared rule — deliberately the weaker "unlocked is fair game" one, since delivering somebody's abandoned
supplies is not theft). 30 m is generous on purpose: the deliverer may be sitting in the vehicle, and a
vehicle's origin is not its cargo door. The **town range** check in `OVT_DeliverMedicalSuppliesAction` was
deliberately **not** duplicated server-side — the server picks the nearest town itself, and adding a radius the
client's copy of `m_iCityRange`/`m_iTownRange` might disagree with would refuse legitimate deliveries (G6).

### P8-5: `GetServer()` reached exactly 0, and the last line standing was a doc comment
P7-5 predicted this lands on the arithmetic, and it did: 8 grep lines / 7 live calls at HEAD, and P8's
re-point list is exactly those 7 — `OVT_JobManagerComponent` ×2, `OVT_PlayerWantedComponent`,
`OVT_OverthrowGameMode` (DiagMenu), `OVT_CaptureBaseAction`, `OVT_DeliverMedicalSuppliesAction`,
`OVT_MainMenuContext:295` — plus `OVT_MainMenuContext.c:14`'s doc comment, which was reworded rather than
deleted because the *cache* it explains still exists (the reason changed: the seam can be re-assigned
mid-session, it is no longer "the accessor VMEs on a dead player"). **`grep -rn "OVT_Global.GetServer()"
Scripts/` is now 0 lines, not merely 0 calls.** Two other greps were made honest the same way, per P7-5's
rule that prose naming an API counts: the monolith's own shell comment says "request handlers" rather than
`RpcAsk_`, and `OVT_EconomyRequestComponent`'s §3.6(b) note no longer spells `RpcAsk_SendNotification` — so
both `grep -c "RpcAsk_\|RpcDo_"` on the monolith and `grep -rn "RpcAsk_SendNotification" Scripts/` are 0.

### P8-6: What is LEFT in the monolith, and why it is not deleted this phase
`OVT_PlayerCommsComponent.c` is now 23 lines: the `OVT_PlayerCommsComponentClass` declaration, the empty
`OVT_PlayerCommsComponent : OVT_Component` body, and a doc comment explaining what it was and when it goes.
Nothing references it from script any more (`OVT_Global.GetServer()` still *returns* it, but nothing calls
that). It is kept because **two prefabs still list the component** — `Prefabs/GameMode/OVT_OverthrowGameMode.et:150`
and `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et:15` — and deleting a script a prefab names
produces a load-time warning on both. The strip and the deletion are one change, and that change is T10.1.

### P1-6: Plan's T1.4 call-site counts were mentions, not calls
`GetUI()` had **zero** live call sites; `GetDifficulty()` had 3, of which only `OVT_OccupyingFactionManager.c:1248` was a now-reachable null deref — it fails closed (no threat reduction that tick) rather than VME.

### P9-1: Every plan-listed call count was re-derived, and five were wrong (2026-08-14)
Counted as `OVT_Global.<helper>` **occurrences** (not `grep -c` lines, which under-count two calls on one
line) over `Scripts/**/*.c`, excluding `OVT_Global.c` itself. Deltas from the plan's list: `SpawnEntityPrefab`
**35** not 34; `GetPrefabName` **33** not 30 (30 *lines*, 33 calls — the two counting methods disagree on this
one, which is exactly the trap G7 warns about); `IsOceanAtPosition` **2** not 3, and both are in one file
(`OVT_DeploymentManager.c`) — the third was the *internal* call inside `GetRandomNonOceanPositionNear`, which
moved with the body and is not a call site; `ResetAIAimState` **1** live call + **1** doc-comment mention, not
2 (the mention is in `OVT_PossessionRequestComponent.c:35` and was reworded, not left dangling);
`RandomizeCivilianClothes` **2** not 1, and one of them is a **method reference**, not a call —
`OVT_TownController.c:169` does `aigroup.GetOnAgentAdded().Insert(OVT_Global.RandomizeCivilianClothes)`, so the
qualifier change had to keep the delegate shape (`OVT_LoadoutUtils.RandomizeCivilianClothes`) rather than gain
parentheses. Both zero-caller helpers were confirmed zero **repo-wide, unqualified** (`NearestPlayer`'s only
other grep hit is `OVT_DeploymentManager.GetNearestPlayerDistance`, an unrelated private method) and deleted.

### P9-2: The three utilities live in `Scripts/Game/Utilities/`, the folder that already existed
`OVT_ItemLimitChecker.c` was already there and is the same shape (a stateless static helper class), so no new
`Util/` folder was invented. All three are `class OVT_X : Managed` — matching `OVT_Global` itself — so nothing
about how they are called changes beyond the qualifier.

### P9-3: This was a move, and it is provable — exactly two body lines differ
The three new files were produced by extracting the original line ranges byte-for-byte, and the result diffs
against the pre-change `OVT_Global.c` in **two** places only, both forced re-qualifications of a helper calling
a sibling that moved with it: `GetRandomNonOceanPositionNear` → `OVT_WorldUtils.IsOceanAtPosition`, and
`ApplyCivilianLoadout` → `OVT_LoadoutUtils.SpawnDefaultCharacterItem`. `ApplyCivilianLoadout`'s
`OVT_Global.GetConfig()` was deliberately **left alone** — that is a locator call, not a moved sibling. What
stayed in `OVT_Global.c` was verified the same way: the surviving text is bit-identical to the corresponding
original lines. The private statics `m_Bodies`/`FilterDeadBodiesAndWeapons` and `s_SpawnPointSearchResults`
moved with their owners (all three had zero external references, so none needed a forwarder).

### P9-4: T9.6 was a no-op, and that is a finding rather than a skipped task
Every `OVT_Global.*` utility use in the test spine is `SpawnEntityPrefab` (×12) or `GetPrefabName` (×4) —
i.e. entirely inside the forwarded three. The tests needed no edit, which is a small piece of evidence that
the forwarder set was chosen on real traffic: the highest-volume helpers are the ones the spine also reaches
for. Nothing outside `Scripts/` references any of these (checked `.conf`/`.et`/`.layout`).

### P9-5: `SpawnDefaultCharacterItem` was moved despite zero external callers, unlike the two deletions
The plan listed it at 1 site; the real count of `OVT_Global.SpawnDefaultCharacterItem` outside the file is
**0** — its only caller is `ApplyCivilianLoadout`, which moved with it. It was still moved rather than deleted
because it is a live dependency of a live helper, which is the opposite of `NearestPlayer` and
`RandomizeCivilianGroupClothes` (nothing calls those, transitively or otherwise). Worth recording for whoever
next greps this name: `OVT_SpawnLogic.c:1321` and `OVT_PersistentRespawnLogic.c:179` each carry their **own
private copy** of a method with the same name and signature, and those copies are untouched — a future
de-duplication is a real opportunity, but it is not a utility split.

### P10-1: The §6 getter-count expectation of **17** is off by one — **16** is correct, and the probe was always counting `GetServer()` (2026-08-14)
§6 item 6's probe is `grep -c "static OVT_.*Component Get" Scripts/Game/Global/OVT_Global.c`. `GetServer()`'s
return type was `OVT_PlayerCommsComponent`, which matches `OVT_.*Component` — so the probe has been counting
the thing it was meant to prove absent all along. The trail: **23** at HEAD, **17** after P1 deleted the six
per-domain controller getters, **16** now that `GetServer()` itself is gone. Same class as P2-1/P4-1/P5-1/P6-1/
P7-5 — plan-time arithmetic that did not model its own probe.

Worth recording separately, because the "17 manager forwarders" figure is repeated throughout the plan: **that
probe is not a manager-forwarder count in either direction.** It counts `GetUI()` (a client-seam accessor, not
a manager) and it misses six accessors whose return type does not end in `Component` — `GetOverthrow`,
`GetOccupyingFaction`, `GetResistanceFaction`, `GetFactions`, `GetDifficulty`, `GetController`. The real
surviving locator surface is **19 manager/config accessors** + `GetDifficulty` + the controller trio
(`s_LocalController`/`SetLocalController`/`GetController`) + `GetUI` + `GetPlayerUID`/`GetLocalPersistentId` +
the three utility forwarders + `ShowHint`. I2's *intent* (zero per-domain controller getters, 302→292 lines,
forwarders byte-identical) is met; only its arithmetic was wrong.

### P10-2: Both prefab blocks were own-declarations, not deltas — the strip is exactly two lines each
`Prefabs/GameMode/OVT_OverthrowGameMode.et` is a **root** prefab (`OVT_OverthrowGameMode {`, no `: "{GUID}…"`
parent), so its `OVT_PlayerCommsComponent "{5D7ACE1228D77F40}" {}` block at `:150-151` was a plain component
entry, not an override of an inherited one. `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` **is**
a delta (`SCR_ChimeraCharacter : "{7A9EE19AB67B298B}…Character_FIA_base.et"`), but the base carries no
`OVT_PlayerCommsComponent` — the block at `:15-16` was an *addition*, so removing it removes the component
rather than resurrecting a base value. Checked before editing precisely because same-GUID overrides are deltas
in this project and a delta-shaped removal can mean the opposite of what it looks like. Both diffs are `-2`
lines and touch nothing else; `Prefabs/Groups/INDFOR/Group_CIV_Recruit.et` references `Character_Player.et` as a
*member prefab*, not by inheritance, so nothing downstream inherits the block either.

### P10-3: The arity audit found ONE defect, and it is a ROUTING defect in a component this feature never migrated
All **75** marshalled `Rpc(Rpc…)` calls across the folder have the right argument count **and order** against
their handlers, and every one has a direct-call twin passing the identical arguments in the identical order.
Zero arity defects. But the same sweep caught `OVT_TowerSabotageComponent.RequestSabotage()` (`:19-22`) doing an
**unconditional** `Rpc(RpcAsk_SabotageTower, towerPos)` to an `RplRcver.Server` handler with no
`Replication.IsServer()` branch — the one component of eighteen with no twin. An `RplRcver.Server` RPC
marshalled *by* the authority is delivered to nobody, so **radio-tower sabotage did nothing at all for a
listen-server host**: no disable, no wanted level, no error line. Exactly the P2-5 class this feature fixed in
ten other components.

Fixed here rather than filed, because the fix is the four-line shape that seventeen sibling components in the
same folder already use, and leaving a known-dead host path in the folder the audit was mandated over would be
worse than the scope creep. **It is a behaviour change and belongs in the play-test** — see the session note.
`OVT_TowerSabotageComponent` predates this feature (it is one of the seven that were already on the controller),
which is precisely why the defect survived: P2-P8 audited what they moved, and nobody had audited what was
already there.

### P10-4: The sweep greps forced **63** doc-comment rewordings, not the plan's 7, and the standard phrase is "the legacy comms monolith"
T10.2's acceptance is `grep -rn "OVT_PlayerCommsComponent\|GetServer()" Scripts/ Prefabs/` → **nothing**, which
means every *mention* has to go, not just every call. The plan estimated 7 doc-comment references in the
controller components; the real count under `Scripts/` was **63 lines across 33 files** — 12 component headers,
5 mid-file citations, 20 test-suite preambles and failure messages, `Scripts/Game/Components/README.md`, the
game mode, the resistance manager, the shop component, a map location panel, and `OVT_TEST_InitSuite`'s two
`GetServer()` mentions. Every one is now past tense and names **"the legacy comms monolith"**, a phrase chosen
so the grep stays a real gate: a future reference to the identifier would stand out immediately, and the
descriptive name still tells a reader what is being described without naming a type that no longer exists.
Two places keep the identifier ON PURPOSE and are outside the gated tree: the anti-pattern example and the
migration-guide "before" snippet in `.claude/skills/overthrow-architecture/overthrow-controller.md`, both now
labelled "DELETED 2026-08-14 — shown so you recognise it in stale docs". `Scripts/Game/Components/README.md`'s
section became a new **Controller Components** section instead of a deletion.

### P10-5: All five `.st` fact-check citations still hold — but nine *co-cited* line numbers had drifted, and that is the finding
Four `Comment` lines (`:1394`, `:2508`, `:2904`, `:3295`) carried **five** citations into the monolith. Every
fact was re-verified against the migrated handler and **not one had changed**: the base-capture faction +
`baseCloseRange` re-check, the CIV-only/20 m/`baseRecruitCost` recruit chain, the half-cost + supporters-gate
tent chain, the `Import`/`IllegalImports` permission gates, and the officer-only resistance-funds account. All
five were re-pointed to `OVT_CampaignRequestComponent:143-178`, `OVT_RecruitRequestComponent:130-197` and
`:211-278`, `OVT_VehicleRequestComponent:429`/`:443`, `OVT_ResistanceRequestComponent:390` and
`OVT_RealEstateRequestComponent:230-234`.

**What actually rotted was the co-citations in the same comments** — nine of them, from this feature's own edits
and earlier ones: `OVT_OccupyingFactionManager.StartBaseQRF` 792-824 → **840-871**, `OVT_CaptureBaseAction.c`
1-24 → **1-29**, `OVT_RecruitCivilianAction.c` 1-40 → **1-48**, `OVT_RecruitFromTentAction.c` 1-80 → **1-81**,
`OVT_EconomyManagerComponent.GetBuyPrice` 565-577 → **582-594**, `OVT_VehicleMenuContext` 172 → **170**,
`OVT_PlayerWantedComponent.CheckUpdate` 496 → **586** and its recruit-inheritance block 488-493 → **576-584**,
`OVT_MainMenuContext.c` 148-157 → **137-147**, `OVT_RealEstateManagerComponent.c` 712-745 → **827-859**,
`OVT_RealEstateContext.c` 59-69 → **59-66**. All corrected, because a stale citation inside a comment this
phase was already rewriting is worse than one it never touched. **The general lesson: a `file:line` fact-check
citation has a half-life measured in features.** Cite a *method name* alongside the line — three of the nine
were only re-findable because the comment named the method too. `OVT_RecruitManagerComponent.c:11
MAX_RECRUITS_PER_PLAYER = 16` and `OVT_PortContext:99` had not moved.

---

## Q10 — Full arity audit (T10.3), 2026-08-14

**Method.** For every file in `Scripts/Game/Components/Controller/`: extract each `[RplRpc]` handler with its
declared parameter list, each marshalled `Rpc(RpcX, …)` call with its argument list, and each direct
`RpcX(…)` twin; then compare count **and order**, argument by argument. The twin is checked because only one
half of the pair is type-checked — `Rpc()` is an untyped variadic prototype (BUG-090), so a wrong count or a
swapped pair compiles clean and dies silently at the wire.

| Component | Handlers | `Rpc()` calls checked | Direct twins | Verdict |
|---|---|---|---|---|
| `OVT_AdminCommandsComponent` | 1 | 1 | 1 | ✅ pass |
| `OVT_BaseServerProgressComponent` | 4 | 4 | 4 | ✅ pass (twins are `IsLocalPlayerOwner()` owner-response branches) |
| `OVT_CampaignRequestComponent` | 5 | 5 | 5 | ✅ pass |
| `OVT_ContainerTransferComponent` | 6 | 6 | 6 | ✅ pass |
| `OVT_ControllerComponent` | — | — | — | n/a — generic accessor, no RPC |
| `OVT_ControllerRequestComponent` | — | — | — | n/a — shared base, no RPC |
| `OVT_EconomyRequestComponent` | 8 | 8 | 8 | ✅ pass |
| `OVT_FOBRequestComponent` | 7 | 7 | 7 | ✅ pass |
| `OVT_JobRequestComponent` | 2 | 2 | 2 | ✅ pass |
| `OVT_LoadoutRequestComponent` | 3 | 3 | 3 | ✅ pass |
| `OVT_PossessionRequestComponent` | 2 | 2 | 2 | ✅ pass |
| `OVT_RealEstateRequestComponent` | 8 | 8 | 8 | ✅ pass |
| `OVT_RecruitRequestComponent` | 4 | 4 | 4 | ✅ pass |
| `OVT_ResistanceRequestComponent` | 7 | 7 | 7 | ✅ pass |
| `OVT_RespawnRequestComponent` | 3 | 3 | 3 | ✅ pass (twins are `playerId == GetLocalPlayerId()` branches) |
| `OVT_ShopTransactionComponent` | 4 | 4 | 4 | ✅ pass |
| `OVT_TowerSabotageComponent` | 1 | 1 | 0 → **1** | ⚠️ **arity OK, ROUTING DEFECT — fixed** (P10-3) |
| `OVT_TravelRequestComponent` | 2 | 2 | 2 | ✅ pass |
| `OVT_TutorialComponent` | 2 | 2 | 2 | ✅ pass |
| `OVT_VehicleRequestComponent` | 6 | 6 | 6 | ✅ pass |
| **TOTAL** | **75** | **75** | **75** (74 before the fix) | **0 arity defects, 1 routing defect** |

**Order spot-checks that could have hidden a same-arity swap** (types would not save these — `Rpc()` is
untyped): `RpcAsk_UndeployFOB(fobRpl.Id(), vehicleRpl.Id())` against `(RplId fobId, RplId vehicleId)`;
`RpcAsk_SellVehicleCargo(vehicleRpl.Id(), shopRpl.Id())` against `(RplId vehicleId, RplId shopId)`;
`RpcAsk_DeleteCamp(rpl.Id(), m_vDeleteCampLocation)` against `(RplId campEntityId, vector pos)`;
`RpcAsk_ImportToVehicle(id, qty, rpl.Id())` against `(int id, int qty, RplId vehicleId)`;
`RpcAsk_TakeFromWarehouseToVehicle(warehouseId, id, qty, rpl.Id())` against
`(int warehouseId, string id, int qty, RplId vehicleId)`. All correct.

**Two negative results worth keeping.** (1) **No dead endpoints:** every one of the 75 handlers has at least
one caller — handler count and call count match per file. (2) **No cross-component `Rpc()`:** every
`Rpc(Rpc…)` under `Scripts/` outside this folder belongs to a manager's own `RpcDo_` broadcast
(`OVT_JobManagerComponent`, `OVT_EconomyManagerComponent`, `OVT_PlayerManagerComponent`,
`OVT_RealEstateManagerComponent`, `OVT_LoadoutManagerComponent`, `OVT_NotificationManagerComponent`,
`OVT_OwnerManagerComponent`, `OVT_ShopComponent`, `OVT_OverthrowController`) — nothing reaches across a class
boundary to marshal a controller component's handler.

---

## Gotchas & Learnings

### Standing hazards (from the plan — keep in view every phase)
- **`Rpc()` arity is a compile-check blind spot** — signatures shrink every phase; `grep -n "Rpc(Rpc"` + hand-check arity per phase, full audit in T10.5.
- **Listen-server owner responses** — `RplRcver.Owner` to the local host does not execute; every owner response uses the `SendSellResult`-style direct-call helper.
- **An authority-marshalled `RplRcver.Server` RPC goes nowhere** — the §3.6 latent bugs; migrated handlers call managers directly.
- **BUG-016 aliasing** — resolve target runtime id → persistent id in the same tick; never cache across frames.
- **BUG-078** — server-side checks use the server's difficulty values, never assume client parity.
- **Test counts are baselines, not doctrine** — re-measure before comparing; Fast 101 / All 142 as of 2026-08-14.

---

## Testing Approach

- **Gates per phase (all of P1–P10):** `tools/compile-check.sh` exit 0 · Fast `{6A6E29FF47ECB840}` exit 0 ≥ 101 · All `{6A6E2A002F53A581}` exit 0 ≥ 142 · `grep -rn "OVT_Global.GetServer()" Scripts/` at or below the phase's ceiling (P2 ≤52, P3 ≤43, P4 ≤31, P5 ≤16, P6 ≤12, P7 ≤5, P8 = 0)
- **Init tier:** one "component resolves off a registered controller" case per new component, added in the same phase, proven able to fail (method recorded in preamble)
- **Campaign tier:** Continue-controller assertion (P1/T1.5)
- **Manual:** the single §6 21-step MP play-test at P10 (user-driven); short SP/listen Continue play-test after P1 (user)

### Needs human verification (running list)
- [x] ✅ (2026-08-14, green) P1: SP/listen-host play-test — save → quit → Continue → balance, one shop purchase, camp menu (T1.5; dedicated round does NOT substitute)
- [x] ✅ (2026-08-14, Workbench loads clean, no GUID conflicts) P2–P8: wire each new component onto `Prefabs/GameMode/OVT_OverthrowController.et` in Workbench (fresh GUIDs, series style in implementation.md §External)
  - P8's two blocks were added **in text** (`OVT_JobRequestComponent "{6AEF8EB71C6A50D4}"`, `OVT_CampaignRequestComponent "{6AFB9FC82D7B61E5}"`, both GUID-verified unique repo-wide). **USER: confirm the prefab loads in Workbench with no missing-component warning and no GUID conflict.**
  - P7's two blocks were added **in text** (`OVT_LoadoutRequestComponent "{6AD76C95FA483EB2}"`, `OVT_PossessionRequestComponent "{6AE37DA60B594FC3}"`, both GUID-verified unique repo-wide). **USER: confirm the prefab loads in Workbench with no missing-component warning and no GUID conflict.**
  - P6's block was added **in text** (`OVT_RecruitRequestComponent "{6ACB5B84E9372FA1}"`, GUID-verified unique repo-wide). **USER: confirm the prefab loads in Workbench with no missing-component warning and no GUID conflict.**
  - P5's two blocks were added **in text** (`OVT_ResistanceRequestComponent "{6AB3E9C61FD5720A}"`, `OVT_FOBRequestComponent "{6ABF4A73D8261E95}"`, both GUID-verified unique repo-wide). **USER: confirm the prefab loads in Workbench with no missing-component warning and no GUID conflict.**
  - P4's block was added **in text** (`OVT_EconomyRequestComponent "{6AA7D8B502E4193C}"`, GUID verified unique repo-wide) for the same reason. **USER: confirm the prefab loads in Workbench with no missing-component warning and no GUID conflict.**
  - P3's block was added **in text** (`OVT_RealEstateRequestComponent "{6A9B3E14C27D08F6}"`, GUID verified unique repo-wide) for the same reason. **USER: confirm the prefab loads in Workbench with no missing-component warning and no GUID conflict.**
  - P2's block was added **in text** (`OVT_VehicleRequestComponent "{6A8F2C7D4E13A590}"`, GUID unique repo-wide) because the Init case cannot assert anything without it. **USER: open the prefab in Workbench and confirm it loads with no missing-component warning and no GUID conflict.**
- [x] ✅ P10: **both prefab blocks stripped in text** — `Prefabs/GameMode/OVT_OverthrowGameMode.et` lost `OVT_PlayerCommsComponent "{5D7ACE1228D77F40}" {}` at `:150-151`, `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` lost `OVT_PlayerCommsComponent "{5D7ACE122292D0CF}" {}` at `:15-16`; two lines each, nothing else in either file changed (P10-2). **USER: open BOTH stripped prefabs in Workbench and confirm they load with no missing-component warning**, then open `Prefabs/GameMode/OVT_OverthrowController.et` and confirm all 17 components resolve with no GUID conflict — that one prefab is where every P2-P8 block was added in text and has never been opened in the editor.
- [x] ✅ **(2026-08-14, ALL 21 STEPS GREEN)** **P10: the full 21-step MP play-test (implementation.md §6)**. Run `tools/launch-server.sh`, then `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`. Watch the server log for `Broken RPC` / arity errors and VMEs throughout; a step that cannot be reached is a failure, not a skip.
  - **Step 2 and step 8 are the acceptance evidence for BUG-161 and BUG-162** (P4-2/P4-3): buying 1 item must move the balance by exactly the shown price AND decrement the stock corner; donating must produce the `PlayerDonated` notification.
  - **Every phase newly fixed a listen-host path** (P2-5 class): on a listen host, re-test buy, sell, donate, send money/funds, tax, buy-skill, place, build, remove, promote, garrison, deploy, undeploy, set-priority, camp privacy, delete-camp, recruit, tent-recruit, rename, dismiss, buy/sell/rent/set-home, Save-from-menu — and now **radio-tower sabotage** (P10-3, newly fixed in this phase).
  - **P7-1: Open Inventory on a dedicated server is new-feature testing, not regression testing** — it could not have worked there before.
  - Rejection probes worth doing from a second client: dismiss/rename another player's recruit, open another player's recruit's inventory, delete/unprivate a camp you do not own, set-priority as a non-officer, place/build far from your character, start a base capture from 1 km away or while dead.

---

## Session Notes

### 2026-08-14 — Feature started (autorun)
- Scaffolded dev docs from the approved plan; status → In Progress.
- Running autonomously via /autorun-feature; git untouched per policy (user owns all git ops).

### 2026-08-14 — Phase 1 complete (network-specialist-advanced)
- New: `OVT_ControllerComponent.c`, `OVT_ControllerRequestComponent.c`, `OVT_TEST_Init_ControllerSeam.c`, `OVT_TEST_Campaign_ContinueControllerRespawn.c`.
- Gates verified by orchestrator: compile 0, Fast 102, All 144, getters 17, GetServer 60, Prefabs/Language/Persistence untouched.
- Proven-to-fail methods recorded in both test preambles (prefab-block removal; `if (false && …)` short-circuit in `PrepareConnectedPlayers`).
- Known test-harness flake (pre-existing, unrelated): one All run wedged at `OVT_TEST_Init_Tutorial_SettingsStoreRoundTrips` with a UserSettings transaction error and hit the 300 s timeout; not reproducible (3 subsequent green runs). If All exits 124 at that case, re-run before investigating.
- USER still owes the T1.5 listen-host Continue play-test (see Needs human verification).

---

### 2026-08-14 — Phase 2 complete (network-specialist)
- New: `OVT_VehicleRequestComponent.c`, `OVT_TEST_Init_VehicleRequestSeam.c`. Changed: `OVT_ControllerRequestComponent` (+`PlayerMayUseVehicle`), `OVT_ShopTransactionComponent` (−`PlayerMayUseVehicle`), `OVT_PlayerCommsComponent` (−6 handlers/wrappers, 2001→1733), `OVT_LockVehicleAction`, `OVT_UnlockVehicleAction`, `SCR_GetInUserAction`, `OVT_ManageVehicleContext`, `OVT_PortContext`, `OVT_ShopContext`, `OVT_OverthrowController.et`.
- Gates: compile **0**; Fast **103** exit 0; All **145** exit 0; `GetServer()` **53** (P2-1); the six handlers are gone from the monolith, not commented out.
- Proven-to-fail: the `OVT_VehicleRequestComponent` block was deleted from `OVT_OverthrowController.et` and the case failed with the intended "returned null while a controller entity exists" message; block restored, green. No `maxAttempts`.
- **Arity audit (Q10, P2 slice):** all six `Rpc(RpcAsk_…)` calls hand-checked against their handlers — SetVehicleLock 2/2, ClaimUnownedVehicle 1/1, UpgradeVehicle 2/2, RepairVehicle 1/1, ImportToVehicle 3/3 (order `id, qty, rplId`), BuyVehicle 2/2. No `Rpc()` call was added or changed anywhere else.
- Play-test debt handed to P10: **F1** plus the two now-fixed listen-host paths (P2-5) and the upgrade charge (P2-2 — confirm the balance moves exactly once).

### 2026-08-14 — Phase 3 complete (component-developer)
- New: `OVT_RealEstateRequestComponent.c`, `OVT_TEST_Init_RealEstateRequestSeam.c`. Changed: `OVT_RealEstateManagerComponent` (+`TransferToWarehouse`, +`TakeFromWarehouseToVehicle`, client branches of `AddToWarehouse`/`TakeFromWarehouse` re-pointed), `OVT_Global.c` (−both warehouse helpers), `OVT_PlayerCommsComponent` (−8 handlers/wrappers, 1733→1515), `OVT_ContainerTransferComponent` (delegates to the manager, not the static), `OVT_RealEstateContext` (×6), `OVT_SetHomeAction`, `OVT_WarehouseContext`, `OVT_OverthrowController.et`. Docs: `economy/real-estate/context.md` (SetBuildingHome disposition).
- Gates: compile **0**; Fast **104** exit 0; All **146** exit 0; `GetServer()` **43** (≤44 per P2-1); `grep TransferToWarehouse\|TakeFromWarehouseToVehicle Scripts/Game/Global/OVT_Global.c` empty; the eight handlers are gone from the monolith, not commented out; Q4 grep over `Components/Controller/` returns nothing.
- Proven-to-fail: the `OVT_RealEstateRequestComponent` block was deleted from `OVT_OverthrowController.et` and the case failed with the intended "…Get() returned null while a controller entity exists" message; block restored, green. No `maxAttempts`.
- **Arity audit (Q10, P3 slice):** all eight `Rpc(RpcAsk_…)` calls hand-checked against their handlers — SetHome 0/0 (zero-arg `Rpc()` has precedent at `OVT_PlayerCommsComponent.c:27`), BuyBuilding 1/1, SellBuilding 1/1, RentBuilding 1/1, StopRentingBuilding 1/1, AddToWarehouse 3/3, TakeFromWarehouse 3/3, TakeFromWarehouseToVehicle 4/4 (order `warehouseId, id, qty, rplId`). No `Rpc()` call elsewhere was added or changed.
- **Harness flake seen on a different case than the known one:** the first Fast run timed out at 300 s having never left the main menu (`No GameMode present in the world, using fallback logic!`, empty `autotest.log`) — the world never loaded at all, so it is a launch flake, not a test failure. The immediate re-run was green in 39 s. Treat a 124 with an empty `autotest.log` as "re-run once" regardless of which case is named.
- Play-test debt handed to P10: **F2** and **F3**, plus the listen-host paths this phase newly fixes (buy/sell/rent/set-home were unconditional `Rpc()` from the authority and therefore no-ops for a host — same class as P2-5).

### 2026-08-14 — Phase 4 complete (network-specialist)
- New: `OVT_EconomyRequestComponent.c`, `OVT_TEST_Init_EconomyRequestSeam.c`, `docs/bugs/BUG-161.md`, `docs/bugs/BUG-162.md`. Changed: `OVT_ShopTransactionComponent` (+`BuyItems`/`RpcAsk_BuyItems` + `MAX_BUY_QUANTITY` + spawn/notify helpers, `RestockShop` delegates), `OVT_ShopComponent` (`AddToInventory`/`TakeFromInventory` are now plain server-side mutations), `OVT_VehicleRequestComponent` (`TakeFromShopStock` delegates), `OVT_PlayerCommsComponent` (−11 handlers/wrappers −3 helpers −`takingMoney`, 1515→1122), `OVT_ShopContext`, `OVT_SellDrugsAction`, `OVT_CharacterSheetContext`, `OVT_ResistanceMenuContext` (×3), `OVT_EconomyManagerComponent` (client branches of `SetResistanceTax`/`TakePlayerMoney` + 3 stale doc comments), `OVT_OverthrowController.et`.
- Gates: compile **0**; Fast **105** exit 0; All **147** exit 0; `GetServer()` **33** grep lines / **31** live calls (P4-1 — the plan's 32 is unreachable arithmetic, not a missed re-point); `git diff --stat Language/ Configs/ Scripts/Game/Persistence/` empty; Q4 grep over `Components/Controller/` returns nothing; the eleven handlers are gone from the monolith, not commented out.
- Proven-to-fail: the `OVT_EconomyRequestComponent` block was deleted from `OVT_OverthrowController.et` and the case failed with the intended "…Get() returned null while a controller entity exists" message; block restored, green (single-case run, 13 s). No `maxAttempts`.
- **Arity audit (Q10, P4 slice):** eight `Rpc(Rpc…)` calls hand-checked against their handlers — SellDrugs 1/1, DonateToResistance 1/1, SendResistanceFunds 2/2, SendMoneyToPlayer 2/2, TakePlayerMoney 1/1, SetResistanceTax 1/1, BuySkill 1/1, `Rpc(RpcDo_DoneTakingMoney)` 0/0, plus BuyItems 3/3 on the shop component (order `shopId, id, num`). Every one has a `Replication.IsServer()` direct-call twin, and each twin was checked to pass **the identical arguments in the identical order** — that is the pair most likely to drift, because only one half of it is type-checked.
- **The §3.6 verdicts are the headline of this phase, not a footnote** — see P4-2/P4-3 and the two filed bugs. P4-4 flips the plan's conditional docs/wiki pass to required.
- Play-test debt handed to P10: **F4** and **F5**, and specifically §6 step 2 (buy 1: balance moves by exactly the shown price, stock corner decrements; buy 5 with room for 3: partial notification, charged for 3) and step 8 (`PlayerDonated` notification appears) — those two steps are now the acceptance evidence for BUG-161/BUG-162. Also newly fixed for a listen host: buy, donate, send money/funds, tax and buy-skill were all authority-marshalled `Rpc()`s and therefore no-ops for a host (same class as P2-5).

### 2026-08-14 — Phase 5 complete (component-developer)
- New: `OVT_ResistanceRequestComponent.c` (6 handlers + `RpcDo_ConvertSupporterResult`), `OVT_FOBRequestComponent.c` (7 handlers), `OVT_TEST_Init_ResistanceRequestSeam.c`, `OVT_TEST_Init_FOBRequestSeam.c`. Changed: `OVT_PlayerCommsComponent` (−13 handlers/wrappers −the convert constants/tick −the delete-camp query helpers, 1122→826), `OVT_PlaceContext` ×2, `OVT_BuildContext` ×2, `OVT_ResistanceMenuContext`, `OVT_BaseMenuContext`, `OVT_FOBMenuContext` ×2, `OVT_CampMenuContext` ×2, `OVT_ConvertSupporterAction`, `OVT_DeployFOBAction`, `OVT_UndeployFOBAction`, `OVT_SetPriorityFOBAction`, `OVT_OverthrowController.et`. **Deleted: `Scripts/Game/UserActions/OVT_UndeployFOBAction_New.c`** (commented-out migration example whose "OLD WAY" was the thing being deleted).
- Gates: compile **0**; Fast **107** exit 0; All **149** exit 0; `GetServer()` **18** grep lines / **17** live calls (P5-1 — re-derived from HEAD, the plan's ≤16 is unreachable arithmetic); `git diff --stat Language/ Configs/ Scripts/Game/Persistence/` empty; Q4 grep over `Components/Controller/` returns nothing; the thirteen handlers are gone from the monolith, not commented out.
- Proven-to-fail: **both** prefab blocks were deleted from `OVT_OverthrowController.et` and the Init suite failed **2 of 29** — exactly `OVT_TEST_Init_Controller_ResistanceRequestResolves` and `OVT_TEST_Init_Controller_FOBRequestResolves`, both with the intended "…Get() returned null while a controller entity exists" message and nothing else red. Blocks restored, suite green 29/29. No `maxAttempts`.
- **Arity audit (Q10, P5 slice):** fourteen `Rpc(Rpc…)` calls hand-checked against their handlers — PlaceItem 4/4 (`placeableIndex, prefabIndex, pos, angles`), RemovePlacedItem 1/1, BuildItem 4/4, AddOfficer 1/1, AddGarrison 2/2 (`base.id, index`), ConvertSupporter 1/1, `Rpc(RpcDo_ConvertSupporterResult, converted)` 1/1; AddGarrisonCamp 2/2 (`camp.location, index`), AddGarrisonFOB 2/2, DeployFOB 1/1, UndeployFOB 1/1, SetPriorityFOB 1/1, SetCampPrivacy 2/2, DeleteCamp 2/2 (`rpl.Id(), m_vDeleteCampLocation`). Every one has a `Replication.IsServer()` direct-call twin and each twin was checked to pass **the identical arguments in the identical order** — only one half of that pair is type-checked.
- Two prefab blocks added **in text** with fresh GUIDs `{6AB3E9C61FD5720A}` (resistance) and `{6ABF4A73D8261E95}` (FOB), both grep-verified unique repo-wide. **USER: confirm the prefab loads in Workbench with no missing-component warning and no GUID conflict.**
- Play-test debt handed to P10: **F6**, **F7** and **F8**, and specifically (a) the two camp endpoints — from a second client, try to delete and to unprivate a camp you neither own nor are an officer over: both must be refused with the `[OVT_FOBRequestComponent] Rejected …` log line and no state change; (b) set-priority from a non-officer must be refused; (c) place/build from a position far from your character must be refused. Also newly fixed for a listen host: place, build, remove, promote, garrison, deploy, undeploy, set-priority, camp privacy and delete-camp were **all** unconditional `Rpc()`s from the authority and therefore no-ops for a host (same class as P2-5) — a host should re-test every one of them.

### 2026-08-14 — Phase 6 complete (component-developer)
- New: `Scripts/Game/Components/Controller/OVT_RecruitRequestComponent.c` (4 handlers), `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_RecruitRequestSeam.c`. Changed: `OVT_PlayerCommsComponent` (−4 handlers/wrappers, 826→**642**), `OVT_RecruitCivilianAction`, `OVT_RecruitFromTentAction`, `OVT_RecruitsContext` ×2, `Prefabs/GameMode/OVT_OverthrowController.et`.
- Gates: compile **0**; Fast **108** exit 0; All **150** exit 0; `GetServer()` **14** grep lines / **13** live calls (P6-1 — the plan's ≤12 is inherited arithmetic, not a missed re-point) and **zero** recruit-shaped calls remain anywhere; `git diff --stat Language/ Configs/ Scripts/Game/Persistence/` empty; Q4 grep over `Components/Controller/` returns nothing; the four handlers are gone from the monolith, not commented out.
- Proven-to-fail: the `OVT_RecruitRequestComponent` block was deleted from `OVT_OverthrowController.et` and the single-case run failed with exactly the intended `OVT_ControllerComponent<OVT_RecruitRequestComponent>.Get() returned null while a controller entity exists` message (junit `<failure type="Result">`); block restored, case green in 13 s. No `maxAttempts`.
- **Arity audit (Q10, P6 slice):** four `Rpc(RpcAsk_…)` calls hand-checked against their handlers — RecruitCivilian 1/1 (`rpl.Id()`), RecruitFromTent 1/1 (`tentPos`), RenameRecruit 2/2 (`recruitId, newName`), DismissRecruit 1/1 (`recruitId`). Every one has a `Replication.IsServer()` direct-call twin passing **the identical arguments in the identical order** — only one half of that pair is type-checked. No `Rpc()` call elsewhere was added or changed.
- **The security headline is dismiss (P6-2).** `RpcAsk_DismissRecruit` validated nothing but the recruit's existence: any modified client could permanently delete any player's recruit — body deleted, record struck, XP gone — and recruit ids are streamed to every client, so it needed no guesswork. Rename was already owner-checked (BUG-052 family) but let an unresolved sender through (P6-3).
- Play-test debt handed to P10: **F9** — (a) from a second client, try to dismiss and to rename a recruit you do not own: both must be refused with the `[OVT_RecruitRequestComponent] Rejected …` log line and no state change; (b) recruit a civilian and from a tent, confirming the balance moves by exactly the shown price **once** and the tent path consumes one town supporter; (c) at the 16-recruit cap the tent path must refuse **without** spawning a civilian at the tent. Also newly fixed for a listen host: all four were unconditional `Rpc()`s from the authority and therefore no-ops (same class as P2-5) — a host should re-test recruit, tent-recruit, rename and dismiss.

### 2026-08-14 — Phase 7 complete (component-developer-advanced)
- New: `Scripts/Game/Components/Controller/OVT_LoadoutRequestComponent.c` (3 handlers), `Scripts/Game/Components/Controller/OVT_PossessionRequestComponent.c` (1 handler + `RpcDo_OpenInventory` + the BUG-147 client lifecycle), `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_LoadoutRequestSeam.c`, `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_PossessionRequestSeam.c`. Changed: `OVT_PlayerCommsComponent` (−4 handlers/wrappers −the possession client lifecycle −`LOADOUT_BOX_MAX_DISTANCE` −the now-dead `ResolveSenderPersistentId`, 642→**315**), `OVT_LoadoutsContext` ×3, `OVT_SaveLoadoutAction`, `OVT_SaveOfficerLoadoutAction`, `OVT_OpenInventoryCommand` (see P7-1 — this one changed machines, not just an accessor), `Prefabs/GameMode/OVT_OverthrowController.et`.
- Gates: compile **0**; Fast **110** exit 0 (38 s); All **152** exit 0 (44 s); `GetServer()` **8** grep lines / **7** live calls (P7-5) and **zero** loadout/possession-shaped calls remain anywhere; `git diff --stat Language/ Configs/ Scripts/Game/Persistence/` empty; Q4 grep over `Components/Controller/` returns nothing; the four handlers and `RpcAsk_RestorePossessedEntity` are gone from the monolith, not commented out.
- Proven-to-fail: **both** prefab blocks were deleted from `OVT_OverthrowController.et` and the Init suite failed **2 of 32** — exactly `OVT_TEST_Init_Controller_LoadoutRequestResolves` and `OVT_TEST_Init_Controller_PossessionRequestResolves`, both with the intended "…Get() returned null while a controller entity exists" message and nothing else red. Blocks restored, suite green 32/32. No `maxAttempts`.
- **Arity audit (Q10, P7 slice):** five `Rpc(Rpc…)` calls hand-checked against their handlers — SaveLoadout 3/3 (`loadoutName, description, isOfficerTemplate`), LoadLoadoutFromBox 3/3 (`loadoutName, boxRpl.Id(), targetRpl.Id()`), DeleteLoadout 2/2 (`loadoutName, isOfficerTemplate`), SetPossessedEntityAndOpenInventory 1/1 (`rpl.Id()`), `Rpc(RpcDo_OpenInventory, targetEntityId)` 1/1. Every one has a `Replication.IsServer()` (or `ShouldRespondLocally`) direct-call twin passing **the identical arguments in the identical order** — only one half of that pair is type-checked. No `Rpc()` call elsewhere was added or changed; the monolith's remaining eleven are all P8's.
- **The security headline is possession (P7-2).** `RpcAsk_SetPossessedEntityAndOpenInventory` validated nothing whatsoever: a claimed playerId plus any RplId bought real control of the named character on the server. Loadouts were already BUG-043-hardened by laundering; this phase made that laundering unnecessary by deleting the parameter.
- **Harness flake, twice, before a clean run:** the first All attempt was killed at 300 s having flagged `OVT_TEST_Logic_Influence_MomentumRangeConstant` and `OVT_TEST_Logic_Territory_GridOriginSnap` as `Failure reason: timeout` with `Output: <none>` (pure-maths cases in files this phase never touched, green in the Fast run three minutes earlier) and then wedged on the Persistence world transition; the second reached PersistenceRoundTrip and flagged `…Capability_SaveGameProducesASave` as a timeout **whose Output contains its own success line**, then wedged on the next case. The third run was green in 44 s. **Add to the flake list: a case flagged `Failure reason: timeout` with either no output or its own success message in the output is a loaded-machine artefact, not a failure — the tell is that the assertion already printed.**
- Play-test debt handed to P10: **F10** and **F11**. Specifically (a) F11's three-open-close probe is now the *only* evidence the moved BUG-147 lifecycle still behaves — exactly three restore requests, and the recruit walks and faces normally afterwards; (b) from a second client, try to open the inventory of a recruit you do not own — must be refused with the `[OVT_PossessionRequestComponent] Rejected open-inventory request…` log line and no possession; (c) apply a loadout to another player's recruit standing at your box → refused, and to your own recruit standing >20 m from the box → refused; (d) **the 20 m possess radius is the number most likely to be wrong** — if a legitimate Open Inventory from normal commanding range is refused, raise `POSSESS_MAX_DISTANCE`, do not delete the check. Also newly correct for a **dedicated server**: Open Inventory could not have worked there at all before (P7-1), so treat F11 on a remote client as new-feature testing rather than regression testing.

### 2026-08-14 — Phase 8 complete (component-developer)
- New: `Scripts/Game/Components/Controller/OVT_JobRequestComponent.c` (2 handlers), `Scripts/Game/Components/Controller/OVT_CampaignRequestComponent.c` (4 handlers + `RpcDo_SaveResult` + the save invoker), `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_JobRequestSeam.c`, `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_CampaignRequestSeam.c`. Changed: `OVT_PlayerCommsComponent` (**emptied** — 315 → 23 lines, zero RPCs), `OVT_JobManagerComponent` ×2, `OVT_CaptureBaseAction`, `OVT_DeliverMedicalSuppliesAction`, `OVT_PlayerWantedComponent`, `OVT_MainMenuContext` ×2 (call site + the `:14` doc comment + the cached field's type), `OVT_OverthrowGameMode` (DiagMenu 254 → a local `#ifdef WORKBENCH InstantCaptureBase()`), `OVT_EconomyRequestComponent` (one doc-comment reword), `Prefabs/GameMode/OVT_OverthrowController.et`.
- Gates: compile **0**; Fast **112** exit 0 (39 s); All **154** exit 0 (46 s); **`grep -rn "OVT_Global.GetServer()" Scripts/` = 0 lines** (target reached exactly, per P7-5's arithmetic); `grep -c "RpcAsk_\|RpcDo_" OVT_PlayerCommsComponent.c` = **0**; `RpcAsk_SendNotification` gone from `Scripts/` (only history in `docs/`); `git diff --stat Language/ Configs/ Scripts/Game/Persistence/` empty; Q4 grep over `Components/Controller/` returns nothing.
- Proven-to-fail: **both** prefab blocks were deleted from `OVT_OverthrowController.et` and the Init suite failed **2 of 34** — exactly `OVT_TEST_Init_Controller_JobRequestResolves` and `OVT_TEST_Init_Controller_CampaignRequestResolves`, both with the intended "…Get() returned null while a controller entity exists" message and nothing else red. Blocks restored, suite green 34/34. No `maxAttempts`.
- **Arity audit (Q10, P8 slice):** seven `Rpc(Rpc…)` calls hand-checked against their handlers — AcceptJob 3/3 (`job.jobIndex, job.townId, job.baseId`), DeclineJob 3/3 (same order), StartBaseCapture 0/0, DeliverMedicalSupplies 1/1 (`rpl.Id()`), LootWantedCheck 0/0, RequestSave 0/0, `Rpc(RpcDo_SaveResult, success)` 1/1. Every one has a `Replication.IsServer()` (or `ShouldRespondLocally`) direct-call twin passing **the identical arguments in the identical order** — only one half of that pair is type-checked. No `Rpc()` call elsewhere was added or changed, and there are none left on the monolith at all.
- **The security headline is medical supplies (P8-4)**, and the correctness headline is that `StartBaseCapture` can no longer be told where the caller is standing. The listen-host headline is **Save**: the pause menu's Save button sent an authority-marshalled `Rpc()` and therefore never started a save for a host at all (P8-3).
- Play-test debt handed to P10: **F12** (jobs) and **F13** (campaign). Specifically (a) accept and decline a job from a second client and confirm the job's owner is the *accepting* player; (b) try to start a base capture from outside `baseCloseRange` and while dead — both refused with the `[OVT_CampaignRequestComponent] Rejected …` line; (c) from a second client, try to deliver the medical supplies in a truck locked to somebody else → refused; (d) **Save from the pause menu on a listen host and on a dedicated client** — the hint must report the real outcome, once, and never on send; (e) loot a body as a remote client and confirm the wanted level rises (the handler's owner changed machines).

### 2026-08-14 — Phase 9 complete (component-developer)
- New: `Scripts/Game/Utilities/OVT_WorldUtils.c` (440 lines, 14 helpers + 3 private statics), `Scripts/Game/Utilities/OVT_PrefabUtils.c` (94, 4 helpers), `Scripts/Game/Utilities/OVT_LoadoutUtils.c` (68, 3 helpers). Changed: `OVT_Global.c` (**902 → 302** lines) plus 36 call-site files. No prefab, no config, no RPC, no localization touched.
- **Re-derived call counts** (occurrences, excluding `OVT_Global.c`): forwarded — SpawnEntityPrefab **35**, GetPrefabName **33**, PlayerInRange **13**. Re-pointed — FindSafeSpawnPosition **10**/8 files, GetRandomNonOceanPositionNear **8**/4, SpawnEntityPrefabMatrix **8**/6, FindNearestRoad **7**/5, GetItemUIInfo **12**/8, GetVehicleUIInfo **4**/3, GetEditableUIInfo **3**/2, GetNearbyBodiesAndWeapons **2**/2, IsOceanAtPosition **2**/1, RandomizeCivilianClothes **2**/2 (one a delegate reference), FindSafeVehicleSpawnPosition **1**, SpawnCharacterEntity **1**, ApplyCivilianLoadout **1**, ResetAIAimState **1** call + 1 doc mention. Total re-pointed **63** call sites (the plan's ~77 counted the three forwarded helpers' internal and doc mentions). Deleted: `NearestPlayer`, `RandomizeCivilianGroupClothes`. Zero external references to `FilterVehicleSpawnEntities`, `FilterSpawnPointEntities`, `s_SpawnPointSearchResults`, `m_Bodies`, `FilterDeadBodiesAndWeapons`.
- Gates: compile **0** (6060 files, 6 s); Fast **112** exit 0 (40 s); All **154** exit 0 (44 s) — both dead level with P8's baseline, as a pure move must be; `wc -l Scripts/Game/Global/OVT_Global.c` = **302** (< 400); `grep -rn "NearestPlayer\|RandomizeCivilianGroupClothes" Scripts/` = 1 line, and it is `OVT_DeploymentManager.GetNearestPlayerDistance` (unrelated); the three forwarders are one-line `return`s with no logic and identical signatures including default parameters; `git status --porcelain Prefabs/` shows only P8's pre-existing `OVT_OverthrowController.et` edit (mtime predates this session — this phase touched no prefab).
- **No proven-to-fail case this phase.** Nothing new is assertable: not one line of behaviour changed, and the two test-visible helpers are forwarded, so a broken split fails the *compile* check, not a test. The tests are here as a regression net, not as new coverage — the correct evidence is the byte-diff in P9-3.
- No arity audit slice: this phase added, removed and changed **zero** `Rpc()` calls.
- Play-test debt handed to P10: **none new.** The one thing worth a glance during the §6 sweep is civilian clothing — `OVT_TownController.c:169` registers `OVT_LoadoutUtils.RandomizeCivilianClothes` as a `GetOnAgentAdded()` delegate, and a delegate that silently stopped firing would look like "civilians all wear the same thing", not like an error.
- Left on `OVT_Global`: the two identity constants + `GetPlayerUID`, `GetServer()` (T10.2's job), `GetUI`, `GetLocalPersistentId`, the controller cache trio (`s_LocalController`/`SetLocalController`/`GetController`), 22 manager/config accessors, the three forwarders and `ShowHint`. It is now a locator and a client seam, and nothing else.

### 2026-08-14 — Phase 10 complete (network-specialist-advanced) — **the monolith is gone**
- **Deleted:** `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` (the 23-line shell P8 left), and `OVT_Global.GetServer()` (`OVT_Global.c` 302 → **292** lines).
- **Prefabs stripped in text** (T10.1): `Prefabs/GameMode/OVT_OverthrowGameMode.et` `-2` lines at `:150-151`, `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` `-2` lines at `:15-16`. Delta-format checked first (P10-2): the game-mode prefab is a root, and the character prefab's base carries no such component, so both blocks were additions and removing them removes the component. `git diff` on both files is exactly those four lines.
- **Changed (sweep, T10.2):** 33 files under `Scripts/` — 12 controller component headers, 5 mid-file citations, 10 Init seam suites (preamble + failure message each), `OVT_TEST_InitSuite`, `Scripts/Game/Components/README.md` (the monolith's section became a **Controller Components** section), `OVT_ShopComponent`, `OVT_OverthrowGameMode`, `OVT_ResistanceFactionManager`, `OVT_MapLocationPort`. Plus `Language/localization_Overthrow.st` — **4 `Comment` lines only**, verified by a script that aborts if any non-`Comment` line would change; no `.<lang>.conf` touched.
- **Changed (fix, T10.3 finding):** `OVT_TowerSabotageComponent.RequestSabotage()` gained the `Replication.IsServer()` direct-call branch it never had. See P10-3 — this is a **behaviour change** and the only one in the phase.
- **Docs (T10.4):** `docs/features/core/epic-overview.md` (status line, game-mode row :23 debt discharged, controller-migration row :27 → built, build-order §5, dependencies note, rollup ×2); `docs/features/core/game-mode/` context.md ×4 + implementation.md ×9 + tasks.md ×1 (the "controller migration stalled / 57 RPCs" headline debt is now marked discharged everywhere it appeared); `.claude/skills/overthrow-architecture/overthrow-controller.md` (P1's checklist verified correct and extended with the arity/listen-host steps; the "Old Pattern", "Migration Guide", DON'T list and the badly stale "Implemented / Planned for Migration" sections rewritten — the roster now lists all 17 components); `.claude/skills/overthrow-architecture/SKILL.md`; `.claude/skills/overthrow-ui-patterns/ui-contexts.md`; `.claude/agents/component-developer.md` and `component-developer-advanced.md`.
- **§6 automated verification, items 1-10, all run this session:** ① compile-check **exit 0** (6059 files, 5 s). ② Fast **112** exit 0 (36 s). ③ All **154** exit 0 (41 s). **No flakes — both suites green on the first attempt.** ④ `grep -rn "OVT_PlayerCommsComponent" Scripts/ Prefabs/ | wc -l` = **0**. ⑤ `grep -rn "OVT_Global.GetServer()" Scripts/ | wc -l` = **0** (and bare `GetServer()` across `Scripts/ Prefabs/` is also 0). ⑥ `grep -c "static OVT_.*Component Get"` = **16, not the plan's 17 — 16 is correct**, see P10-1. ⑦ `wc -l OVT_Global.c` = **292** (< 400). ⑧ `ls Scripts/Game/Components/Controller/` = **20 files** (17 components + `OVT_ControllerComponent.c` + `OVT_ControllerRequestComponent.c` + `OVT_BaseServerProgressComponent.c`). ⑨ `git diff --stat Language/` = **only `localization_Overthrow.st`**, 4 insertions / 4 deletions. ⑩ `git diff --stat Configs/Systems/Persistence/ Scripts/Game/Persistence/` = **empty**, and `git status --porcelain` on both trees is empty too.
- **Arity audit (Q10, full):** recorded above as its own section. 75 calls, 75 twins, zero arity defects, one routing defect found and fixed.
- **Play-test debt handed to the user:** the whole §6 21-step pass (see the Needs-human-verification list, which now carries the per-phase emphasis each phase asked for), plus the three Workbench prefab opens. Add **radio-tower sabotage on a listen host** to the list — it is newly working as of this phase.
- **Not done, deliberately:** the Discovered Task (folding the three remaining `ResolveOwningPlayerId` copies in `OVT_TravelRequestComponent` / `OVT_RespawnRequestComponent` / `OVT_TowerSabotageComponent` onto `OVT_ControllerRequestComponent`) is still open. It is a hierarchy change with real play-test surface, and P10 is a deletion sweep; doing it here would have put two behaviour changes into the phase whose whole point is that nothing should move. `OVT_TowerSabotageComponent`'s own comment still says "candidate for a shared base class when a third controller component needs it" — that condition was met long ago.

---

*Update this file at the end of each work session. Run `/update-feature core/controller-migration` before compacting conversations.*

### 2026-08-14 — FEATURE COMPLETE: all human gates green
- User ran the full §6 21-step MP play-test (dedicated server + client): **all 21 steps green**, including the rejection probes, the BUG-161/162 acceptance evidence (steps 2 & 8), the three-open-close possession probe (F11), and the newly-working listen-host paths.
- Workbench loads all three text-edited prefabs clean — `OVT_OverthrowController.et` (17 components, 10 text-added GUIDs), both stripped prefabs, no missing-component warnings, no GUID conflicts.
- Listen-host save → quit → Continue green (F15/T1.5 — the BUG-104-family debt is settled at runtime, not just by the Campaign case).
- This ran on the tree AFTER the `origin/main` merge (`275172b3`), so main's BUG-163/165 fast-travel road-spawn work (ported to `OVT_WorldUtils`) was in play too.
- Remaining open items are the two Discovered follow-ups only (fold the 3 `ResolveOwningPlayerId` duplicates; validation audit of the 7 pre-migration components) — tech debt, not gates.

### 2026-08-14 — Post-completion: the 3-duplicate fold (user-approved)
- `OVT_TravelRequestComponent`, `OVT_RespawnRequestComponent`, `OVT_TowerSabotageComponent` now extend `OVT_ControllerRequestComponent` (Class decls re-parented too); their byte-identical private `ResolveOwningPlayerId()` copies deleted. Exactly ONE definition remains, on the base.
- Safe now because the deferral reason (play-test surface) was discharged by the green 21-step pass; the Init suite re-proves all three still resolve off the controller under the new hierarchy.
- Gates: compile 0, Fast 112, All 154. Remaining tech debt: only the validation audit of the 7 pre-migration components.
