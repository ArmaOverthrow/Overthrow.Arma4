# Map Respawn - Task Checklist

**Last Updated:** 2026-08-10
**Progress:** ✅ **COMPLETE — 152/152 (100%).** Phases 0–8 built 2026-08-10/11 in one `/autorun-feature` run; **the Phase 9 verification gate was run by the user and reported all green on 2026-08-11**, discharging every user-driven row including the two-client MP matrix, the gamepad-only pass and V-8.

> Generated from `implementation.md` §4 by `/start-feature map/respawn`.
> **Phases 1, 6 and 7 are ADVANCED** (`ui-developer-advanced`, `component-developer-advanced`,
> `network-specialist-advanced` for 5) — the screen is unskippable and console-critical, and Phase 6
> edits the spawn system where a mistake means players with no character.
>
> ⚠️ **The runtime half of this feature could not be automated.** `.conf`, `.layout`, `.et` and `.st` edits
> are invisible to `tools/compile-check.sh` **and** to both test groups. Phase 9 was the only evidence
> those six file classes are sound. Anything the autonomous session could not observe was marked
> **user-driven** and left unticked rather than assumed.
>
> ✅ **2026-08-11 — the user ran the gate and reported all green.** Every user-driven row is now ticked on
> that report. **No code change was required to pass it**: the tree at the moment of the play-test was
> byte-identical to the tree at the end of Phase 8 (no new files, no commits, the six localization exports
> still +22/−0). That is the strongest single statement available about this feature — the highest-risk
> unknowns (P1-G5's `ActionContext` flags, R1's workspace-hosted map taking input at all, and I-5's
> empty-persistent-id case) all resolved in its favour without a fix.

---

## Phase 0 — Baseline — **S — no agent**

Re-measured 2026-08-10 on `new-map` at `4287b2f1` + working tree (identical to the plan's recorded values).

- [x] `tools/compile-check.sh` → exit 0, **5958 files**, Game module
- [x] `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) → OK, **44 tests**
- [x] `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) → OK, **79 tests**
- [x] ⚠️ `CLAUDE.md`'s "Fast 38 / All 66" is stale — never quote it; a *changed* count is a finding

---

## Phase 1 — Spike: can a workspace-hosted SPAWNSCREEN map be driven? — `ui-developer-advanced` (**ADVANCED**)

- [x] `UI/Layouts/Respawn/OVT_RespawnScreen.layout` — embeds `{0651202E9F2646DE}UI/layouts/Map/Map.layout`
      as an inherited `FrameWidgetClass` (this is what supplies `MapFrame`/`MapWidget`)
- [x] Title bar + `RespawnHomeButton` (`WLib_NavigationButton`) + a status/hint text on that layout
- [x] `Configs/Map/MapRespawn.conf` — **new GUID**, `m_iMapMode SPAWNSCREEN`, modelled on vanilla
      `MapSpawnMenu.conf`; **not** a same-GUID delta, so every module/layer/props config is listed explicitly
- [x] `m_bShowSpawnPoints 0` and `m_bShowTasks 0` so vanilla's own spawn icons do not draw over Overthrow's
- [x] `Scripts/Game/UI/Context/OVT_RespawnContext.c` — `OnShow` → `SetupMapConfig(SPAWNSCREEN, …, m_wRoot)`
      + `OpenMap`; `OnClose` → `CloseMap`; `m_sCloseAction` **empty**
- [x] Temporary harness to show the context without a server change (admin command / debug keybind)
- [x] Record the `SetupMapConfig` cache constraint (early-returns the active config when the mode matches —
      correct only while Overthrow has exactly one SPAWNSCREEN config)
- [x] Gate: `tools/compile-check.sh` exit 0 — **5959 files** (5958 + 1 new `.c`)
- [x] 🔴 **FINDING P1-A** — `MapContext` is activated by the *menu preset*, not by `SCR_MapEntity`. A workspace layout has no preset, so the context calls `ActivateContext("MapContext")` itself every frame (see `context.md`)
- [x] 🔴 **FINDING P1-B** — the plan's `KC_H` binding is **taken** three times over in `MapContext`; rebound to `KC_HOME`
- [x] `OverthrowRespawnAtHome` + `ActionContext OverthrowRespawnContext` (pulled forward from Phase 7 — a button with no action draws no glyph)
- [x] `check-input-conflicts.py` exit 0, and the checker proven to actually read the new layout
- [x] **user-driven** Measure: does the map pan, zoom, decluster? Do markers hover and select?
- [x] **user-driven** Measure: is `MapContext` active — `MapSelect` on mouse **and** `gamepad0:a`? Cursor visible?
- [x] **user-driven** Decide + record: primary (workspace `OVT_UIContext`) or the `ChimeraMenuBase` fallback

---

## Phase 2 — Corpse-independent identity — `component-developer`

- [x] `OVT_Global.GetLocalPersistentId()` — `SCR_PlayerController.GetLocalPlayerId()` →
      `GetPersistentIDFromPlayerID`, with a `//!` block saying it must never be reachable from the server
- [x] `OVT_Global.GetController()` — add a no-controlled-entity fallback; the existing branch stays **first
      and byte-identical** so the living path cannot change
- [x] ⏭️ **DONE in Phase 5** `OVT_Global.GetRespawnRequests()` in the shape of `GetTravelRequests()` —
      it returns `OVT_RespawnRequestComponent`, which Phase 5 creates. Adding a stub class now would mean a
      half-implemented networking component sitting in the tree; nothing in Phase 2–4 calls the accessor
      (Phase 4's `OnRespawnClicked` is the first consumer and lands with Phase 5's component available)
- [x] Route `OVT_MapLocationType.GetCurrentPlayerID()` through the new resolver
- [x] Route `OVT_MapLocationElement.GetCurrentPlayerID()` through the new resolver
- [x] Route `OVT_OverthrowMapUI.GetCurrentPlayerID()` through the new resolver
- [x] Gate: `grep -rn "GetLocalControlledEntity" Scripts/Game/UI/Map/` matches only entity-scoped helpers —
      **finding:** the plan's enumeration was incomplete. Six hits remain, all position/gadget-scoped and
      none in an identity path: `OVT_MapLocationData.GetDistanceFromPlayer`,
      `OVT_MapLocationElement.UpdateDistance` (a *second* distance path the plan did not list),
      `OVT_OverthrowMapUI.GetMap`/`StowMapGadget`, and `OVT_MapPlayerLocation.OnMapOpen`/`ZoomInOnPlayer`
      (draws the living player's own position — meaningless without an entity, correctly entity-scoped)
- [x] Gate: compile 0 (**5959 files**); Fast **44**; All **79**
- [x] **user-driven** Living map: houses, private camps and player vehicles still appear for their owner and
      still do **not** appear for anybody else (the N1 privacy contract)

---

## Phase 3 — `OVT_RespawnService` + the `CanRespawn` contract + Logic tests — `component-developer`

- [x] `Scripts/Game/Services/OVT_RespawnService.c` with the fenced CLIENT-ONLY discipline of
      `OVT_FastTravelService.c`
- [x] `enum OVT_RespawnDestination { HOME, LOCATION }`
- [x] `enum OVT_RespawnResult { OK, OK_FELL_BACK_HOME, NO_PLAYER, NOT_ELIGIBLE, SPAWN_FAILED }`
- [x] Pure predicate `IsBaseEligible(bool isOccupying)`
- [x] Pure predicate `IsFobEligible()`
- [x] Pure predicate `IsCampEligible(bool isPrivate, string ownerId, string playerId)`
- [x] Pure predicate `IsHouseEligible(string ownerId, string renterId, string playerId)`
- [x] Pure predicate `IsInsideQrf(bool qrfActive, vector qrfLocation, vector pos)`
- [x] Pure `PositionsMatch(vector a, vector b)` + `ReasonKeyFor(int result)`
- [x] `static const float MATCH_TOLERANCE = 2.0;` with the why-not-zero / why-small comment
- [x] Shared world lookup `IsPositionInActiveQRF(vector pos)` using `OVT_QRFControllerComponent.QRF_RANGE`
- [x] Server enumeration `CollectEligiblePositions(string persId, notnull array<vector> out)`
- [x] Server enumeration `ResolveRespawnPosition(persId, destination, requestedPos, out resolvedPos)`
- [x] ❌ `//!` line recording that nothing here calls `OVT_FastTravelService.CanGlobalFastTravel`, and why
- [x] `OVT_MapLocationType.CanRespawn(location, playerID, out reason)` — virtual, **defaults to refuse**,
      Doxygen'd as a hot path, placed immediately after `CanFastTravel`
- [x] `[Attribute(defvalue: "false")] bool m_bRespawnOnly;` on `OVT_MapLocationType`
- [x] Gate `ShouldShowLocation` on `m_bRespawnOnly` (no type overrides it today — verify that still holds)
- [x] `OVT_MapLocationBase.CanRespawn` — refuse `isOccupying`; `#OVT-Respawn_EnemyBase`
- [x] `OVT_MapLocationFOB.CanRespawn` — always eligible
- [x] `OVT_MapLocationCamp.CanRespawn` — refuse private-and-not-yours; `#OVT-Respawn_PrivateCamp`
- [x] `OVT_MapLocationHouse.CanRespawn` — refuse not-owner-and-not-renter; `#OVT-Respawn_NotYourHouse`
- [x] All four apply the QRF exclusion (`#OVT-Respawn_QRF`)
- [x] T1: new code uses idiom A (inherited manager cache); `OVT_MapLocationBase`'s shadow is left alone
- [x] `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_RespawnRules.c` — 10 cases, world-free, the
      manager accessor's identifier absent from the whole file including comments
- [x] Register the new case class in `OVT_TEST_LogicSuite`
- [x] **Prove every new case can fail** (invert → exit 1 → revert) and record the method here
- [x] Gate: compile 0; Fast 44+N; All 79+N with the **same** N — any other delta is a finding
- [x] **user-driven** Living fullscreen map visually unchanged (ten types, three zoom levels)

**Failure-proof method (Phase 3 task: "prove every new case can fail").** N = **10** new Logic case
classes in one file; Fast 44 -> 54, All 79 -> 89, compile 5959 -> 5961 files. Every individual
assertion was inverted and observed to fail, in five batched passes. Each pass: apply the
inversions, run `tools/run-tests.sh "{6A6E29FF47ECB840}"`, read `.tmp/run-tests/junit.xml`, revert
from a pristine copy.

| Pass | What was inverted | Observed |
|---|---|---|
| A | the FIRST assertion of all ten case classes, simultaneously | exit **1**, exactly **10** failures, one per new class and no others |
| B | every remaining assertion in HouseTenure, HouseEmptyIds, QrfInactive, QrfRadius, PositionMatch, and the shared `ExpectSpoken` helper in ReasonKeys | exit **1**, **6** failures, each on the expected message |
| C | the still-unreached tail assertions: `IsBaseEligible(false)`, HouseTenure's "them/them/me", HouseEmptyIds' "/them/", PositionMatch's just-outside, **plus** the service's default `ReasonKeyFor` branch forced to `""` | exit **1**, **5** failures, including `NO_PLAYER produced no text at all` |
| D | production side only: `ReasonKeyFor(NOT_ELIGIBLE)` forced to `""` | exit **1**, **1** failure: `Result code NOT_ELIGIBLE produced no text at all` |
| E | production side only: `ReasonKeyFor(SPAWN_FAILED)` forced to `""` | exit **1**, **1** failure: `Result code SPAWN_FAILED produced no text at all` |

Passes C-E invert the *subject* rather than the assertion, because a case short-circuits on its
first failure and the later result codes are otherwise unreachable. No `maxAttempts` anywhere.
After the final revert: compile exit 0 / **5961** files, Fast **54**, All **89**.

---

## Phase 4 — The respawn map: config, eligible-only markers, respawn info panel — `ui-developer`

- [x] `Configs/Map/OverthrowMapRespawn.conf` — `OVT_OverthrowMapConfig`, **only** Base/FOB/Camp/House
      (GUID `{6A83D5A000000070}`; type instances `…0071`–`…0074`)
- [x] Per type: `m_fVisibilityZoom 0`, `m_bRespawnOnly 1`, `m_fRefreshInterval 0`, `m_bShowDistance 0`,
      `m_bShowName 1`, `m_fShowNameZoom 0` — with the reason for each of the three zeros in a comment.
      ⚠️ **FINDING P4-A** — `//` comments are **unattested** in `.conf`: zero of the repo's configs and
      zero of vanilla's 1897 carry one. The block is written anyway (as instructed) but the same
      rationale is duplicated verbatim in `OVT_RespawnMapUI.c`'s header, so if V-3 reports a parse
      error on this file the fix is "delete the comment block" and nothing is lost
- [x] `m_bCanFastTravel` deliberately left at its default `0` on all four (belt to the layout's braces)
- [x] `Scripts/Game/UI/Map/OVT_RespawnMapUI.c` extending `OVT_OverthrowMapUI`
- [x] `SetupTravelButton(location)` overridden **without calling super**, wiring `RespawnButton`
- [x] `UI/Layouts/Map/Core/OVT_MapInfoPanelRespawn.layout` (+ `.meta`, GUID `{6A83D5A000000075}`) —
      `LocationName`, `LocationType`, `ContentSlot`, `CloseButton`, `RespawnButton`; **no**
      `FastTravelButton`/`FastTravelReason`/`BringRecruitsButton`/`Distance`. `Owner` is also omitted:
      every record that reaches this panel is the player's own or public, so it would say nothing
- [x] Point `MapRespawn.conf` at `OVT_RespawnMapUI` + the shared `OVT_MapLocationElement.layout` +
      the new info panel + `OverthrowMapRespawn.conf` — **P1-G3 is closed**: the spike's pointer at the
      living map's `OVT_MapInfoPanel.layout` (which really did carry Fast Travel and Bring Recruits) is gone
- [x] `OnRespawnClicked()` — ⏭️ **body deferred to Phase 5** with a marked `TODO(map/respawn Phase 5)`.
      It reads `m_SelectedElement`, null-guards the location and logs the request it would have made.
      `OVT_Global.GetRespawnRequests()` and `OVT_RespawnRequestComponent` do not exist yet and stubbing
      a networking component early is worse than a phase boundary; the tree must compile at every phase
- [x] `OverthrowRespawnHere` added to `Configs/System/chimeraInputCommon.conf` (`keyboard:KC_RETURN` +
      `gamepad0:x`, GUIDs `…0090`–`…0094`) and listed in `ActionContext OverthrowRespawnContext`.
      Added rather than deferred because a `WLib_NavigationButton` with no action draws **no glyph**,
      which is the same reason Phase 1 pulled `OverthrowRespawnAtHome` forward
- [x] ⚠️ **FINDING P4-B** — the live `MapContext` surface was re-enumerated by hand (the checker cannot
      see inline `ActionContext` actions). `keyboard:KC_RETURN` is unclaimed there. `gamepad0:x` **is**
      claimed, by `MapContextualMenu`; only `SCR_MapRadialUI` and `SCR_MapDrawingUI` listen for it and
      neither module is carried by `MapRespawn.conf`, so it is inert. Recorded in the checker's
      `ACKNOWLEDGED` map with that mechanism named
- [x] ⚠️ **FINDING P4-C** — Phase 1's `OverthrowRespawnAtHome` on `gamepad0:y` collides with vanilla
      `HintDismiss` (`gamepad0:y`), which **is** in `MapContext`'s `ActionRefs`. R8 claims "no
      `MapContext` action claims x or y" and is wrong on both counts. Not changed here — it is Phase 1's
      binding and dismissing a hint is harmless — but V-7 should watch for a double-fire
- [x] **Layout↔code name audit** — `RespawnButton`, `LocationName`, `LocationType`, `ContentSlot`,
      `CloseButton` each grepped in both the layout and the reading code; results in the session report
- [x] Gate: compile **0 / 5962 files** (5961 + 1 new `.c`); Fast **54**; All **89** — all unchanged from
      Phase 3 apart from the expected file count
- [x] **user-driven** Only Base/FOB/Camp/House draw, at maximum zoom-out
- [x] **user-driven** Enemy base / foreign private camp / foreign house are **absent**, not greyed
- [x] **user-driven** Panel shows a working "Respawn here" and no travel controls / cost / reason / distance

---

## Phase 5 — `OVT_RespawnRequestComponent` — `network-specialist-advanced` (**ADVANCED**)

- [x] `Scripts/Game/Components/Controller/OVT_RespawnRequestComponent.c`, copied from
      `OVT_TravelRequestComponent.c` including its discipline
- [x] `[RplRpc(Reliable, Server)] RpcAsk_Respawn(int destination, vector targetPos)`
- [x] `[RplRpc(Reliable, Owner)] RpcDo_ShowRespawnScreen()`
- [x] `[RplRpc(Reliable, Owner)] RpcDo_RespawnResult(int result)`
- [x] ⚠️ BUG-090: hand-check every `Rpc()` arity — a wrong count compiles clean and dies at the wire
- [x] `ResolveOwningPlayerId()` verbatim — identity from the controller entity, **never** the payload
- [x] Listen-server short-circuit in `SendRespawnResult` **and** `AskShowRespawnScreen`
- [x] `RequestRespawn` calls directly when `Replication.IsServer()`, else `Rpc()`
- [x] `RpcAsk_Respawn` order: server guard → arrival `Print` → reject out-of-range `destination` →
      `ResolveOwningPlayerId` (refuse `<= 0`) → `CompleteRespawn` → `SendRespawnResult` for **every** outcome
- [x] `m_OnShowRespawnScreen` + `m_OnRespawnResult(int)` script invokers; `RpcDo_RespawnResult` shows a hint
      and **never** mutates state
- [x] Register the component on `Prefabs/GameMode/OVT_OverthrowController.et` with a fresh GUID
- [x] ❌ Confirm nothing was added to `OVT_PlayerCommsComponent` (`grep -n "Respawn"` → empty)
- [x] Gate: compile 0 (**5963 files**); Fast **54**; All **89** — unchanged
- [x] ⏭️ **Picked up from Phase 2:** `OVT_Global.GetRespawnRequests()` added, shaped on `GetTravelRequests()`
- [x] `OVT_RespawnMapUI.OnRespawnClicked()` — Phase 4's TODO stub replaced with the real call
- [x] **BUG-090 arity audit** — 3 `Rpc()` sites hand-checked against their handlers (2/2, 0/0, 1/1), independently re-verified by the orchestrator
- [x] ⚠️ **Seam left for Phase 6** — `CompleteRespawn` is a local method whose body is one ERROR `Print` + `SPAWN_FAILED`; Phase 6 replaces **only that body**
- [x] **user-driven** SP: the arrival `Print` fires and the result `Print` reports a code
- [x] **user-driven** Two clients: A's request never resolves to B (assert on the printed player ids)

---

## Phase 6 — Defer the death path in `OVT_SpawnLogic` — `component-developer-advanced` (**ADVANCED**)

- [x] `protected ref array<int> m_aAwaitingRespawn = {};` mirroring the `m_aPendingBodySpawns` idiom
- [x] Replace **only** the final `Callqueue().Call(CreateCharacter, …)` line of `OnPlayerKilled_S` with
      `BeginAwaitingRespawn(playerId, playerUid)` — `ChargeRespawn`, the body-id clear, the last-known-position
      clear and the gear-snapshot clear are all untouched. Proven by `git diff -U20`: every executable
      statement in the handler is a context line and the only `-`/`+` pair is the final one;
      `ChargeRespawn` appears in **no** diff hunk anywhere in the tree (DoD **I-6**)
- [x] `BeginAwaitingRespawn` — idempotent
- [x] `BeginAwaitingRespawn` — **capability check at t=0**: missing controller or missing
      `OVT_RespawnRequestComponent` ⇒ `Print(LogLevel.ERROR)` naming the prefab + spawn immediately
- [x] 🔴 **FINDING P6-A** — the degrade path must reproduce the *deferred* call, not a synchronous one
      (see `context.md` P6-G1)
- [x] `ReAskRespawnScreen` — 5 s `CallLater` chain, re-scheduling while the entry survives
- [x] Tick order: entry gone ⇒ stop · no controller ⇒ drop + stop · **living** entity ⇒ drop + stop ·
      else re-send + reschedule
- [x] `CompleteRespawn(playerId, destination, requestedPos)` returning `OVT_RespawnResult`
- [x] `CompleteRespawn` — resolve `persId`, refuse `NO_PLAYER` on empty (claim deliberately **not**
      consumed on that branch, so a later re-ask still has an exit)
- [x] `CompleteRespawn` — **consume the awaiting entry first; absent ⇒ return without spawning** (this is
      what makes two characters for one death impossible)
- [x] `CompleteRespawn` HOME ⇒ the untouched `CreateFreshCharacter(playerId, persId)`
- [x] `CompleteRespawn` LOCATION ⇒ `ResolveRespawnPosition`; match ⇒ `CreateFreshCharacterAt` with the
      **server's** vector; no match ⇒ home + `OK_FELL_BACK_HOME`
- [x] Split `CreateFreshCharacter` into the 2-arg wrapper + `CreateFreshCharacterAt(playerId, persId,
      useChosenPosition, chosenPosition)` — **a parameter, never a member and never a map**
- [x] Confirm the four existing callers still call the 2-arg form and are otherwise unedited — **four
      confirmed** (`CreateCharacter`, both `OnPlayerBodySpawned` fallbacks, `OnPlayerBodySpawnTimeout`);
      none appears as a `+`/`-` line in the diff, and the 2-arg signature line itself is unchanged context
- [x] Fill the Phase 5 seam — `OVT_RespawnRequestComponent.CompleteRespawn` now forwards to
      `OVT_SpawnLogic.GetInstance().CompleteRespawn(...)`, exactly the substitution its Doxygen specified;
      `RpcAsk_Respawn`'s validation order is untouched
- [x] Override `OnPlayerDisconnected_S` — `super`, then drop any awaiting entry; no character is created
- [x] **Audit the widened corpse window** — reasoning written down for **seven** sites, not the five the
      plan listed (see `context.md` P6-A1). 🔴 **FINDING P6-B**: the plan's enumeration missed
      `CreateAndJoinGroup`, and mis-stated which sites omit the dead check — three do, not two
- [x] Gate: compile **0 / 5963 files**; Fast **54**; All **89**; **no tier moved** (Campaign 11, Init 17,
      Logic 37, PersistenceRoundTrip 13, Persistence 11 — identical before and after)
- [x] **user-driven** SP: die ⇒ no character created; the log shows the awaiting entry and the re-ask ticks
- [x] **user-driven** `CompleteRespawn` twice for one death creates **one** character
- [x] **user-driven** Disconnect while awaiting ⇒ reconnect spawns at home; the log shows the entry dropped
- [x] **user-driven** Removing the component from the controller prefab reproduces today's behaviour with a
      loud ERROR (one-off proof of the degrade path, revert immediately)

---

## Phase 7 — Wire the screen end to end, gamepad, localization — `ui-developer-advanced` (**ADVANCED**)

- [x] `Scripts/Game/Components/Player/OVT_RespawnScreenHandlerComponent.c` modelled on
      `OVT_PlayerStartMenuHandlerComponent.c`, on `Prefabs/Characters/Core/OVT_PlayerController.et`
- [x] `EOnFrame` drives `m_RespawnContext.EOnFrame(owner, timeSlice)` so the input context stays active
- [x] Poll for `OVT_Global.GetRespawnRequests()`, subscribe once to both invokers, then stop polling
- [x] `OVT_OverthrowGameMode` — `[Attribute()] ref OVT_RespawnContext m_RespawnUIContext;` +
      `GetRespawnContext()`, mirroring `m_StartGameUIContext`
- [x] Configure the context on `Prefabs/GameMode/OVT_OverthrowGameMode.et`
- [x] **Re-open guard** — poll `SCR_MapEntity.GetMapInstance().IsOpen()` and re-open if it closed.
      ❌ No `SCR_MapEntity.GetOn*` subscription (`grep -rn "SCR_MapEntity.GetOn"` must stay empty)
- [x] `RespawnHomeButton` → `RequestRespawn(HOME, vector.Zero)`
- [x] Close on **either** an OK/OK_FELL_BACK_HOME result **or** the local player acquiring a living entity
- [x] Non-OK results leave the screen open and show the reason
- [x] `Configs/System/chimeraInputCommon.conf` — `OverthrowRespawnHere` (`KC_RETURN` + `gamepad0:x`) and
      `OverthrowRespawnAtHome` (`KC_H` + `gamepad0:y`)
- [x] `ActionContext OverthrowRespawnContext` with `MenuUp/Down/Left/Right/MenuSelect`
- [x] ⚠️ Hand-check against vanilla's `MapContext` block and Overthrow's `MapContext` `ActionRefs` — the
      repo's conflict checker cannot see inline `ActionContext` actions
- [x] 11 new ids in **`Language/localization_Overthrow.st` only**; literal text in layouts until the user
      regenerates. ❌ Never edit `localization_Overthrow.<lang>.conf`
- [x] Gate: compile 0 (**5964 files**); Fast **54**; All **89** — unchanged
- [x] `grep -rn "SCR_MapEntity.GetOn" Scripts/` → **empty** (BUG-069 part 4 stays structurally closed)
- [x] ⚠️ **P1-G6 discharged** — the Phase 1 spike host is removed; `git diff` on `Character_Player.et` is
      empty, i.e. that prefab is back to its committed state. `OVT_UIManagerComponent.OnPlayerDeath()`
      closes every context it owns, so a screen hosted there would have been torn down by the very event
      that must open it
- [x] `/respawn-screen` harness **kept and retargeted** to the shipped path, so it shows exactly what a
      death shows rather than a copy
- [x] 🔴 **Bug found and fixed while wiring** — `OnRespawnHomeActivated` had the `m_OnClicked` signature,
      not `m_OnActivated`'s. Likely why the Phase 1 spike button would have looked dead
- [x] Q-5 layout↔code audit re-run across all nine names this phase touches
- [x] **user-driven** Die → screen ≤ ~1 s → pick a marker → spawn there → screen closes, HUD returns
- [x] **user-driven** "Respawn at home" works from the moment the screen appears, nothing selected
- [x] **user-driven** The screen cannot be dismissed (`Esc`, `M`, gadget key, `MenuBack`, main-menu key)
- [x] **user-driven** Full gamepad-only pass, every button glyphed

---

## Phase 8 — Docs and contract records — `component-developer`

- [x] Two contract rows in `docs/features/map/core/context.md` (`CanRespawn`, `m_bRespawnOnly`), in that
      file's exact format with the bolded attribution. Both plan-proposed rows verified true against the
      shipped code; each gained one accurate clause (`CanRespawn` is **advisory only**; `m_bRespawnOnly`
      is set in all four entries of `OverthrowMapRespawn.conf` and nowhere else)
- [x] Layout ↔ code name rows for `RespawnButton`, `RespawnHomeButton`, `StatusText` and the inherited
      `MapFrame`/`MapWidget`, each re-grepped rather than copied. ⚠️ **FINDING P8-A** — `MAP_FRAME_NAME`
      is **not** unconsumed: it has three vanilla consumers (`SCR_MapDrawingUI`, `SCR_MapMarkerBase`,
      `SCR_MapMarkerEntity`), none of whose modules `MapRespawn.conf` carries. Recorded as "defined and
      not looked up **on this screen**", which is the true and the fragile statement
- [x] `docs/features/map/epic-overview.md` row 5 + the rollup, plus a correction to row 4's claim that
      `HideMap()` was retained "for `map/respawn`"
- [x] Correct `OVT_MapContext.HideMap()`'s `//!` block — it names `map/respawn` as a consumer that does not
      exist (K13); keep the method and its play-test-derived ordering rationale
- [x] `docs/features/map/respawn/context.md` — Quick Status brought to the finished build, a consolidated
      **Still unverified** section (A–G) and a **Where to look when it doesn't work** triage section; the
      Phase 1–7 findings are left exactly as written
- [x] ⏭️ **Picked up from P6-G4** — the stale comment in `OVT_OverthrowGameMode.OnPlayerDisconnected`
      ("a player who leaves while dead or in the respawn menu has no controlled entity") corrected; they
      now hold a corpse. Comment only, no behaviour change
- [x] Q-6: no `file:line` pointers in any new code comment (epic K-9 discipline) — **clean**. Every added
      line across the six new `.c` files and every `+` line in the eleven changed ones was swept for
      `.c:NNN`/`:NNN-NNN`; zero hits. The pre-existing pointers in `OVT_SpawnLogic.c`, `OVT_OverthrowGameMode.c`
      and `OVT_OverthrowMapUI.c` are untouched context lines and belong to the epic's standing audit
- [x] Gate: `tools/compile-check.sh` exit 0, **5964 files** (comment-only phase); Fast **54**; All **89**

---

## Phase 9 — Verification gate — **user-driven, no agent**

⚠️ **This is the only evidence that exists** for `MapRespawn.conf`, `OverthrowMapRespawn.conf`,
`OVT_RespawnScreen.layout`, `OVT_MapInfoPanelRespawn.layout` and the three edited `.et` prefabs — six
file classes invisible to `compile-check.sh` and to both test groups.

- [x] **V-1** compile exit 0, 5958 + new `.c` files
- [x] **V-2** Fast 44+N / All 79+N, same N, each new case proven able to fail
- [x] **V-3** Workbench clean load — no missing-resource / unknown-class / dangling-GUID errors; open each
      new `.conf` and `.layout` and confirm every GUID resolves (the orphaned `OVT_MapThreatGrid` `.meta`
      `{B8F4C6A8C9D3E4F1}` is pre-existing and not this feature)
- [x] **V-4** Single-player pass — F-1 … F-8, I-1, I-2, I-6, Q-1 … Q-4
- [x] **V-5** Two-client MP — simultaneous respawn, JIP death, I-4 privacy, I-5 identity, I-2 race,
      listen-server host path ⚠️ warn before launching; `--timeout 3600`
- [x] **V-6** Disconnect while awaiting, then reconnect
- [x] **V-7** Gamepad-only pass, no mouse
- [x] **V-8** Regenerate the six localization exports, then confirm no raw `#OVT-Respawn_*` renders
