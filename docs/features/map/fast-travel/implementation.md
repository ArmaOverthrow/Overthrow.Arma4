# Map Fast Travel — Implementation Plan

**Status:** Planning
**Epic:** map (feature 3 of 7)
**Started:** 2026-08-10
**Target Completion:** TBD
**Last Updated:** 2026-08-10

> This file replaces the retrospective discovery document produced by `/discover-feature`. The
> discovered architecture and findings F1–F7 are preserved under **§3 Current State / Current
> Architecture**, corrected and extended with this session's code reading; everything after §3 is
> forward-looking. All `file:line` citations are load-bearing — keep them when editing.

---

## 1. Executive Summary

This feature makes Overthrow's two travel verbs — **fast travel** and **bus travel** — server-authoritative,
and moves bus travel onto the new map so the map is the single travel surface.

`OVT_FastTravelService` (`Scripts/Game/Services/OVT_FastTravelService.c`, 171 lines) already holds the
global rule set and cost model, and every travel-capable location type delegates to it. That half is
healthy and stays. **The execution path is the problem.** `ExecuteFastTravel` contains two opposite
authority models in one function (F1), debits money on the client (F2), routes its one server call through
the deprecated `OVT_PlayerCommsComponent` (F3), and silently dropped recruit accompaniment (F4). Bus travel
was never migrated and is still a flag-based mode in legacy `OVT_MapContext` (F5).

Two facts discovered while planning change the shape of the work beyond what the retrospective recorded:

1. **The rule set cannot be reused server-side as written.** `CanGlobalFastTravel` resolves the acting
   player with `SCR_PlayerController.GetLocalControlledEntity()` (`:14`). On a dedicated server that is
   null (fails closed, travel never works); on a **listen server** it is the *host's* character, so the
   server would validate the host's wanted level and the host's position against another player's request.
   Parameterizing the actor is therefore the **first** refactor, not a tidy-up.
2. **The existing server RPCs validate nothing.** `RpcAsk_RequestFastTravel`
   (`OVT_PlayerCommsComponent.c:1489-1493`) resolves the sender and calls `SCR_Global.TeleportPlayer` —
   no eligibility check, no distance check, no payment. Any client can teleport **itself anywhere, for
   free**, today. Requirement 16 is therefore not a refactor; the server side must gain validation it has
   never had.

The end state: one entity-parameterized rule/cost implementation shared by client and server, one new
specialized component on `OVT_OverthrowController` that validates, charges and teleports **atomically**,
bus eligibility **derived from the player's position** rather than an armed mode (so BUG-069 part 2 cannot
recur by construction), and a **player-controlled "bring recruits" toggle** that drives accompaniment and
fare together.

Completion is gated on a two-client MP/JIP play-test and a gamepad pass — the epic has no separate
verification feature. This feature **hard-gates `map/legacy-retirement`**.

---

## 2. Goals

### Primary

1. **Server authority for both verbs.** Validation, payment and teleport happen on the server, in one
   routine, driven from a specialized component on `OVT_OverthrowController` (F1/F2/F3, requirement 16).
2. **One rule set, one cost model, callable from both sides** — because requirement 15 ("displayed
   availability must match enforced availability") is only achievable if client and server run the *same*
   code, not two copies.
3. **Recruits travel with the player again** (F4), as an explicit **opt-out** choice rather than an
   automatic one, with the fare and the accompaniment driven by the same flag.
4. **Bus travel on the map** (F5), with eligibility derived from position and no armed mode.
5. **Correct in multiplayer and on JIP**, and **operable on gamepad/console**.

### Secondary

6. Delete `CanFastTravelToLocationType` (F6) and collapse the double affordability check into one advisory
   client check and one authoritative server check (F7).
7. Unify the service's identity types (`tasks.md:62`) — one player id form through the whole service.
8. Leave `map/legacy-retirement` a **precise list** of what is now dead.

### Explicit non-goals

- New travel mechanics: no route planning, no vehicles-as-anchors, no new travel modes
  (`requirements.md:38`).
- Fixing `map/core`'s D2/D3 (no working panel close) or D7 (`OnLocationClicked` unreachable — the panel
  appears via **hover**). Do not design against a click-to-open model that does not exist.
- Owning the per-type gates (`Base`, `Camp`, `House`, `FOB`, `Town`) — those are `map/location-types`.
- Adding the bus-stop marker type or its registry — `map/location-types` Phase 1 owns it; this feature
  **consumes** the published API.
- Deleting the legacy modes. This feature makes them unused; `map/legacy-retirement` deletes them.

---

## 3. Current State / Current Architecture

*(Discovered by reading the merged `new-map` branch. Nothing below has been observed at runtime — the
branch has not been play-tested since 2025-08-02. Corrections and additions made during planning are
marked **CORRECTED** / **NEW**.)*

### 3.1 The rule set — `CanGlobalFastTravel(targetPos, playerID, out reason)` (`:6-74`)

Evaluated in order, each returning a localized reason key:

| # | Check | Refusal reason |
|---|---|---|
| 0 | `m_bDebugMode` → returns `true` immediately (`:8-9`) | — |
| 1 | Local controlled entity exists (`:14`) | `#OVT-CannotFastTravelThere` |
| 2 | `dist >= m_Difficulty.minFastTravelDistance` (`:19-24`) | `#OVT-CannotFastTravelDistance` |
| 3 | `OVT_PlayerWantedComponent.GetWantedLevel() == 0` (`:27-32`) | `#OVT-CannotFastTravelWanted` |
| 4 | QRF active **and** `QRFFastTravelMode != FREE`: `DISABLED` refuses outright; otherwise target must be ≥ `QRF_RANGE` from `m_vQRFLocation` (`:35-51`) | `#OVT-CannotFastTravelDuringQRF` / `#OVT-CannotFastTravelToQRF` |
| 5 | Player can afford `CalculateFastTravelCost` (`:53-71`) | `#OVT-CannotAfford` |

All five refusal keys exist in `Language/localization_Overthrow.st` (`:669`, `:691`, `:720`, `:742`,
`:764`, plus `OVT-CannotAfford` `:559`).

### 3.2 The cost model — `CalculateFastTravelCost(targetPos, playerID)` (`:77-95`)

`round(max(1.0, dist/1000) × m_Difficulty.fastTravelCost)`. Zero in debug mode. Distance is measured from
the **player's current entity**, so cost is origin-dependent, not a fixed per-destination price.

**NEW — the two functions differ in exactly the way that matters.** `CalculateFastTravelCost` resolves the
actor with `GetGame().GetPlayerManager().GetPlayerControlledEntity(playerID)` (`:85`) — player-scoped, and
therefore correct on either machine. `CanGlobalFastTravel` resolves it with
`SCR_PlayerController.GetLocalControlledEntity()` (`:14`) — machine-scoped, and therefore **wrong on a
server**. Unifying the two on the parameterized form resolves both the reuse trap and the identity-type
inconsistency noted in `tasks.md:62` (`CanGlobalFastTravel` takes a persistent-id string,
`CalculateFastTravelCost` an int, and the service converts mid-function at `:58`).

### 3.3 Per-type gates layered on top

Owned by `map/location-types`, listed here because they complete the picture: `Base` refuses enemy-held,
`Camp` refuses private-not-yours, `House` refuses not-owner-and-not-renter, `FOB` adds nothing, `Town`
refuses unconditionally. Each then calls `CanGlobalFastTravel`. **This feature owns the global rules only.**

### 3.4 UI wiring (works — preserve it)

`OVT_OverthrowMapUI.SetupFastTravelButton` (`:449-520`):

- Hides the button entirely when the type's `m_bCanFastTravel` is false (`:471-477`) — "this type never
  supports travel".
- Otherwise shows it, enables/disables from `CanFastTravel`, and shows the refusal reason in a
  `FastTravelReason` widget (`:488-502`) — "you cannot travel right now".
- Appends ` ($cost)` to `#OVT-MainMenu_FastTravel` when cost > 0 (`:509-514`).
- Clears then re-inserts `m_OnActivated` (`:517-518`) — correctly avoids double-firing across panel
  rebuilds.

`OnFastTravelClicked` (`:523-539`) hides the panel, closes the map via `HideMap()`, then calls
`ExecuteFastTravel`.

The panel layout carries `FastTravelButton` (a `ButtonWidget` with `SCR_InputButtonComponent`,
`m_sActionName "OverthrowFastTravel"`, `UI/Layouts/Map/Core/OVT_MapInfoPanel.layout:190-199`) and
`FastTravelReason` (`:202-204`). The keybinding `OverthrowFastTravel`
(`keyboard:KC_SPACE`, `gamepad0:x`, `MapContext`) is declared at `Configs/System/chimeraInputCommon.conf:650`
and referenced by the `MapContext` action list (`:704`) with `ActionRefs +{ }` — an **additive delta** onto
vanilla's `MapContext`.

Interaction model (from `map/core`): **hover shows, click pins, click-empty dismisses**
(`OVT_OverthrowMapUI.c:44-69`).

### 3.5 Findings

#### F1 — `ExecuteFastTravel` contains two different authority models (**high severity**)

`ExecuteFastTravel` (`:98-157`), called from the client's map UI:

- **In a vehicle, as driver** (`:129-141`): finds a safe vehicle spawn, then
  `OVT_Global.GetServer().RequestFastTravel(playerID, vehicleSpawnPos)` — a **server request**.
- **In a vehicle, as passenger** (`:142-146`): refused with `#OVT-MustBeDriver`.
- **On foot** (`:149-156`): `SCR_Global.TeleportPlayer(playerID, targetPos)` directly — moves the entity
  **on the machine that called it** (`ArmaReforger/scripts/Game/Global/Functions.c:1638`). On foot is the
  *default* path.

**NEW — the in-vehicle path is correct; do not "fix" it.** Vanilla's `TeleportPlayer` explicitly teleports
the *vehicle* when the player is in one (`Functions.c:1657-1663`, "When in a vehicle, teleport the vehicle
instead"). The driver branch genuinely brings the vehicle along. Keep
`OVT_Global.FindSafeVehicleSpawnPosition` (`OVT_Global.c:373`) for the driver case and keep the
`#OVT-MustBeDriver` refusal for passengers.

#### F2 — Money is debited on the client (**high severity**)

Both branches call `economy.TakePlayerMoneyPersistentId(playerPersistentID, cost)` (`:139`, `:154`). That
method (`OVT_EconomyManagerComponent.c:1168-1181`) mutates `player.money` in place and calls
`StreamPlayerMoney(playerId)` — the shape of a **server-side** authoritative write that pushes the new
value to a client. Invoked from a client it mutates only the client's copy, which the server's next
authoritative value overwrites.

#### F3 — The one server call routes through the deprecated component

`OVT_Global.GetServer()` returns `OVT_PlayerCommsComponent` (`OVT_Global.c:67-75`); `RequestFastTravel`
lives at `OVT_PlayerCommsComponent.c:1483`. `CLAUDE.md` and `overthrow-controller.md` are explicit: new
client→server operations belong on a specialized component on `OVT_OverthrowController`.

#### F4 — Recruits no longer travel with the player (**parity regression**)

Legacy `OVT_MapContext` calls `RequestFastTravelWithRecruits(m_iPlayerID, pos, RECRUIT_TRAVEL_RADIUS)`
(`:441`, `:503`); `OVT_FastTravelService` calls only `RequestFastTravel` (`:140`). A player fast-travelling
from the new map leaves their squad behind.

#### F5 — Bus travel is not migrated

Bus travel remains `m_bBusTravelActive` in `OVT_MapContext` (`:287-295`, click handling at `:451-510`),
entered from the world action `OVT_CatchBusAction` (`:10`), pricing by distance to a
`SCR_MapDescriptorComponent` found via `OVT_TownManagerComponent.GetNearestBusStop` (`:881`).

#### F6 — Dead code

`CanFastTravelToLocationType` (`:160-170`) has **no callers**; its body defers entirely to the location
type's own `CanFastTravel`.

#### F7 — Affordability is checked twice, teleport target computed twice

`CanGlobalFastTravel` checks affordability (`:62-71`) and `ExecuteFastTravel` checks it again (`:110-114`).
The on-foot branch also re-runs `OVT_Global.FindSafeSpawnPosition(targetPos)` (`:152`) after the button
already validated the destination.

### 3.6 NEW findings from planning (2026-08-10)

These are not in the retrospective and several change the shape of the work.

- **N1 — 🔴 `CanGlobalFastTravel` cannot run on a server.** See §3.2. On a dedicated server it returns
  `false` (silent, fails closed); on a **listen server** it validates the *host's* wanted level and
  position against another player's request — this project's known listen-server bug class (cf. commit
  `46b0b470`, "Listen-server host never receives its own owner-targeted RPCs"). **This refactor must land
  before or with F1.**
- **N2 — 🔴 The existing server RPCs validate nothing.** `RpcAsk_RequestFastTravel`
  (`OVT_PlayerCommsComponent.c:1489-1493`) is `ResolveSenderPlayerId` + `TeleportPlayer`.
  `RpcAsk_RequestFastTravelWithRecruits` (`:1500-1548`) is the same plus recruit gathering.
  `ResolveSenderPlayerId` does prevent a client teleporting *another* player, but **any client can
  teleport itself anywhere, for free, today**. This is the security core of the feature.
- **N3 — Payment is never atomic with the teleport.** Neither RPC takes money; all payment is the
  client-side `TakePlayerMoneyPersistentId` at `:139`/`:154`.
- **N4 — `TeleportPlayer` can fail after the money is gone.** It returns `false` when the destination is
  outside terrain bounds (`Functions.c:1640-1641`) or the player entity cannot be resolved (`:1643-1645`).
  Today the client has already debited before calling. Requirement 15 says a player "must never be charged
  for travel that does not happen".
- **N5 — CORRECTION: legacy fast travel *did* charge per recruit.** `OVT_MapContext.c:387-397` adds
  `recruitCount × fastTravelCost` ("Same cost per recruit", `:395`) before charging, exactly as the bus
  branch does at `:464-472`. The retrospective and the feature brief both recorded fast-travel recruits as
  free; **the code says otherwise**. What legacy did *not* do is scale by distance: its base fare is a flat
  `m_Difficulty.fastTravelCost` (`:384`), whereas the shipped service uses
  `max(1 km, dist/1000) × fastTravelCost` (`:91-94`). See K3 — this makes the fare decision
  parity-restoring in structure and inflationary only through the distance term the branch already shipped.
- **N6 — Legacy bus travel applies none of the fast-travel rules.** The bus branch (`:451-510`) checks only:
  destination is near a bus stop (`#OVT-NeedBusStop`), affordability (`#OVT-CannotAfford`), and not in a
  vehicle (`#OVT-MustExitVehicle`). No wanted level, no QRF, no minimum distance. **Preserve that** —
  requirement `requirements.md:38` forbids new travel mechanics, and adding a wanted check to buses would
  be one.
- **N7 — Legacy never tested the player's *origin* against a bus stop.** The origin was guaranteed
  implicitly by `OVT_CatchBusAction` living on the bus-stop sign prefab; `GetNearestBusStop(pos)` (`:453`)
  tests the **clicked destination**. With no armed mode, the origin test becomes explicit — and the
  existing `#OVT-NeedBusStop` text ("You must click near another bus stop",
  `Language/localization_Overthrow.st:4354`) no longer fits the origin sense. A new id is needed.
- **N8 — Bus fare has no 1 km floor.** `round((dist/1000) × busTicketPrice)` (`:462`), measured from the
  player's position to the destination. Fast travel has the floor; bus does not. Preserve both.
- **N9 — The recruit count is client-readable, but this is unverified in MP.**
  `GetPlayerRecruitsInRadius` (`OVT_RecruitManagerComponent.c:246-274`) resolves entities through
  `FindRecruitEntity` (`:1640-1670`), which has an explicit **client branch** over `m_mRplIdToRecruit`, and
  the whole recruit table reaches clients through `RplSave`/`RplLoad` (`:2053`, `:2111`, and the comment at
  `:323`). Legacy already relied on this client-side at `:393`/`:469`. Whether a dedicated-server client
  gets the same count as the server is a **play-test question** (see R4).
- **N10 — 🔴 `OverthrowFastTravel`'s `gamepad0:x` collides with a vanilla map action.** Vanilla binds
  `MapContextualMenu` to `mouse:button1` + **`gamepad0:x`** inside `ActionContext MapContext`
  (`ArmaReforger/Configs/System/chimeraInputCommon.conf:8178`), and `SCR_MapRadialUI` — the module that
  consumes it (`scripts/Game/Commanding/SCR_PlayerControllerCommandingComponent.c:246`) — is in vanilla's
  `MapFullscreen.conf` module list (`:17`) and is **not** disabled by Overthrow's same-GUID delta
  (`Configs/Map/MapFullscreen.conf:2-33`). Requirement 20 asked for this check; here it is, found by
  reading rather than by the script (which cannot see inline `ActionContext` actions).
  `keyboard:KC_SPACE` is **free** in vanilla's `MapContext` (its keyboard bindings are NUMPAD2/4/6/8,
  DELETE, K, O, B, N, ADD, SUBTRACT).
- **N11 — Pad-button space inside `MapContext` is nearly exhausted.** Used by vanilla: `a` (MapSelect /
  MapMultiSelectGamepad), `x` (contextual menu), `pad_left` (tool menu focus), `pad_up` (watch, pencil),
  `pad_down` (compass, protractor), `left_trigger`/`right_trigger` (zoom), `shoulder_right` (modifier),
  `thumb_right` (drag), both thumbsticks (pan/cursor). **`pad_right` is the one clearly-free pad input.**
  `b`/`y` are unbound *in this context* but are conventionally back/cancel in overlapping ones, and
  `shoulder_left` is VON at priority 110 (project memory). See K7.
- **N12 — Clicking a panel button may unpin the panel.** `OnMapSelection` (`OVT_OverthrowMapUI.c:44-69`)
  treats any map selection with no hovered element as "clicked empty space" → unpin →
  `ForceHideLocationInfo()`. `MapSelect` is `mouse:button0` / `gamepad0:a`. Today this is invisible because
  `OnFastTravelClicked` closes the panel anyway; a **toggle that must leave the panel open** makes it
  visible. See R6.
- **N13 — `OVT_ShopTransactionComponent` is the model to copy**
  (`Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c`): a server-authoritative
  replacement for a legacy comms RPC, result enum sent **as an int** (`:1-14`), identity resolved from the
  controller entity and never from the payload (`ResolveOwningPlayerId`, `:657-678`), explicit distance
  constants with their rationale (`:36-46`), a `ScriptInvoker` fired on the requesting client only
  (`:50`, `:255-260`), a listen-server short-circuit in `SendSellResult` (`:629-638`), and a doc comment
  explaining why it does **not** extend `OVT_BaseServerProgressComponent` (`:26-32`).

### 3.7 What is healthy and must survive

- One rule set, one cost model, one implementation — no duplication between the map UI and the location
  types.
- Localized refusal reasons surfaced on the panel rather than silent failures.
- Cost shown on the button before the player commits.
- Button state distinguishing "this type never supports travel" (hidden) from "you cannot travel right
  now" (shown, disabled, with a reason).
- The in-vehicle driver path, including `FindSafeVehicleSpawnPosition` and the passenger refusal.
- The recruit ring-placement logic at `OVT_PlayerCommsComponent.c:1529-1547` — reuse it, do not rewrite it.

---

## 4. Architecture Overview

### 4.1 Component hierarchy

```
OVT_OverthrowController (prefab: Prefabs/GameMode/OVT_OverthrowController.et)   per-player GenericEntity
├── OVT_ContainerTransferComponent      existing
├── OVT_ShopTransactionComponent        existing  <- the model (N13)
├── OVT_TowerSabotageComponent          existing
├── RplComponent                        existing
└── OVT_TravelRequestComponent          NEW  ** needs a Workbench-authored GUID on the prefab **
        RequestTravel(verb, targetPos, bringRecruits)      client entry point
        RpcAsk_Travel(int verb, vector targetPos, bool bringRecruits)   [Server]
        RpcDo_TravelResult(int result, int amountCharged)               [Owner]
        m_OnTravelResult : ScriptInvoker                    display only

OVT_Global
└── static OVT_TravelRequestComponent GetTravelRequests()   NEW, alongside GetShopTransactions() etc.
                                                            (Scripts/Game/Global/OVT_Global.c:98-130)

Scripts/Game/Services/OVT_FastTravelService.c      the ONE rule/cost implementation, entity-parameterized
```

Nothing new is replicated. The component carries no state between calls; it is a request relay plus a
server routine, exactly like `OVT_ShopTransactionComponent`.

### 4.2 The entity-parameterized shared service

The service stays a static class — but **no function inside it may call
`SCR_PlayerController.GetLocalControlledEntity()`**. The caller supplies the actor: the client passes its
local controlled entity, the server passes `GetPlayerControlledEntity(playerId)`.

```
enum OVT_TravelVerb   { FAST_TRAVEL, BUS }

enum OVT_TravelResult { OK, NO_ACTOR, TOO_CLOSE, WANTED, QRF_ACTIVE, QRF_TOO_CLOSE,
                        NOT_ALLOWED, CANNOT_AFFORD, MUST_BE_DRIVER, MUST_EXIT_VEHICLE,
                        NOT_AT_BUS_STOP, BAD_DESTINATION, TELEPORT_FAILED }

class OVT_FastTravelService
{
  static const float RECRUIT_TRAVEL_RADIUS = 50.0;   // OVT_MapContext.c:25
  static const float BUS_STOP_RADIUS       = 15.0;   // OVT_TownManagerComponent.c:881 sphere

  // THE authoritative gate. No local-entity resolution inside. Runs identically on both machines.
  static int  ValidateTravel(int verb, vector targetPos, int playerId, IEntity actor, int recruitCount)

  // result code -> localized key, so the panel and the server refusal speak one vocabulary
  static string ReasonKeyFor(int result)

  // pure fare maths, world-free (see §9 for the Logic-tier case)
  static int  ComputeFare(float distMeters, int unitPrice, bool applyKmFloor, int recruitCount)
  static int  CalculateTravelCost(int verb, vector targetPos, IEntity actor, int recruitCount)

  static int  CountRecruitsInRadius(string persId, vector originPos)
  static bool IsAtBusStop(vector pos)          // registry query, callable on BOTH sides

  // CLIENT-ONLY convenience wrapper. Signature UNCHANGED so the five location types compile untouched.
  // Resolves the LOCAL actor and delegates. Documented as client-only, because that is the trap (N1).
  static bool CanGlobalFastTravel(vector targetPos, string playerID, out string reason)

  // DELETED: CanFastTravelToLocationType (F6), ExecuteFastTravel (client-side execution, F1/F2)
}
```

**Why `CanGlobalFastTravel` keeps its signature:** `map/location-types` is being implemented **right now**
and its five per-type `CanFastTravel` overrides call it. Changing the signature would collide with that
work for no benefit — the wrapper is a two-line delegate, and the trap is closed because the *core*
(`ValidateTravel`) no longer touches the local entity.

`ValidateTravel` folds the verb-specific rules in one place:

| Check | FAST_TRAVEL | BUS |
|---|---|---|
| debug bypass | yes (`:8-9`) | yes |
| actor exists | yes | yes |
| min distance | yes | **no** (N6) |
| wanted level | yes | **no** (N6) |
| QRF mode | yes | **no** (N6) |
| in a vehicle | driver ok, passenger `MUST_BE_DRIVER` | any vehicle → `MUST_EXIT_VEHICLE` |
| origin is a bus stop | — | `NOT_AT_BUS_STOP` |
| destination is a bus stop | — | `BAD_DESTINATION` |
| affordability at the **full** fare (incl. recruits) | yes | yes |

### 4.3 Client → server → client message shape

```
CLIENT (OVT_OverthrowMapUI)
  panel shows: verb label + " ($" + total + ")"   total = ComputeFare(...) x (1 + recruitCount if toggled)
  button click ->  OVT_Global.GetTravelRequests().RequestTravel(verb, targetPos, m_bBringRecruits)
                   -> Replication.IsServer() ? RpcAsk_Travel(...) : Rpc(RpcAsk_Travel, ...)

SERVER (OVT_TravelRequestComponent.RpcAsk_Travel)
   1. if(!Replication.IsServer()) return
   2. playerId = ResolveOwningPlayerId()          identity from THIS controller entity, never the payload
   3. actor    = GetPlayerControlledEntity(playerId)          -> NO_ACTOR
   4. originPos = actor.GetOrigin()               captured BEFORE the teleport (comms :1514-1516)
   5. recruits = {}; if(bringRecruits) recruits = GetPlayerRecruitEntitiesInRadius(persId, originPos, 50)
      recruitCount = recruits.Count()             ONE list, used for BOTH the fare and the teleport
   6. result = OVT_FastTravelService.ValidateTravel(verb, targetPos, playerId, actor, recruitCount)
      if(result != OK) -> SendResult(result, 0); return
   7. dest = driver ? FindSafeVehicleSpawnPosition(targetPos,...) : FindSafeSpawnPosition(targetPos)
   8. cost = CalculateTravelCost(verb, targetPos, actor, recruitCount)   BEFORE the teleport moves actor
   9. if(!SCR_Global.TeleportPlayer(playerId, dest)) -> SendResult(TELEPORT_FAILED, 0); return
  10. if(cost > 0) economy.TakePlayerMoneyPersistentId(persId, cost)
  11. teleport each entity in `recruits` into a ring around dest   (lift comms :1529-1547 verbatim)
  12. SendResult(OK, cost)

CLIENT (RpcDo_TravelResult, Owner-targeted; listen-server short-circuit as in SendSellResult :629-638)
   result != OK -> OVT_Global.ShowHint(ReasonKeyFor(result))
   result == OK && amountCharged > 0 -> ShowHint("#OVT-Travelled ($" + amountCharged + ")")
   m_OnTravelResult.Invoke(result, amountCharged)              display only, never mutates money
```

**Step order is load-bearing.** Cost is computed **before** the teleport (the actor moves synchronously and
the fare is origin-dependent), and money is taken **after** a successful teleport, so a refused or
out-of-bounds teleport cannot charge (N4). A crash between 9 and 10 loses the fare in the *player's*
favour — the fail-safe direction. No refund path is needed and none is written.

**One RPC pair for both verbs.** Two signatures in the whole feature, which is the practical mitigation for
the `Rpc()` arity blind spot (R7).

### 4.4 Bus eligibility is derived from position — there is no armed mode

`OVT_CatchBusAction` **opens the map and nothing else**. Bus-stop markers are always selectable. Their info
panel shows the fare always, and the travel button is **enabled only when the player is currently standing
at a bus stop**, determined by
`OVT_Global.GetMapMarkers().GetNearestMarker(actorPos, OVT_MapMarkerCategory.BUS_STOP, 15)`.

**Why this is the acceptance criterion for requirement 18.** BUG-069 part 2 was: an engine-side map close
left `m_bBusTravelActive` set, and the next click on a *later* open silently charged a fare. `map/core`
already argues that the rewrite is immune to BUG-069's other defects **structurally**, because it is a map
module driven by the engine's own open/close rather than an input-handler-driven mode
(`docs/features/map/core/implementation.md:132-139`). Position-derived bus eligibility extends the same
argument to the last of the four: **there is no flag to leave set.** A player who opens the map far from
any stop sees a disabled button with a reason; a player who walks to a stop and opens the map sees an
enabled one. No close path — engine-side, death, another menu — can leave anything armed, because nothing
is armed. This is not "we remembered to clear the flag"; it is "there is no flag", which is the strongest
form of the guarantee and the main reason the rewrite exists.

Side benefit (a user goal): a player can browse fares from anywhere before walking to a stop.

**The server re-derives the origin, it does not trust the client.** `ValidateTravel(BUS, ...)` runs
`IsAtBusStop(actor.GetOrigin())` on the server with the server's own actor position, and separately
requires the **destination** to be a bus stop (`BAD_DESTINATION`) — the client names an arbitrary vector.
Radius **15 m on both sides**, the number already in the codebase for "near a bus stop"
(`OVT_TownManagerComponent.c:881`, and the radius `map/location-types` Phase 1 re-points `OVT_MapContext.c:453`
to). Invariant to preserve if it ever changes: **the server's radius must be ≥ the client's**, so the server
never refuses something the panel offered.

### 4.5 Recruits are opt-out, on both verbs

A second control on the info panel, shown **only when the player has recruits within 50 m**:

```
recruitCount == 0   -> the toggle widget is hidden entirely (no dead control)
recruitCount  > 0   -> shown; default ON, reset to ON in OnMapOpen
                       label ON : "#OVT-Map_BringRecruits" + " " + N + " (+$" + extra + ")"
                       label OFF: "#OVT-Map_LeaveRecruits" + " " + N
                       flipping it re-runs SetupTravelButton -> the travel button's total updates live
```

**Default ON**, because legacy fast travel always brought them (`OVT_MapContext.c:441`); defaulting OFF
would silently reproduce exactly the regression F4 was filed for. The fare is on the button before the
player commits (requirement 19), so the cost of the default is visible, not surprising.

**The flag drives accompaniment and fare together, by construction.** The server takes the recruit entity
list **once** (step 5) and uses that same list for both the fare and the teleport. It is therefore not
possible to pay solo and arrive with a squad, or pay for a squad and arrive alone — not because the two
paths are kept in sync, but because there is only one path.

**The client-vs-server count race.** The client displays a fare from the recruits it can see; the server
recounts at execution time and may get a different number (a recruit wandered out of radius, the player
moved). **Resolution: the server charges its own count and reports the actual total back**, and the client
shows it (`#OVT-Travelled ($N)`). Refusing on drift is worse: the player has already committed and the map
has already closed. Drift is bounded — one fare per recruit — and it is now *visible* rather than silent,
which is what requirement 15 is actually protecting.

**`m_bBringRecruits` is reset to `true` in `OnMapOpen`.** It is not an armed mode (it cannot cause a charge
on its own; the player must still press the travel button), but resetting it on the same unconditional path
that force-hides the panel keeps the "no travel state survives a map close" property whole and checkable.

### 4.6 UI wiring changes

`SetupFastTravelButton` becomes `SetupTravelButton(location)`:

```
verb = FAST_TRAVEL
if (OVT_MapLocationBusStop.Cast(locationType)) verb = BUS       // a cast, not a new core virtual (K6)

if (verb == FAST_TRAVEL && !locationType.m_bCanFastTravel) { hide button + reason + toggle; return; }

show button
recruitCount = CountRecruitsInRadius(localPersId, actorPos)
show/hide the recruit toggle on recruitCount > 0; label it

if (verb == FAST_TRAVEL) canTravel = locationType.CanFastTravel(location, playerID, reason)   // unchanged
else                     { r = ValidateTravel(BUS, ...); canTravel = (r == OK); reason = ReasonKeyFor(r); }

label = verbLabel + " ($" + CalculateTravelCost(verb, pos, actor, effectiveRecruitCount) + ")"
        verbLabel: "#OVT-MainMenu_FastTravel" | "#OVT-CatchBus"
m_OnActivated.Clear(); m_OnActivated.Insert(OnTravelClicked)      // keep the existing anti-double-fire idiom
```

`OnTravelClicked` keeps today's shape — hide panel, close map, send the request — with the request now
going to `OVT_Global.GetTravelRequests()`.

**The refusal-after-close case is new and must be surfaced.** Server authority means a click can be refused
*after* the map has closed. `RpcDo_TravelResult` maps the result to a localized key and calls
`OVT_Global.ShowHint` (`OVT_Global.c:835`), so the player always gets an explanation. The client-side check
that drives the button state is now **advisory**; the server check is **authoritative** — that is the
resolution of F7's double check.

---

## 5. Implementation Phases

Effort is **S / M / L** relative to a single focused session. "Agent" is the routing hint for `/proceed`.

---

### Phase 0 — Verify before fixing: two-client MP session — **M — user-driven, no agent**

> F1/F2/F4 are **code-reading inferences**. Fix what is observed, not what is deduced. This phase produces
> the baseline the rest of the plan is aimed at, and it is cheap: no code is written.

> ⚠️ Client launches open a real window on the user's desktop and can orphan. Always pass a long
> `--timeout` — the default 600 s will kill the client mid-test.

**Tasks**

1. `tools/launch-server.sh`; then
   `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`
   and a second client with `--profile OverthrowClient2`.
2. **F1** — on-foot fast travel from the new map on client A. Does A move? Does the *server* agree (does B
   see A at the destination)? Does A snap back?
3. **F2** — note A's money before and after, then force an authoritative money update (buy something, or
   wait for the next `StreamPlayerMoney`) and note it again. A client-side debit looks correct locally and
   is then overwritten — that is the whole point of F2.
4. **F4** — with two recruits standing next to A, fast travel and confirm they are left behind.
5. In-vehicle-as-driver: does the vehicle come along? Passenger: is the refusal shown?
6. **N9 / cost display** — does the button show a non-zero cost on a *dedicated-server client*? If it shows
   `$0`, `GetPlayerControlledEntity(localId)` is returning null client-side (`OVT_FastTravelService.c:85-87`)
   and the displayed cost is already wrong in MP. Record the answer; it decides whether the client wrapper
   must resolve the actor via `SCR_PlayerController.GetLocalControlledEntity()` instead.
7. **N10** — press `gamepad0:x` on the fullscreen map with a controller. Does vanilla's radial menu open?
   Does fast travel fire? Both? Record exactly what happens; this decides K7's branch.
8. **N12** — click the fast-travel button with the mouse while the panel is *pinned*. Does the panel
   survive the click (i.e. does `OnMapSelection` unpin underneath it)? Hard to see today because the
   handler closes the map; watch for a one-frame flicker, or temporarily comment out the `HideMap()` call
   locally to observe.

**Acceptance:** every observation above written into `context.md` as observed fact, with which of F1/F2/F4
are confirmed, which are not, and what actually happens for N9/N10/N12.

---

### Phase 1 — Parameterize the service, unify identity, delete dead code — **M — `component-developer`**

**Tasks**

1. Add `OVT_TravelVerb` and `OVT_TravelResult` enums (file-top of the service, as
   `OVT_ShopSellResult` is at `OVT_ShopTransactionComponent.c:1-14`).
2. Add `ValidateTravel(verb, targetPos, playerId, actor, recruitCount)` returning `OVT_TravelResult`,
   containing the whole rule table from §4.2. **No local-entity resolution anywhere inside.**
3. Add `ReasonKeyFor(result)` mapping every code to an existing `.st` key (§10 lists the four new ones).
4. Add `ComputeFare(distMeters, unitPrice, applyKmFloor, recruitCount)` — pure maths, no world access —
   and `CalculateTravelCost(verb, targetPos, actor, recruitCount)` on top of it.
5. Add `CountRecruitsInRadius(persId, originPos)` and `IsAtBusStop(pos)` (the latter guarded to return
   `false` when `OVT_Global.GetMapMarkers()` is null, so this phase compiles before `map/location-types`
   Phase 1 lands — see R1).
6. Rewrite `CanGlobalFastTravel` as the **client-only** wrapper with its **signature unchanged**, with a
   doc comment stating why it is client-only (N1) and that server callers must use `ValidateTravel`.
7. **Delete `CanFastTravelToLocationType`** (F6, `:160-170`).
8. Leave `ExecuteFastTravel` in place for now (Phase 2 deletes it) so the tree stays runnable.

**Acceptance**
- `tools/compile-check.sh` exit 0; `tools/run-tests.sh "{6A6E29FF47ECB840}"` exit 0.
- `grep -n "GetLocalControlledEntity" Scripts/Game/Services/OVT_FastTravelService.c` matches **only** the
  documented client wrapper.
- The five location types are **unmodified** (`git diff --stat` shows no file under
  `Scripts/Game/UI/Map/LocationTypes/`).
- Fast travel still works in single-player exactly as before (this phase changes no behaviour).

---

### Phase 2 — `OVT_TravelRequestComponent`: server authority, payment and recruits — **L — `network-specialist-advanced`**

> **Advanced.** This is the authority migration: a new controller component, a new RPC pair, the first
> server-side validation this verb has ever had, atomic payment, and the recruit path. It is the highest-risk
> phase in the epic and squarely `network-specialist`'s domain.

**Tasks**

1. `Scripts/Game/Components/Controller/OVT_TravelRequestComponent.c`, modelled on
   `OVT_ShopTransactionComponent`:
   - `RequestTravel(int verb, vector targetPos, bool bringRecruits)` with the
     `Replication.IsServer()` short-circuit (`ShopTransaction :69-74`).
   - `RpcAsk_Travel` implementing §4.3 steps 1–12 in that order.
   - `RpcDo_TravelResult(int result, int amountCharged)` + `m_OnTravelResult`, with the listen-server
     short-circuit copied from `SendSellResult` (`:629-638`).
   - `ResolveOwningPlayerId()` copied from `ShopTransaction :657-678` — identity from the controller
     entity, never the payload.
   - A class doc comment in the house style explaining why it does **not** extend
     `OVT_BaseServerProgressComponent` (travel completes in one frame; the result carries money, which the
     progress base's `(transferred, skipped)` pair cannot express).
2. Recruit teleport: lift the ring placement from `OVT_PlayerCommsComponent.c:1529-1547` **verbatim** —
   angle `i × 360/count`, radius `3.0 + i × 0.5`, `FindSafeSpawnPosition(..., skipSpawnPointSearch = true)`.
3. `Scripts/Game/Global/OVT_Global.c` — add `static OVT_TravelRequestComponent GetTravelRequests()`
   alongside `GetShopTransactions()` / `GetTowerSabotage()` / `GetContainerTransfer()` (`:98-130`).
4. `Prefabs/GameMode/OVT_OverthrowController.et` — add the component block with a **fresh unique GUID**.
   ⚠️ **User/Workbench dependency** (§10): a component that exists in script but not on the prefab is a
   silent no-op.
5. `OVT_OverthrowMapUI.OnFastTravelClicked` → `OnTravelClicked`, calling
   `OVT_Global.GetTravelRequests().RequestTravel(FAST_TRAVEL, pos, m_bBringRecruits)`.
6. **Delete `OVT_FastTravelService.ExecuteFastTravel`** (F1/F2 closed).
7. Verify no caller of `OVT_PlayerCommsComponent.RequestFastTravel` /
   `RequestFastTravelWithRecruits` remains (legacy `OVT_MapContext` still calls them — that is expected and
   is retirement's to delete; the assertion is that **no new-map code** calls them).

**Acceptance**
- `tools/compile-check.sh` exit 0; `tools/run-tests.sh "{6A6E2A002F53A581}"` exit 0.
- On a dedicated server: a client fast-travels, **moves**, and is **charged**, and the balance survives the
  next authoritative sync (F1/F2).
- Recruits within 50 m arrive in a ring around the destination (F4).
- With a debug build that sends a request for a destination the panel refused (or outside terrain bounds),
  the server refuses, **no money is taken**, and the client sees a hint.

---

### Phase 3 — Recruit opt-out toggle — **M — `ui-developer`**

**Tasks**

1. `UI/Layouts/Map/Core/OVT_MapInfoPanel.layout` — add a `BringRecruitsButton` beside `FastTravelButton`,
   built from `WLib_NavigationButtonSmall` with an `SCR_InputButtonComponent` whose **component GUID is the
   copied `{5D346C3DD81D95CD}`**, not a fresh one (the most common layout bug —
   `overthrow-ui-patterns/navigation-buttons.md`). Fresh **widget** GUID.
2. `Configs/System/chimeraInputCommon.conf` — new `Action OverthrowToggleRecruits`
   (`keyboard:KC_R`; gamepad source per K7) and add it to `ActionContext MapContext { ActionRefs +{ } }`
   at `:704`. `KC_R` is free in vanilla's `MapContext` (N10).
3. `OVT_OverthrowMapUI` — `protected bool m_bBringRecruits = true;` reset to `true` in `OnMapOpen`;
   subscribe/unsubscribe the toggle's `m_OnActivated` with the same `Clear()`-then-`Insert()` idiom as the
   travel button; the handler flips the flag and re-runs `SetupTravelButton(currentLocation)` so **both**
   labels update live.
4. Hide the toggle entirely when `CountRecruitsInRadius(...) == 0`. Remember that hiding a button also
   kills its shortcut (`SCR_InputButtonComponent.OnInput` early-returns on an invisible root) — which is
   the desired behaviour here.
5. Pass `m_bBringRecruits` through `RequestTravel`.
6. New `.st` ids (§10). Until the user regenerates the exports, the layout/labels use **literal text**.

**Acceptance**
- With 0 recruits nearby: no toggle, and the fare is the solo fare.
- With N recruits nearby: toggle visible, **ON by default**, labelled with N and the extra cost; flipping
  it changes the travel button's total **immediately**, both ways.
- Toggle OFF → recruits stay put and are not charged for. Toggle ON → they travel and are charged.
- Operable with a controller: see the gamepad gate (V-5) and R6.

---

### Phase 4 — Bus travel migration (F5) — **M — `component-developer` + `ui-developer`**

> **Sequenced last of the build phases** because it is the only one that cannot compile until
> `map/location-types` Phase 1 publishes `OVT_Global.GetMapMarkers()`, `GetNearestMarker` and
> `OVT_MapLocationBusStop`. See R1 for what to do if that has not landed.

**Tasks**

1. `OVT_FastTravelService.IsAtBusStop` — switch from the null-guarded stub to the real registry query.
2. `ValidateTravel(BUS, ...)` — origin stop, destination stop, in-vehicle refusal, affordability. **No**
   wanted/QRF/min-distance checks (N6).
3. `CalculateTravelCost(BUS, ...)` — `round((dist/1000) × busTicketPrice) × (1 + recruitCount)`,
   **no 1 km floor** (N8), distance measured from the actor's position.
4. `OVT_OverthrowMapUI.SetupTravelButton` — the bus branch of §4.6, including the `OVT_MapLocationBusStop`
   cast and the `#OVT-CatchBus` label.
5. `OVT_CatchBusAction.c:10` — replace `EnableBusTravel()` with a map-open-only call
   (`OVT_MapContext.OpenMap()`: `if(!ShowMap()) ShowNotification("MustHaveMap");`). **This feature makes the
   change**, overlapping `legacy-retirement/requirements.md:18`, because leaving the action arming the
   legacy mode would resurrect BUG-069 part 2 the moment both systems are live at once.
6. Coordinate one conf value with `map/location-types`: the `OVT_MapLocationBusStop` entry in
   `Configs/Map/OverthrowMap.conf` sets `m_bCanFastTravel 0` — bus stops are bus destinations, not
   fast-travel destinations (legacy never offered fast travel to one).

**Acceptance**
- Standing at a bus stop → open map → bus-stop marker panel offers "Catch a bus ($fare)" **enabled**;
  selecting it charges the fare and travels.
- Standing away from any stop → the same panel shows the fare but the button is **disabled** with the
  not-at-a-stop reason.
- In a vehicle → `#OVT-MustExitVehicle`.
- The fare matches `round((dist/1000) × busTicketPrice)` for a solo trip.
- No script anywhere calls `EnableBusTravel`.

---

### Phase 5 — Input verification, cleanup, retirement handoff — **S — `ui-developer`**

**Tasks**

1. Resolve N10 per K7 using Phase 0's observation: either rebind `OverthrowFastTravel`'s gamepad source off
   `gamepad0:x`, or record with evidence that vanilla's radial menu is inert on Overthrow's map.
2. Confirm `OverthrowToggleRecruits`' inputs against the vanilla `MapContext` block by hand (§ N10/N11
   lists the occupied inputs) — the conflict script cannot see inline `ActionContext` actions.
3. Write the **exact** dead-code list for `map/legacy-retirement` into this feature's `context.md` (see
   I-1).
4. Delete anything this feature orphaned in the service; confirm `ExecuteFastTravel` and
   `CanFastTravelToLocationType` are gone.

---

### Phase 6 — Verification gate — **M — user-driven, no agent**

Run the full Definition of Done §7 Verification Method. Nothing ships without V-1 … V-6.

---

## 6. Key Technical Decisions

**K1 — The service stays the single rule/cost implementation, but becomes entity-parameterized.**
Requirement 15 ("displayed availability must match enforced availability") is only satisfiable if client
and server run the *same* rules. Two copies drift; one copy with a machine-scoped actor lookup is worse
than two, because it fails **silently and differently** on dedicated vs listen servers (N1). The fix is one
parameter. `CanGlobalFastTravel` keeps its exact signature as a documented client-only wrapper so the five
per-type overrides — being edited **right now** by `map/location-types` — are untouched.

**K2 — Execution and payment move to a new component on `OVT_OverthrowController`, never to
`OVT_PlayerCommsComponent`.** `CLAUDE.md` and `overthrow-controller.md` forbid the latter outright.
`OVT_ShopTransactionComponent` is the closest precedent and is copied deliberately: int-only RPC payloads,
identity resolved from the controller entity, a result enum, an Owner-targeted result RPC with a
listen-server short-circuit, and named constants carrying their own rationale. Reusing a proven shape is
worth more here than any refinement of it.

**K3 — Recruits are charged per-recruit on BOTH verbs. Deliberate, user-approved, and it departs from
`requirements.md:39`.**
`requirements.md:39` puts "rebalancing travel economics" out of scope and says cost formulas are preserved
as-is. **The user has knowingly overridden that line**, under `requirements.md:23`'s allowance that recruit
behaviour may be "deliberately changed with the change recorded". This is that record. It is not a bug fix
and it is not buried in a refactor.

Rationale: internal consistency between the two verbs, and closing the "ferry a squad anywhere for one
fare" exploit.

**Correction that makes it more defensible than it looked (N5):** legacy fast travel **already** charged
per recruit — `OVT_MapContext.c:387-397`, comment "Same cost per recruit" (`:395`), the same structure the
bus branch uses at `:464-472`. The brief and the retrospective both recorded fast-travel recruits as free;
the code disagrees. So the per-recruit multiplier is **parity-restoring**, not novel. What *is* different
is the base fare: legacy fast travel charged a flat `fastTravelCost` (`:384`) while the shipped service
charges `max(1 km, dist/1000) × fastTravelCost` (`:91-94`) — a change the `new-map` branch already made,
independently of this feature. The compound effect is that a long trip with a squad costs materially more
than it did in 1.4.x.

**Player-visible consequence, stated plainly: fast travel with a squad now costs more than it did.**

**Mitigation (K4):** it is opt-out. A player is never forced to pay for recruits they did not choose to
bring — which is also why the toggle, not the charge, is the load-bearing part of this decision.

**K4 — Bringing recruits is a player choice, default ON, and the flag drives fare and accompaniment
together.**
You should not have to dismiss your squad to travel alone. The toggle appears only when recruits are
actually within 50 m — no dead control — and defaults **ON** because legacy always brought them
(`OVT_MapContext.c:441`), so defaulting OFF would silently reproduce the exact regression F4 was filed for.
The divergence risk ("pay solo, arrive with a squad") is eliminated **by construction**: the server takes
the recruit entity list once and uses that same list for both the fare and the teleport. The client/server
count race resolves in favour of the server's count, reported back and shown to the player, because
refusing after the map has closed is worse UX than a visible ±1 fare (§4.5).

**K5 — Bus eligibility is derived from position; there is no bus mode.**
Requirement 18 demands that bus mode must not survive a map close. With no flag, that defect **cannot
recur by construction** — the same structural argument `map/core` makes for BUG-069's other three defects
(`core/implementation.md:132-139`), extended to the fourth. This is the single strongest justification for
the rewrite existing, so it is stated as an acceptance criterion (Q-3), not as an implementation note. The
server re-derives the origin stop from its own actor position and separately validates the destination —
the client is not trusted about where it is standing.

**K6 — The bus verb is selected by a cast, not by a new virtual on `map/core`'s contract.**
`OVT_MapLocationBusStop.Cast(locationType)` in `SetupTravelButton` costs nothing and changes no shared
contract. `map/location-types` is already making one additive change to that contract (its K5); a second
concurrent one is an unnecessary merge hazard. If a second bus-capable type ever exists, promote the cast
to a virtual then — YAGNI until it does.

**K7 — Input allocation on the map is nearly exhausted, and the existing binding already collides.**
Grounded from vanilla `Configs/System/chimeraInputCommon.conf:8128-8479`:

| Input | Vanilla owner in `MapContext` |
|---|---|
| `gamepad0:a` | `MapSelect`, `MapMultiSelectGamepad` |
| **`gamepad0:x`** | **`MapContextualMenu` → `SCR_MapRadialUI`** (collides with `OverthrowFastTravel`) |
| `gamepad0:pad_left` / `pad_up` / `pad_down` | tool menu focus / watch + pencil / compass + protractor |
| `gamepad0:left_trigger` / `right_trigger` | zoom out / in |
| `gamepad0:shoulder_right` | `MapModifierKey`, `MapModifClick` |
| `gamepad0:thumb_right` | `MapDragGamepad` |
| both thumbsticks | pan and cursor |
| **`gamepad0:pad_right`** | **free** |
| `gamepad0:b` / `y` | unbound *here*, but conventionally back/cancel in overlapping contexts |
| `gamepad0:shoulder_left` | avoid — VON at priority 110 (project memory) |
| `keyboard:KC_SPACE`, `KC_R` | free (vanilla uses NUMPAD2/4/6/8, DELETE, K, O, B, N, ADD, SUBTRACT) |

**Decision, with the branch resolved by Phase 0's observation:**
- *If the radial menu visibly opens on `gamepad0:x`* — move `OverthrowFastTravel`'s gamepad source to
  `gamepad0:pad_right`, and give `OverthrowToggleRecruits` **no** gamepad binding (keyboard `KC_R` plus the
  cursor path: move the map cursor onto the button and press `gamepad0:a`, which is `MapSelect`).
- *If the radial menu is inert on Overthrow's map* — leave fast travel on `x` and give
  `OverthrowToggleRecruits` `gamepad0:pad_right`, which is the better affordance (`WLib_NavigationButton`
  renders the pad glyph next to the label).

Either way both controls are reachable on a controller, and the cursor path is a real fallback: the map's
gamepad navigation model **is** a cursor (`MapGamepadCursorX/Y` + `MapSelect`), and project memory records
that mouse-wired buttons are fine on gamepad for exactly that reason. R6 covers the one hazard in that
path.

**K8 — Teleport first, charge second; no refund path.**
`SCR_Global.TeleportPlayer` returns `false` on an out-of-bounds destination or an unresolvable player
(`Functions.c:1640-1645`), and today the client has already debited before calling (N4). Ordering the
server routine as *validate → compute cost → teleport → charge* makes "never charged for travel that does
not happen" true by construction and needs no compensating transaction. The residual failure — a crash
between teleport and charge — loses the fare in the player's favour, which is the correct direction.

**K9 — The client check is advisory; the server check is authoritative.** F7's double affordability check
resolves itself once payment is server-side: `CanGlobalFastTravel` on the client drives *button state*, and
`ValidateTravel` on the server drives *whether anything happens*. Both call the same code, so they normally
agree; when they disagree (latency, a QRF that started, money spent in another window), the server wins and
the player is told why via `RpcDo_TravelResult` → `OVT_Global.ShowHint`.

**K10 — One RPC pair for both verbs.** `Rpc()`'s prototype is untyped variadic, so a wrong argument count
compiles clean and dies silently at the wire (BUG-090). The practical defence is to have as few signatures
as possible and read each one by hand: this feature adds exactly two.

---

## 7. Definition of Done

Written so an evaluator with no implementation context can verify each item.

### Functional

**F-1 — Refusal rules, one at a time (fast travel).** Each shows the button *visible but disabled* with the
stated reason, and pressing the keybind does nothing:

- [ ] Wanted level > 0 → `#OVT-CannotFastTravelWanted`
- [ ] Target closer than `m_Difficulty.minFastTravelDistance` → `#OVT-CannotFastTravelDistance`
- [ ] QRF active, `QRFFastTravelMode = DISABLED` → `#OVT-CannotFastTravelDuringQRF`
- [ ] QRF active, `QRFFastTravelMode` not `DISABLED`/`FREE`, target within `QRF_RANGE` of the QRF →
      `#OVT-CannotFastTravelToQRF`; a target **outside** that range is allowed
- [ ] QRF active, `QRFFastTravelMode = FREE` → allowed
- [ ] Insufficient funds → `#OVT-CannotAfford`
- [ ] A location type with `m_bCanFastTravel 0` (e.g. Town) → button **hidden**, not disabled

**F-2 — On foot.** Fast travel from the panel moves the player to the destination and charges the displayed
amount.

**F-3 — In a vehicle.** As **driver**, the player *and the vehicle* arrive. As **passenger**, refused with
`#OVT-MustBeDriver` and nothing is charged.

**F-4 — Bus travel.** Standing at a bus stop, open the map, select another bus stop: fare shown, travel
happens, fare charged. Standing away from any stop, the button is disabled with the not-at-a-stop reason.
In a vehicle: `#OVT-MustExitVehicle`.

**F-5 — Bus fare model preserved.** A solo bus trip costs `round((dist/1000) × busTicketPrice)` measured
from the player's position — with **no** 1 km floor (unlike fast travel).

**F-6 — Recruits accompany and are charged, on both verbs.** With N recruits within 50 m and the toggle ON:
they arrive in a ring around the destination and the fare is `(1 + N) ×` the solo fare. With the toggle OFF:
they stay where they were and the fare is the solo fare.

**F-7 — The recruit toggle behaves.** Hidden with 0 recruits nearby. Shown and **ON by default** with ≥ 1.
Flipping it updates the travel button's displayed total immediately, in both directions. Its state does not
survive a map close (reopen → ON).

**F-8 — Intentional behaviour change is recorded.** Fast travel with a squad costs more than it did: the
per-recruit fare now applies to fast travel, at the distance-scaled rate. This is stated in K3, is
opt-out, and is not presented as a bug fix.

### Security / authority

**S-1 — A client cannot teleport itself.** With the map closed and no UI involved, a request issued
directly to `OVT_TravelRequestComponent` for a destination the rules refuse results in **no movement and no
charge**. (Practical test: in a debug build, call `RequestTravel` for a target inside `minFastTravelDistance`,
or with wanted level > 0.)

**S-2 — A client cannot pay itself, or avoid paying.** Money is only ever mutated on the server.
`grep -rn "TakePlayerMoneyPersistentId\|TakePlayerMoney" Scripts/Game/Services/ Scripts/Game/UI/Map/`
returns nothing.

**S-3 — The server does not trust the client's position.** Bus travel validates the origin stop from the
**server's** actor origin and the destination from the registry. A request naming a valid destination stop
while the player stands nowhere near a stop is refused.

**S-4 — Identity comes from the controller entity.** `RpcAsk_Travel` resolves the acting player with
`ResolveOwningPlayerId()` and never from an RPC parameter — verified by reading: there is no player id in
the RPC signature at all.

**S-5 — The recruit set is server-derived.** The count used for the fare and the entities teleported come
from **one** `GetPlayerRecruitEntitiesInRadius` call on the server. No client-supplied count reaches the
fare.

### Quality

**Q-1 — No charge when the teleport fails.** Requesting travel to a destination outside terrain bounds
leaves the player's money unchanged and produces a hint, not silence.

**Q-2 — Refusal after the map has closed is surfaced.** Every non-OK result produces an on-screen hint with
the specific reason. Nothing fails silently.

**Q-3 — Bus eligibility cannot survive a map close — structurally.** Verified by *reading*, not only by
testing: there is no `m_bBusTravelActive`-equivalent field anywhere in the new map path. Eligibility is
computed from the player's position each time the panel is built. Test to corroborate: at a bus stop, open
the map, close it with the gadget key / by dying / by opening another menu, walk 500 m away, reopen the map
and click a bus-stop marker — **no fare is charged and nothing happens**. (This is BUG-069 part 2's exact
reproduction.)

**Q-4 — Displayed cost equals charged cost** for a solo trip, and equals the reported `amountCharged` for a
recruit trip. Where they differ (recruit drift), the actual amount is shown to the player.

**Q-5 — No new client→server RPC on `OVT_PlayerCommsComponent`.** Verified by diff: that file's only change
in this feature is *none*.

**Q-6 — Every `FindAnyWidget` name added exists in the layout.** Name-by-name audit for
`BringRecruitsButton` and anything else added — `FindAnyWidget` returning null is a silent no-op the
compiler cannot catch, and it is how `map/core` D1/D2 shipped dead.

**Q-7 — No `Rpc()` arity mistakes.** Both new RPC signatures read by hand against their call sites and the
match recorded in `context.md`. Wrong arity compiles clean and dies at the wire (BUG-090).

### Integration

**I-1 — A precise dead list is handed to `map/legacy-retirement`.** After this feature, the following have
no callers and exist only to be deleted:

| File | Symbols left dead |
|---|---|
| `Scripts/Game/UI/Context/OVT_MapContext.c` | `m_bFastTravelActive` (`:20`), `m_bBusTravelActive` (`:21`), `RECRUIT_TRAVEL_RADIUS` (`:25`), `CanFastTravel` (`:53-95` — the duplicate rule set), `EnableFastTravel` (`:277-285`), `EnableBusTravel` (`:287-295`), `DisableFastTravel` (`:312-315`), `DisableBusTravel` (`:317-320`), the fast-travel branch of `MapClick` (`:373-449`), the bus branch (`:451-510`), and the `DisableFastTravel`/`DisableBusTravel` calls in `OnMapExit` (`:303-304`) |
| `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` | `RequestFastTravel` (`:1483-1486`), `RpcAsk_RequestFastTravel` (`:1488-1493`), `RequestFastTravelWithRecruits` (`:1495-1498`), `RpcAsk_RequestFastTravelWithRecruits` (`:1500-1548`) |

**Not** left dead by this feature and **not** retirement's to attribute here: `m_bMapInfoActive`,
`EnableMapInfo`/`DisableMapInfo`, `ShowTownInfo` — those belong to the map-info mode, which
`legacy-retirement` retires on its own terms.

**I-2 — `OVT_PlayerCommsComponent`'s fast-travel RPCs have no remaining callers.** Verified by
`grep -rn "RequestFastTravel" --include=*.c Scripts/` returning matches **only** inside
`OVT_PlayerCommsComponent.c` itself and inside legacy `OVT_MapContext.c`.

**I-3 — `map/core`'s contract is unchanged.** No new virtual, attribute or field on `OVT_MapLocationType`,
`OVT_MapLocationData` or `OVT_MapLocationElement` (K6). Verified by diff.

**I-4 — `map/location-types` is consumed, not duplicated.** This feature adds no marker component, no
registry and no bus-stop location type; it calls `OVT_Global.GetMapMarkers().GetNearestMarker(...)`. The
per-type `CanFastTravel` gates are untouched.

**I-5 — `OVT_MapRestrictedAreas` is untouched** (BUG-070 must not regress).

### Verification Method

Run in order. Stop and fix at the first failure.

**V-1 — Compile.** `tools/compile-check.sh`. Expect exit 0 and no `file:line:` output.

**V-2 — Regression tests.** `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) then
`"{6A6E2A002F53A581}"` (All). Expect exit 0. **No suite covers travel or map UI** — these are guards that
nothing *else* broke, not evidence this feature works. If the `ComputeFare` Logic-tier case from §9 is
added, it runs here.

**V-3 — Single-player sweep.** Start a campaign in a build with `m_bDebugMode 0`. Walk F-1 through F-8
in order, noting money before and after every trip.

**V-4 — Two-client MP/JIP gate (highest risk; do not skip).**

> ⚠️ Client launches open a real window on the user's desktop and can orphan. Always pass
> `--timeout 3600`; the 600 s default will kill a client mid-test.

1. `tools/launch-server.sh`
2. Client **A**: `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`
3. As A: note the money balance. Recruit two recruits and keep them within 50 m. Fast travel on foot to a
   distant FOB with the toggle **ON**.
   - [ ] A arrives; both recruits arrive in a ring around A
   - [ ] A is charged `3 ×` the solo fare, and the hint reports that amount
   - [ ] **Money is verified on both sides**: A's HUD balance *and* the server's view of it (open a shop, or
         trigger any authoritative money update) agree. F2's whole point is that a client-side debit looks
         correct locally and is then overwritten
4. Client **B** joins now (**JIP** — after A has travelled and spent):
   `tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001`
   - [ ] **B fast-travels within 60 s of joining.** B moves, is charged, and A sees B at the destination
   - [ ] B's displayed cost is non-zero and matches what B is charged (this is the N9 / `GetPlayerControlledEntity`
         question)
5. **Concurrent travel:** A and B both press travel within the same second, to different destinations.
   - [ ] Both arrive at the correct, different destinations
   - [ ] Each is charged their own fare; neither is charged the other's
   - [ ] Neither player's recruits follow the wrong player
6. **Toggle OFF across the wire:** A stands with two recruits next to B. A travels with the toggle **OFF**.
   - [ ] **B observes the recruits stay put**
   - [ ] A is charged the solo fare only
7. **Bus travel in MP:** A walks to a bus stop, opens the map, takes a bus to another stop.
   - [ ] A arrives, is charged the distance fare, and B sees A at the destination
   - [ ] With A standing away from any stop, the bus button is disabled for A
8. **Authority probe:** with A standing 20 m from a marker (inside `minFastTravelDistance`), have A press
   the travel keybind repeatedly.
   - [ ] A never moves and is never charged (S-1)

**V-5 — Gamepad/console gate.** Controller only, no mouse or keyboard:
- [ ] Move the map cursor onto a travel-capable marker; the info panel appears
- [ ] The travel button is reachable and activates (either by its pad glyph or by cursor + `A`)
- [ ] The recruit toggle is reachable and flips, and the travel button's cost visibly updates
- [ ] Flipping the toggle does **not** dismiss the info panel (R6/N12)
- [ ] Complete a full bus trip end to end without touching mouse or keyboard
- [ ] **Manual input-collision check (N10):** press `gamepad0:x` on the fullscreen map. Record whether
      vanilla's radial/contextual menu opens, whether fast travel fires, or both. Resolve per K7. The
      project's conflict script cannot see inline `ActionContext` actions — this is a human observation,
      not a script run
- [ ] Confirm `OverthrowToggleRecruits`' chosen inputs fire nothing else on the map

**V-6 — Dead-code handoff.** Produce the I-1 table with line numbers re-checked against the tree as it
stands at sign-off, and paste it into `context.md` for `map/legacy-retirement`.

---

## 8. Quality Bar

This is a **server-authority and multiplayer-correctness** feature first, and a UI feature second. The bar
is set accordingly.

**Authority — the non-negotiables**
- **No client can teleport itself.** Today one can, anywhere, for free (N2). The measure of success is not
  that the UI stopped calling `TeleportPlayer` — it is that the server refuses a request the UI never
  would have sent.
- **No client can move money.** Every mutation goes through the server routine. A client-side debit is
  indistinguishable from a working one *locally*, which is exactly why F2 survived a release.
- **Displayed availability matches enforced availability**, because both sides run one implementation. If a
  refusal reason ever exists on one side only, the rule has been duplicated and the design has failed.
- **Money and position never disagree between client and server.** Verify money on *both* sides after every
  test trip, not just on the travelling client's HUD.
- **Identity from the entity, never the payload.** The RPC has no player-id parameter to spoof.

**Multiplayer**
- **Listen server and dedicated server are different machines with different bugs.** N1 is a listen-server
  bug that a dedicated-server test would miss entirely (it fails closed there). Where a decision differs
  between the two, say which, and short-circuit Owner-targeted RPCs to the local player exactly as
  `SendSellResult` does (`ShopTransaction :629-638`).
- **JIP is this project's most common regression class.** A client that joined 30 seconds ago must be able
  to travel — and see a correct cost — with no special handling.
- Every server-side lookup tolerates partial state: a null economy manager, a null recruit manager, a null
  marker registry, an unresolvable actor. Refusing cleanly always beats a script error.

**UI**
- The travel flow must be completable **on gamepad**, including the recruit toggle. That is an acceptance
  criterion, not a nicety; the whole rewrite sits on vanilla's `SCR_MapUIElement` for this reason.
- Nothing fails silently. A refusal after the map has closed still tells the player why.
- `FindAnyWidget` returning null is a silent no-op — audit every new name (Q-6).
- Do not design around a working panel close; `map/core` D2/D3 mean there isn't one.

**Discipline**
- Do not invent behaviour. Every rule in this plan is cited to a line in the tree; if a fact is not in the
  code, it does not go in the implementation.
- Do not "fix" the in-vehicle path — vanilla already teleports the vehicle (`Functions.c:1657-1663`).
- Do not add wanted/QRF checks to bus travel. Legacy had none (N6) and `requirements.md:38` forbids new
  travel mechanics.

---

## 9. Testing Strategy

**Automated — mostly a regression guard.** No suite covers map UI or travel, and neither is automatable
here. Run `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast, 38 cases ~15 s) after each phase and
`"{6A6E2A002F53A581}"` (All, 66 cases ~19 s) before sign-off, purely to prove nothing else broke.

**One case is genuinely worth adding.** `ComputeFare(distMeters, unitPrice, applyKmFloor, recruitCount)`
(§4.2) is pure arithmetic with no world access, so it belongs in the **Logic** tier
(`Scripts/Game/Tests/TestSuites/Logic/`). Assert: the 1 km floor applies to fast travel and not to buses; a
0 m trip still costs one unit with the floor and 0 without; the recruit multiplier is `(1 + N)` exactly;
rounding matches `Math.Round`. **Prove it can fail before shipping it** — flip `applyKmFloor`'s effect in
the implementation, confirm the case goes red, revert, and record the method in `context.md`.
❌ **No `maxAttempts`.**

**Manual — the real gate.**

| # | Scenario | Expected |
|---|---|---|
| 1 | Fast travel on foot, solo | Moves, charged `round(max(1,d/1000) × fastTravelCost)` |
| 2 | Fast travel as driver | Player **and vehicle** arrive |
| 3 | Fast travel as passenger | `#OVT-MustBeDriver`, no charge, no movement |
| 4 | Each refusal rule (F-1) | Button disabled with the specific reason; keybind inert |
| 5 | Recruit toggle ON / OFF | Accompaniment and fare move together, both ways |
| 6 | Toggle with 0 recruits nearby | Toggle hidden; solo fare |
| 7 | Bus from a stop to a stop | Fare = `round(d/1000 × busTicketPrice)`, no 1 km floor |
| 8 | Bus while not at a stop | Disabled with the not-at-a-stop reason; fare still visible |
| 9 | Bus while in a vehicle | `#OVT-MustExitVehicle` |
| 10 | BUG-069 part 2 reproduction (Q-3) | Nothing charged, nothing happens |
| 11 | Destination outside terrain bounds | No charge, hint shown |
| 12 | Two clients travelling concurrently | Correct, independent outcomes and charges |
| 13 | JIP client travelling < 60 s after joining | Moves, correct cost displayed and charged |
| 14 | Controller only, full bus trip | Completable, toggle included |

**Debugging.** No debugger — `Print()` only. When travel does nothing, the three usual causes are: the
component is not on the controller prefab (the RPC never leaves the client), the RPC arity is wrong (it
leaves and is dropped — BUG-090), or `ValidateTravel` refused. Print at each of those three points
separately; do not guess.

---

## 10. Dependencies

### Internal (code)

- **`map/location-types` Phase 1 — hard dependency for Phase 4 only.** This feature consumes, and must not
  redefine: `OVT_MapMarkerComponent` with `m_eCategory` (`BUS_STOP | POI`),
  `OVT_MapMarkerManagerComponent` on the game mode (populated by a world scan on **every** machine, with
  **no replication**), `OVT_Global.GetMapMarkers()` → `GetMarkers(category)` /
  `GetNearestMarker(pos, category, maxDist)`, and the `OVT_MapLocationBusStop` type. See
  `docs/features/map/location-types/implementation.md` §4.1–§4.3 and its I-2.
- **`map/core`** — the info panel, hover/pin selection, `SetupFastTravelButton`'s host. Its D2/D3/D7 stay
  its own. Contract unchanged by this feature (I-3).
- **`core/controller-migration`** — `OVT_OverthrowController`, `OVT_Global.GetController()`, and the
  `OVT_ShopTransactionComponent` precedent.
- **`occupying/core`** — `m_bQRFActive`, `m_vQRFLocation`, `OVT_QRFControllerComponent.QRF_RANGE`.
- **`resistance/wanted-system`** — `OVT_PlayerWantedComponent.GetWantedLevel()`.
- **`economy/*`** — `PlayerHasMoney` (`:1027`), `TakePlayerMoneyPersistentId` (`:1168`),
  `m_Difficulty.fastTravelCost` / `busTicketPrice` / `minFastTravelDistance`
  (`OVT_DifficultySettings.c:59,71,89`).
- **`resistance/recruits`** — `GetPlayerRecruitsInRadius` (`:246`),
  `GetPlayerRecruitEntitiesInRadius` (`:278`).
- **`map/legacy-retirement`** — hard-gated on this feature; receives the I-1 dead list.

### External — user / Workbench work

| Item | Blocking? | Notes |
|---|---|---|
| **`OVT_TravelRequestComponent` block + fresh GUID on `Prefabs/GameMode/OVT_OverthrowController.et`** | **YES** | The prefab currently lists `OVT_ContainerTransferComponent`, `OVT_ShopTransactionComponent`, `OVT_TowerSabotageComponent`, `RplComponent`, each with a GUID. A component in script but not on the prefab is a **silent no-op** — the client's RPC never leaves the machine |
| **Localization export regeneration** for the four new `.st` ids | No | Layout and labels use literal English until regenerated; the user rebuilds the six `localization_Overthrow.<lang>.conf` exports in Workbench |
| Workbench verification of the new layout widget and input action | No | Both are plain text and hand-editable; the user confirms they load |

**Localization is master-only.** New ids go in `Language/localization_Overthrow.st` **only** — never edit
`Language/localization_Overthrow.<lang>.conf` (Workbench-generated; hand-editing has corrupted all six files
before). Final copy is the user's:

```
OVT-NotAtBusStop        "You must be at a bus stop to catch a bus"
OVT-Map_BringRecruits   "Bring recruits"        (count and extra cost appended in code)
OVT-Map_LeaveRecruits   "Leave recruits behind" (count appended in code)
OVT-Travelled           "Travelled"             (amount appended in code)
```

Reused, already exported: `OVT-CannotFastTravelThere` (`:720`), `-Distance` (`:669`), `-Wanted` (`:764`),
`-DuringQRF` (`:691`), `-ToQRF` (`:742`), `OVT-CannotAfford` (`:559`), `OVT-MustBeDriver` (`:4223`),
`OVT-MustExitVehicle` (`:4289`), `OVT-MainMenu_FastTravel` (`:2862`), `OVT-CatchBus` (`:855`).
`OVT-NeedBusStop` (`:4354`, "You must click near another bus stop") becomes unused by the new path — its
destination-side meaning is now structural — and is left for retirement to remove.

### New and changed files

```
Scripts/Game/
├── Services/OVT_FastTravelService.c            + OVT_TravelVerb / OVT_TravelResult enums
│                                               + ValidateTravel, ReasonKeyFor, ComputeFare,
│                                                 CalculateTravelCost, CountRecruitsInRadius, IsAtBusStop
│                                               ~ CanGlobalFastTravel -> client-only wrapper (SIGNATURE KEPT)
│                                               - ExecuteFastTravel, CanFastTravelToLocationType
├── Components/Controller/
│   └── OVT_TravelRequestComponent.c            NEW
├── Global/OVT_Global.c                         + GetTravelRequests()
├── UI/Map/OVT_OverthrowMapUI.c                 ~ SetupTravelButton, OnTravelClicked,
│                                                 m_bBringRecruits (+ reset in OnMapOpen)
├── UI/Context/OVT_MapContext.c                 + OpenMap() (mode-free map open for the bus action)
└── UserActions/OVT_CatchBusAction.c            ~ :10 opens the map instead of EnableBusTravel()

UI/Layouts/Map/Core/OVT_MapInfoPanel.layout     + BringRecruitsButton (WLib_NavigationButtonSmall,
                                                  SCR_InputButtonComponent GUID {5D346C3DD81D95CD} COPIED)

Configs/System/chimeraInputCommon.conf          + Action OverthrowToggleRecruits, + MapContext ActionRef
                                                ~ OverthrowFastTravel gamepad source (K7 branch)

Prefabs/GameMode/OVT_OverthrowController.et     + OVT_TravelRequestComponent (fresh GUID — Workbench)

Language/localization_Overthrow.st              + 4 ids (master only)

Scripts/Game/Tests/TestSuites/Logic/            + ComputeFare case (optional, §9)
```

---

## 11. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **Parallel development:** Phase 4 cannot compile until `map/location-types` Phase 1 publishes `GetMapMarkers()` / `GetNearestMarker` / `OVT_MapLocationBusStop` | High | Medium | Bus is deliberately the **last** build phase. Phase 1 defines `IsAtBusStop` against the published signature with a null-guard that returns `false` when the registry is absent, so Phases 1–3 compile and ship against a tree without markers. Re-run `tools/compile-check.sh` the moment Phase 1 lands. Do **not** stub a local bus-stop lookup — that would fork the registry |
| **R2** | **`OVT_TravelRequestComponent` is not on the controller prefab**, so every client request silently vanishes | Medium | High | Called out as a **blocking** user dependency (§10). First debug print of Phase 2 goes in `RpcAsk_Travel`; if it never fires from a client but fires on a listen-server host, the prefab is the cause |
| **R3** | **The listen-server case is validated against the wrong player** — N1's failure mode reappearing somewhere else | Medium | High | The rule that closes it: **no function reachable from the server may call `SCR_PlayerController.GetLocalControlledEntity()`**. Enforced by the Phase 1 grep acceptance and by the client-only doc comment. Test on a listen server as well as a dedicated one |
| **R4** | **Client recruit count ≠ server recruit count** on a dedicated server (N9), so the displayed fare is wrong | Medium | Medium | The server charges its own count and **reports the actual amount back**, which the client displays — drift becomes visible, not silent (§4.5). V-4 step 3 checks the two agree in the normal case. If they systematically disagree, the toggle's label is wrong and the cause is `FindRecruitEntity`'s client branch (`:1659-1670`) — file against `resistance/recruits`, do not paper over it |
| **R5** | **Displayed cost is already `$0` on MP clients** because `GetPlayerControlledEntity(localId)` returns null client-side (`OVT_FastTravelService.c:85-87`) | Medium | Medium | Phase 0 step 6 measures it **before** any code is written. If confirmed, the client wrapper resolves the actor with `SCR_PlayerController.GetLocalControlledEntity()` and passes it in — which the parameterized design makes a one-line change |
| **R6** | **Clicking the recruit toggle dismisses the panel** — `OnMapSelection` treats a click with no hovered element as "empty space" → unpin → `ForceHideLocationInfo` (N12) | Medium | High for gamepad | Phase 0 step 8 observes it. Fix if present: guard `OnMapSelection` to ignore selections whose cursor position lies inside the info panel's rect. This is a `map/core`-adjacent change — keep it minimal, and record it in `map/core`'s `context.md`. Fallback: bind the toggle to `gamepad0:pad_right` per K7 so the pad path never routes through `MapSelect` |
| **R7** | **`Rpc()` arity blind spot** — wrong argument count compiles clean and dies silently at the wire (BUG-090) | Medium | High | Only **two** new RPC signatures exist (K10). Both are read by hand against every call site, and the check is recorded in `context.md` (Q-7). A `Print()` at the top of `RpcAsk_Travel` distinguishes "never arrived" from "refused" |
| **R8** | **`gamepad0:x` collision with vanilla's radial menu** (N10) makes fast travel unusable or double-triggering on a controller | Medium | Medium | Observed in Phase 0, resolved in Phase 5 per K7's two branches. `gamepad0:pad_right` is the one demonstrably free pad input |
| **R9** | **The per-recruit fare on fast travel is perceived as a stealth nerf** | Medium | Low | K3 records it prominently as user-approved, notes the N5 correction (legacy already charged per recruit), and states the player-visible consequence. The opt-out toggle means nobody pays for recruits they did not choose to bring |
| **R10** | **Money is taken but the player did not move**, or vice versa | Low | High | Ordering is the mitigation (K8): validate → cost → teleport → charge. `TeleportPlayer`'s return value is checked (`Functions.c:1640-1645`). Q-1 tests it explicitly with an out-of-bounds destination |
| **R11** | **A refusal after the map closes looks like the game ignored the click** | Medium | Medium | Every non-OK result maps to a localized key via `ReasonKeyFor` and reaches the player through `OVT_Global.ShowHint` (Q-2). Test the QRF case specifically: start a QRF between opening the map and pressing travel |
| **R12** | **Concurrent parallel edits** to `OVT_OverthrowMapUI.c` / `OverthrowMap.conf` collide with `map/location-types` work in the same tree | Medium | Low | Touch only `SetupFastTravelButton`/`OnFastTravelClicked`/`OnMapOpen` in the map UI; add no location type and no conf entry (the bus-stop entry is location-types'). Re-check `git status` before each phase — parallel sessions commit mid-work in this tree |

---

*Plan created 2026-08-10 by `/plan-feature map/fast-travel`, replacing the retrospective discovery document.
Discovered architecture and findings F1–F7 preserved and corrected in §3. Use `/proceed` to execute the
phases in order — Phase 0 and Phase 6 are user-driven play-tests, and Phase 2 is flagged for
`network-specialist-advanced`.*
