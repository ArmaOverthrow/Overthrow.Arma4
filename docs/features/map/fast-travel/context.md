# Map Fast Travel - Context & Decisions

**Last Updated:** 2026-08-10
**Current Phase:** Retrospective Documentation
**Status:** ⚠️ Partially Implemented — rules done, execution path and bus travel outstanding

---

## Quick Status

**What's Done:**
- ✅ `OVT_FastTravelService` extracted from `OVT_MapContext` with the global rule set and cost model
- ✅ All four travel-capable location types delegate to it
- ✅ Info-panel button wired with cost, enable state and localized refusal reasons
- ✅ `OverthrowFastTravel` keybinding registered (`KC_SPACE` / `gamepad0:x`, `MapContext`)
- ✅ Retrospective documentation created

**What's Next:**
- 🔴 **F1/F2 — server authority.** On-foot travel teleports and debits money client-side. Verify on a real server *before* fixing, so the fix is aimed at a confirmed behaviour.
- 🔴 **F4 — recruits regressed.** New service never calls `RequestFastTravelWithRecruits`; legacy did.
- 🔴 **F3 — off `OVT_PlayerCommsComponent`** and onto `OVT_OverthrowController`.
- 🔴 **F5 — migrate bus travel** (blocked on `map/location-types` G4).

**Blockers:**
- F5 is blocked on the bus-stop marker component (`map/location-types` G4).
- F1/F2 verification needs the two-client MP harness.

---

## Key Files

| File | Role |
|---|---|
| `Scripts/Game/Services/OVT_FastTravelService.c` | The service — rules, cost, execution (171 lines) |
| `Scripts/Game/UI/Map/OVT_OverthrowMapUI.c:449-546` | Button setup, cost display, click handler |
| `Configs/System/chimeraInputCommon.conf:650,704` | `OverthrowFastTravel` action + `MapContext` reference |
| `UI/Layouts/Map/Core/OVT_MapInfoPanel.layout` | `FastTravelButton`, `FastTravelReason`, `OverthrowFastTravel` widgets |
| `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:1483,1495` | `RequestFastTravel` / `RequestFastTravelWithRecruits` — **deprecated component** |
| `Scripts/Game/UI/Context/OVT_MapContext.c:287-294,441,451,503` | **Legacy** — bus travel mode, and the recruits-variant call the new service dropped |
| `Scripts/Game/UserActions/OVT_CatchBusAction.c:10` | Entry point for bus travel |

---

## Important Decisions

**1. One rule set, one cost model, shared by every location type.**
Types apply a local eligibility gate then call `CanGlobalFastTravel`. There is exactly one implementation of wanted/QRF/distance/affordability. This is the healthy part of the feature and should be preserved through any refactor.

**2. Cost is distance-from-the-player, not per-destination.**
`max(1 km, dist/1000) × m_Difficulty.fastTravelCost`. The same destination costs different amounts depending on where you stand. Debug mode makes it free.

**3. Button state distinguishes "never" from "not now".**
Hidden when the type's `m_bCanFastTravel` is false; shown-but-disabled with a reason when the rules refuse. Worth keeping — it is better than the legacy menu flow.

**4. The service is a static class.**
Convenient, but it makes the network boundary implicit — which is how a client-side teleport and a client-side money debit survived the extraction. When fixing F1/F2, prefer an explicit server-side entry point over another static.

---

## Gotchas & Learnings

- **`ExecuteFastTravel` has two authority models in one function.** In-vehicle-as-driver asks the server (`RequestFastTravel`); on-foot calls `SCR_Global.TeleportPlayer` directly, which moves the entity **on the calling machine** (`ArmaReforger/.../Functions.c:1638`). On foot is the default path.
- **`TakePlayerMoneyPersistentId` is a server-shaped method** — it mutates `player.money` then calls `StreamPlayerMoney` to push the value out (`OVT_EconomyManagerComponent.c:1168-1180`). Calling it from the client mutates only the client's copy. Do not treat single-player success as evidence it works.
- **`OVT_Global.GetServer()` is `OVT_PlayerCommsComponent`** (`OVT_Global.c:67`) — the deprecated component. Any existing call through it is a migration target, not a precedent to copy.
- **The legacy context called the *recruits* variant, the new service does not.** `RequestFastTravelWithRecruits(..., RECRUIT_TRAVEL_RADIUS)` at `OVT_MapContext.c:441,503` vs `RequestFastTravel` at `OVT_FastTravelService.c:140`. Silent behaviour loss — the kind that no compile check or test would surface.
- **`CanFastTravelToLocationType` is dead** (`:160-170`) and its body admits it defers to the type's own method.
- **The `gamepad0:x` binding needs manual conflict checking.** The project's input-conflict script cannot see inline `ActionContext` actions, so a collision with a vanilla map action would not be reported.
- **`CanGlobalFastTravel` refuses everything when the player has no controlled entity** (`:14-16`), and its minimum-distance and cost rules are both measured *from the player's current position*. That is fine for fast travel and fatal for anything involving a dead player — **`map/respawn` (feature 5) must not call it**, and instead reuses only the per-type ownership/control gates. Recorded here because the trap is in this file, not in respawn's.
- **Identity types are mixed:** `CanGlobalFastTravel` takes a persistent-ID string, `CalculateFastTravelCost` takes an int player ID, and the service converts between them mid-function (`:58`).

---

*This context file was created retrospectively by analyzing existing code.*
