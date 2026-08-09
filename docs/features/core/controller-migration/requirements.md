# Controller Migration — Requirements

**Epic:** core
**Created:** 2026-08-03

## Overview

Migrate all player→server communication off the 1,776-line, 60-RPC `OVT_PlayerCommsComponent` monolith and onto domain components attached to `OVT_OverthrowController` — the per-player owned controller entity that is the engine-intended seam for client→server communication. The target pattern already exists and is proven: the controller entity is spawned per player by `OVT_PlayerManagerComponent`, and `OVT_ContainerTransferComponent` (extending `OVT_BaseServerProgressComponent`) is the one domain migrated so far. This feature finishes the job the game-mode discovery docs flagged as "controller migration stalled."

## Requirements

- **Controller lifecycle hardened first:** spawn, ownership assignment (`RpcDo_NotifyOwnerAssignment`), JIP/reconnect re-registration, and cleanup on disconnect are made robust and documented before any domain moves — every migrated RPC depends on this seam existing reliably for every player in every join order. Three specific items, scoped 2026-08-04:
  - **One generic accessor replaces the per-domain getters.** Each migrated domain currently adds a near-identical getter to `OVT_Global` (`GetContainerTransfer()`, `GetShopTransactions()`, plus the `LootBattlefield()` wrapper — domain logic that leaked onto the locator); ~10 domains would mean ~10 more copies of controller → null-check → `FindComponent` cast. EnforceScript has no generic *methods*, but generic *classes* work — `OVT_ComponentFinder<Class T>` (`Scripts/Game/Components/OVT_Component.c:10`) is the in-repo precedent. **Compile-verified against the live tree 2026-08-04** (1.7.0.54, 5901 files, exit 0), both directly and composed with `OVT_ComponentFinder<T>`:

    ```c
    class OVT_ControllerComponent<Class T>
    {
    	static T Get()
    	{
    		return OVT_ComponentFinder<T>.Find(OVT_Global.GetController());
    	}
    }
    ```

    Call site: `OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get()`. The three existing convenience getters are deleted, and the migration adds domains **without touching `OVT_Global` at all**.
  - **Cache the local controller at owner assignment instead of deriving it from the possessed body.** `OVT_Global.GetController()` (`OVT_Global.c:55-62`) resolves local controlled entity → `SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity()` → `GetPlayers().GetController(playerId)`, so it returns null whenever the player has no character — start menu, death, pre-spawn — which is exactly when shop/loadout UI wants the seam. `RpcDo_NotifyOwnerAssignment(playerId)` (`OVT_OverthrowController.c:26`) already hands the client both its playerId *and* its controller; cache it there so the accessor is a field read that does not depend on possession. Keep the map lookup as the fallback.
  - **Guard the unguarded derefs on the accessors that survive the feature.** `GetServer()` (`:42-43`) and `GetUI()` (`:48-49`) both call `player.FindComponent()` with no null check on the local controlled entity — a guaranteed VME with no body — and 52 of the 63 `GetServer()` sites chain the result unguarded as well. `GetDifficulty()` (`:111`) unguarded-derefs `GetConfig()`. `GetServer()` is deleted with the monolith so it needs no investment beyond that; `GetUI()` and `GetDifficulty()` outlive this feature and must be made null-safe.
- **Domain-by-domain migration, monolith shrinking each time:** the 60 RPCs split into targeted controller components. Candidate split from the RPC inventory (final grouping decided in `/plan-feature`): shops/economy (buy/sell/drugs/money transfer), real estate (buy/sell/rent/home), resistance ops (garrisons, FOBs, officers, supporters, tax, funds), recruits, loadouts, jobs, inventory/warehouse transfer, vehicles (claim/lock/repair/upgrade/import), fast travel, campaign actions (base capture, save requests).
- **Validation rides with the migration:** every migrated RPC lands with server-side validation (ownership, proximity, faction, affordability, rate-limiting as applicable). The migration is the vehicle for retiring the client-trust bug class (BUG-025 and the BUG-032/033/034/043/047/048/051/060/063 family) — holes are not ported, and validation already added to the monolith in 1.4.x patches must be preserved, not regressed.
- **Call sites re-pointed:** the 62 `OVT_Global.GetServer()` call sites migrate to controller/domain accessors as their domain moves; `OVT_PlayerCommsComponent` is **deleted at the end**, not left deprecated indefinitely.
- **Owner-targeted responses:** client-specific server→client responses (`RpcDo_OpenInventory`, `RpcDo_SaveResult`, money-taking flow) use `RplRcver.Owner` routing on the controller instead of the monolith's patterns.
- **Thin seams, fat managers:** controller components validate and delegate to the existing manager singletons — no domain logic moves into the components.
- **`OVT_Global` warehouse helpers move onto their manager:** `TransferToWarehouse()` and `TakeFromWarehouseToVehicle()` (`Scripts/Game/Global/OVT_Global.c:290-363`) are gameplay logic — they mutate warehouse state and spawn items — living on the static locator and called from the monolith. They move onto `OVT_RealEstateManagerComponent` as part of the real-estate domain's migration, so the new controller component validates and delegates to a manager method rather than to a static utility (the "thin seams, fat managers" rule above, applied to the one place it is currently violated).
- **`OVT_Global`'s utility half splits out:** ~470 of the file's 659 lines are utilities, not access — geometry/spawn helpers, prefab + `UIInfo` reflection, civilian loadout. They move to `OVT_WorldUtils` / `OVT_PrefabUtils` / `OVT_LoadoutUtils`, leaving `OVT_Global` as the locator + controller seam it is named for. Low-traffic helpers get their call sites updated; thin forwarders stay for the high-traffic three (`SpawnEntityPrefab` 23 sites, `GetPrefabName` 23, `PlayerInRange` 13) so the split does not churn ~60 files for cosmetics. Independent of the RPC work — can land in any phase.
- **No player-visible behavior change** except rejected-invalid-requests; per-domain JIP/multiplayer equivalence is play-tested (client on a dedicated server) with specific test steps per migrated domain, since MP is outside the automated test spine.
- Long-running operations that report progress extend `OVT_BaseServerProgressComponent`, as container transfer already does.

## Dependencies

- `core/game-mode` and `core/player-manager` discovery docs (controller spawn/registration live there; related debt BUG-012/BUG-017).
- **Schedule after 1.4.x settles:** `OVT_PlayerCommsComponent.c` is actively patched on the `1.4.0-bugfixes` branch (BUG-054 in progress at time of writing); starting the migration mid-patch-cycle would churn the same file from two directions. Critical exploit-class validation bugs (e.g. BUG-025 instant capture) should be hot-fixed on the monolith in 1.4.x — this feature then carries those fixes forward.
- Pattern reference: `Scripts/Game/Components/Controller/OVT_ContainerTransferComponent.c`.
- No dependency on the virtualization epic — can run in parallel with, before, or after it.

## Out of Scope

- Changing what any RPC does beyond adding validation — no new gameplay, and no manager-side refactors beyond the two `OVT_Global` moves named in the requirements.
- **Renaming or removing `OVT_Global`'s 17 manager forwarders** (`GetConfig` 313 call sites, `GetPlayers` 178, `GetTowns` 168 — ~900 in total, plus 152 uses across the test spine). They are consistent one-liners and the churn buys no behaviour change. New code should prefer `OVT_Global.GetX()` over `X.GetInstance()` (35 direct sites exist today), but existing sites are left alone.
- UI changes beyond re-pointing calls to the new accessors.
- Replication patterns elsewhere (RplProp state, JIP snapshots, manager broadcast RpcDo) — only the player→server request seam and its owner-targeted responses.
- Fixing non-RPC bugs in the domains being migrated (they stay with their feature's bug list).
