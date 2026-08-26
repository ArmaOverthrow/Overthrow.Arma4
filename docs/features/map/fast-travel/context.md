# Map Fast Travel - Context & Decisions

**Last Updated:** 2026-08-11
**Current Phase:** ✅ **CLOSED** — every gate discharged, including multiplayer
**Status:** ✅ **CLOSED 2026-08-11** — Phases 1–5 landed, reviewed, fixed, play-tested green 2026-08-10; one
post-ship fix (map item not stowed on close) re-tested green; and the Phase 0 two-client MP matrix
discharged 2026-08-11. **The user reports all play-test items green, including MP.**

---

## Quick Status

**What's Done:**

- ✅ **Phase 1** — `OVT_FastTravelService` parameterized. `OVT_TravelVerb` / `OVT_TravelResult` enums, `ValidateTravel` (the one authoritative rule table, actor passed in), `ReasonKeyFor`, pure `ComputeFare`, `CalculateTravelCost`, `CountRecruitsInRadius`, `IsAtBusStop`. `CanGlobalFastTravel` is now a documented client-only wrapper with its signature intact. `CanFastTravelToLocationType` deleted (**F6 closed**).
- ✅ New Logic-tier test `OVT_TEST_Logic_TravelFares` (30 assertions), proven able to fail.
- ✅ Pre-existing: the rule set, cost model, per-type delegation, info-panel button, keybinding, in-vehicle handling.

- ✅ **Phase 2** — `OVT_TravelRequestComponent`: server authority, atomic payment, recruit accompaniment (**F1/F2/F3/F4**).
- ✅ **Phase 3** — recruit opt-out toggle (`BringRecruitsButton`, `OverthrowToggleRecruits`, R6 guard).
- ✅ **Phase 4** — bus travel migration (**F5**); `OVT_CatchBusAction` now calls `OVT_MapContext.OpenMap()` and arms nothing.
- ✅ **Phase 5** — K7 resolved (fast travel moved off `gamepad0:x`), the advisory affordability gap closed, and the dead-code hand-off written below.

- ✅ **Adversarial-review fix pass (2026-08-10)** — nine findings closed. The two that change behaviour:
  the legacy `OVT_MapContext.MapClick` listener now refuses a click that landed on the new info panel
  (it could otherwise fire _alongside_ the panel's own travel request: two debits, two destinations), and
  every travel label/hint is now built with `%1`/`%2` placeholders through `WidgetManager.Translate`
  instead of concatenating free text onto a `#OVT-` key. `OVT_FastTravelService.VerbUsesKmFloor` now owns
  the verb→fare-floor mapping and the Logic tier asserts it directly.

- ✅ **Localization exports regenerated** by the user 2026-08-10 — all five new ids resolve; the raw-key
  hazard described under Gotchas is discharged (the Gotcha is kept as a _learning_, not a live warning).
- ✅ **Phase 6** — the single-session verification gate, user-run and green 2026-08-10.
- ✅ **Post-ship fix** — map closed but the item stayed raised in hand; fixed and re-tested green 2026-08-11.
- ✅ **Phase 0** — the two-client dedicated-server MP matrix, user-run and green 2026-08-11. It was written
  as a _verify-before_ and was discharged as a _verify-after_; that distinction is recorded in `tasks.md`.

- ✅ **Post-close fix: server-side destination authority (2026-08-12, from the pre-submit PR review).**
  Phase 2 made *who may travel* server-authoritative but left *where* client-chosen: `ValidateTravel`
  checked distance, wanted level, QRF, vehicle seat and affordability, and then the component teleported
  to whatever vector arrived. The "owned house / your camp / a FOB / a base we hold" rule lived only in
  the per-type `CanFastTravel` overrides, which are client code and which this service labels advisory —
  so a crafted `RequestTravel` could name **any** coordinate and be teleported to it for the fare.
  **Closed by reusing the respawn enumeration rather than writing a second one**: exactly the four types
  carrying `m_bCanFastTravel 1` (Bases, FOBs, Camps, Houses) apply the same four rules, from the same four
  managers, reading the same vectors (`base.location`, `fob.location`, `camp.location`,
  `house.GetOrigin()`, warehouses skipped) that `OVT_RespawnService.CollectEligiblePositions` already
  enumerates — verified field by field before the change, because a mismatch would have refused every
  legitimate trip.
  - `CollectEligiblePositions` gained `excludeActiveQrf` (default `true`, so respawn is byte-for-byte
    unchanged). Fast travel passes `false`: it has its own QRF rules and `QRFFastTravelMode.FREE` permits
    a trip the respawn-shaped filter would have reported as an unrecognised destination.
  - New `OVT_FastTravelService.ResolveFastTravelDestination` + pure `MatchesAnyPosition`, and
    `OVT_TravelRequestComponent` step **6b**, which **reassigns `targetPos`** to the server's own vector so
    the fare, the arrival point and the recruit ring structurally cannot read the client's number.
  - Refuses with the existing `BAD_DESTINATION` → `#OVT-CannotFastTravelThere`. **No new string id.**
  - Debug mode still travels free and anywhere, stated in both places rather than inferred.
  - BUS is untouched: `IsAtBusStop` already made its destination server-authoritative.
  - New Logic case `OVT_TEST_Logic_TravelDestination`, **proven able to fail twice** — echoing the caller's
    vector back fails the identity assertions, and an always-true tail fails the empty-set case.

**What's Next:** nothing. **The feature is closed.** Two things are carried forward as record rather than work:

- The **listen-server-host result short-circuit** is the one branch nobody has stood in front of — the MP
  testing ran against a dedicated server. Not a known defect; an untested branch.
- **F4's pre-fix observation** is now permanently unobservable (the fix shipped first). The post-fix
  behaviour — recruits ride along, the fare prices them — is what was verified.

**Blockers:** none, and none remain. Phase 4's hard dependency (`map/location-types` G4) landed — `OVT_Global.GetMapMarkers()` (`:199`), `OVT_MapMarkerManagerComponent.GetNearestMarker()` (`:176`) and `OVT_MapLocationBusStop` all exist in this tree.

---

## Phase 0 substitutions — how the build was justified before anyone tested it

> ✅ **Superseded by observation 2026-08-11.** The user has now run the two-client MP play-test and
> reported it green, so S1 and S3 below are no longer standing on static evidence alone — the branches
> they chose were confirmed in a live dedicated-server session. This section is kept because it records
> **why the build was allowed to proceed untested**, and because a later reader must still be able to
> tell which conclusions were originally measured and which were inferred.

**Phase 0 was not run before the fix.** It is a two-client multiplayer play-test (`tools/launch-server.sh`

- two `tools/launch-game.sh` clients) and an autonomous session cannot drive two Reforger windows and
  observe them. Phases 1–5 therefore proceeded on the plan's code-reading inferences.

Phase 0 existed to settle three open branch decisions. Two of the three were settled from **static
evidence instead**, and the substitution is recorded here because a later reader must not mistake these
for observed facts:

**S1 — N9/R5, the `$0`-cost question → resolved by taking the safe branch, not by measuring.**
The question was whether `GetPlayerControlledEntity(localId)` returns null on a dedicated-server client,
making the displayed fare `$0`. Rather than measure, Phase 1 switched the client path to
`SCR_PlayerController.GetLocalControlledEntity()`, which is correct on **both** dedicated and listen
servers. The branch is therefore safe whichever way the observation would have gone. Both client entry
points (`CanGlobalFastTravel` **and** the fare call the button label actually makes, now
`OVT_FastTravelService.CalculateTravelCost` at `OVT_OverthrowMapUI.c:624`) route through a single
`GetLocalActor()` helper — fixing only the first would
have left the _displayed cost_ wrong while the _enable state_ was right, which is precisely the
client/server disagreement this feature exists to eliminate.

**S2 — N10/K7, the gamepad collision → CONFIRMED REAL from config, no play-test needed.**

- Vanilla binds `Action MapContextualMenu` → `gamepad0:x` inside `ActionContext MapContext` (Priority 50)
  — `ArmaReforger/Configs/System/chimeraInputCommon.conf:8128+`, the action block at offset +42/+51.
- `SCR_MapRadialUI` **is live on Overthrow's map**: it is in vanilla's `Configs/Map/MapOverthrow.conf:17`
  and Overthrow's same-GUID override is a **delta**, which disables only `OVT_MapIcons` and
  `OVT_MapThreatGrid` — it does not remove the radial UI.
- `OverthrowFastTravel` currently binds `keyboard:KC_SPACE` + `gamepad0:x`
  (`Configs/System/chimeraInputCommon.conf:650`).
- `gamepad0:pad_right` is confirmed **free** in vanilla's `MapContext` — it is the only free pad input.

→ **K7's first branch is taken:** move `OverthrowFastTravel`'s gamepad source to `gamepad0:pad_right`, and
give `OverthrowToggleRecruits` **no** gamepad binding (keyboard `KC_R` plus the map-cursor path).

**S2 — EXECUTED in Phase 5.** `Configs/System/chimeraInputCommon.conf:659` now reads
`Input "gamepad0:pad_right"`; `keyboard:KC_SPACE` is unchanged.

**S2b — `OverthrowToggleRecruits` on `keyboard:KC_R` was re-examined after Phase 5 and KEPT.** Phase 5
only established that `KC_R` is free _within_ `MapContext`, which is narrower than it sounds: `KC_R` is
also `GadgetActivate` (`GadgetContextToggleable`, Priority 40), `CharacterInspect` (`GadgetContext`,
Priority 40), `CharacterReload` (`CharacterWeaponContext`, Priority 10) and `CharacterUseItem`
(`CharacterGeneralContext`, Priority 10). Three independent lines of evidence say `MapContext`'s
Priority 50 wins:

- **Higher priority suppresses lower, and this project has already measured it.** BUG-092 — VON at
  Priority 110 eats `gamepad0:shoulder_left` from Overthrow menu contexts at Priority 50, so the press
  never arrives at the lower context rather than firing both. Direction settled: bigger number wins.
- **Overthrow already ships this exact shape, on this exact key row.** `OverthrowFastTravel` has been
  `keyboard:KC_SPACE` in `MapContext` for years, while `CharacterMovementContext` (Priority 10) binds
  `keyboard:KC_SPACE` → `CharacterJump` and is live whenever the player controls a character. Space
  fast-travels from the open map and the character does not also jump. `KC_R` vs `CharacterReload` is
  the same 50-vs-10 relationship.
- **BI designs this way deliberately.** Vanilla `MapContext` reuses `KC_K` (`MapToolCompass` vs
  `GadgetCompass`), `KC_O` (`MapToolWatch` vs `GadgetWatch`), `KC_B` (`MapToolProtractor` vs
  `GadgetBinoculars`) and `KC_F` (`MapOpenBase` vs `CharacterAction`) — every one of them also bound in
  `CharacterGeneralContext` at Priority 10.

The specific worry — "R would stow the map" — is additionally false at the source: `GadgetActivate`
lives in `GadgetContextToggleable`, which `SCR_GadgetManagerComponent.Update` (`:1214-1215`) activates
only when the held gadget's use mask contains `FROM_ACTION`. `SCR_GadgetComponent.m_eUseMask` defaults to
`NONE` (`SCR_GadgetComponent.c:59`) and no map prefab in the base game overrides it, so that context is
never activated for the map. The map is toggled by `GadgetMap` (`KC_M` / `gamepad0:view`) in
`GadgetMapContext` (Priority **55**), not by `GadgetActivate`.

A sweep of every context that binds `keyboard:KC_R` found nothing above Priority 50 that can be active
with the fullscreen map open: `WorkshopDownloadManagerContext`/`VONMenuContext` (100),
`CharacterWeaponInspectionContext` (90), `MapMarkerEditContext` (60) and `DialogContext` (51) are all
mutually exclusive with the panel, and the last two outranking us is the safe direction anyway.

⚠️ **Still true and still unaddressed:** `OverthrowToggleRecruits` has **no gamepad source** (S2 spent
`pad_right` on fast travel). A controller reaches the toggle only by pointing the map cursor at it and
pressing `MapSelect`, which is precisely why R6's `IsSelectionOnInfoPanel` guard is load-bearing rather
than theoretical.

**S2a — correction to N10 found while hand-verifying (Phase 5).** N10 says "`keyboard:KC_SPACE` is free in
vanilla's `MapContext`". That is true of the context's **inline** actions but **not** of its `ActionRefs`
list, which N10 never enumerated. `MapContext` also references `ManualCameraTeleportPlayer`
(`ArmaReforger/Configs/System/chimeraInputCommon.conf` `MapContext` `ActionRefs` block, action defined in
`Configs/System/Actions/ManualCameraTeleportPlayer.conf:6,12`) → **`keyboard:KC_SPACE` + `gamepad0:a`**.
So `OverthrowFastTravel`'s keyboard source shares `KC_SPACE` with the manual-camera "teleport player" debug
action inside this context. It is left as-is: that action only does anything with the manual/debug camera
active, and changing the long-standing fast-travel key is out of Phase 5's scope. **Recorded so it is not
rediscovered as a surprise.** The same `ActionRefs` sweep is what confirms `gamepad0:b`, `y` and
`shoulder_left` are _not_ free here (`TasksOpen` → `b`; `ChatToggle`/`HintDismiss` → `y`; `ChatToggle` →
`shoulder_left`), leaving `gamepad0:pad_right` as the only genuinely free pad button.

**S3 — N12/R6, the panel-dismiss hazard → CONFIRMED REAL by code reading; NOT confirmed at runtime.**
`OVT_OverthrowMapUI.OnMapSelection` (`:92-121`) unconditionally runs `m_bSelectionPinned = false;
m_PinnedElement = null; ForceHideLocationInfo();` in its `else` branch — i.e. whenever no map _element_ is
hovered, which is what a click landing on the info panel looks like. Today this is **masked**, because the
travel handler closes the map anyway; it is not masked for the recruit toggle, which must leave the panel
open. What remains genuinely unobserved is whether the widget consumes the click before the map's
selection handler sees it. Phase 3 applies R6's guard regardless — it is cheap and inert if the hazard
does not manifest.

**Paid 2026-08-11, with one permanent gap.** F1/F2/F4 were never _observed_ pre-fix, and never will be —
the fixes shipped first, so the old behaviour no longer exists in any build to watch fail. The fixes were
justified independently (a client-side teleport and a client-side money debit are wrong on a dedicated
server by construction, and `CLAUDE.md` forbids the pattern) and the **post-fix** behaviour is now
observed green in a two-client dedicated-server session. That is the strongest form of this evidence
still available; it is not the same thing as having reproduced the bugs.

---

## Key Files

| File                                                                           | Role                                                                                                                                     |
| ------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `Scripts/Game/Services/OVT_FastTravelService.c`                                | The service — `ValidateTravel` (authoritative), fares, shared lookups, and a clearly fenced CLIENT-ONLY section                          |
| `Scripts/Game/Components/Controller/OVT_TravelRequestComponent.c`              | **Phase 2** — the server-authoritative request relay                                                                                     |
| `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_TravelFares.c`             | `VerbUsesKmFloor` mapping + `ComputeFare` arithmetic, 34 assertions                                                                      |
| `Scripts/Game/UI/Map/OVT_OverthrowMapUI.c:558-686`                             | `SetupTravelButton` — verb, fare, enable state, label                                                                                    |
| `Scripts/Game/UI/Map/OVT_OverthrowMapUI.c:690,732,769,859`                     | `CanAffordEffectiveFare`, `EvaluateTravel`, `SetupRecruitToggle`, `OnTravelClicked`                                                      |
| `Configs/System/chimeraInputCommon.conf:650-665,666-677,714-719`               | `OverthrowFastTravel` + `OverthrowToggleRecruits` actions and the additive `MapContext` `ActionRefs` delta                               |
| `UI/Layouts/Map/Core/OVT_MapInfoPanel.layout`                                  | `FastTravelButton`, `FastTravelReason`, `OverthrowFastTravel` widgets                                                                    |
| `Prefabs/GameMode/OVT_OverthrowController.et`                                  | Plain text, hand-editable — the new component's home                                                                                     |
| `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:1483,1495`          | `RequestFastTravel*` — **deprecated component**, retirement's to delete                                                                  |
| `Scripts/Game/UI/Context/OVT_MapContext.c:376-384`                             | `IsOverthrowInfoPanelVisible` — the guard that stops the legacy `MapClick` listener double-handling a click on the new info panel        |
| `Scripts/Game/UI/Context/OVT_MapContext.c:301-309,331-334,471,483,493-559,552` | **Legacy** — bus travel mode and the recruits-variant calls; see the dead-code hand-off at the end of this file for current line numbers |
| `Scripts/Game/UserActions/OVT_CatchBusAction.c:16`                             | Entry point for bus travel — now `OpenMap()`, arms nothing                                                                               |

---

## Important Decisions

**1. One rule set, one cost model, shared by every location type — now genuinely shared.**
Phase 1 made the actor a _parameter_, so client and server run byte-identical rules. This is the healthy
part of the feature and the thing that makes requirement 15 ("displayed availability matches enforced
availability") satisfiable at all.

**2. Cost is distance-from-the-player, not per-destination.**
`max(1 km, dist/1000) × fastTravelCost`. The same destination costs different amounts depending on where
you stand. Debug mode makes it free. Bus fares have **no** 1 km floor.

**3. Button state distinguishes "never" from "not now".**
Hidden when the type's `m_bCanFastTravel` is false; shown-but-disabled with a reason when the rules refuse.

**4. The service is still a static class, but the network boundary is now explicit in the file's shape.**
A `CLIENT-ONLY WRAPPERS` section header and a class doc comment carry the rule that no server-reachable
function may resolve a local entity. The grep gate (`GetLocalControlledEntity` appears only in that
section) is what keeps it true.

**5. `CanGlobalFastTravel` keeps its exact signature** so `map/location-types`' five per-type overrides
compile untouched. The identity-type mismatch is therefore _contained_ to one conversion inside one
client-only wrapper rather than _resolved_ — full unification needs a signature change K1 forbids while
the sibling feature is in flight.

---

## Gotchas & Learnings

- **`compile-check.sh` has a second blind spot: argument-count overflow on base-class methods with
  defaulted params.** A test file calling `SetResultFailure` with more than a format string plus **three**
  substitution params (`SCR_AutotestCaseBase.c:50`) compiled clean and was then rejected by the _runtime_
  script compiler — `run-tests.sh` reported INDETERMINATE / exit 2 with no `junit.xml`. Concatenate
  instead. This sits alongside the known `Rpc()` arity blind spot: **exit 0 from compile-check does not
  mean the scripts load.** Always read `run-tests.sh`'s verdict, not just the compiler's.
- **A passenger now sees the travel button _disabled_ rather than enabled-then-refused.** Phase 1's
  wrapper delegates the whole rule table, including the in-vehicle check that previously lived only in
  `ExecuteFastTravel`. Intended (it is F-3's end state) and better UX, but it is a visible single-player
  change, so Phase 1 was not the pure no-op its acceptance criterion claimed.
- **`TakePlayerMoneyPersistentId` is a server-shaped method** — it mutates `player.money` then calls
  `StreamPlayerMoney` to push the value out (`OVT_EconomyManagerComponent.c:1168-1180`). Calling it from a
  client mutates only that client's copy. **Do not treat single-player success as evidence it works.**
- **`SCR_Global.TeleportPlayer` moves the entity on the _calling machine_** (`ArmaReforger/.../Functions.c:1638`)
  and returns `false` on an out-of-bounds destination (`:1640-1645`). Both facts are load-bearing: the
  first is why F1 was a bug, the second is why the charge-after-teleport ordering needs no refund path.
- **`OVT_Global.GetServer()` is `OVT_PlayerCommsComponent`** (`OVT_Global.c:67`) — the deprecated
  component. Any call through it is a migration target, not a precedent to copy.
- **`CanGlobalFastTravel` refuses everything when the player has no controlled entity**, and its minimum
  distance and cost are both measured _from the player's current position_. Fine for fast travel, fatal
  for anything involving a dead player — **`map/respawn` (feature 5) must not call it** and should reuse
  only the per-type ownership/control gates. Recorded here because the trap is in this file, not respawn's.
- **`CanGlobalFastTravel`'s affordability term prices a SOLO trip, always.** K1 froze its signature, so it
  has no recruit count to price with. Phase 5 closed the resulting gap in the UI only:
  `OVT_OverthrowMapUI.CanAffordEffectiveFare` re-tests `PlayerHasMoney` at the fare the button _displays_
  and forces the button disabled with `#OVT-CannotAfford`. Without it, a player who could afford the solo
  fare but not the squad fare saw an **enabled** button and was refused by the server only after the map
  had closed. Client-side advisory; the server is unchanged and remains authoritative. **If
  `CanGlobalFastTravel` ever gains a recruit parameter, delete this helper — do not keep two checks.**
- **The legacy context called the _recruits_ variant; the extracted service did not.** That silent
  behaviour loss (F4) is the kind no compile check or test surfaces — only reading both call sites does.
- **Closing the map view is NOT the same as stowing the map item — and Overthrow had it backwards.**
  `SCR_MapGadgetComponent.SetMapMode(false)` closes the _view_ only. Vanilla stows the gadget
  (`SCR_GadgetManagerComponent.SetGadgetMode(gadget, IN_STORAGE)`) and lets the view close as a
  consequence. Calling `SetMapMode(false)` directly left the character holding the map raised in hand
  after every close — found by the user's play-test, not by any gate. Use `ToggleFocused(false)` (a
  strict superset: closes the view, clears `m_bFocused`, **and** clears
  `SCR_PlayerController.SetGadgetFocus`) and then stow. The trap that invites the wrong fix:
  `SetGadgetMode`'s hand→storage branch **early-returns for `EGadgetType.MAP`**
  (`SCR_GadgetManagerComponent.c:272-273`), so stowing alone never closes the view synchronously.
- **Same-GUID `.conf`/prefab overrides are DELTAS, not replacements.** This is why `SCR_MapRadialUI` is
  still live on Overthrow's map despite our own `MapOverthrow.conf` never mentioning it — and therefore
  why the `gamepad0:x` collision is real.
- **✅ (RESOLVED 2026-08-10 — kept as a learning, not a live warning) The five ids this feature adds were
  in the `.st` master and NOT in the generated runtime exports.**
  Measured, not guessed: a throwaway autotest case calling `WidgetManager.Translate` in a live client
  returned the raw key for `#OVT-Map_BringRecruits`, `#OVT-Map_LeaveRecruits`, `#OVT-Travelled`,
  `#OVT-NotAtBusStop` and `#OVT-TravelWithFare`, while every pre-existing key resolved
  (`#OVT-MainMenu_FastTravel` → "Fast Travel", `#OVT-CannotAfford` → "You cannot afford that").
  `grep '"OVT-Travelled"' Language/localization_Overthrow.en-us.conf` returns nothing. **Until the user
  regenerates the exports in the Workbench, the recruit toggle reads `#OVT-Map_BringRecruits`, the travel
  button reads `#OVT-TravelWithFare`, and the post-trip hint reads `#OVT-Travelled`** — with the numbers
  dropped, because `Translate` cannot substitute into a key it failed to resolve. This is a _pre-existing_
  gate on Phase 6, not something the fix pass introduced (`#OVT-NotAtBusStop` was already in this state);
  the fix pass moved the numbers from concatenated free text into placeholders, which is what makes the
  regeneration load-bearing rather than cosmetic.
- **A leading `#` is resolved as a TOKEN inside a larger string, not only as a whole string.** Also
  measured: `Translate("#OVT-MainMenu_FastTravel ($300)")` → `"Fast Travel ($300)"` and
  `Translate("#OVT-Owner: Bob")` → `"Owner: Bob"`. So the concatenation this feature shipped in Phase 3
  was _not_ silently dropping the price the way a review assumed — it worked, for any exported key. It was
  replaced anyway because it hard-codes English word order: a translator cannot move the count or the fare
  relative to the label. `%1`/`%2` substitution happens in `WidgetManager.Translate` and in
  `TextWidget.SetTextFormat`; **`SetText` does not substitute**, and `SCR_InputButtonComponent.SetLabel`
  and `SCR_HintManagerComponent.ShowCustom` both end at `SetText`, so the string must arrive finished.
  An unsupplied `%n` renders as empty, so every placeholder must always be passed something.
- **`SCR_InputButtonComponent` refuses BOTH input paths on a disabled widget**, not just the mouse:
  `OnInput` (the keybind) and `OnClick` (the mouse) share the same
  `!IsVisibleInHierarchy() || !IsEnabledInHierarchy()` early return (`:731`, `:247`), gated on
  `m_bCanBeDisabled` which defaults to `1`. That is why the recruit toggle uses `SetEnabled(false)` rather
  than `SetVisible(false)` when travel is refused — the fare it names stays readable while the control is
  inert on keyboard, mouse and pad alike.
- **A refused trip disables the recruit toggle — EXCEPT when the refusal is the fare.** Switching recruits
  off is the player's only remedy for `#OVT-CannotAfford`, so disabling the toggle there would be a dead
  end reachable in one click (bring recruits ON → squad fare unaffordable → both controls dead). The
  affordability refusal is tracked separately from the reason string for exactly this reason.

---

_Phase 0 and Phase 6 were the user-driven play-tests this feature rested on. **Both are discharged** —
Phase 6 on 2026-08-10, Phase 0 (multiplayer) on 2026-08-11. Nothing is outstanding._

---

## Session note — 2026-08-11 (closing)

**No code changed this session.** The user reported the outcome of the play-tests that were the feature's
last open gate: **all items green, including multiplayer**, and asked for the feature to be closed.

What that discharged, in order of how much it was worth:

1. **Phase 0's two-client dedicated-server matrix** — the gate that had been open since the plan was
   written, and the only one that could confirm the server-authoritative path against a _real_ second
   client rather than against a code reading. F1 (the player moves and the server agrees) and F2 (the
   charge is real and survives the next authoritative sync) are the two that mattered; N9 confirms S1's
   conservative branch was the right one, and N10/N12 close the two input hazards.
2. **The post-ship map-stow fix** — re-tested with the map genuinely in hand.

Two items are carried in the docs as **record, not work**: the listen-server-host result short-circuit is
an untested branch (MP testing used a dedicated server), and F4's pre-fix observation is permanently
unobservable because the fix shipped before anyone watched the bug. Both are written into `tasks.md` and
the Quick Status rather than quietly dropped, because "closed" should not read as "everything was seen".

**Next session:** nothing on this feature. The map epic's next unbuilt work is `map/respawn` — note the
standing trap recorded above under Gotchas: **respawn must not call `CanGlobalFastTravel`**, whose
minimum-distance and cost terms both measure from a living player's current position.

---

## Dead-code hand-off to `map/legacy-retirement`

Written at the end of Phase 5 (2026-08-10); **re-verified line by line on 2026-08-10 after the
adversarial-review fix pass**, which added `IsOverthrowInfoPanelVisible` to `OVT_MapContext` and shifted
everything below `:365` down by ~21 lines. Line numbers are from the working tree, not from the plan
(`implementation.md` I-1's numbers predate Phases 2–4 and have drifted; where I-1 and this list disagree,
**this list is correct**).

> ⚠️ **NEVER delete by line range.** Every number here is a _pointer for a human to look at_, not a
> `sed` argument. This list has already been wrong once (the `OVT-NeedBusStop` entry in §4 named a
> four-line range inside an eighteen-line block; acting on it would have left a corrupt `.st` master).
> Locate each item by its **symbol name** or its **`Id` string**, confirm the block boundaries by eye,
> and delete whole syntactic units.

Retirement owns the deletions because this feature only had to make them _unreachable from the new map
path_; several are still wired into the legacy `OVT_MapContext` mode machinery, and unpicking that machinery
is retirement's job, not a fast-travel change.

### 1. Legacy bus-travel mode — fully dead, no callers outside its own file

| Symbol                            | `file:line`                                   | Evidence                                                                                                                                                                        |
| --------------------------------- | --------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `m_bBusTravelActive`              | `Scripts/Game/UI/Context/OVT_MapContext.c:21` | Named directly only at `:21,308,333,365,388,493`, plus the `DisableBusTravel()` calls that write it at `:318,506,527,532,541` — every one inside `OVT_MapContext.c`             |
| `EnableBusTravel()`               | `OVT_MapContext.c:301-309`                    | **No caller anywhere.** `OVT_CatchBusAction.c:16` now calls `OpenMap()` instead (Phase 4). `grep -rn "EnableBusTravel"` matches only the definition and a doc comment at `:291` |
| `DisableBusTravel()`              | `OVT_MapContext.c:331-334`                    | Called only from `OnMapExit` (`:318`) and from inside the dead bus branch (`:506,527,532,541`)                                                                                  |
| The bus branch of `MapClick`      | `OVT_MapContext.c:493-559`                    | Guarded by `m_bBusTravelActive`, which nothing can now set                                                                                                                      |
| `ShowNotification("NeedBusStop")` | `OVT_MapContext.c:505`                        | Inside the dead bus branch — see §4                                                                                                                                             |

### 2. `OVT_PlayerCommsComponent` fast-travel RPCs — dead once §3 goes

`grep -rn "RequestFastTravel" --include=*.c Scripts/` returns **exactly** the definitions plus three call
sites, and **every one of those call sites is in legacy `OVT_MapContext`** (two doc-comment mentions in
`OVT_TravelRequestComponent.c:9,291` are prose, not calls). Confirmed: **no new-map code calls these**
(I-2 satisfied).

| Symbol                                 | `file:line`                            | Remaining callers                                                                                                                                                                                                                           |
| -------------------------------------- | -------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `RequestFastTravel`                    | `OVT_PlayerCommsComponent.c:1483-1486` | `OVT_MapContext.c:471` only                                                                                                                                                                                                                 |
| `RpcAsk_RequestFastTravel`             | `OVT_PlayerCommsComponent.c:1488-1493` | its own wrapper only                                                                                                                                                                                                                        |
| `RequestFastTravelWithRecruits`        | `OVT_PlayerCommsComponent.c:1495-1498` | `OVT_MapContext.c:483` and `:552` only                                                                                                                                                                                                      |
| `RpcAsk_RequestFastTravelWithRecruits` | `OVT_PlayerCommsComponent.c:1500-1548` | its own wrapper only. ⚠️ Retirement should **lift the recruit ring-placement block first** if it ever needs it again — the loop at `:1529-1547` was already copied verbatim into `OVT_TravelRequestComponent.TeleportRecruits` (`:298-321`) |

⚠️ **These RPCs are the live security hole N2 describes** (`ResolveSenderPlayerId` + `TeleportPlayer`, no
validation, no payment — any client can teleport itself anywhere for free). They are unreachable from the
new map, but they are still **registered RPCs on a live component**. Deleting them is a security fix, not
only a cleanup; treat it as the highest-priority item on this list.

### 3. Legacy fast-travel mode — 🔴 **NOT dead. One live caller remains, and it is NOT this feature's to cut**

`implementation.md` I-1 lists `EnableFastTravel` as dead. **It is not.**

```
Scripts/Game/UI/Context/OVT_MainMenuContext.c:218
    OVT_MapContext.Cast(m_UIManager.GetContext(OVT_MapContext)).EnableFastTravel();
```

The Overthrow main menu's "Fast Travel" entry still arms `m_bFastTravelActive` and drives the legacy
click-to-travel path (`OVT_MapContext.c:415-491`), including the client-side teleport at `:487` and the
`RequestFastTravel*` calls in §2. This feature never touched the main menu — its scope was the **new map's**
info panel — so the legacy mode is still reachable by a player today.

**Retirement must remove the `MainMenuContext` entry (`:215-219`) as part of, or before, deleting
`EnableFastTravel`.** Until it does, the following are _reachable_, not dead, and must not be deleted
piecemeal:

| Symbol                                                    | `file:line`                                                                                                                                                                                                                                                            |
| --------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `m_bFastTravelActive`                                     | `OVT_MapContext.c:20`                                                                                                                                                                                                                                                  |
| `EnableFastTravel()`                                      | `OVT_MapContext.c:277-285`                                                                                                                                                                                                                                             |
| `DisableFastTravel()`                                     | `OVT_MapContext.c:326-329`                                                                                                                                                                                                                                             |
| `CanFastTravel(pos, out reason)` — the duplicate rule set | `OVT_MapContext.c:53-133` (only caller: `:418`)                                                                                                                                                                                                                        |
| `MAX_HOUSE_TRAVEL_DIS` / `MAX_FOB_TRAVEL_DIS`             | `OVT_MapContext.c:23-24` (used only by that duplicate rule set, `:93,102,113,123`)                                                                                                                                                                                     |
| `RECRUIT_TRAVEL_RADIUS`                                   | `OVT_MapContext.c:25` (used at **all four** of `:435,483,518,552` — `:435` and `:518` are the recruit _counts_ that price the two legacy fares, `:483` and `:552` the `RequestFastTravelWithRecruits` calls; the new path has its own at `OVT_FastTravelService.c:51`) |
| the fast-travel branch of `MapClick`                      | `OVT_MapContext.c:415-491`                                                                                                                                                                                                                                             |

Once §1 and §3 are both gone, `MapClick`'s remaining body is the `m_bMapInfoActive` branch (`:561+`), and
the guards at `:365` and `:388` collapse to that single flag — which is the map-info mode, **not this
feature's** and attributed to retirement on its own terms (unchanged from I-1).

🟢 **Delete `IsOverthrowInfoPanelVisible` with them.** `OVT_MapContext.c:376-384` and its call at `:405`
exist only to stop this legacy `MapClick` listener double-handling a click that landed on the new map
UI's info panel (a second debit at the flat legacy fare plus a teleport to the world position under the
panel). Once nothing arms a legacy mode, the guard has nothing to guard. Its counterpart
`OVT_OverthrowMapUI.IsInfoPanelVisible()` (`:438`) has no other caller and goes at the same time.

### 4. `OVT-NeedBusStop` — 🟡 unreachable, but it has a second wiring the plan did not record

`implementation.md` §10 says the id becomes "unused". More precisely: **its only script reference is inside
the dead bus branch**, but it is reached through the broadcast-message config, not directly, so there are
**two** things to delete, not one:

| Item                                                                       | How to find it — **do not use a line range**                                                                                                                                                                                                                                                                                                                                                   |
| -------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Localization id `OVT-NeedBusStop` ("You must click near another bus stop") | `Language/localization_Overthrow.st`: find `Id "OVT-NeedBusStop"` and delete the **whole enclosing `CustomStringTableItem "{…}" { … }` block**, opening brace to matching close. It currently runs `:4353-4374` and holds six `Target_*` translations — an eighteen-line unit, not the four lines an earlier draft of this table named. Deleting a partial range corrupts the master silently. |
| The `NeedBusStop` broadcast-message entry that `ShowNotification` resolves | `Configs/overthrowBroadcastMessages.conf`: find `m_sTag "NeedBusStop"` and delete the **whole enclosing `SCR_SimpleMessagePreset "{…}" { … }` block** (currently `:396-403`, including its nested `m_UIInfo`)                                                                                                                                                                                  |
| The call site                                                              | `OVT_MapContext.c:505` (dead — see §1)                                                                                                                                                                                                                                                                                                                                                         |

Its destination-side meaning ("click _near another_ bus stop") became **structural** in the new path: the
destination is a bus-stop marker or the panel simply is not a bus panel, so the refusal cannot be phrased.
The origin-side sense is now `#OVT-NotAtBusStop`, a **new, live** id — do not delete that one.

⚠️ `Language/localization_Overthrow.<lang>.conf` are Workbench-generated exports. Retirement removes the id
from the `.st` master **only**; the user regenerates.

### 5. Confirmed already gone — nothing owed

Verified absent from the tree (no definition, no caller):

- `OVT_FastTravelService.ExecuteFastTravel` — only tombstone comments remain
  (`OVT_FastTravelService.c:385`, and prose at `OVT_TravelRequestComponent.c:7,245,328`)
- `OVT_FastTravelService.CanFastTravelToLocationType` — **zero** matches tree-wide
- `OVT_FastTravelService.CalculateFastTravelCost` — only a tombstone comment at
  `OVT_FastTravelService.c:380`
- `OVT_TownManagerComponent.GetNearestBusStop` — **zero** matches tree-wide (removed by
  `map/location-types` when bus stops became marker components)

No symbol in `OVT_FastTravelService.c` is orphaned: `ValidateTravel`, `ReasonKeyFor`, `ComputeFare`,
`VerbUsesKmFloor`, `CalculateTravelCost`, `CountRecruitsInRadius`, `GetLocalActor` and
`CanGlobalFastTravel` all have external callers; `IsAtBusStop` / `BUS_STOP_RADIUS` are used by
`ValidateTravel` (`:138,141`); and `FARE_LABEL_FORMAT` (`:197`) is used by the map panel's travel button
and by `OVT_TravelRequestComponent.RpcDo_TravelResult`.

---

## 2026-08-25 — A captured base inside a QRF ring stays travellable

**Author:** *"with fast travel/respawn you should be able to fast travel or respawn at a captured base inside a QRF as long as that base isn't the actual QRF being counter-attacked."*

A QRF ring is `OVT_QRFControllerComponent.QRF_RANGE` wide and used to blank **every** destination inside it. A base the player had already taken, sitting in that ring but not itself under attack, is not part of the fight — refusing it stranded players a kilometre from where they were needed.

**One predicate, three call sites** — `OVT_RespawnService.IsCapturedBaseAwayFromQrfTarget(vector)`, next to the QRF helpers it belongs with:

| Surface | Site |
|---|---|
| Server respawn enumeration | `OVT_RespawnService.EnumerateEligible` base loop |
| Client respawn marker | `OVT_MapLocationBase.CanRespawn` |
| Fast travel, **both** machines | `OVT_FastTravelService.ValidateTravel`, beside the existing `fobExempt` |

The map's `OVT_MapLocationBase.CanFastTravel` needed no edit — it tails into `CanGlobalFastTravel` → the same `ValidateTravel`, which is the one-rule-set invariant this service is built on. The exemption applies in **both** `QRFFastTravelMode` modes (DISABLED and the range-limited one), for the same reason the FOB exemption does.

⚠ **Matched on position against `m_vQRFLocation`, never on `m_iCurrentQRFBase`.** The index is not in the JIP payload — a standing epic defect, and the reason JIP clients draw the wrong map circle — while `m_vQRFLocation` is broadcast by `RpcDo_SetQRFActive` to every machine. This predicate has to give the same answer on the client (button state) and the server (the act), so it may only read state both provably have.

⚠ **The target base is identified two ways, deliberately.** `m_vQRFLocation` is `base.GetOwner().GetOrigin()` and the record carries `ent.GetOrigin()` of that same entity, so they agree today — but this rule *inverts* if they ever drift (it would let a player travel into the base being counter-attacked, the one thing the author said must stay blocked), and that is not worth resting on a 2 m `MATCH_TOLERANCE`. It also resolves the nearest base record to the QRF point, within `QRF_TARGET_RESOLVE_M` 50. The distance bound is what keeps a **town** QRF correct: `GetNearestBase` answers at any range, so without it a town battle would mark some distant base as its target.

`tools/compile-check.sh` exit 0 (6347 files). Suite not run; MP play-test owed — the JIP path is exactly where the client/server agreement could break.
