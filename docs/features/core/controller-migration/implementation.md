# Controller Migration — Implementation Plan

**Status:** Ready for Review — all 10 phases built and gate-verified; the §6 21-step MP play-test + Workbench prefab confirmation are the remaining user-driven gates
**Started:** 2026-08-14
**Target Completion:** TBD
**Last Updated:** 2026-08-14 (built end-to-end via /autorun-feature; Fast 112 / All 154 / zero monolith refs)

**Epic:** `core` (feature #5 — see `docs/features/core/epic-overview.md:44`)
**Requirements:** `docs/features/core/controller-migration/requirements.md` (authoritative for scope)
**Approach:** foundation-first, per-domain granularity, utility split in scope, **one final MP play-test pass** (user-approved 2026-08-14)

---

## 1. Executive Summary

`OVT_PlayerCommsComponent` is Overthrow's original client→server seam: **2,001 lines, 55 `RpcAsk_*` handlers and 4 `RpcDo_*` responses** on a single component that lives on *both* the game-mode prefab and the player character prefab, reached through `OVT_Global.GetServer()` from **60 call sites across 38 files**. It is frozen by project rule and every sibling feature has committed to never touching it again.

This feature finishes the migration that `OVT_ContainerTransferComponent` started: every remaining request moves onto a **domain component on `OVT_OverthrowController`**, the per-player, per-client owned entity that is the engine-intended seam. Seven components already live there (container transfer, shop selling, fast travel, respawn, tower sabotage, tutorials, admin commands) — this plan adds **ten more**, extends one, and then **deletes the monolith and `OVT_Global.GetServer()` entirely**.

Three things make it more than a file move:

1. **The seam gets rebuilt first.** A compile-verified generic `OVT_ControllerComponent<Class T>.Get()` replaces the six per-domain `OVT_Global` getters, so this migration — and every future one — adds nothing to the locator. This name is already committed to in prose by `options/requirements.md:41` and `player-groups/implementation.md:397`; it is epic-level API.
2. **Validation rides with every RPC.** The monolith's identity model is "the handler runs on the caller's character, so `GetOwner()` is the truth" (`ResolveSenderPlayerId`). The controller's model is "the handler runs on the caller's controller entity, so the entity is the truth" (`ResolveOwningPlayerId`). Every migrated handler swaps one for the other and re-derives ownership/proximity/faction/affordability server-side. Validation already hardened in 1.4.x (BUG-025, BUG-032, BUG-033, BUG-043, BUG-063, BUG-087, BUG-102) is **carried forward line by line, never re-derived from scratch**.
3. **Three dead RPCs are deleted, not migrated, and two latent authority bugs are fixed rather than preserved.** "No behaviour change" is the wrong goal where the current behaviour is a silently dropped packet — see §3.6.

**Deliberately not a rewrite.** Handler bodies are relocated near-verbatim; the manager layer is untouched except for the two `OVT_Global` warehouse helpers the requirements name. Thin seams, fat managers.

---

## 2. Goals

### Primary

- **G1** `OVT_PlayerCommsComponent.c` is **deleted**, along with `OVT_Global.GetServer()`, and the component is removed from both prefabs that carry it. Zero references remain under `Scripts/` and `Prefabs/`.
- **G2** Every surviving request is served by a domain component on `OVT_OverthrowController`, reachable via `OVT_ControllerComponent<T>.Get()`. `OVT_Global` gains **no** new getter and loses the six it has.
- **G3** Every migrated handler resolves the caller from **its own controller entity**, never from a client-supplied `playerId`/`persistentId` parameter. No migrated RPC accepts an identity argument.
- **G4** Every validation the monolith performs today is present in the migrated handler, verified RPC-by-RPC against the mapping tables in §4. No exploit-class regression.
- **G5** The controller seam is available whenever a client needs it — including with no controlled entity (start menu, dead, pre-spawn) and after a **Continue** (BUG-104 family) — and that availability is evidenced, not assumed.
- **G6** No player-visible behaviour change except: invalid requests are rejected (silently or with an existing notification), and the two latent authority bugs in §3.6 start working.

### Secondary

- **G7** `OVT_Global` is reduced to the locator + controller seam it is named for; ~470 lines of utilities move to `OVT_WorldUtils` / `OVT_PrefabUtils` / `OVT_LoadoutUtils` with thin forwarders for the high-traffic three.
- **G8** `OVT_Global.TransferToWarehouse()` and `TakeFromWarehouseToVehicle()` move onto `OVT_RealEstateManagerComponent` — the one place "thin seams, fat managers" is currently violated.
- **G9** `GetUI()` and `GetDifficulty()`, which outlive this feature, become null-safe.
- **G10** `RpcDo_NotifyOwnerAssignment`'s **once-per-assignment vs once-per-player** contract is made explicit and its consumers made idempotent.

### Explicitly out of scope

- Renaming or removing `OVT_Global`'s 17 manager forwarders (~900 call sites).
- Any gameplay change beyond validation and the §3.6 fixes.
- Replication patterns other than the player→server request seam and its owner-targeted responses (no RplProp/JIP-snapshot work).
- Non-RPC bugs in the migrated domains — they stay with their own feature's bug list.
- Re-planning the seven already-migrated domains.

---

## 3. Architecture Overview

### 3.1 The seam today — verified against the tree, 2026-08-14

```
OVT_PlayerCommsComponent.c ........... 2,001 lines
  RpcAsk_* handlers .................. 55
  RpcDo_* responses .................. 4   (3 × RplRcver.Owner, 1 × Broadcast)
  public entry points ................ 56
  lives on ........................... Prefabs/GameMode/OVT_OverthrowGameMode.et
                                       Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et

OVT_Global.GetServer() ............... 60 call sites / 38 files
OVT_Global.c ......................... 1,007 lines
  per-domain controller getters ...... 6  (+ LootBattlefield wrapper) — all deleted in P1
  manager forwarders ................. 17 — frozen, untouched
  utilities .......................... ~24 statics, ~470 lines — split in P9

OVT_PlayerCommsComponent refs ........ 58 occurrences / 21 files under Scripts/
                                       2 prefab files
                                       4 doc-comment occurrences in localization_Overthrow.st
```

Already on `Prefabs/GameMode/OVT_OverthrowController.et` (do not re-plan): `OVT_ContainerTransferComponent`, `OVT_ShopTransactionComponent`, `OVT_TowerSabotageComponent`, `OVT_TutorialComponent`, `OVT_AdminCommandsComponent`, `OVT_TravelRequestComponent`, `OVT_RespawnRequestComponent`, plus the `OVT_BaseServerProgressComponent` base.

### 3.2 The generic accessor — epic-level API

```c
class OVT_ControllerComponent<Class T>
{
	static T Get()
	{
		return OVT_ComponentFinder<T>.Find(OVT_Global.GetController());
	}
}
```

Compile-verified against the live tree 2026-08-04 (`requirements.md:15-23`), both standalone and composed with `OVT_ComponentFinder<T>` (`Scripts/Game/Components/OVT_Component.c:10` — the in-repo precedent for generic *classes* standing in for EnforceScript's missing generic *methods*).

Call site: `OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get()`.

**The name and shape are load-bearing.** `options/requirements.md:41` and `player-groups/implementation.md:397` both name `OVT_ControllerComponent<Class T>.Get()` in prose as the contract future features bind to. Do not rename it.

Placement: new file `Scripts/Game/Components/Controller/OVT_ControllerComponent.c`, next to the components it locates.

### 3.3 Component roster

Ten new components, one extended. Naming follows the established `OVT_<DomainNoun>Component` convention; all live in `Scripts/Game/Components/Controller/`; all extend `OVT_Component` unless they report progress.

| # | Component | Phase | RPCs | Delegates to |
|---|---|---|---|---|
| 1 | `OVT_VehicleRequestComponent` | P2 | 6 | `OVT_VehicleManagerComponent`, `OVT_EconomyManagerComponent` |
| 2 | `OVT_RealEstateRequestComponent` | P3 | 8 | `OVT_RealEstateManagerComponent` |
| 3 | `OVT_EconomyRequestComponent` | P4 | 7 (+1 `RpcDo`) | `OVT_EconomyManagerComponent`, `OVT_SkillManagerComponent` |
| — | `OVT_ShopTransactionComponent` *(extended)* | P4 | +1 | `OVT_EconomyManagerComponent` |
| 4 | `OVT_ResistanceRequestComponent` | P5 | 6 (+1 `RpcDo`) | `OVT_ResistanceFactionManager`, `OVT_TownManagerComponent` |
| 5 | `OVT_FOBRequestComponent` | P5 | 7 | `OVT_ResistanceFactionManager` |
| 6 | `OVT_RecruitRequestComponent` | P6 | 4 | `OVT_RecruitManagerComponent` |
| 7 | `OVT_LoadoutRequestComponent` | P7 | 3 | `OVT_LoadoutManagerComponent` |
| 8 | `OVT_PossessionRequestComponent` | P7 | 1 (+1 `RpcDo`) | `SCR_PlayerController` |
| 9 | `OVT_JobRequestComponent` | P8 | 2 | `OVT_JobManagerComponent` |
| 10 | `OVT_CampaignRequestComponent` | P8 | 5 (+1 `RpcDo`) | `OVT_OccupyingFactionManager`, `OVT_PersistenceManagerComponent`, `OVT_TownManagerComponent` |

**Grouping rule: one seam per manager.** Warehouses live on `OVT_RealEstateManagerComponent`, so warehouse requests share `OVT_RealEstateRequestComponent` rather than getting a component of their own. The one deliberate exception is `OVT_FOBRequestComponent`, split out of `OVT_ResistanceFactionManager`'s seam because FOBs/camps are a carved-out feature (`resistance/fob`) with their own bug list, and a single 13-RPC resistance component would be the monolith in miniature.

Three RPCs land on components whose domain is a stretch (`DeliverMedicalSupplies`, `LootWantedCheck` on campaign). That is recorded, not hidden — see D8.

### 3.4 Validation pattern

Every migrated handler follows `OVT_ShopTransactionComponent.RpcAsk_SellItems` (`:112-161`) as its template:

```
1. if(!Replication.IsServer()) return;
2. request-shape sanity (quantity > 0, id in range, string non-empty)
3. int playerId = ResolveOwningPlayerId();  if(playerId <= 0) return;
4. resolve the caller's character (where the check needs one)
5. resolve every entity argument from its RplId; bail on any null
6. proximity / ownership / faction / permission / affordability
7. delegate to the manager singleton
```

`ResolveOwningPlayerId()` (`OVT_ShopTransactionComponent.c:657-678`) scans connected players for the one whose controller is `GetOwner()`. It is currently duplicated in `OVT_ShopTransactionComponent` and `OVT_AdminCommandsComponent`. **P1 hoists it into a shared base** — see T1.6 — so ten new components do not become ten more copies.

**Client-supplied identity parameters are dropped from the signature**, not merely ignored. `RpcAsk_BuyBuilding(int playerId, bool useResistanceFunds)` becomes `RpcAsk_BuyBuilding(bool useResistanceFunds)`. This is the single most valuable property of the migration: an identity argument that no longer exists cannot be trusted by a future edit.

**Advisory client checks stay advisory.** BUG-013's residue (BUG-078, still open) means seven difficulty fields — including `baseRange`, `baseCloseRange`, `fastTravelCost`, `minFastTravelDistance` — may hold prefab defaults on a client. Server-side checks must therefore be authored against the **server's** `OVT_Global.GetDifficulty()` values and must never assume the client used the same number. Where a client check exists for UX, it is left in place and duplicated (not replaced) server-side.

### 3.5 Owner-response routing

Four responses come back to one client. All use `RplRcver.Owner` **on the controller**, which is genuinely owner-routed because the controller entity is owned by that player — unlike the monolith, whose character-hosted instance made "Owner" mean "whoever controls this body".

| Response | Today | After |
|---|---|---|
| `RpcDo_SaveResult(bool)` | `RplRcver.Owner` on comms (`:70`) | `RplRcver.Owner` on `OVT_CampaignRequestComponent` |
| `RpcDo_ConvertSupporterResult(bool)` | `RplRcver.Owner` on comms (`:162`) | `RplRcver.Owner` on `OVT_ResistanceRequestComponent` |
| `RpcDo_DoneTakingMoney()` | `RplRcver.Owner` on comms (`:1084`) | `RplRcver.Owner` on `OVT_EconomyRequestComponent` |
| `RpcDo_OpenInventory(...)` | **`RplRcver.Broadcast`** + a client-side `localPlayerId != playerId` filter (`:1681-1687`) | `RplRcver.Owner`, filter deleted |

The `RpcDo_OpenInventory` change is the only routing *change*: broadcasting a per-player inventory command to every client and filtering it client-side is wasteful and fragile. Owner routing makes the filter unnecessary.

**Listen-server hazard, mandatory for all four.** An Owner-targeted RPC sent by a listen-server host to itself does not execute locally. `OVT_ShopTransactionComponent.SendSellResult` (`:629-638`) is the reference fix: when `playerId == SCR_PlayerController.GetLocalPlayerId()`, call the handler directly and do **not** send. Every owner response in this plan uses that helper shape.

### 3.6 Two latent authority bugs the migration must fix, not preserve

The project's established rule — `if (Replication.IsServer()) { direct call } else { Rpc(...) }` — exists because **an `RplRcver.Server` RPC marshalled by the authority goes nowhere** (the BUG-045 / BUG-052 / BUG-088 family). The monolith violates this in two places, both from inside server-side handlers:

**(a) `RpcAsk_Buy` does not take money or decrement stock.** At `OVT_PlayerCommsComponent.c:686-687`, the server-side buy handler does:
```c
Rpc(RpcAsk_TakePlayerMoney, playerId, actualCost);
Rpc(RpcAsk_TakeFromInventory, shopId, id, successfulPurchases);
```
`RpcAsk_BuyVehicle` at `:904-906` calls the *same two handlers directly* — someone fixed the vehicle path and not the item path. `OVT_ShopTransactionComponent.RestockShop` (`:601-604`) independently documents the same conclusion: *"`OVT_ShopComponent.AddToInventory` routes through a client→server ask on the legacy comms component, which is not a server-side call path."*

**(b) Three economy notifications never fire.** `SendNotification()` (`:76-79`) is `Rpc(RpcAsk_SendNotification, ...)` unconditionally, and its only three callers are inside server-side handlers (`:1009` PlayerDonated, `:1035` PlayerSentFunds, `:1062` PlayerSentMoney).

**Both are resolved by migrating correctly**, regardless of what the runtime turns out to do: the migrated handlers call `economy.DoTakePlayerMoney(...)`, mutate `shop.m_aInventory` + `StreamInventory(...)`, and call `OVT_Global.GetNotify().SendTextNotification(...)` directly. **P4 must confirm the before/after behaviour in the play-test** (buy an item, watch the balance and the stock corner; donate, watch for the notification) and record the verdict in `context.md` — if buying was free, that is a shipped economy exploit and belongs in `docs/bugs/`.

### 3.7 Dead RPCs — deleted, not migrated

Verified repo-wide (all file types, excluding `.git`):

| Method | Evidence | Disposition |
|---|---|---|
| `SetBuildingHome` / `RpcAsk_SetBuildingHome` (`:422-435`) | Zero callers anywhere. `economy/real-estate/implementation.md:160` already lists it as dead code. | **Delete.** Real estate's planned `IsHome`/`SetAsHome` fix (`real-estate/implementation.md:175`) re-adds a *validated* `SetBuildingHome` to `OVT_RealEstateRequestComponent` when that work lands — cheaper than carrying an unvalidated endpoint through ten phases. Record this in `real-estate/context.md` during P3. |
| `RestorePossessedEntity` / `RpcAsk_RestorePossessedEntity` (`:1765-1797`) | Zero callers. Superseded by `SCR_PlayerController.RequestRestorePossession()` (`Scripts/Game/Player/Modded/SCR_PlayerController.c:4`), which is what the client actually calls at `:1756`. | **Delete.** |
| `SendNotification` / `RpcAsk_SendNotification` (`:76-85`) | Zero external callers; three internal server-side calls. As a network endpoint it lets any client send any notification to any `playerId` with no validation at all. | **Delete the RPC**; the three internal calls become direct `OVT_Global.GetNotify().SendTextNotification(...)`. |

Two more are **not client endpoints** and become plain server-side code rather than migrating:

| Method | Reality | Disposition |
|---|---|---|
| `RpcAsk_AddToInventory` (`:928`) | Reached only from `OVT_ShopComponent.AddToInventory` (server-side manager code, `OVT_ShopComponent.c:47`) and from inside `RpcAsk_BuyVehicle`. | Becomes a server-side method on `OVT_ShopComponent`; RPC deleted. |
| `RpcAsk_TakeFromInventory` (`:951`) | Same, via `OVT_ShopComponent.TakeFromInventory` (`:52`). | Same. |

**Net: 55 `RpcAsk_` → 50 migrate, 3 delete, 2 become server-side methods.** All 4 `RpcDo_` migrate.

---

## 4. Implementation Phases

Estimates assume one agent per phase. **Automated gates run at the end of every phase; the MP play-test runs once, in P10.** That is the user's explicit choice: per-domain MP sessions would cost nine round-trips for a mechanical refactor whose risk is concentrated in the seam (P1) and the deletion sweep (P10).

Each phase is independently shippable: after it, the tree compiles, all tests pass, and the migrated domain works — because the monolith's version of that RPC is deleted in the same phase, there is never a two-implementations window.

**Every phase ends with the same four gates** (stated once, not repeated per phase):
- `tools/compile-check.sh` → exit **0**
- `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) → exit **0**, ≥ **101** cases
- `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) → exit **0**, ≥ **142** cases
- `grep -rn "OVT_Global.GetServer()" Scripts/` shows the expected reduced count for that phase

*(Baselines measured 2026-08-14: Fast **101**, All **142**, both exit 0. Never take these from a doc — re-measure.)*

**⚠️ `Rpc()` arity is a compile-check blind spot.** `Rpc()` is an untyped variadic prototype: a wrong argument count compiles clean and dies silently at the wire. Every phase moves RPC signatures, and most *shrink* them (dropping identity parameters). Mandatory per phase: after editing, `grep -n "Rpc(Rpc" <newfile>` and hand-check each call's arity against its handler. This is why the P10 play-test is itemised per domain.

---

### Phase 1 — Seam hardening ⚠️ ADVANCED AGENT

*Every later phase stands on this. It also settles a question no automated test can reach (Continue ≠ connect) and touches a hook two shipped features already consume.*

**Agent:** `network-specialist-advanced`
**Estimate:** 8-12 h + one short user play-test (T1.5 only).

**Tasks**

1. **T1.1 Generic accessor.** New file `Scripts/Game/Components/Controller/OVT_ControllerComponent.c` with the §3.2 snippet verbatim. Doc-comment it as the epic-level seam and cite the two features that bind to the name.
2. **T1.2 Delete the six per-domain getters.** Remove `GetContainerTransfer`, `GetShopTransactions`, `GetTowerSabotage`, `GetTravelRequests`, `GetRespawnRequests`, `GetTutorials` from `OVT_Global.c:135-204` and re-point all **32** call sites (`GetTutorials` 11, `GetContainerTransfer` 8, `GetShopTransactions` 5, `GetRespawnRequests` 5, `GetTravelRequests` 2, `GetTowerSabotage` 1) to `OVT_ControllerComponent<T>.Get()`. Preserve every existing null guard at the call site — the accessor still returns null on a dedicated server and before owner assignment.
3. **T1.3 `LootBattlefield()` wrapper.** `OVT_Global.c:210-217` is domain logic on the locator. Move the body to its single call site (`OVT_Global.LootBattlefield` has **1** caller) and delete the wrapper.
4. **T1.4 Null-guard the survivors.** `GetUI()` (`:77-81`) dereferences `SCR_PlayerController.GetLocalControlledEntity()` with no null check — a guaranteed VME with no body. `GetDifficulty()` (`:229-232`) dereferences `GetConfig()` unguarded, and on a client `m_ConfigFile` is null until `RplLoad`, so both the accessor **and** its sub-objects need guarding. Add guards; audit the **2** `GetUI` and **6** `GetDifficulty` call sites for now-reachable null returns.
5. **T1.5 Settle Continue ≠ connect — evidence, not assumption.** `PrepareConnectedPlayers()` **does** call `SetupPlayer()` for an unmapped connected player (`OVT_OverthrowGameMode.c:624-628`), and `SetupPlayer` spawns the controller — so the wiring exists. What is unverified is that it *works*: `persistence/context.md:93` still owes *"save → restart → Continue, then check the balance, a shop purchase, and the camp/recruit menus (which ride the controller)"*, and `OVT_TEST_Campaign_ContinuePlayerIdMapping` asserts the ID mapping only, never the controller entity.
   - Extend that Campaign case (or add a sibling) to assert `OVT_Global.GetPlayers().GetController(playerId)` is non-null *and not deleted* after `PrepareConnectedPlayers()`. Prove it can fail by removing the `SetupPlayer` call.
   - **Two specific traps to check and fix if real:** (a) `SetupPlayer`'s already-mapped branch (`OVT_PlayerManagerComponent.c:648-655`) never revalidates the stored entity with `IsDeleted()`, unlike `CleanupPlayerController` (`:777`) — a stale map entry silently keeps a dead controller; (b) `CheckDisconnectedPlayers` deletes the controller with a raw `delete controller` (`:779`) rather than `RplComponent.DeleteRplEntity`, which may leave clients holding a replica.
   - **User play-test (short, SP/listen host only):** save → quit → Continue → confirm balance, one shop purchase, and the camp menu. Dedicated servers Continue at boot, pre-connect, and are immune — a dedicated-green round proves nothing here.
   - Write the verdict into `docs/features/core/controller-migration/context.md` (create it).
6. **T1.6 Shared caller-identity base.** `ResolveOwningPlayerId()` is duplicated in `OVT_ShopTransactionComponent.c:657-678` and `OVT_AdminCommandsComponent`. Hoist it into a shared base — either `OVT_Component` (if no non-controller component would inherit something nonsensical) or a new `OVT_ControllerRequestComponent : OVT_Component` that the ten new components extend. **Prefer the latter**: it also gives one home for `ResolveEntity(RplId)`, `GetEntityRpl(IEntity)` and the listen-server-safe owner-response helper (§3.5), all three of which are already duplicated. Re-point the two existing components; leave `OVT_BaseServerProgressComponent`'s own hierarchy alone.
7. **T1.7 Cache the controller at owner assignment — with the fallback kept.** `OVT_Global.GetController()` (`:114-130`) already has a no-controlled-entity fallback via `GetLocalPlayerId()` → `GetPlayers().GetController()`, so this is an optimisation and robustness change, **not a hole fix**. Add a `static OVT_OverthrowController s_LocalController` set in `OVT_OverthrowController.RpcDo_NotifyOwnerAssignment` (`:26-40`); `GetController()` returns it when non-null and not deleted, otherwise falls through to the existing two-step lookup unchanged.
   - **⚠️ Listen-server trap:** an Owner-targeted RPC to the local host may not execute, so on a listen server `RpcDo_NotifyOwnerAssignment` may never run and a cache set *only* there would be null forever for the host. The map-lookup fallback must therefore stay, and the cache must never be treated as authoritative. Do not "simplify" `GetController()` down to the field read.
8. **T1.8 Make `RpcDo_NotifyOwnerAssignment` idempotent and document its real contract.** It is **not** once-per-player. `AssignControllerOwnership` (`OVT_PlayerManagerComponent.c:678-710`) fires it unconditionally, and `SetupPlayer` calls that on **both** branches — fresh spawn (`:671`) and already-mapped reconnect/Continue (`:653`). It also fires even when the `GiveExt` did not happen (invalid `RplIdentity`, missing `RplComponent`, PlayerController not ready) because the notify block sits outside those guards (`:699-704`).
   - Make `OVT_AdminCommandsComponent.RegisterChatCommands()` idempotent (guard flag, or `Remove()` before `Insert()`), since it currently double-registers on any second assignment.
   - Rewrite the doc comment on `RpcDo_NotifyOwnerAssignment` to state "fires once per ownership assignment, which may happen more than once per player (reconnect, Continue) — consumers must be idempotent", and fix the stale *"This runs exactly once"* comment at `OVT_OverthrowController.c:35-37`.
9. **T1.9 Fix the skill doc.** `.claude/skills/overthrow-architecture/overthrow-controller.md:514-528` step 4 still says *"Add convenience accessor to `OVT_Global`"*. Replace with the generic accessor, and note that no `OVT_Global` getter is ever added.

**Acceptance criteria** (in addition to the four standard gates)

- `OVT_Global.c` contains **zero** per-domain controller getters and no `LootBattlefield`. `grep -c "static OVT_.*Component Get" Scripts/Game/Global/OVT_Global.c` counts only the 17 frozen manager forwarders.
- `OVT_ControllerComponent<T>.Get()` resolves all seven existing components from a client.
- A new/extended Campaign-tier case asserts a non-null, non-deleted controller after `PrepareConnectedPlayers()`, with its proven-to-fail method recorded in the file's preamble.
- `context.md` records the Continue verdict with log evidence (`Created controller entity for player N` present or absent after the reload).
- `GetUI()` and `GetDifficulty()` return null rather than VME with no controlled entity / no config.
- `OVT_AdminCommandsComponent.RegisterChatCommands()` called twice registers one set of commands.

---

### Phase 2 — Vehicles

**Agent:** `network-specialist`
**Estimate:** 6-8 h.

**New:** `OVT_VehicleRequestComponent`

| Monolith | New method | Validation carried / added |
|---|---|---|
| `RpcAsk_SetVehicleLock(RplId, bool)` `:334` | `RpcAsk_SetVehicleLock(RplId, bool)` | **BUG-087 fix, carry verbatim** (`:345-357`): caller uid resolved server-side; `playerOwner.GetPlayerOwnerUid() == callerUid`; `VEHICLE_MAX_DISTANCE` 15 m. The character-vs-game-mode branch collapses: on the controller the caller is *always* a player, so the checks become unconditional. **The persistence reservation model's "locked stays hidden" split depends on this holding.** |
| `RpcAsk_ClaimUnownedVehicle(RplId, int)` `:370` | `RpcAsk_ClaimUnownedVehicle(RplId)` | Carry `currentOwner == ""`, 15 m proximity, `RegisterPlayerVehicle`. Drop the `playerId` param. |
| `RpcAsk_UpgradeVehicle(RplId, int)` `:1269` | `RpcAsk_UpgradeVehicle(RplId, int)` | **Currently zero validation** — a bare forward to `GetVehicles().UpgradeVehicle`. **Add:** valid resource id; caller owns or may use the vehicle (reuse `PlayerMayUseVehicle`, `OVT_ShopTransactionComponent.c:708-720`); proximity; **affordability** — confirm against `OVT_VehicleManagerComponent.UpgradeVehicle` whether it charges, and add the check here if it does not. |
| `RpcAsk_RepairVehicle(RplId)` `:1282` | `RpcAsk_RepairVehicle(RplId)` | **Currently zero validation.** Add the same ownership/proximity/affordability trio. |
| `RpcAsk_ImportToVehicle(int,int,RplId,int)` `:776` | `RpcAsk_ImportToVehicle(int,int,RplId)` | **BUG-033/BUG-102 fixes, carry verbatim** (`:778-847`): qty 1-100; `IsValidResourceId` and not a vehicle; not an occupying-faction item; `"Import"` permission; `IsSoldAtAnyNonVehicleShop` or `"IllegalImports"`; player **and** vehicle within `IMPORT_MAX_PORT_DISTANCE` 30 m of a port; affordability. **Every rejection keeps its notification** — a bare return is what made BUG-102 read as a dead button. |
| `RpcAsk_BuyVehicle(RplId,int,int)` `:869` | `RpcAsk_BuyVehicle(RplId,int)` | Carry `SHOP_MAX_DISTANCE` 30 m, procurement pricing, affordability, parking-spot fallback. **Fix §3.6(a):** replace `RpcAsk_TakePlayerMoney` / `RpcAsk_TakeFromInventory` calls with `economy.DoTakePlayerMoney(...)` and a direct `shop.m_aInventory` decrement + `StreamInventory(...)`. |

**Also:** re-point `OVT_LockVehicleAction`, `OVT_UnlockVehicleAction`, `SCR_GetInUserAction` (modded), `OVT_ManageVehicleContext` (×2), `OVT_PortContext`, `OVT_ShopContext` (BuyVehicle). Delete the six handlers + wrappers from the monolith. **User wires the component onto `OVT_OverthrowController.et`.**

**Acceptance criteria:** standard gates; `GetServer()` sites ≤ **52**; `UpgradeVehicle`/`RepairVehicle` reject a request naming a vehicle the caller neither owns nor is standing next to; every removed handler is gone from the monolith (not commented out).

---

### Phase 3 — Real estate + warehouses

**Agent:** `component-developer`
**Estimate:** 6-8 h.

**New:** `OVT_RealEstateRequestComponent`

| Monolith | New method | Validation |
|---|---|---|
| `RpcAsk_SetHome(int)` `:414` | `RpcAsk_SetHome()` | Carry: caller has a controlled entity. |
| `RpcAsk_SetBuildingHome` `:429` | — | **Deleted** (§3.7). Note the disposition in `economy/real-estate/context.md`. |
| `RpcAsk_BuyBuilding(int,bool)` `:443` | `RpcAsk_BuyBuilding(bool)` | Carry: nearest building resolved server-side; not owned/rented; officer check for resistance funds; funds check on the right account. |
| `RpcAsk_SellBuilding(int,bool)` `:479` | `RpcAsk_SellBuilding(bool)` | Carry: officer + `"resistance"` owner for resistance funds; `IsOwner`; not `IsHome`; **not the caller's last house** (`m_mOwned[persId].Count() == 1`). |
| `RpcAsk_RentBuilding(int,bool)` `:514` | `RpcAsk_RentBuilding(bool)` | Carry the full `isOwner` / `IsHome` / `IsRented` / `IsOwned` lattice and the conditional charge. |
| `RpcAsk_StopRentingBuilding(int,bool)` `:562` | `RpcAsk_StopRentingBuilding(bool)` | Carry `IsRenter` + officer branch. |
| `RpcAsk_AddToWarehouse(int,string,int)` `:1396` | `RpcAsk_AddToWarehouse(int,string,int)` | **Currently zero validation.** Add: `count > 0`; warehouse id in range; **resource string is a registered resource** (`economy.IsRegisteredResource`) — today any client can add any arbitrary string in any quantity to any warehouse. |
| `RpcAsk_TakeFromWarehouse(int,string,int)` `:1407` | same | **Currently zero validation.** Same three, plus stock available. |
| `RpcAsk_TakeFromWarehouseToVehicle(int,string,int,RplId)` `:1419` | same | Carry `qty > 0` + warehouse range. **Add:** registered resource; caller near the vehicle; caller may use the vehicle. |

**T3.x — `OVT_Global` helpers move to their manager** (requirements.md:33). `OVT_Global.TransferToWarehouse()` (`:572-620`) and `TakeFromWarehouseToVehicle()` (`:621-646`) mutate warehouse state and spawn items — gameplay logic on the static locator. Move both onto `OVT_RealEstateManagerComponent` as public server-side methods; the new component delegates to the manager, not to a static. Both have exactly **1** call site each, so this is contained.

**Also:** re-point `OVT_RealEstateContext` (×6), `OVT_SetHomeAction`, `OVT_WarehouseContext`, and the client branches of `OVT_RealEstateManagerComponent.AddToWarehouse` (`:506`) / `TakeFromWarehouse` (`:540`). **User wires the component.**

**Acceptance criteria:** standard gates; `GetServer()` sites ≤ **43**; a warehouse request naming an unregistered resource string is rejected; `OVT_Global.c` no longer contains either warehouse helper.

---

### Phase 4 — Economy, money and shop purchase

**Agent:** `network-specialist`
**Estimate:** 8-10 h.

**New:** `OVT_EconomyRequestComponent`. **Extended:** `OVT_ShopTransactionComponent`.

| Monolith | Destination | Validation |
|---|---|---|
| `RpcAsk_Buy(RplId,int,int,int)` `:595` | **`OVT_ShopTransactionComponent.RpcAsk_BuyItems(RplId,int,int)`** | Carry: `SHOP_MAX_DISTANCE` 30 m; server-derived unit cost; affordability; the spawn/insert/equip loop; partial-purchase and failure notifications. **Fix §3.6(a)** — direct `economy.DoTakePlayerMoney` + direct stock decrement. **Add:** `IsValidResourceId`, `num` bounded (1..100, matching Import). |
| `RpcAsk_AddToInventory` `:928` | — | **RPC deleted**; becomes a server-side method on `OVT_ShopComponent` (§3.7). |
| `RpcAsk_TakeFromInventory` `:951` | — | Same. |
| `RpcAsk_SellDrugs(int,RplId)` `:728` | `OVT_EconomyRequestComponent` | Carry `DEALER_MAX_DISTANCE` 10 m + the `DrugsWeed_01` filter. |
| `RpcAsk_DonateToResistance(int,int)` `:998` | `RpcAsk_DonateToResistance(int)` | Carry `amount > 0` + affordability. **Fix §3.6(b):** direct notification call. |
| `RpcAsk_SendResistanceFunds(int,int,int)` `:1021` | `RpcAsk_SendResistanceFunds(int toPlayerId, int amount)` | Carry officer check, `ResistanceHasMoney`, target-exists. Sender resolved from the controller. Direct notification. |
| `RpcAsk_SendMoneyToPlayer(int,int,int)` `:1047` | `RpcAsk_SendMoneyToPlayer(int toPlayerId, int amount)` | Carry `amount > 0`, `from != to`, affordability, target-exists. Direct notification. **⚠️ BUG-016 note:** `toPlayerId` is a *runtime* id from the client. Resolve it to a persistent id and verify the record exists **in the same tick** — a stale id can alias a different joiner. |
| `RpcAsk_TakePlayerMoney(int,int)` `:1074` + `RpcDo_DoneTakingMoney` `:1085` | `RpcAsk_TakePlayerMoney(int amount)` + owner response | Carry `amount > 0` and the always-reply contract (the `takingMoney` latch never clears otherwise). Owner response uses the listen-server-safe helper. |
| `RpcAsk_SetResistanceTax(float)` `:1096` | same | Carry officer gate + 0..1 clamp. |
| `RpcAsk_BuySkill(int,string)` `:179` | `RpcAsk_BuySkill(string key)` | Manager already validates (BUG-032 fix at `OVT_SkillManagerComponent.c:116-125`: spendable points, level cap, skill exists). Carry the caller resolution; do **not** duplicate the manager's checks. |

**Also:** re-point `OVT_ShopContext` (×2), `OVT_ResistanceMenuContext` (×3), `OVT_SellDrugsAction`, `OVT_CharacterSheetContext`, `OVT_ShopComponent` (×2), and the client branches of `OVT_EconomyManagerComponent.TakePlayerMoney` (`:1160`) / `SetResistanceTax` (`:1127`). **User wires `OVT_EconomyRequestComponent`.**

**Acceptance criteria:** standard gates; `GetServer()` sites ≤ **31**; **the §3.6 verdict is recorded in `context.md`** — buying an item debits the balance and decrements the shop's stock corner, and donating produces the `PlayerDonated` notification.

---

### Phase 5 — Resistance operations and FOBs

**Agent:** `component-developer`
**Estimate:** 8-10 h.

**New:** `OVT_ResistanceRequestComponent`, `OVT_FOBRequestComponent`.

`OVT_ResistanceRequestComponent`:

| Monolith | Validation |
|---|---|
| `RpcAsk_PlaceItem(int,int,vector,vector,int)` `:1114` | Drop `playerId`. **Currently a bare forward.** Add: indices in range; `pos` within a sane radius of the caller's character (placement is a client-side ghost — the position is entirely client-supplied today). |
| `RpcAsk_RemovePlacedItem(RplId,int)` `:1126` | Drop `playerId`. Confirm `RemovePlacedItem` checks ownership; if not, add it here. |
| `RpcAsk_BuildItem(int,int,vector,vector,int)` `:1139` | Drop `playerId`. Same index + proximity checks as `PlaceItem`. |
| `RpcAsk_AddOfficer(int,int)` `:1156` | Drop `promoterId`. Carry: promoter is an officer; target is not already one; target record exists. |
| `RpcAsk_AddGarrison(int,int,int)` `:1179` | Drop `playerId`. **Currently a bare forward.** Add: `baseId` valid; `prefabIndex` in `m_aGroupPrefabSlots` range; caller may buy garrison at that base. |
| `RpcAsk_ConvertSupporter(RplId)` `:124` + `RpcDo_ConvertSupporterResult` `:163` | **BUG-063 fixes, carry verbatim:** `CONVERT_MAX_DISTANCE` 10 m; `CONVERT_COOLDOWN_MS` 2000 rate limit; `TryMarkCivilianConvertAttempted` once-per-civilian; server-side diplomacy roll. Owner response via the listen-server-safe helper. |

`OVT_FOBRequestComponent`:

| Monolith | Validation |
|---|---|
| `RpcAsk_AddGarrisonCamp(vector,int,int)` `:1193` | Carry the 50 m camp-position check + registry-null guard. Add prefab-index range. |
| `RpcAsk_AddGarrisonFOB(vector,int,int)` `:1212` | Carry the 50 m FOB check. Add prefab-index range. |
| `RpcAsk_DeployFOB(RplId,int)` `:1235` | Drop `playerId`. **Bare forward** — add proximity + vehicle-usable checks. |
| `RpcAsk_UndeployFOB(RplId,int)` `:1254` | Same. |
| `RpcAsk_SetPriorityFOB(RplId)` `:1950` | Carry entity resolution. **Add:** caller is an officer (this changes global FOB priority for everyone). |
| `RpcAsk_SetCampPrivacy(vector,bool)` `:1887` | **Zero validation** — any client can flip any camp's privacy from anywhere. Add: camp exists near `pos`; caller owns it or is an officer; proximity. |
| `RpcAsk_DeleteCamp(RplId,vector)` `:1934` | **Zero validation** — any client can delete any camp. Add: camp resolves; caller owns it or is an officer; proximity. Keep the client-side `QueryEntitiesBySphere` camp-finding in the *client* wrapper. |

**Also:** re-point `OVT_PlaceContext` (×2), `OVT_BuildContext` (×2), `OVT_ResistanceMenuContext` (AddOfficer), `OVT_BaseMenuContext`, `OVT_FOBMenuContext` (×2), `OVT_CampMenuContext` (×2), `OVT_ConvertSupporterAction`, `OVT_DeployFOBAction`, `OVT_UndeployFOBAction`, `OVT_SetPriorityFOBAction`. **Delete `OVT_UndeployFOBAction_New.c`** — it is a commented-out migration example whose "OLD WAY" is the thing being deleted. **User wires both components.**

**Acceptance criteria:** standard gates; `GetServer()` sites ≤ **16**; a client request to delete or unprivate a camp it does not own is rejected; `OVT_UndeployFOBAction_New.c` is gone.

---

### Phase 6 — Recruits

**Agent:** `component-developer`
**Estimate:** 4-6 h.

**New:** `OVT_RecruitRequestComponent`

| Monolith | Validation |
|---|---|
| `RpcAsk_RecruitCivilian(RplId,int)` `:1295` | Drop `playerId`. Carry verbatim: `CIV` faction only; not already a recruit; not a possessed character; 20 m; explicit affordability (`DoTakePlayerMoney` clamps at zero); charge only after the recruit exists. |
| `RpcAsk_RecruitFromTent(vector,int)` `:1342` | Drop `playerId`. Carry: `CanRecruit` cap; `NearestTownHasSupporters`; 20 m; affordability at half cost; delete the spawned civilian if `RecruitCivilian` fails. |
| `RpcAsk_RenameRecruit(string,string)` `:1808` | Carry ownership check (BUG-052 family — this RPC exists because the client-side rename never reached the authority). Manager validates length 1-32. Simplify: the `senderId > 0` branch becomes unconditional. |
| `RpcAsk_DismissRecruit(string)` `:1838` | **No ownership check at all today — any client can dismiss any player's recruit.** Add: `recruit.m_sOwnerPersistentId == caller's persistent id`. Carry the group-removal + entity-delete + `RemoveRecruit` sequence. |

**Also:** re-point `OVT_RecruitCivilianAction`, `OVT_RecruitFromTentAction`, `OVT_RecruitsContext` (×2). **User wires the component.**

**Acceptance criteria:** standard gates; `GetServer()` sites ≤ **12**; a dismiss request naming another player's recruit is rejected server-side.

---

### Phase 7 — Loadouts, possession and inventory ⚠️ ADVANCED AGENT

*The hairiest cluster. Possession has a stateful client-side lifecycle (invoker subscription, 300 ms deferred restore, BUG-147) that must survive the move intact, and the loadout apply path is the one place a bug hands out free kit (BUG-043).*

**Agent:** `component-developer-advanced`
**Estimate:** 8-12 h.

**New:** `OVT_LoadoutRequestComponent`, `OVT_PossessionRequestComponent`.

`OVT_LoadoutRequestComponent` — all three currently take a client-supplied `string playerId` and launder it through `ResolveSenderPersistentId`. **Drop the parameter entirely:**

| Monolith | New signature | Validation |
|---|---|---|
| `RpcAsk_SaveLoadout(string,string,string,bool)` `:1496` | `RpcAsk_SaveLoadout(string name, string description, bool isOfficerTemplate)` | Carry: caller's own persistent id only (BUG-043); caller has a character. **Add:** name length bound and non-empty. |
| `RpcAsk_LoadLoadoutFromBox(string,string,RplId,RplId)` `:1552` | `RpcAsk_LoadLoadoutFromBox(string name, RplId boxId, RplId targetId)` | Carry verbatim: `LOADOUT_BOX_MAX_DISTANCE` 20 m from **both** sender and target to the box; target is the sender **or one of the sender's own recruits**. On the controller the sender is always a player, so the `senderCharacter` branch becomes unconditional — **this is a tightening, and it is intended**. |
| `RpcAsk_DeleteLoadout(string,string,bool)` `:1614` | `RpcAsk_DeleteLoadout(string name, bool isOfficerTemplate)` | Carry BUG-043 ownership. |

Preserve the deliberate absence of a no-box `LoadLoadout` endpoint (`:1531-1533`) — it was a free-item exploit and must not reappear.

`OVT_PossessionRequestComponent`:

| Monolith | New | Notes |
|---|---|---|
| `RpcAsk_SetPossessedEntityAndOpenInventory(int,RplId)` `:1645` | `RpcAsk_SetPossessedEntityAndOpenInventory(RplId)` | **No ownership check today** — any client can possess any entity's inventory. **Add:** target is one of the caller's own recruits (`OVT_RecruitData.GetRecruitDataFromEntity(...).m_sOwnerPersistentId`); proximity. |
| `RpcDo_OpenInventory(RplId,int,RplId)` `:1682` | `RpcDo_OpenInventory(RplId targetEntityId)` | **Broadcast → `RplRcver.Owner`.** Delete the `localPlayerId != playerId` filter and the now-redundant `playerControllerId`. The server's belt-and-braces direct call at `:1676` becomes the listen-server-safe helper. |
| `RpcAsk_RestorePossessedEntity` `:1772` | — | **Deleted** (§3.7). |

**Carry the client-side lifecycle verbatim** (`:1713-1762`): `m_PossessedInventoryManager`; the **one-shot** invoker subscription (`Remove` in the handler — every Open Inventory used to stack another); the **300 ms `CallLater`** before `RequestRestorePossession()` (BUG-147, verified in play 2026-08-13 — the close event fires at menu-teardown *start*, and unpossessing under a live menu pins the recruit's facing forever). Do not "clean this up".

**Also:** re-point `OVT_LoadoutsContext` (×3), `OVT_SaveLoadoutAction`, `OVT_SaveOfficerLoadoutAction`, `OVT_OpenInventoryCommand`. **User wires both components.**

**Acceptance criteria:** standard gates; `GetServer()` sites ≤ **5**; a possess-inventory request naming an entity the caller does not own is rejected; opening and closing a recruit's inventory three times in a row produces exactly three restore requests (not 1+2+3), and the recruit's facing is normal afterwards.

---

### Phase 8 — Jobs and campaign actions

**Agent:** `component-developer`
**Estimate:** 5-7 h.

**New:** `OVT_JobRequestComponent`, `OVT_CampaignRequestComponent`.

| Monolith | Destination | Validation |
|---|---|---|
| `RpcAsk_AcceptJob(int,int,int,int)` `:1434` | `OVT_JobRequestComponent` | Drop `playerId`. Carry the public/private ownership branch. |
| `RpcAsk_DeclineJob(int,int,int,int)` `:1461` | same | Drop `playerId`. Same branch. |
| `RpcAsk_StartBaseCapture(vector)` `:193` | `OVT_CampaignRequestComponent` | **BUG-025 fixes, carry verbatim:** no QRF already active; caller alive; **position taken from the caller's character, not the payload**; base is occupying-held; within `baseCloseRange`. Drop the `vector` parameter entirely — it is ignored for remote callers today and there are no other callers once the game-mode copy is gone. |
| `RpcAsk_InstantCaptureBase(vector,int)` `:225` | same | `#ifdef WORKBENCH`-only debug cheat (BUG-025). **Keep the guard.** Its one caller is the DiagMenu at `OVT_OverthrowGameMode.c:683` — a server-side call, so on the controller it becomes a plain method, not an RPC. Prefer: **make it a non-RPC `#ifdef WORKBENCH` method** and drop the network endpoint completely. |
| `RpcAsk_DeliverMedicalSupplies(RplId)` `:261` | same | **Add:** caller near the vehicle; caller may use the vehicle. Carry the drug-shop item filter, per-item delete-then-count, the support/stability modifier loop and the sound. |
| `RpcAsk_LootWantedCheck()` `:95` | same | **⚠️ Owner changes.** Today the handler relies on `GetOwner()` being the caller's character (`:99-104`) to find `OVT_PlayerWantedComponent`. On the controller it must resolve `playerId` → controlled entity → the wanted component. **Add:** the character exists and is alive. Re-point `OVT_PlayerWantedComponent.c:364-366`. |
| `RpcAsk_RequestSave()` `:31` + `RpcDo_SaveResult(bool)` `:71` | same | **Preserve the client invoker contract exactly:** `GetOnSaveResult()` must exist on the new component, and `OVT_MainMenuContext.c:295-327` must keep its subscribe-before-call ordering and its `m_SaveRequestComms` one-shot removal (BUG-006 — the menu used to claim success unconditionally). Carry the `Remove`-then-`Insert` on `GetOnSaveFinished()`. **Add:** caller is an officer (the menu already gates on it client-side at `:290`). Owner response via the listen-server-safe helper. |
| `SendNotification` / `RpcAsk_SendNotification` `:76-85` | — | **Deleted** (§3.7). Its three internal callers were re-pointed in P4. |

**Also:** re-point `OVT_JobManagerComponent` client branches (`:137`, `:172`), `OVT_CaptureBaseAction`, `OVT_DeliverMedicalSuppliesAction`, `OVT_MainMenuContext` (×2), `OVT_PlayerWantedComponent`, `OVT_OverthrowGameMode` (DiagMenu). **User wires both components.**

**Acceptance criteria:** standard gates; `GetServer()` sites = **0**; the save menu still reports a real success/failure and never claims success on send; `RpcAsk_SendNotification` no longer exists anywhere.

---

### Phase 9 — `OVT_Global` utility split

*Independent of the RPC work — can slot anywhere after P1. Scheduled here so it does not compete with the seam work for the same file.*

**Agent:** `component-developer`
**Estimate:** 4-6 h.

**Tasks**

1. **T9.1 `OVT_WorldUtils`** — `FindSafeSpawnPosition` (10 sites), `FindSafeVehicleSpawnPosition` (1), `FilterVehicleSpawnEntities`, `FilterSpawnPointEntities`, `s_SpawnPointSearchResults`, `SpawnEntityPrefab` (34), `SpawnEntityPrefabMatrix` (8), `SpawnCharacterEntity` (1), `IsOceanAtPosition` (3), `GetRandomNonOceanPositionNear` (8), `GetNearbyBodiesAndWeapons` (2), `FindNearestRoad` (7), `PlayerInRange` (13), `NearestPlayer` (**0 — delete**), `ResetAIAimState` (2).
2. **T9.2 `OVT_PrefabUtils`** — `GetPrefabName` (30), `GetVehicleUIInfo` (4), `GetEditableUIInfo` (3), `GetItemUIInfo` (12).
3. **T9.3 `OVT_LoadoutUtils`** — `ApplyCivilianLoadout` (1), `RandomizeCivilianClothes` (1), `RandomizeCivilianGroupClothes` (**0 — delete**), `SpawnDefaultCharacterItem` (1).
4. **T9.4 Thin forwarders stay** for the high-traffic three only (`requirements.md:34`): `SpawnEntityPrefab` **34**, `GetPrefabName` **30**, `PlayerInRange` **13** — one-line `return OVT_WorldUtils.X(...)` / `return OVT_PrefabUtils.X(...)`. Every other moved helper gets its call sites updated (77 sites total across the rest).
5. **T9.5 Leave `ShowHint` (4) and `GetLocalPersistentId` (6) on `OVT_Global`** — client-seam helpers, not utilities; they belong with the locator.
6. **T9.6 Update the test spine's `OVT_Global.*` uses** for anything not forwarded.

**Acceptance criteria:** standard gates; `OVT_Global.c` is under **400** lines; the two zero-caller helpers are deleted, not moved; the three forwarders are one-liners with no logic.

---

### Phase 10 — Delete the monolith + final MP play-test ⚠️ ADVANCED AGENT

*Mass deletion plus a repo-wide sweep. The compile check catches broken references but not a missed prefab component or a wrong `Rpc()` arity.*

**Agent:** `network-specialist-advanced`, then **user-driven play-test — M**
**Estimate:** 4-6 h agent + one full MP session.

**Tasks**

1. **T10.1 Delete `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c`.**
2. **T10.2 Delete `OVT_Global.GetServer()`** (`:67-75`).
3. **T10.3 Sweep.** `grep -rn "OVT_PlayerCommsComponent\|GetServer()" Scripts/ Prefabs/` returns nothing. Update the 7 doc-comment references in the existing controller components (they cite the monolith as "the legacy thing this replaces" — reword to past tense) and the **4** fact-check comments in `Language/localization_Overthrow.st` that cite `OVT_PlayerCommsComponent.c:<line>` (lines 1394, 2508, 2904 — **comments only; do not touch any `Ids{}`/`Texts{}` block, and never edit a `localization_Overthrow.<lang>.conf`**).
4. **T10.4 Prefab strip — USER.** Remove `OVT_PlayerCommsComponent` from **both** `Prefabs/GameMode/OVT_OverthrowGameMode.et` and `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et`.
5. **T10.5 Arity audit.** For every one of the ten new components: `grep -n "Rpc(Rpc" <file>` and hand-verify each call's argument count against its handler. This is the one class of defect the compile check cannot see.
6. **T10.6 Update the docs.** `epic-overview.md` rows :23/:27/:60; `game-mode`'s "controller migration stalled" headline debt; `overthrow-controller.md`'s component-creation checklist (:514-528).

**Acceptance criteria:** standard gates; the greps in T10.3 return zero; both prefabs open in Workbench without a missing-component warning; the §6 play-test checklist passes with zero defects.

---

### External — user / Workbench work

Prefab editing is done by the user in Workbench; the agent cannot do it. Each of these blocks its phase's play-testability but not its compile/test gates.

1. **P1** — short SP/listen-host play-test: save → quit → **Continue** → check balance, one shop purchase, the camp menu. (T1.5. A dedicated-server round does **not** substitute.)
2. **P2** — add `OVT_VehicleRequestComponent` to `Prefabs/GameMode/OVT_OverthrowController.et` (fresh GUID).
3. **P3** — add `OVT_RealEstateRequestComponent`.
4. **P4** — add `OVT_EconomyRequestComponent`.
5. **P5** — add `OVT_ResistanceRequestComponent` and `OVT_FOBRequestComponent`.
6. **P6** — add `OVT_RecruitRequestComponent`.
7. **P7** — add `OVT_LoadoutRequestComponent` and `OVT_PossessionRequestComponent`.
8. **P8** — add `OVT_JobRequestComponent` and `OVT_CampaignRequestComponent`.
9. **P10** — remove `OVT_PlayerCommsComponent` from **both** prefabs; run the full MP play-test in §6.

Existing controller components use the GUID series `6A7A1F3C…`, `6A7D9B2E…`, `6A7F3E5C…`, `6A83D5A0…`, `6B3A0000…`; allocate fresh ones in the same style.

---

## 5. Key Technical Decisions

**D1 — Generic class, not a generic method.** EnforceScript has no generic methods. `OVT_ControllerComponent<Class T>` with a `static T Get()` is the same trick `OVT_ComponentFinder<Class T>` already uses (`OVT_Component.c:10`), and it is compile-verified against the live tree. *Rejected:* one getter per domain — that is the pattern being deleted, and it would add ten more copies of controller → null-check → cast.

**D2 — Delete the six `OVT_Global` getters in P1, not incrementally.** Doing it up front means no phase has to decide which idiom to use, and the "no new `OVT_Global` getter" rule that `options` and `player-groups` already committed to becomes mechanically true from phase one. Cost: 32 call-site edits in a phase that touches nothing else.

**D3 — Identity parameters are deleted from signatures, not ignored.** `ResolveSenderPlayerId` laundering a client-supplied id is safe *today* only because the component sits on the caller's character. Deleting the parameter makes the safety structural. It also shrinks every wire payload. *Cost:* every re-pointed call site changes shape, which is why the arity audit (T10.5) is mandatory.

**D4 — One seam per manager, with one deliberate exception.** Warehouses ride `OVT_RealEstateRequestComponent` because they are `OVT_RealEstateManagerComponent` state. FOBs/camps get their own component despite sharing `OVT_ResistanceFactionManager`, because `resistance/fob` is a carved-out feature and a 13-RPC resistance component would recreate the monolith at 1/4 scale. Net: **10 components**, inside the approved 8-10 band.

**D5 — Shop *buying* joins `OVT_ShopTransactionComponent`; vehicle buying does not.** The sell component's own header already argues that implementing a shop's two halves separately guarantees one of them eventually forgets a rule. Buy and sell share the 30 m gate, the price model and the stock table, so they share a component. `BuyVehicle` and `ImportToVehicle` stay with vehicles: both end in a *spawned or loaded vehicle*, both need `OVT_VehicleManagerComponent` and parking resolution, and neither touches the shop stock model the same way. *Rejected:* a separate `OVT_ShopBuyComponent` (drift risk); putting `BuyVehicle` on the shop component (would split P2's vehicle-spawn work across two phases and two components).

**D6 — Dead RPCs are deleted, not migrated.** `SetBuildingHome`, `RestorePossessedEntity` and `SendNotification` have zero callers. Carrying an unvalidated endpoint through ten phases so a future feature *might* use it is exactly the debt this migration exists to clear. Real estate is explicitly notified (§3.7) that re-adding a validated `SetBuildingHome` is a one-method job when its `IsHome`/`SetAsHome` fix lands.

**D7 — The §3.6 latent bugs are fixed, not preserved.** "No behaviour change" is the governing rule everywhere except where current behaviour is a dropped packet from an authority-marshalled `RplRcver.Server` RPC. Migrating those correctly is not a behaviour change *request* — it is what a correct migration produces. The play-test records the before/after so the finding can be filed if it was live.

**D8 — Three RPCs land on a component whose domain is a stretch,** and this is recorded rather than solved by fragmentation. `DeliverMedicalSupplies` (towns) and `LootWantedCheck` (wanted system) sit on `OVT_CampaignRequestComponent` because neither has a better home and neither justifies a component of its own (YAGNI). If a future feature adds a towns or wanted seam, they move there — one method each.

**D9 — The controller cache never replaces the map lookup.** An Owner-targeted RPC to a listen-server host may not execute, so a cache set only in `RpcDo_NotifyOwnerAssignment` would be null forever for the host. `GetController()` keeps its existing two-step fallback (`:114-130`) verbatim, with the cache as a fast path in front of it.

**D10 — `RpcDo_NotifyOwnerAssignment` is once-per-*assignment*, not once-per-player.** Proven from code: `SetupPlayer` calls `AssignControllerOwnership` on both branches (`OVT_PlayerManagerComponent.c:653` and `:671`), and that fires the notify unconditionally (`:699-704`) — even when `GiveExt` did not happen. Rather than trying to make it fire once, the contract is documented as-is and **consumers are made idempotent**. `OVT_AdminCommandsComponent.RegisterChatCommands()` already double-registers on reconnect today.

**D11 — Extend the existing test tiers; do not add a tier.** The seam is world-dependent, so almost none of it is Logic-tier. Init tier gets "each new component resolves on a registered controller"; Campaign tier gets the Continue-controller assertion. Everything else is P10's manual pass.

---

## 6. Definition of Done

An evaluator with **no implementation context** should be able to verify every item below.

### Functional Criteria — per domain, from a pure client on a dedicated server

Each item names the request, what must work, and one specific invalid request that must be **rejected server-side**.

- **F1 Vehicles.** Lock/unlock your own vehicle works; claim an unowned vehicle by entering the driver's seat works; upgrade and repair work and charge. **Rejected:** a lock request naming a vehicle owned by another player (ownership check), and an upgrade request from 500 m away (proximity check).
- **F2 Real estate.** Buy, sell, rent and stop-renting a building work on both the personal and resistance accounts; Set Home works. **Rejected:** selling your last owned house (`m_mOwned.Count() == 1`), and a resistance-funds purchase by a non-officer (officer check).
- **F3 Warehouses.** Add/take/take-to-vehicle work from the warehouse menu. **Rejected:** a request naming a resource string that is not a registered resource, and a negative/zero count.
- **F4 Economy.** Buying an item at a shop **debits the balance and decrements the displayed stock**; drug selling pays; donate, send-resistance-funds and send-money-to-player all move money **and produce their notification**; the tax slider works. **Rejected:** send-money with `amount <= 0` or to yourself, and a tax change by a non-officer.
- **F5 Skills.** Buying a skill level works and is refused with no spendable points (manager check preserved). **Rejected:** a buy naming a skill key that does not exist.
- **F6 Resistance ops.** Place, build, remove-placed, add-officer and add-garrison work. **Rejected:** a place/build request whose position is far from the caller's character, and an add-officer from a non-officer.
- **F7 FOBs and camps.** Deploy, undeploy, set-priority, garrison-at-camp, garrison-at-FOB, camp privacy and delete-camp all work. **Rejected:** deleting or unprivating a camp the caller neither owns nor is an officer over, and a set-priority from a non-officer.
- **F8 Supporters.** Converting a civilian works, is rate-limited to one attempt per 2 s, is refused beyond 10 m, and each civilian can only be attempted once globally. The hint appears on the requesting client only.
- **F9 Recruits.** Recruit a civilian, recruit from a tent, rename and dismiss all work. **Rejected:** dismissing or renaming another player's recruit (ownership check), and recruiting with insufficient funds (no partial charge, no orphan civilian).
- **F10 Loadouts.** Save, apply-from-box and delete work, including applying to your own recruit standing at the box. **Rejected:** applying a loadout to another player's recruit, and applying to a target more than 20 m from the box.
- **F11 Possession.** Open a recruit's inventory from the commanding menu; close it; possession is restored and the recruit's facing is normal. Opening and closing three times produces exactly three restore requests. **Rejected:** a possess request naming an entity the caller does not own.
- **F12 Jobs.** Accept and decline work for public and private jobs.
- **F13 Campaign.** Starting a base capture works at a nearby occupying-held base and is refused while a QRF is active, when dead, or from out of range. Delivering medical supplies credits the town. Save-from-menu reports a **real** success or failure (never success-on-send) and is refused for a non-officer.
- **F14 Availability.** Every above request works for a client **who has no controlled entity at the moment of the request** where that is meaningful (menu-driven ones: shop, real estate, loadouts, save) — no VME, no silent no-op.
- **F15 Continue.** After save → quit → **Continue** on a listen host: balance is correct, a shop purchase works, and the camp and recruit menus open and function. (This is the outstanding BUG-104 debt; P1 settles it, P10 re-confirms it.)

### Quality Criteria

- **Q1** `tools/compile-check.sh` exits **0** with no `file:line: message` output.
- **Q2** `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) exits **0** with ≥ **101** cases; `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) exits **0** with ≥ **142** cases. *(Baselines measured 2026-08-14 — re-measure, never quote a doc.)*
- **Q3** Every new test case has a recorded proof it can fail (the exact edit used, in a preamble comment). **No `maxAttempts` anywhere.**
- **Q4** No migrated `RpcAsk_*` accepts a player id or persistent id as a parameter. `grep -n "RpcAsk_.*int playerId\|RpcAsk_.*string playerId" Scripts/Game/Components/Controller/` returns nothing.
- **Q5** Every migrated handler begins with `if(!Replication.IsServer()) return;` and resolves the caller via the shared `ResolveOwningPlayerId()`.
- **Q6** No VME on the death / pre-spawn / start-menu paths: opening the main menu, the shop menu and the loadout menu with no controlled entity produces no script error in the log.
- **Q7** No new `OVT_Global` getter. No new RPC on `OVT_PlayerCommsComponent` (it does not exist).
- **Q8** No user-facing string is hardcoded English where an `#OVT-` key belongs; new keys go in `Language/localization_Overthrow.st` only. `git diff --stat Language/` shows **only** `localization_Overthrow.st`.
- **Q9** Every server-side rejection either sends an existing notification or logs at `LogLevel.WARNING` with the reason. A rejected request is never indistinguishable from a dropped packet with no trace at all.
- **Q10** `Rpc()` arity hand-audited for all ten new components (T10.5), with the audit recorded in `context.md`.

### Integration Criteria

- **I1 Monolith gone.** `grep -rn "OVT_PlayerCommsComponent" Scripts/ Prefabs/` returns **0** results (from 58 occurrences / 21 files and 2 prefabs at plan time). `grep -rn "OVT_Global.GetServer()" Scripts/` returns **0** (from 60).
- **I2 `OVT_Global` shape.** Zero per-domain controller getters (from 6). No `LootBattlefield`. No `TransferToWarehouse` / `TakeFromWarehouseToVehicle`. The 17 manager forwarders are byte-identical. File under **400** lines (from 1,007).
- **I3 Conventions honoured.** Every new component is in `Scripts/Game/Components/Controller/`, named `OVT_<DomainNoun>Component`, wired on `Prefabs/GameMode/OVT_OverthrowController.et`, and reachable **only** via `OVT_ControllerComponent<T>.Get()` — the exact API `options/requirements.md:41` and `player-groups/implementation.md:397` bind to.
- **I4 No manager refactors.** `git diff --stat Scripts/Game/GameMode/Managers/` shows changes confined to: the two warehouse helpers arriving on `OVT_RealEstateManagerComponent`, and client-branch re-points in `OVT_EconomyManagerComponent`, `OVT_JobManagerComponent`, `OVT_RealEstateManagerComponent`. No domain logic moved into a controller component.
- **I5 Persistence untouched.** `Configs/Systems/Persistence/Overthrow.conf` and everything under `Scripts/Game/Persistence/` are unchanged. The controller entity is not persisted (it is rebuilt per session by `SetupPlayer`).
- **I6 Docs synced.** `epic-overview.md` rows for `core/controller-migration` and `core/game-mode` no longer describe the migration as stalled; `overthrow-controller.md`'s checklist no longer says "add a getter to `OVT_Global`".

### Verification Method

**Automated, from the repo root, in order:**

1. `tools/compile-check.sh` → exit 0, no output.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit 0, `run-tests: OK (N tests, …)` with N ≥ 101.
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit 0, N ≥ 142.
4. `grep -rn "OVT_PlayerCommsComponent" Scripts/ Prefabs/ | wc -l` → **0**
5. `grep -rn "OVT_Global.GetServer()" Scripts/ | wc -l` → **0**
6. `grep -cn "static OVT_.*Component Get" Scripts/Game/Global/OVT_Global.c` → **17** (manager forwarders only)
7. `wc -l Scripts/Game/Global/OVT_Global.c` → **< 400**
8. `ls Scripts/Game/Components/Controller/` → 17 files existing + new + `OVT_ControllerComponent.c`
9. `git diff --stat Language/` → only `localization_Overthrow.st`
10. `git diff --stat Configs/Systems/Persistence/ Scripts/Game/Persistence/` → empty

**Manual — one client on a dedicated server**, launched with `tools/launch-server.sh` then `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`. Run every step; a step that cannot be reached is a failure, not a skip. The server log must show **no** `Broken RPC` / arity errors and no VME throughout.

1. Connect and spawn. Open the main menu, the shop menu and the loadout menu. → **F14**, **Q6**
2. Buy 1 item at a general shop. **Check the balance changed by exactly the shown price and the stock corner decremented.** Buy 5 with room for 3. → **F4**, §3.6(a)
3. Sell items at the same shop (already-migrated path — regression check).
4. Buy a vehicle. Import 10 of an item to it at a port. Try importing 200 (rejected) and importing from 500 m away (rejected). → **F1**
5. Lock, unlock, upgrade and repair your vehicle. Have the server operator spawn a second owner, or use a vehicle you do not own, and attempt a lock → **rejected**. → **F1**
6. Buy a house, set it home, rent another, stop renting, sell a non-home house. Attempt to sell your last house → **rejected**. → **F2**
7. Add to and take from a warehouse; take to a vehicle. → **F3**
8. Donate to the resistance → **the `PlayerDonated` notification appears**. Send resistance funds and player money. → **F4**, §3.6(b)
9. Set the resistance tax as an officer; verify a non-officer is refused. → **F4**
10. Buy a skill level from the character sheet. → **F5**
11. Place an item, build an item, remove a placed item, promote an officer, buy a base garrison. → **F6**
12. Deploy a FOB, garrison it, set it priority, undeploy it. Create a camp, toggle privacy, garrison it, delete it. → **F7**
13. Convert a civilian; immediately try again (rate-limited); try the same civilian again (refused). → **F8**
14. Recruit a civilian and recruit from a tent. Rename and dismiss a recruit. → **F9**
15. Save a loadout at an equipment box; apply it to yourself; apply it to your own recruit; delete it. → **F10**
16. Open a recruit's inventory from the commanding menu, close it, **three times in a row**; check the recruit walks and faces normally afterwards. → **F11**
17. Accept a job; decline another. → **F12**
18. Start a base capture at a nearby occupying base; try again while the QRF runs (refused); try from 1 km away (refused). Deliver medical supplies. → **F13**
19. Save from the main menu as an officer; confirm the **real** result is reported. Confirm a non-officer is refused. → **F13**
20. Die and, while dead/awaiting respawn, open the main menu and the loadout menu. → **F14**
21. **Listen-host only:** save → quit → **Continue** → check the balance, buy an item, open the camp and recruit menus. → **F15**

---

## 7. Testing Strategy

**Automated coverage is a spine, not the surface.** Baseline 2026-08-14: **Fast 101**, **All 142**, both green. This feature is a networking refactor, so most of its risk sits precisely where the spine does not reach.

### Logic tier — almost nothing, honestly

There is no pure-maths surface here. The one candidate is any helper extracted during P9 that is world-free (e.g. a position-sanity predicate for `PlaceItem`/`BuildItem`). **Do not manufacture Logic cases for RPC bodies** — a case that has to stand up a world is not a Logic case.

### Init tier — the main automatable gain

One case per new component: **"the component type resolves off a registered controller."** The Init world registers controllers, so this genuinely runs. It catches the single most likely silent failure in this whole feature — **a component written, compiled and never added to the prefab**, which produces no compile error and no runtime error, just a request that never happens.

Add these **per phase**, not all at the end: P2 → 1 case, P3 → 1, P4 → 1, P5 → 2, P6 → 1, P7 → 2, P8 → 2. Prove each can fail by removing the component from the prefab (or by asserting against a deliberately wrong type).

Also Init: **`OVT_ControllerComponent<T>.Get()` resolves each of the seven pre-existing components** (P1) — this pins the generic accessor itself.

### Campaign tier — the Continue assertion

Extend `OVT_TEST_Campaign_ContinuePlayerIdMapping` (or add a sibling) to assert a **non-null, non-deleted** `OVT_OverthrowController` after `PrepareConnectedPlayers()`. Today it asserts the ID mapping and the balance only, and the failure message already *names* the missing controller as a consequence without checking it. Proven-to-fail method: remove the `SetupPlayer` call at `OVT_OverthrowGameMode.c:627`.

### Persistence tier — no change expected

The controller is session state, rebuilt by `SetupPlayer`, and is not persisted. **I5 asserts persistence files are untouched.** If any phase finds itself editing a serializer, that is a signal the migration has strayed out of scope.

### What stays manual, and why

**JIP/multiplayer, UI and the true Continue path are outside the spine.** Specifically uncoverable:

- Whether a moved `Rpc()` call has the right arity (the compile-check blind spot) — only the wire proves it.
- Whether an owner-routed response reaches the right client, and only that client.
- Whether a component is actually on the prefab *and owned by the requesting player* (the Init case proves presence, not ownership transfer).
- Whether the listen-server direct-call branch is taken correctly (a listen host is not the test world).
- Every rejection path — the spine has no adversarial client.

That is the entire content of §6's 21-step manual pass, itemised per domain so a failure names the phase that caused it.

---

## 8. Dependencies

**All scheduling dependencies are discharged.**

| Dependency | Status |
|---|---|
| 1.4.x settling — the monolith was being patched on `1.4.0-bugfixes` | **Discharged.** PR #152 merged 2026-08-04. |
| `economy/shop-ux` branch still adding `OVT_Global` controller getters (`epic-overview.md:51`) | **Discharged.** PR #154 merged; `OVT_ShopTransactionComponent` and its getter are in the tree and are P1's input, not a moving target. |
| BUG-012 / BUG-017 (`core/game-mode`) | **Closed.** No work owed. |
| BUG-025 / BUG-087 (client-trust family) | **Closed** — fixed on the monolith. This feature's job is to **carry those fixes forward**, verbatim, not to re-derive them. |
| BUG-013 / BUG-015 / BUG-016 | **Closed.** Not in scope to fix. BUG-016's aliasing hazard still shapes validation design (see R4). |

**Live dependencies (constraints, not blockers):**

- **`core/game-mode` + `core/player-manager` own the controller lifecycle.** `SetupPlayer` / `AssignControllerOwnership` / `CheckDisconnectedPlayers` live in `OVT_PlayerManagerComponent`; P1 edits them. Any change there must not regress reconnect.
- **BUG-078 (open)** — BUG-013's residue: unversioned positional JIP bitstream for difficulty. Server-side checks must use server values (§3.4); do not assume a client's difficulty read matches.
- **`core/options`** plans an `OVT_OptionsComponent` on the controller reached via this feature's accessor (`options/requirements.md:40-41`). It is not blocked — it lands its own component either way — but the accessor name must not change after P1.
- **`core/player-groups` (complete)** committed to "no new `OVT_PlayerCommsComponent` RPC, no new `OVT_Global` getter" (`implementation.md:397`, Q6, I3). This feature makes those rules mechanically enforceable.
- **`OVT_ReconnectComponent`** keeps disconnected bodies alive, so the character-hosted monolith currently survives disconnect on a reserved body. Deleting it in P10 removes that instance too — verify a reserved body still behaves (it should: nothing calls into the component on a body with no player).
- **Pattern reference:** `Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c` (validation order, `ResolveOwningPlayerId`, listen-server owner response) and `OVT_ContainerTransferComponent.c` (progress base).

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **`Rpc()` arity blind spot.** Wrong argument count compiles clean and dies silently at the wire. Every phase shrinks RPC signatures. | **High** | High — a dead request with no error anywhere | Mandatory per-phase `grep -n "Rpc(Rpc"` + hand-check (§4 preamble); T10.5 repeats it across all ten components; §6's 21-step pass exercises every migrated request at least once. |
| **R2** | **Continue ≠ connect.** A loaded save replaces the world with nobody connecting; if the controller is not re-spawned, every migrated request is dead for the rest of the session. Dedicated servers are immune, so a green MP round proves nothing. | Medium | **Critical** — the whole feature is inert after a Continue | P1/T1.5 settles it with an automated Campaign assertion **and** a listen-host play-test, before any domain moves. Two named traps (stale map entry not `IsDeleted()`-checked; raw `delete` instead of `DeleteRplEntity`) are checked explicitly. §6/F15 re-confirms at the end. |
| **R3** | **`RpcDo_NotifyOwnerAssignment` double-fire.** Proven from code: it fires on every `AssignControllerOwnership`, which `SetupPlayer` calls on both branches, and even when `GiveExt` failed. Consumers that `Insert()` into an invoker double-register. | **High** (already happening) | Medium — duplicated chat commands, doubled handlers | D10 / T1.8: document the real contract, make `RegisterChatCommands()` idempotent, and require idempotence of every future consumer. The P1 controller cache is a plain field write, so it is idempotent by construction. |
| **R4** | **BUG-016 runtime-id aliasing.** ID maps are session-scoped and reusable; a stale `toPlayerId` from a client can name a different joiner. `SendMoneyToPlayer` / `SendResistanceFunds` / `AddOfficer` all take a target runtime id. | Low | High — money or officer status to the wrong player | P4/P5: resolve target runtime id → persistent id **and verify the record exists in the same tick**, exactly as the monolith already does (`:1030-1031`, `:1057-1058`, `:1164-1165`). Carry that pattern; never cache a resolved target across frames. |
| **R5** | **Regression surface: 60 `GetServer()` re-points + 32 getter re-points across 38+ files.** A missed null guard turns an accessor that used to be non-null into a VME. | **High** | Medium | The accessor returns null in strictly *more* situations than `GetServer()` did on a client, never fewer. Every re-point preserves or adds a null guard; Q6 makes the death/pre-spawn paths an explicit gate. Phases are small and each ends green. |
| **R6** | **A component is written but never wired to the prefab.** No compile error, no runtime error — the request simply never happens. | **High** (10 user round-trips) | High | One Init-tier case per component asserting it resolves off a registered controller, added **in the same phase** as the component. This is the single highest-value automated case in the feature. |
| **R7** | **Prefab-wiring user round-trips block play-testability.** Ten components across seven phases. | Certain | Low | Batched in the External section with the exact prefab path and GUID style; compile + test gates never depend on the wiring, only the play-test does — and there is only one play-test. |
| **R8** | **A carried validation is dropped in transcription.** The monolith's checks encode closed bugs (BUG-025/032/033/043/063/087/102); losing one silently reopens an exploit. | Medium | **High** | The §4 tables are the checklist: every row names the checks to carry and cites the source lines. Reviewer diffs the new handler against the cited monolith range before the phase closes. Rejections are itemised in §6's functional criteria so an evaluator can probe them without reading code. |
| **R9** | **The §3.6 "fixes" turn out to change live behaviour** — e.g. buying items was free and now costs money, which players will notice. | Medium | Medium | Treated as a finding, not a silent change: P4 records the before/after verdict in `context.md` and files a bug if it was live. This is the correct outcome — an economy that does not charge is not a feature. |
| **R10** | **Scope creep into manager refactors.** Ten phases inside domain code is a standing temptation. | Medium | Medium | I4 pins `Scripts/Game/GameMode/Managers/` to exactly three allowed changes. Anything else fails the gate. "Thin seams, fat managers" is the rule; a component that grows domain logic is a defect. |
| **R11** | **P9's utility split collides with in-flight work in other epics** (`SpawnEntityPrefab` alone has 34 sites across many features). | Medium | Low | Thin forwarders stay for the three highest-traffic helpers, so ~60 files never change. P9 is independent and can be re-scheduled to a quiet window without blocking any other phase. |
| **R12** | **Possession lifecycle regression (BUG-147).** The 300 ms deferral and one-shot subscription are non-obvious and look like cruft. | Medium | Medium | P7 is an advanced agent; the plan states the carry-verbatim requirement and cites the runtime verification date. §6/F11's three-open-close probe is designed to catch exactly the stacked-subscription failure. |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Seam hardening (accessor, getters, lifecycle, Continue) | `network-specialist-advanced` | **yes** — epic-level API, edits the shared `OVT_Global` + the player-manager lifecycle, and settles a question no test can reach |
| 2 — Vehicles | `network-specialist` | no |
| 3 — Real estate + warehouses | `component-developer` | no |
| 4 — Economy, money, shop purchase | `network-specialist` | no — but it carries the §3.6(a)/(b) authority fixes; brief it explicitly |
| 5 — Resistance ops + FOBs | `component-developer` | no |
| 6 — Recruits | `component-developer` | no |
| 7 — Loadouts, possession, inventory | `component-developer-advanced` | **yes** — stateful client lifecycle (BUG-147), the free-kit exploit surface (BUG-043), a Broadcast→Owner routing change |
| 8 — Jobs + campaign actions | `component-developer` | no — but the save-invoker contract is fragile; brief it explicitly |
| 9 — `OVT_Global` utility split | `component-developer` | no |
| 10 — Delete the monolith + sweep | `network-specialist-advanced` | **yes** — mass deletion, repo-wide sweep, arity audit across ten components, two prefab strips |

**Skills to activate:** `enforcescript-patterns` (all phases), `overthrow-architecture` (all phases — especially `overthrow-controller.md`), `workbench-workflow` (all phases). No UI phase: `overthrow-ui-patterns` is only needed if a re-pointed context turns out to need layout work, which is not expected.

**No help-docs-sync phase.** This is an internal refactor: no player-facing behaviour changes except that invalid requests are rejected, which no tutorial or Field Manual page describes. The **exception to re-check at P10**: if §3.6's verdict is that buying items was free, that *is* a player-facing economy change and the docs/wiki pass becomes required — decide at P4, record in `context.md`.
