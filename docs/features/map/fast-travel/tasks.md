# Map Fast Travel - Task Checklist

**Last Updated:** 2026-08-10
**Progress:** ✅ COMPLETE — 5/5 build phases, review + fix pass, and the Phase 6 play-test gate discharged 2026-08-10

> Rebuilt 2026-08-10 from `implementation.md` §5, replacing the retrospective checklist.
> The old F1–F7 finding list is preserved in `implementation.md` §3.5–3.6; each finding is now
> closed by a numbered phase below.

---

## Already shipped (before this plan)

Built on the `new-map` branch, 2025-05 → 2025-08-02.

- [x] ✅ `OVT_FastTravelService` extracted from `OVT_MapContext`
- [x] ✅ Global rule set: debug bypass, min distance, wanted level, QRF mode, affordability
- [x] ✅ Distance-based cost model (`max(1 km, dist/1000) × fastTravelCost`)
- [x] ✅ All four travel-capable location types delegate to `CanGlobalFastTravel`
- [x] ✅ Info-panel button: hidden / disabled+reason / enabled, with cost in the label
- [x] ✅ `OverthrowFastTravel` keybinding registered and referenced by `MapContext`
- [x] ✅ In-vehicle handling: driver travels with the vehicle, passenger refused

---

## Phase 0 — Verify before fixing: two-client MP session — **user-driven, no agent**

⚠️ **NOT RUN — cannot be automated.** An autonomous run cannot drive two Reforger clients and
observe them. The build phases proceeded on the plan's code-reading inferences; the two branch
decisions Phase 0 was meant to settle were resolved conservatively instead (see `context.md`
"Phase 0 substitutions").

- [ ] Two clients via `tools/launch-server.sh` + `tools/launch-game.sh --timeout 3600 --profile ...`
- [ ] **F1** On-foot fast travel on a dedicated server: does the player move, and does the server agree?
- [ ] **F2** Is the player actually charged, and does the balance survive the next authoritative sync?
- [ ] **F4** Confirm recruits were being left behind pre-fix
- [ ] **N9** Does the button show a non-zero cost on a dedicated-server client?
- [ ] **N10** Does `gamepad0:x` open vanilla's radial menu on the map?
- [ ] **N12** Does clicking the travel button while the panel is pinned dismiss the panel?

---

## Phase 1 — Parameterize the service, unify identity, delete dead code — `component-developer`

- [x] `OVT_TravelVerb` + `OVT_TravelResult` enums at the file top
- [x] `ValidateTravel(verb, targetPos, playerId, actor, recruitCount)` — the whole rule table, no local-entity resolution
- [x] `ReasonKeyFor(result)` — every code maps to a localization key
- [x] `ComputeFare(distMeters, unitPrice, applyKmFloor, recruitCount)` — pure maths, world-free
- [x] `CalculateTravelCost(verb, targetPos, actor, recruitCount)` on top of it
- [x] `CountRecruitsInRadius(persId, originPos)` and `IsAtBusStop(pos)` (null-guarded)
- [x] `CanGlobalFastTravel` → documented **client-only** wrapper, signature unchanged
- [x] **F6** Delete `CanFastTravelToLocationType`
- [x] Leave `ExecuteFastTravel` in place (Phase 2 deletes it)
- [x] Gate: compile 0, Fast green, `GetLocalControlledEntity` grep matches only the client wrapper
- [x] Gate: no file under `Scripts/Game/UI/Map/LocationTypes/` modified

---

## Phase 2 — `OVT_TravelRequestComponent`: server authority, payment, recruits — `network-specialist-advanced` (**ADVANCED**)

- [x] `Scripts/Game/Components/Controller/OVT_TravelRequestComponent.c` modelled on `OVT_ShopTransactionComponent`
- [x] `RequestTravel` / `RpcAsk_Travel` / `RpcDo_TravelResult` + `m_OnTravelResult`
- [x] `ResolveOwningPlayerId()` — identity from the controller entity, never the payload
- [x] §4.3 step order: validate → cost → teleport → charge (**load-bearing**)
- [x] Recruit ring teleport lifted verbatim from `OVT_PlayerCommsComponent.c:1529-1547`
- [x] `OVT_Global.GetTravelRequests()`
- [x] `Prefabs/GameMode/OVT_OverthrowController.et` — component block with a fresh GUID
- [x] `OnFastTravelClicked` → `OnTravelClicked`, routed through the new component
- [x] **F1/F2** Delete `OVT_FastTravelService.ExecuteFastTravel`
- [x] **F3** No new-map code calls `OVT_PlayerCommsComponent.RequestFastTravel*`
- [x] Gate: compile 0, All green

---

## Phase 3 — Recruit opt-out toggle — `ui-developer`

- [x] `OVT_MapInfoPanel.layout` — `BringRecruitsButton`, `SCR_InputButtonComponent` GUID **copied** `{5D346C3DD81D95CD}`
- [x] `chimeraInputCommon.conf` — `Action OverthrowToggleRecruits` + `MapContext` ActionRef
- [x] `m_bBringRecruits = true`, reset in `OnMapOpen`
- [x] Toggle re-runs `SetupTravelButton` so both labels update live
- [x] Toggle hidden entirely when `CountRecruitsInRadius(...) == 0`
- [x] **F4** `m_bBringRecruits` passed through `RequestTravel`
- [x] New `.st` ids in the master file only

---

## Phase 4 — Bus travel migration (F5) — `component-developer` + `ui-developer`

- [x] `IsAtBusStop` — real registry query, replacing the null-guarded stub
- [x] `ValidateTravel(BUS, ...)` — origin stop, destination stop, in-vehicle refusal, affordability, **no** wanted/QRF/min-distance
- [x] `CalculateTravelCost(BUS, ...)` — no 1 km floor, per `OVT_FastTravelService.VerbUsesKmFloor` (`:246`), the one place the verb→floor mapping lives
- [x] `SetupTravelButton` bus branch — `OVT_MapLocationBusStop` cast, `#OVT-CatchBus` label
- [x] `OVT_CatchBusAction.PerformAction` (`OVT_CatchBusAction.c:16`) — opens the map instead of `EnableBusTravel()`
- [x] **Selection state must not survive a map close** — no armed mode exists to survive
- [x] Gate: no script calls `EnableBusTravel`

---

## Phase 5 — Input verification, cleanup, retirement handoff — `ui-developer`

- [x] Resolve N10 per K7 (gamepad source for `OverthrowFastTravel`)
- [x] Hand-confirm `OverthrowToggleRecruits`' inputs against vanilla's `MapContext` block
- [x] Write the exact dead-code list for `map/legacy-retirement` into `context.md`
- [x] Confirm `ExecuteFastTravel` and `CanFastTravelToLocationType` are gone

---

## Cross-phase review + fix pass — 2026-08-10

An adversarial review by an agent with no implementation context read the whole diff as one change.
Verdict: **shippable to a play-test; nothing critical in the new code.** The authority fence holds
transitively, both RPC arities are correct, all 9 paths through the server routine and all 6 fare
combinations were walked, and "pay solo, arrive with a squad" is structurally impossible.

11 findings; 9 fixed, 2 deliberately deferred.

- [x] **F2** `OVT_MapContext.MapClick` had no info-panel guard — a second, independent `MapSelect`
      listener meant one click on the travel button could fire both handlers (two debits at two
      different fare models, two conflicting teleports). Guarded via `SCR_MapEntity.GetMapUIComponent`
- [x] **F3** Label text moved to `WidgetManager.Translate` with `%1`/`%2` placeholders. The review's
      premise (that `#key` + free text fails to render) was **disproven by a live probe** — it renders
      fine — but the concatenation hard-coded English word order, so the fix stands on i18n grounds
- [x] **F5** `VerbUsesKmFloor(verb)` extracted so the verb→floor mapping lives in one place, and
      asserted world-free. **Proven able to fail** (inverted the returns → red, reverted → green)
- [x] **F6** `KC_R` investigated and **kept**, on three lines of evidence — the decisive one being that
      `GadgetContextToggleable` only activates for gadgets whose use-mask contains `FROM_ACTION`, which
      no map prefab sets. R cannot stow the map
- [x] **F7** `m_HoveredElement` now cleared in `OnMapClose` — it was the one member surviving a close,
      and it feeds a money path
- [x] **F8** Affordability re-check restricted to `FAST_TRAVEL` (redundant for `BUS`)
- [x] **F9** Recruit toggle stays visible but non-interactive on a refused trip — **except** when the
      refusal is affordability, since switching recruits off is then the player's only remedy
- [x] **F10** A successful *free* trip now shows a hint instead of being silent
- [x] **F11** Dead-code hand-off line references corrected and re-derived; **the `.st` guidance no
      longer gives a line range at all** (the quoted range would have corrupted the master mid-block —
      the exact failure CLAUDE.md records happening once before)
- [ ] **F1** *(deferred — `map/legacy-retirement` owns it)* Legacy fast travel is still reachable from
      `OVT_MainMenuContext.c:218` and still runs the client-side teleport, the flat fare and the
      unvalidated comms RPCs. **Both paths are live in this build**
- [ ] **F4** *(user action)* The five new `.st` ids are not in the generated exports — see the gate below

---

## ✅ Gate before Phase 6: regenerate the localization exports — DONE

`OVT-Map_BringRecruits`, `OVT-Map_LeaveRecruits`, `OVT-Travelled`, `OVT-TravelWithFare` and
`OVT-NotAtBusStop` were added to `Language/localization_Overthrow.st` (master) only. **The user
regenerated all six `localization_Overthrow.<lang>.conf` exports in Workbench 2026-08-10** — verified:
all five ids resolve in `en-us`, and all six exports are modified in the working tree.

Because F3 moved the numbers *inside* the string values, an unresolved key would have dropped the price
as well as the words. That risk is discharged.

❌ Never hand-edit the `.conf` exports.

---

## Phase 6 — Verification gate — **✅ DISCHARGED 2026-08-10**

**The user play-tested the feature and reported it all green.** This is the gate the whole feature
rested on: `compile-check` and both test groups cannot see `.layout` files, `.conf` entries, the string
table, input contexts, UI behaviour or anything multiplayer, so "green" from the automated spine only
ever meant "the scripts compile".

- [x] V-1 … V-6 — user-reported green

What this discharges, that nothing automated could:
- The server-authoritative path actually executes (**F1/F2**) — travel happens and money is taken by
  the server, not just in a code reading.
- The recruit toggle renders, labels correctly, and updates the fare live (**F4**) — including that
  the R6 `IsSelectionOnInfoPanel` guard works, which was the single item flagged "unverified at
  runtime, may be inert" and was the toggle's only gamepad path.
- Bus travel end to end, and the parameterised localization strings render with their numbers.
- `keyboard:KC_R` does not stow the map — the FIX 4 conclusion rested on inference from
  `SCR_GadgetManagerComponent`'s use-mask rather than on a shipped example.
- `gamepad0:pad_right` does not shadow a vanilla map action.

**Still not exercised by a green single-session play-test** (record honestly rather than assume):
Phase 0's two-client dedicated-server scenarios — concurrent travel by two players, JIP travel shortly
after joining, and the listen-server-host result short-circuit. See `implementation.md` §9 rows 12–13.

---

## Post-ship fix — map item left in hand (2026-08-10)

User-reported from the Phase 6 play-test: *"after fast travelling I was holding the map item up, the
map did close but didn't stow the item."*

**Root cause — Overthrow closed the map backwards relative to vanilla.** Both `HideMap()` methods called
`SCR_MapGadgetComponent.SetMapMode(false)`, which closes the **map view** and nothing else. Vanilla's own
close goes the other way: `SCR_GadgetManagerComponent.HandleInput` stows the gadget
(`SetGadgetMode(gadget, IN_STORAGE)`) and the view closes as a *consequence*. So the screen closed and the
item stayed raised in hand.

Why it was written that way: `SetGadgetMode`'s hand→storage branch **early-returns for `EGadgetType.MAP`**
(`SCR_GadgetManagerComponent.c:272-273`, a hotfix against input spam), so stowing alone never closes the
view synchronously — which is presumably what pushed someone to call `SetMapMode(false)` directly.

- [x] `OVT_OverthrowMapUI.HideMap()` — clear focus, then stow through a guarded `StowMapGadget()` helper
- [x] `OVT_MapContext.HideMap()` — same fix inline; all **eight** legacy callers route through it
- [x] Guarded: no-ops on no gadget entity, no local controlled character, dead character, no gadget
      manager, or when the held gadget is not this map — so nothing else is ever stowed
- [x] **Ordering is load-bearing:** `ToggleFocused(false)` **first**, stow second. Clearing focus closes
      the view immediately (stowing would delay it by the stow animation, changing *when* the map closes)
      and sets `m_bFocused = false`, so the later `ModeClear(IN_HAND)` skips a redundant second close
- [x] `SetMapMode(false)` removed as redundant — `ToggleFocused(false)` is a strict superset
- [x] **Second, quieter half of the same bug fixed:** the direct `SetMapMode(false)` never ran
      `SCR_PlayerController.SetGadgetFocus(false)`, so the controller stayed flagged gadget-focused after
      **every** map close. `ToggleFocused(false)` clears it
- [x] Gates: compile 0 (5958 files), All **79/79**
- [ ] **Re-test needed:** travel with the map genuinely in hand (opened via the gadget key) → map closes
      *and* the character stows it; and confirm nothing is stowed when the map was opened from a context
      without the item in hand

> `ShowMap()` still uses `SetMapMode(true)` deliberately — Overthrow opens the map from contexts with no
> equip animation, and changing that would change how the map opens.

---

## Future Enhancements

Out of scope: no new travel modes, no route planning, no vehicles-as-anchors, no rebalancing of
the cost formula beyond the recorded K3 recruit-fare decision.
