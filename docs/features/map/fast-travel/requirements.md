# Map Fast Travel — Requirements

**Epic:** map
**Created:** 2026-08-10

## Overview

This feature owns the map's **travel verbs**: fast travel and bus travel. It consolidates the eligibility rules, cost model and execution path into `OVT_FastTravelService` (`Scripts/Game/Services/OVT_FastTravelService.c`), surfaces travel from the location info panel and the `OverthrowFastTravel` keybinding, and migrates bus travel out of `OVT_MapContext` so the map is the single travel surface.

The service already exists on the branch with `CanGlobalFastTravel` / `CalculateFastTravelCost` / `ExecuteFastTravel`, and a keybinding is registered (`OverthrowFastTravel` — `keyboard:KC_SPACE`, `gamepad0:x`, in `MapContext`). Bus travel does **not** yet exist in the new system: it is still one of three flag-based modes inside legacy `OVT_MapContext`, entered from the world action `OVT_CatchBusAction` (`:10`).

## Requirements

- **One rule set, one cost model.** Fast-travel eligibility (wanted level, QRF proximity and the configured `QRFFastTravelMode`, minimum distance, per-type `m_bCanFastTravel`, anchor/ownership rules) and pricing (including any per-recruit fee) must live in `OVT_FastTravelService` and be the only implementation. No second copy in a UI context.
- **Displayed availability must match enforced availability.** When the info panel offers travel, the server must accept it; when it refuses, the panel must say so with the actual reason. A player must never be charged for travel that does not happen, nor be silently refused without explanation.
- **Server authority.** Travel executes server-side and is validated there — a client must not be able to teleport itself by driving the UI. The request must go through `OVT_Global.GetController()` on `OVT_OverthrowController`; **never** add a new client→server RPC to the deprecated `OVT_PlayerCommsComponent`.
- **Migrate bus travel onto the map.** `OVT_CatchBusAction` must put the player into a destination-selection state on the map, where bus-stop markers (from `map/location-types`) are the selectable targets; selecting one charges by distance and travels. The legacy `EnableBusTravel`/`m_bBusTravelActive` mode in `OVT_MapContext` is retired by this migration.
- **Bus mode must not survive a map close.** The legacy defect where an engine-side close left bus mode armed — so the next map click on a later open silently charged a fare (BUG-069, part 2) — must not be reproduced. Entering and leaving travel selection must be symmetric across every close path.
- Travel cost must be visible **before** committing, and the player's ability to pay checked before the map commits to the action.
- **Keybinding must be safe and console-legal.** Verify `OverthrowFastTravel` (`KC_SPACE` / `gamepad0:x`, `MapContext`) does not collide with a vanilla map binding — note that the project's input-conflict checker does not see inline `ActionContext` actions, so this needs manual confirmation, not just the script.
- The whole flow — select marker, read cost, confirm, travel — must be completable **on gamepad/console**.
- Must work in **multiplayer**, including a JIP client travelling shortly after joining, and must behave correctly when two players travel concurrently.
- Travel must interact correctly with recruits/groups: whatever the legacy system did about bringing followers along (and charging for them) must be preserved or deliberately changed with the change recorded.

## Dependencies

- **`map/core`** — element selection/click delegation and the info-panel hook the travel button lives on.
- **`map/location-types`** — destination markers must exist, including the migrated bus-stop component; fast travel also depends on per-type `m_bCanFastTravel` being correctly configured.
- **`core/controller-migration`** — the server-side request path on `OVT_OverthrowController`.
- **`occupying/core`** — QRF state and `QRFFastTravelMode` gating.
- **`resistance/wanted-system`** — wanted level gating.
- **`economy/*`** — the player's funds and the payment path.
- **`towns/core`** — bus fare distance pricing as currently implemented in `OVT_MapContext`.
- Blocks `map/legacy-retirement` (the `OVT_MapContext` travel modes cannot be stripped until this lands).

## Out of Scope

- **New travel mechanics.** Parity with what shipped — no route planning, no vehicles as fast-travel anchors, no new travel modes.
- **Rebalancing travel economics.** Cost formulas are preserved as-is; any tuning is a separate decision, not smuggled in under a refactor.
- **Deleting the legacy modes.** This feature *replaces* them functionally; the actual deletion of `EnableMapInfo`/`EnableFastTravel`/`EnableBusTravel` and their main-menu entries is `map/legacy-retirement`.
- **Adding the bus-stop marker type itself** — that is `map/location-types`; this feature consumes it.
