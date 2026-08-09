# Map Fast Travel - Implementation Plan (Retrospective)

**Status:** Partially Implemented — service extracted and wired, **server authority is inconsistent and bus travel is not migrated**
**Epic:** map (feature 3 of 7)
**Originally Implemented:** 2025-05 → 2025-08-02, on the `new-map` branch
**Documented:** 2026-08-10
**Last Updated:** 2026-08-10

---

## Executive Summary

`OVT_FastTravelService` (`Scripts/Game/Services/OVT_FastTravelService.c`, 171 lines) is a static class extracted from `OVT_MapContext` — its own header says so (`:2`). It holds the global fast-travel rule set, the distance-based cost model and the execution path, and it is the single implementation those rules have: all four travel-capable location types call `CanGlobalFastTravel` after applying their own gate, and the map's info-panel button calls `CalculateFastTravelCost` and `ExecuteFastTravel`.

The rule set and cost model are in good shape. **The execution path is not.** `ExecuteFastTravel` runs client-side and contains two different authority models in one function: the in-vehicle branch correctly asks the server, while the on-foot branch teleports the player and debits their money **on the client**. It also routes its one server call through `OVT_PlayerCommsComponent`, the component `CLAUDE.md` explicitly forbids adding to. And it silently dropped a shipped behaviour: recruits no longer travel with the player.

Bus travel has not been migrated at all — it remains one of three flag-based modes inside legacy `OVT_MapContext`.

**Note:** Retrospective plan created by reading code on the merged `new-map` branch. Claims are cited to `file:line`. Nothing observed at runtime; the branch has not been play-tested since 2025-08-02 and MP behaviour is inferred, not measured.

---

## Goals

### Primary Goals
- One rule set and one cost model for fast travel, shared by every location type.
- Travel driven from the map info panel rather than a separate menu mode.
- Server-authoritative execution.

### Success Criteria
- [x] Global rule set consolidated into one place
- [x] Distance-based cost model, surfaced on the button before committing
- [x] Every travel-capable location type delegates to it
- [x] Keybinding registered and wired to the panel button
- [ ] **Server-authoritative execution** — the on-foot path is client-side (F1)
- [ ] **Server-side payment** — money is debited client-side (F2)
- [ ] **Off the deprecated `OVT_PlayerCommsComponent`** (F3)
- [ ] **Recruits travel with the player** — regressed from legacy (F4)
- [ ] **Bus travel migrated** off `OVT_MapContext` (F5)

---

## Current Architecture

### The rule set — `CanGlobalFastTravel(targetPos, playerID, out reason)` (`:6-74`)

Evaluated in order, each returning a localized reason key:

| # | Check | Refusal reason |
|---|---|---|
| 0 | `m_bDebugMode` → returns `true` immediately (`:8-9`) | — |
| 1 | Local controlled entity exists | `#OVT-CannotFastTravelThere` |
| 2 | `dist >= m_Difficulty.minFastTravelDistance` | `#OVT-CannotFastTravelDistance` |
| 3 | `OVT_PlayerWantedComponent.GetWantedLevel() == 0` | `#OVT-CannotFastTravelWanted` |
| 4 | QRF active **and** `QRFFastTravelMode != FREE`: `DISABLED` refuses outright; otherwise target must be ≥ `QRF_RANGE` from `m_vQRFLocation` | `#OVT-CannotFastTravelDuringQRF` / `#OVT-CannotFastTravelToQRF` |
| 5 | Player can afford `CalculateFastTravelCost` | `#OVT-CannotAfford` |

### The cost model — `CalculateFastTravelCost(targetPos, playerID)` (`:77-95`)

`max(1.0, distance / 1000)` km × `m_Difficulty.fastTravelCost`, rounded. Zero in debug mode. Distance is measured from the **player's current entity**, so cost is origin-dependent, not a fixed per-destination price.

### Per-type gates layered on top

Owned by `map/location-types`, listed here because they complete the picture: `Base` refuses enemy-held, `Camp` refuses private-not-yours, `House` refuses not-owner-and-not-renter, `FOB` adds nothing, `Town` refuses unconditionally. Each then calls `CanGlobalFastTravel`.

### UI wiring

`OVT_OverthrowMapUI.SetupFastTravelButton` (`:449-520`):
- Hides the button entirely when the type's `m_bCanFastTravel` is false (`:471-477`).
- Otherwise shows it, enables/disables from `CanFastTravel`, and shows the refusal reason in a `FastTravelReason` widget.
- Sets the label to `#OVT-MainMenu_FastTravel` plus ` ($cost)` when cost > 0 (`:509-514`).
- Clears then re-inserts `m_OnActivated` (`:517-518`) — correctly avoids double-firing across panel rebuilds.

The keybinding `OverthrowFastTravel` (`keyboard:KC_SPACE`, `gamepad0:x`, in `MapContext`) is declared in `Configs/System/chimeraInputCommon.conf:650` and referenced by the `MapContext` action list (`:704`); the panel layout carries a matching `OverthrowFastTravel` widget bound through `SCR_InputButtonComponent`.

`OnFastTravelClicked` (`:523-539`) hides the panel, closes the map via `SCR_MapGadgetComponent.SetMapMode(false)`, then calls `ExecuteFastTravel`.

---

## Findings

### F1 — `ExecuteFastTravel` contains two different authority models (**high severity**)

`ExecuteFastTravel` (`:98-157`) is called from the client's map UI. Inside it:

- **In a vehicle, as driver** (`:129-141`): finds a safe vehicle spawn, then calls `OVT_Global.GetServer().RequestFastTravel(playerID, vehicleSpawnPos)` — a **server request**. Correct shape.
- **In a vehicle, as passenger** (`:142-146`): refused with `#OVT-MustBeDriver`.
- **On foot** (`:149-156`): calls `SCR_Global.TeleportPlayer(playerID, targetPos)` directly. That vanilla helper (`ArmaReforger/scripts/Game/Global/Functions.c:1638`) resolves the player entity and moves it **on the machine that called it** — here, the client.

So the two most common paths through one function use opposite authority models. The on-foot path is also the *default* path.

### F2 — Money is debited on the client (**high severity**)

Both branches call `economy.TakePlayerMoneyPersistentId(playerPersistentID, cost)` (`:139`, `:154`). That method (`OVT_EconomyManagerComponent.c:1168-1180`) mutates `player.money` in place and then calls `StreamPlayerMoney(playerId)` — the shape of a **server-side** authoritative write that pushes the new value to a client. Invoked from a client it mutates only the client's copy, which the server's next authoritative value overwrites.

Combined with F1, the likely MP outcome is that on-foot fast travel either does not move the player or does not charge them (or both), while appearing to work in single-player. **This is inference from code reading and must be confirmed on a real server** — it is the single highest-value verification in this feature.

### F3 — The one server call routes through the deprecated component

`OVT_Global.GetServer()` returns `OVT_PlayerCommsComponent` (`OVT_Global.c:67`), and `RequestFastTravel` lives at `OVT_PlayerCommsComponent.c:1483`. `CLAUDE.md` is explicit: never add new client→server RPCs there; new operations belong on a specialized component on `OVT_OverthrowController`. The service inherited this call from `OVT_MapContext` rather than introducing it, but the migration lands in this feature.

### F4 — Recruits no longer travel with the player (**parity regression**)

Legacy `OVT_MapContext` calls `RequestFastTravelWithRecruits(m_iPlayerID, pos, RECRUIT_TRAVEL_RADIUS)` (`:441`, `:503`) — gathering recruits within a radius and bringing them along, and `OVT_PlayerCommsComponent` implements both variants (`:1495-1501`). `OVT_FastTravelService` calls only `RequestFastTravel` (`:140`) and never the recruits variant. A player fast-travelling from the new map leaves their squad behind.

The epic's requirements flagged this as "must be preserved or deliberately changed with the change recorded" — it is currently changed by omission.

### F5 — Bus travel is not migrated

`OVT_FastTravelService` has no bus-travel code. Bus travel remains `m_bBusTravelActive` in `OVT_MapContext` (`:287-294`, click handling at `:451`), entered from the world action `OVT_CatchBusAction` (`:10`), pricing by distance between vanilla `MDT_BUSSTOP` descriptors found via `OVT_TownManagerComponent.GetNearestBusStop` (`:881`). Migration depends on `map/location-types` G4 (bus stops as an Overthrow marker component).

### F6 — Dead code

`CanFastTravelToLocationType` (`:160-170`) has **no callers**. Its body defers entirely to the location type's own `CanFastTravel` and its comment says as much. Either delete it or make it the single entry point the map UI uses.

### F7 — Affordability is checked twice, teleport target computed twice

`CanGlobalFastTravel` checks affordability (`:62-71`) and `ExecuteFastTravel` checks it again (`:110-114`) — harmless belt-and-braces, but the second check is the one that matters and it lives on the wrong side of the authority boundary. The on-foot branch also re-runs `OVT_Global.FindSafeSpawnPosition(targetPos)` (`:152`) after the button already validated the destination.

---

## Current State

### What's working
- One rule set, one cost model, one implementation — no duplication between the map UI and the location types.
- Reasons are localized keys surfaced directly on the panel, so refusals are explained rather than silent.
- Cost is shown on the button before the player commits.
- Button state (hidden / disabled / enabled) correctly distinguishes "this type never supports travel" from "you cannot travel right now".

### Known issues
F1–F7 above. F1/F2 are correctness issues in multiplayer; F4 is a player-visible regression; F3 is a standards violation; F5 is unfinished scope; F6/F7 are cleanliness.

### Technical debt
- The service is a **static class with no instance state**, which makes it easy to call from anywhere — including, as it turns out, from the wrong side of the network boundary. A server-side entry point would make the authority boundary explicit rather than implicit.
- `CalculateFastTravelCost` takes an `int playerID` while `CanGlobalFastTravel` takes a `string playerID` (persistent), forcing a conversion mid-function (`:58`). One identity type would be simpler.

---

## Testing

### Current coverage
None. No autotest suite covers fast travel. Nothing in the Fast (38) or All (76) groups touches it.

### Testing gaps — ranked
1. **On-foot fast travel on a dedicated server** (F1/F2). Two clients via `tools/launch-server.sh` + `tools/launch-game.sh --profile`. Verify the player actually moves *and* is actually charged, and that the server agrees.
2. **In-vehicle fast travel as driver** — the vehicle comes along, the passenger case is refused.
3. **Recruit accompaniment** (F4) — confirm the regression, then confirm the fix.
4. Each refusal rule in turn: wanted level, QRF active under each `QRFFastTravelMode`, below `minFastTravelDistance`, insufficient funds.
5. Gamepad: reach and activate the button, and confirm the `gamepad0:x` binding does not collide with a vanilla map action — the project's input-conflict checker cannot see inline `ActionContext` actions, so this needs manual confirmation.

---

## Dependencies

### Internal
- `map/core` — the info panel the button lives on and the click delegation.
- `map/location-types` — per-type gates; and **G4 (bus-stop component) blocks F5**.
- `core/controller-migration` — the destination for F3.
- `occupying/core` (QRF state, `QRF_RANGE`), `resistance/wanted-system` (wanted level), `economy/*` (funds, payment), `towns/core` (bus-stop discovery, bus fare distance pricing), `resistance/recruits` (F4).

### External
- Vanilla `SCR_Global.TeleportPlayer`, `SCR_MapGadgetComponent`, `CompartmentAccessComponent`, `SCR_InputButtonComponent`.

---

## Notes

**Discovered information**
- The service's own header comment — "Extracted from OVT_MapContext for reusability" (`:2`) — is accurate and explains the shape: the client-side authority model came along with the extraction rather than being introduced by it. The legacy context ran client-side too, but it called the *recruits* variant and did its teleport through the server request.
- Every travel-capable location type already funnels through `CanGlobalFastTravel`, so fixing the rules in one place fixes them everywhere. The rules are the healthy part of this feature.

**Retrospective assessment**
- *What works well:* consolidating the rule set and cost model was the right move and is genuinely done. Surfacing the refusal reason and the price on the button is better UX than the legacy menu flow.
- *What could be improved:* the extraction stopped at the rules and did not revisit the execution path, so a client-side teleport and a client-side money debit survived into a system that is meant to be server-authoritative. A static utility class made that easy to miss.
- *Lesson:* when extracting a service out of a client-side UI context, the authority boundary needs to be re-derived, not inherited. "It worked before" is not evidence in a codebase where the before was also client-side and the bug was latent.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature map/fast-travel` to begin fixes.*
