# Map Fast Travel - Task Checklist

**Last Updated:** 2026-08-10
**Progress:** Partially implemented — rule set done, execution path and bus travel outstanding

---

## Shipped (COMPLETED)

Built on the `new-map` branch, 2025-05 → 2025-08-02.

- [x] ✅ `OVT_FastTravelService` extracted from `OVT_MapContext`
- [x] ✅ Global rule set: debug bypass, min distance, wanted level, QRF mode, affordability
- [x] ✅ Distance-based cost model (`max(1 km, dist/1000) × fastTravelCost`)
- [x] ✅ All four travel-capable location types delegate to `CanGlobalFastTravel`
- [x] ✅ Info-panel button: hidden / disabled+reason / enabled, with cost in the label
- [x] ✅ `OverthrowFastTravel` keybinding registered and referenced by `MapContext`
- [x] ✅ In-vehicle handling: driver travels with the vehicle, passenger refused
- [x] ✅ Retrospective documentation created (2026-08-10)

---

## Verify before fixing

The authority findings are code-reading inferences. Confirm the real behaviour on a server first so fixes target something observed.

- [ ] Two clients via `tools/launch-server.sh` + `tools/launch-game.sh --timeout 3600 --profile ...`
- [ ] **F1** On-foot fast travel on a dedicated server: does the player actually move, and does the server agree?
- [ ] **F2** Is the player actually charged, and does the balance survive the next authoritative sync?
- [ ] **F4** Confirm recruits are left behind
- [ ] In-vehicle-as-driver: vehicle travels; passenger correctly refused
- [ ] Concurrent travel by two players

---

## Fixes

- [ ] **F1 — Make execution server-authoritative.** The on-foot branch calls `SCR_Global.TeleportPlayer` client-side (`:152-155`) while the vehicle branch asks the server (`:140`). Unify on a server request.
- [ ] **F2 — Move payment server-side.** `TakePlayerMoneyPersistentId` (`:139`, `:154`) is a server-shaped write called from the client. The server should validate eligibility *and* take payment atomically with the teleport.
- [ ] **F3 — Migrate off `OVT_PlayerCommsComponent`.** `RequestFastTravel` lives at `OVT_PlayerCommsComponent.c:1483`, reached via `OVT_Global.GetServer()`. Move to a specialized component on `OVT_OverthrowController` per `core/controller-migration`.
- [ ] **F4 — Restore recruit accompaniment.** Legacy used `RequestFastTravelWithRecruits(..., RECRUIT_TRAVEL_RADIUS)` (`OVT_MapContext.c:441,503`); the service only calls `RequestFastTravel`. Restore it, or record the removal as a deliberate design change.
- [ ] Re-validate eligibility **on the server** — a client must not be able to travel by driving the UI regardless of what the panel showed.

---

## Bus travel migration (F5)

**Blocked on `map/location-types` G4** (bus stops as an Overthrow marker component).

- [ ] `OVT_CatchBusAction` (`:10`) puts the player into destination-selection on the map instead of `EnableBusTravel()`
- [ ] Bus-stop markers become the selectable destinations
- [ ] Distance-based fare preserved; charged server-side
- [ ] **Selection state must not survive a map close** — the legacy defect where an engine-side close left bus mode armed and the next map click charged a fare (BUG-069 part 2) must not be reintroduced
- [ ] Retire `EnableBusTravel`/`DisableBusTravel`/`m_bBusTravelActive` from `OVT_MapContext` (executed in `map/legacy-retirement`)

---

## Cleanup

- [ ] **F6** Delete `CanFastTravelToLocationType` (`:160-170`) — no callers — or make it the single entry point
- [ ] **F7** Reconcile the double affordability check (`:62-71` and `:110-114`) once payment is server-side
- [ ] Unify identity types — `CanGlobalFastTravel` takes a persistent-ID string, `CalculateFastTravelCost` takes an int; conversion happens mid-function (`:58`)
- [ ] Consider replacing the static class with an explicit server-side entry point so the authority boundary is visible in the type system

---

## Verification (after fixes)

- [ ] Each refusal rule in turn: wanted > 0, QRF under each `QRFFastTravelMode`, below `minFastTravelDistance`, insufficient funds
- [ ] Displayed availability and cost match what the server enforces and charges
- [ ] **Gamepad:** button reachable and activatable; manually confirm `gamepad0:x` does not collide with a vanilla map action (the input-conflict checker cannot see inline `ActionContext` actions)
- [ ] JIP client travelling shortly after joining
- [ ] `tools/compile-check.sh` clean, `tools/run-tests.sh "{6A6E2A002F53A581}"` green

---

## Future Enhancements

Out of scope for this feature: no new travel modes, no route planning, no vehicles-as-anchors, no rebalancing of the cost formula.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
