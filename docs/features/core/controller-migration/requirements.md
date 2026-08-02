# Controller Migration — Requirements

**Epic:** core
**Created:** 2026-08-03

## Overview

Migrate all player→server communication off the 1,776-line, 60-RPC `OVT_PlayerCommsComponent` monolith and onto domain components attached to `OVT_OverthrowController` — the per-player owned controller entity that is the engine-intended seam for client→server communication. The target pattern already exists and is proven: the controller entity is spawned per player by `OVT_PlayerManagerComponent`, and `OVT_ContainerTransferComponent` (extending `OVT_BaseServerProgressComponent`) is the one domain migrated so far. This feature finishes the job the game-mode discovery docs flagged as "controller migration stalled."

## Requirements

- **Controller lifecycle hardened first:** spawn, ownership assignment (`RpcDo_NotifyOwnerAssignment`), JIP/reconnect re-registration, and cleanup on disconnect are made robust and documented before any domain moves — every migrated RPC depends on this seam existing reliably for every player in every join order.
- **Domain-by-domain migration, monolith shrinking each time:** the 60 RPCs split into targeted controller components. Candidate split from the RPC inventory (final grouping decided in `/plan-feature`): shops/economy (buy/sell/drugs/money transfer), real estate (buy/sell/rent/home), resistance ops (garrisons, FOBs, officers, supporters, tax, funds), recruits, loadouts, jobs, inventory/warehouse transfer, vehicles (claim/lock/repair/upgrade/import), fast travel, campaign actions (base capture, save requests).
- **Validation rides with the migration:** every migrated RPC lands with server-side validation (ownership, proximity, faction, affordability, rate-limiting as applicable). The migration is the vehicle for retiring the client-trust bug class (BUG-025 and the BUG-032/033/034/043/047/048/051/060/063 family) — holes are not ported, and validation already added to the monolith in 1.4.x patches must be preserved, not regressed.
- **Call sites re-pointed:** the 62 `OVT_Global.GetServer()` call sites migrate to controller/domain accessors as their domain moves; `OVT_PlayerCommsComponent` is **deleted at the end**, not left deprecated indefinitely.
- **Owner-targeted responses:** client-specific server→client responses (`RpcDo_OpenInventory`, `RpcDo_SaveResult`, money-taking flow) use `RplRcver.Owner` routing on the controller instead of the monolith's patterns.
- **Thin seams, fat managers:** controller components validate and delegate to the existing manager singletons — no domain logic moves into the components.
- **No player-visible behavior change** except rejected-invalid-requests; per-domain JIP/multiplayer equivalence is play-tested (client on a dedicated server) with specific test steps per migrated domain, since MP is outside the automated test spine.
- Long-running operations that report progress extend `OVT_BaseServerProgressComponent`, as container transfer already does.

## Dependencies

- `core/game-mode` and `core/player-manager` discovery docs (controller spawn/registration live there; related debt BUG-012/BUG-017).
- **Schedule after 1.4.x settles:** `OVT_PlayerCommsComponent.c` is actively patched on the `1.4.0-bugfixes` branch (BUG-054 in progress at time of writing); starting the migration mid-patch-cycle would churn the same file from two directions. Critical exploit-class validation bugs (e.g. BUG-025 instant capture) should be hot-fixed on the monolith in 1.4.x — this feature then carries those fixes forward.
- Pattern reference: `Scripts/Game/Components/Controller/OVT_ContainerTransferComponent.c`.
- No dependency on the virtualization epic — can run in parallel with, before, or after it.

## Out of Scope

- Changing what any RPC does beyond adding validation — no new gameplay, no manager-side refactors.
- UI changes beyond re-pointing calls to the new accessors.
- Replication patterns elsewhere (RplProp state, JIP snapshots, manager broadcast RpcDo) — only the player→server request seam and its owner-targeted responses.
- Fixing non-RPC bugs in the domains being migrated (they stay with their feature's bug list).
