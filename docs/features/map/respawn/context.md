# Map Respawn - Context & Decisions

**Last Updated:** 2026-08-11
**Current Phase:** ✅ Complete — Phase 9 discharged 2026-08-11
**Status:** ✅ **COMPLETE** — built Phases 0–8, play-tested green by the user 2026-08-11, no fix required

---

## Quick Status

**What's Done:**

- ✅ **Phase 0** — baselines re-measured on `new-map` at `4287b2f1` + working tree, identical to the plan's
  recorded values: compile **exit 0 / 5958 files**, Fast **44**, All **79**.
- ✅ **Phase 1 (spike)** — `OVT_RespawnScreen.layout` (embedding vanilla's `Map.layout` for `MapFrame`/
  `MapWidget`), `Configs/Map/MapRespawn.conf` (SPAWNSCREEN, standalone GUID), `OVT_RespawnContext`, and a
  `/respawn-screen` chat harness. 25 fresh GUIDs in the `6A83D5A0…` block, grep-verified unique.
  **The spike's own verdict — does the map actually take input — is still unmeasured** and needs a human;
  see P1-D1 for the change that makes it plausible and P1-G5 for the one thing that could still sink it.
- ✅ **Phase 2** — corpse-independent identity. `OVT_Global.GetLocalPersistentId()`, the `GetController()`
  no-controlled-entity fallback, and all three duplicate `GetCurrentPlayerID()` bodies delegating to it.
- ✅ **Phase 3** — `OVT_RespawnService` (2 enums, 7 pure predicates, `IsPositionInActiveQRF`,
  `CollectEligiblePositions`/`ResolveRespawnPosition`, `MATCH_TOLERANCE`), `CanRespawn` + `m_bRespawnOnly`
  on `OVT_MapLocationType` with the `ShouldShowLocation` gate, the four type overrides, and
  **10 Logic cases** each proven able to fail.
- ✅ **Phase 4** — the respawn map. `Configs/Map/OverthrowMapRespawn.conf` (four types only, the three
  zeros), `OVT_RespawnMapUI` overriding `SetupTravelButton` **without super**, and
  `OVT_MapInfoPanelRespawn.layout` — which meets "no travel affordances, no cost, no reason text, no
  distance row" by **omitting the widgets**, not by a code branch.
- ✅ **Phase 5** — `OVT_RespawnRequestComponent` on `OVT_OverthrowController.et`: three RPCs, identity
  from the controller entity and never the payload, listen-server short-circuits in both directions, and
  a result for **every** outcome including OK. BUG-090 arity hand-checked on all three `Rpc()` sites.

- ✅ **Phase 6** — the death path is deferred. `m_aAwaitingRespawn`, `BeginAwaitingRespawn` with the t=0
  capability check, the self-cancelling 5 s `ReAskRespawnScreen` chain, `CompleteRespawn` with the
  one-shot claim, the `CreateFreshCharacter`/`CreateFreshCharacterAt` split (position as a **parameter**,
  never stored), and `OnPlayerDisconnected_S`. Phase 5's seam is filled. The corpse-window audit covers
  **seven** sites, not the five the plan listed (P6-A1).

- ✅ **Phase 7** — the screen is wired end to end. `OVT_RespawnScreenHandlerComponent` on the player
  **controller** (not the character — `OVT_UIManagerComponent.OnPlayerDeath()` closes every context it
  owns, which is the exact moment this screen must open, so P1-G6's spike host is removed and
  `Character_Player.et` is back to its committed state), `m_RespawnUIContext` on the game mode, the
  polled map re-open guard (never a `SCR_MapEntity.GetOn*` subscription), the living-character belt, the
  input actions and 11 `.st` ids. One real bug was caught while wiring: `OnRespawnHomeActivated` had the
  `m_OnClicked` signature rather than `m_OnActivated`'s, which is very likely why the Phase 1 spike
  button would have looked dead.
- 🟡 **Phase 8 (this pass)** — contract rows and the layout↔code audit added to
  `docs/features/map/core/context.md`, epic row 5 + rollup rewritten, `OVT_MapContext.HideMap()`'s false
  consumer claim corrected (K13 — the method is **kept**), and the two sections below written.

**What's Next:**

- **Phase 9 — the verification gate, and it is user-driven.** It is not bookkeeping: it is the **only**
  evidence that exists for this feature. See _Still unverified_ below for the single list to work from
  and _Where to look when it doesn't work_ for triage.

**Blockers:** none. **Owed to the user:** ⚠️ **nothing in this feature has ever been executed.** Every
code gate is green — compile **0 / 5964 files**, Fast **54**, All **89** — and not one of them can see
the death path, the screen, the two new `.conf`s, the two new `.layout`s, the three `.et` edits or the
input bindings. Compiled and reasoned, never run.

---

## Baselines (Phase 0)

| Gate                                             | Value                               | Measured   |
| ------------------------------------------------ | ----------------------------------- | ---------- |
| `tools/compile-check.sh`                         | exit 0, **5958 files**, Game module | 2026-08-10 |
| `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) | OK, **44 tests**                    | 2026-08-10 |
| `tools/run-tests.sh "{6A6E2A002F53A581}"` (All)  | OK, **79 tests**                    | 2026-08-10 |

⚠️ `CLAUDE.md` says Fast 38 / All 66 and is **stale**. A changed count is a finding to investigate, never
a number to update.

### Running gate values (each re-measured, never quoted)

| After              | compile                                                | Fast        | All         |
| ------------------ | ------------------------------------------------------ | ----------- | ----------- |
| Phase 0 (baseline) | 0 / **5958**                                           | 44          | 79          |
| Phase 1            | 0 / **5959** (+1 `.c`)                                 | —           | —           |
| Phase 2            | 0 / **5959**                                           | 44          | 79          |
| Phase 3            | 0 / **5961** (+2 `.c`)                                 | **54** (+N) | **89** (+N) |
| Phase 4 / 5        | 0 / **5963** (+2 `.c`)                                 | 54          | 89          |
| Phase 6            | 0 / **5963** (no new `.c`)                             | 54          | 89          |
| Phase 7            | 0 / **5964** (+1 `.c`)                                 | 54          | 89          |
| Phase 8            | 0 / **5964** (two comment edits, no executable change) | 54          | 89          |

Phase 6 per-tier, measured before **and** after, identical: Campaign 11, Init 17, Logic 37,
PersistenceRoundTrip 13, Persistence 11. Nothing moved — which is the expected result, since the
autotest world has no players and `OnPlayerKilled_S` never fires in it.

**N = 10** — the ten Logic cases in `OVT_TEST_Logic_RespawnRules`. Same N in both groups; no other tier
moved. From here on the expected counts are **Fast 54 / All 89**, and any further movement is a finding.

---

## What the automated gates cannot see

This feature adds two `.conf` files, two `.layout` files, three `.et` prefab edits and one `.st` edit.
**None of those six file classes is visible to `tools/compile-check.sh` or to either test group.** A
dangling GUID or a mistyped widget name therefore passes every automated gate and fails in the world.
Phase 9 (and the user-driven rows inside Phases 1–7) is the only evidence those files are sound.

The autotest world also has no players, so `OnPlayerKilled_S` never fires and there is no UI tier —
nothing in the death path or the screen can be asserted. What _can_ be asserted is the pure predicate
half of `OVT_RespawnService`, which is exactly why the plan splits it out (§7).

---

## Still unverified — the consolidated list (Phase 8)

**One list, gathered from every phase's Gotchas and its own "what I could not verify".** Nothing below
has been observed at runtime; each item names the symptom to watch for so a play-test can distinguish
"works" from "silently does nothing". Ordered by how badly it hurts if it is wrong, not by phase.

### A. Can this screen be operated at all (the spike's own verdict is still owed)

| #   | What                                                                                                                                                                                                                                                        | Symptom if wrong                                                                                    | Cheapest fix                                                                                                                                       |
| --- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| A1  | **Does the workspace-hosted SPAWNSCREEN map take input?** Pan, zoom, decluster, marker hover, marker select — on **mouse** and, separately, on **gamepad only**. This is the question Phase 1 existed to answer and it was never measured.                  | Map renders perfectly and ignores everything.                                                       | The recorded fallback: host the screen in a `ChimeraMenuBase` modelled on `SCR_DeployMenuBase`, keeping `OVT_RespawnContext` as the state machine. |
| A2  | **P1-G5 — `ActionContext` flags.** `OverthrowRespawnContext` is `Priority 50 / Flags 4`; `MapContext` is `Priority 50 / Flags 0x6c`. Whether they coexist or one suppresses the other could not be determined — the flag enum is not in the extracted tree. | A visible map that does not respond (indistinguishable from A1 by eye).                             | One line: `Flags 0x6c`, or drop `Flags` entirely as vanilla's `DeployMenuContext` does.                                                            |
| A3  | **Does every affordance show a glyph and reach on a pad with the mouse unplugged?** `SCR_InputButtonComponent` refuses **both** its input paths on an invisible or disabled widget, so hidden ≠ merely invisible.                                           | A button that exists and cannot be pressed on console.                                              | —                                                                                                                                                  |
| A4  | **P4-G1 / P1-G2 — `gamepad0:x` (Respawn here)** is also `MapContextualMenu`. It is inert **only because** `MapRespawn.conf` omits `SCR_MapRadialUI` and `SCR_MapDrawingUI`.                                                                                 | Double-fire the day either module is added back.                                                    | Re-check whenever `MapRespawn.conf` gains a module.                                                                                                |
| A5  | **P4-G1 — `gamepad0:y` (Respawn at home)** is vanilla `HintDismiss`, which **is** in `MapContext`'s `ActionRefs` and therefore live here.                                                                                                                   | Pressing Y visibly dismisses a hint at the same moment it respawns you. Harmless, but watch for it. | —                                                                                                                                                  |
| A6  | **P4-G2 — `keyboard:KC_RETURN` versus an open chat line.** `ChatToggle` is in `MapContext`'s `ActionRefs`, so chat can be opened on this screen; whether `ChatSendMessage` consumes Enter first is unknown.                                                 | Typing a chat message and pressing Enter also respawns you.                                         | Deliberate try: open chat on the screen, type, press Enter.                                                                                        |
| A7  | **The screen genuinely cannot be dismissed.** `Esc`, the map-gadget key, `MenuBack`/`gamepad0:b`, the main-menu key, clicking empty map.                                                                                                                    | A dead player in a live world with no HUD and no screen.                                            | —                                                                                                                                                  |

### B. The six file classes no automated gate can see

Every one of these passes `compile-check.sh` **and** both test groups while being completely broken.

- `Configs/Map/MapRespawn.conf` (+`.meta`) — new GUID, standalone (**not** a same-GUID delta), so it
  inherits nothing: every module, layer and props config is listed explicitly and any one of them
  dangling is invisible until the world loads.
- `Configs/Map/OverthrowMapRespawn.conf` (+`.meta`) — the four type entries and their GUIDs.
- `UI/Layouts/Respawn/OVT_RespawnScreen.layout` (+`.meta`) — including the inherited
  `{0651202E9F2646DE}UI/layouts/Map/Map.layout` reference that supplies `MapFrame`/`MapWidget`.
- `UI/Layouts/Map/Core/OVT_MapInfoPanelRespawn.layout` (+`.meta`).
- Three `.et` edits: `OVT_OverthrowController.et` (the request component),
  `OVT_OverthrowGameMode.et` (`m_RespawnUIContext`), `OVT_PlayerController.et` (the screen handler).
- `Configs/System/chimeraInputCommon.conf` — two actions plus `ActionContext OverthrowRespawnContext`.
- **Widget names.** `RespawnButton`, `RespawnHomeButton` and `StatusText` were grep-audited against the
  layouts that define them, but a null `FindAnyWidget` is only observable at runtime. All three now log
  an ERROR naming the missing widget, so the failure is loud **in the log** — it is still silent on
  screen apart from the button simply not working.
- ✅ **Localization is measured, not assumed:** the 11 `#OVT-Respawn_*` ids exist in
  `Language/localization_Overthrow.st` **and** in all six generated `.<lang>.conf` exports in the
  working tree (the user regenerated them), and the layouts reference the keys rather than literal text.
  What is still unverified is only that they _render_ — a raw key on screen would mean a stale export,
  not a bad key.

### C. The deferred death path (Phase 6) — the highest-consequence unexecuted code

- Die ⇒ **no character is created**; the awaiting entry and the 5 s re-ask ticks appear in the server log.
- `CompleteRespawn` twice for one death creates **one** character (the one-shot claim).
- Disconnect while awaiting, then reconnect ⇒ home, with the entry dropped by `OnPlayerDisconnected_S`.
- The **degrade path**: removing `OVT_RespawnRequestComponent` from the controller prefab reproduces
  today's immediate spawn with a loud ERROR. One-off proof, revert immediately.
- **P6-G2** — `CompleteRespawn` returning OK does not prove a character exists. `CreateFreshCharacter*`
  returns `void`, so a spawn failure after the claim is consumed leaves a player with no claim, no
  character, no re-ask chain and a client that was told OK. Recorded rather than fixed; worth one
  deliberate look.
- **P6-G3** — a save taken while a player is awaiting is safe only because `CapturePlayerBodyId`,
  `CapturePlayerBodyTransform` and `CapturePlayerGearSnapshot` are each independently dead-guarded.
  Reasoned, never observed. Test: die, leave the screen up, save, respawn, reload.
- **P6-G4** — disconnecting while awaiting now enters `OVT_OverthrowGameMode.OnPlayerDisconnected`'s
  `if(controlledEntity)` branch on a **corpse**, which it never used to. Reasoned safe; the stale comment
  there was corrected in this phase, the behaviour was not.
- **P6-A1** — the seven-site corpse-window audit is a **reading**, not a measurement. The three bare
  `GetControlledEntity()` tests are argued unreachable from death; nothing proves it.

### D. Networking (nothing here has crossed a wire)

- **P5-G1** — the whole feature rests on _a dead player keeping ownership of its controller_
  (`AssignControllerOwnership` binds ownership to the **connection**, and the entity is only deleted for
  disconnected players). Source reading. If it is wrong, the `RplRcver.Server` ask is refused and the
  `RplRcver.Owner` sends never arrive.
- **Listen-server short-circuits, both directions** (`SendRespawnResult`, `AskShowRespawnScreen`) — never
  executed. `map/fast-travel` recorded the identical branch as _its_ one untested path, because MP
  testing used a dedicated server.
- **Two clients**: A's request must never resolve to B (assert on the printed player ids); simultaneous
  respawns; a JIP death.
- **P2-G2** — between connect and `RpcDo_RegisterPlayer` arriving, `GetLocalPersistentId()` returns `""`.
  Fail-closed, and a second reason the re-ask tick exists — but the window itself is unmeasured.
- **P2-G3** — the `GetController()` no-controlled-entity fallback has never executed; nothing in the tree
  produced that state before the death path deferred. Its first run ever will be in this play-test.

### E. The map content and the client-side rules

- Only Base / FOB / Camp / House draw, **at maximum zoom-out** (`m_fVisibilityZoom 0`).
- An enemy-held base, another player's private camp and a house you neither own nor rent are **absent**,
  not greyed — and no town, shop, vehicle, waypoint or POI appears at all.
- **The QRF exclusion on both ends** (P3-D1). The pure `IsInsideQrf` predicate is asserted in the Logic
  tier; `IsPositionInActiveQRF` and `CollectEligiblePositions` read live managers and are not.
- **P3-D2** — `IsPositionInActiveQRF` fails **open** on a missing manager, deliberately. Unobserved.
- **P3-D3** — `OVT_MapLocationHouse.CanRespawn` ignores `m_bCanFastTravel`, unlike its `CanFastTravel`
  sibling. The one place the new override is not a strict subset of the old check.
- **P4-D1** — the info panel's distance row is suppressed _structurally_ (no `Distance` widget), because
  `SetupLocationInfoBase` ignores `m_bShowDistance` and would have printed "Distance: Unknown".
- **F-8 — the living fullscreen map must be unchanged.** Ten types, three zoom levels, fast travel, bus
  travel, the recruit toggle, and no "Respawn here" control anywhere.

### F. Screen lifecycle

- **The living-character belt** (`UpdateLivingCharacterBelt`) is a _transition_, not a state test, because
  the player still controls their corpse when the screen opens. If the transition arming is wrong the
  screen closes on the frame it opened and the player waits ~5 s for the re-ask to recover it.
- **The map re-open poll** (`EnsureMapOpen`) has never fired. One failure is logged and then the screen
  carries on **without a map** — "Respawn at Home" still works, which is the intended degrade.
- **P1-G4** — two deaths in one session reuse the cached SPAWNSCREEN config
  (`SetupMapConfig` early-returns when the mode matches, swapping only the root widget). Reasoned correct
  from source; correct **only while Overthrow has exactly one SPAWNSCREEN config**.
- **K4** — no life-state teardown on SPAWNSCREEN (the hooks are FULLSCREEN-only). Read from source; it is
  the property the whole screen depends on.

### G. Environmental, not this feature

- **P3-G2** — a zombie `ArmaReforgerSteamDiag.exe` (PID 30624) survived every kill and is still listed.
  Nine subsequent runs were normal, so it is not blocking; it may want a reboot.

---

## Where to look when it doesn't work

Three signatures, and they point at three different halves of the feature. Take them in this order.

**1. Neither `Print` fires on the server when you press a button ⇒ the request never left the client.**
The pair to look for is `Respawn request received: destination=… pos=…` (arrival) followed by
`Respawn result … for player …`. If the arrival line is absent, nothing on the server ran, so no rule
refused anything. Suspects, in order:

- **The prefab entry.** `OVT_RespawnRequestComponent` exists in script but is missing from
  `Prefabs/GameMode/OVT_OverthrowController.et` ⇒ `OVT_Global.GetRespawnRequests()` returns null. Both
  call sites log an ERROR for exactly this and the "Respawn at Home" path also writes
  `#OVT-Respawn_Failed` to the status line.
- **`Rpc()` arity — BUG-090.** `Rpc()`'s prototype is untyped variadic, so a wrong argument count
  **compiles clean and dies silently at the wire**. The three sites were hand-checked (2/2, 0/0, 1/1);
  re-check them before suspecting anything subtler.
- **Ownership** — P5-G1. If a dead player does _not_ keep their controller, the `RplRcver.Server` ask is
  simply dropped. Same silence, different cause; distinguish it by whether an _alive_ player can drive
  the same path from the `/respawn-screen` harness.
- **The button was never wired.** A null `FindAnyWidget("RespawnButton")` / `("RespawnHomeButton")`, or a
  missing `SCR_InputButtonComponent` on it — each logs an ERROR naming the widget at screen-open time,
  before any press.

**2. Both `Print`s fire and the result is not OK ⇒ a rule refused, and the reason is on screen.**
The result line names the `OVT_RespawnResult` code and the client puts
`OVT_RespawnService.ReasonKeyFor(result)` on the status line under the title (the transient hint can be
missed; that line cannot). Read the code:

- `NOT_ELIGIBLE` most often means _the claim was already spent_ — a second press after a first that
  worked (P6-D2). It does **not** mean the location was rejected.
- `OK_FELL_BACK_HOME` means the server did not recognise the position you sent: `ResolveRespawnPosition`
  found no match within `MATCH_TOLERANCE` in its own `CollectEligiblePositions` set. Expected when a QRF
  started on the location while the screen was open (P3-D1); unexpected otherwise, and then it points at
  the client and server disagreeing about the eligible set.
- `NO_PLAYER` means the persistent id was unresolvable server-side. The claim is deliberately **not**
  consumed on that branch, so the re-ask chain still has an exit.
- `SPAWN_FAILED` means the hand-off itself failed and the player is still awaiting.

**3. The screen opens but the map is empty, or shows only some types ⇒ the persistent id resolved empty.**
This is the feature's designed-loudest failure: `OnShow` prints
`Respawn screen opening. Local persistent id: '…'`. **An empty string there is the answer.** Everything
ownership-keyed fails closed and silently on it — `OVT_MapLocationHouse.PopulateLocations` returns
immediately, `OVT_MapLocationCamp` filters every private camp out, and `OVT_Global.GetController()`
returns null so the buttons do nothing either. The distinguishing detail is _which_ markers survive:
bases and FOBs are not ownership-keyed, so **"bases and FOBs but no houses or camps" is the signature of
an empty id**, whereas nothing at all points at the map config or the container instead. If the id is
empty, look at `P2-G2` (the client's mapping is written by an RPC, so there is a window after connect)
before looking at anything in this feature.

---

## P6-A1 — The widened corpse window: audit of every `GetControlledEntity()` guard

Deferral stretches "the controller still references the corpse" from **one frame** to **as long as the
player takes to choose**. Every site in `OVT_SpawnLogic` that reads a controlled entity, re-read against
that. The plan listed five; there are seven.

| #   | Site                       | Test               | What it guards                                                                  | Still correct?                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| --- | -------------------------- | ------------------ | ------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 1   | `SpawnDeferredPlayer`      | **bare**           | "they already have a body, don't build another"                                 | ✅ Yes, and conservatively. With a corpse it declines to act — which is what we want: an awaiting player's spawn belongs to `CompleteRespawn`, and creating one here would bypass the pick. Not stranding, because the awaiting entry still owns the outcome. Unreachable mid-death anyway: its callers are connect (`DoSpawn_S`) and `FinalizePlayerPreparation`, which is once-per-session guarded.                                                                                                                                                                                                  |
| 2   | `RetryCreateCharacter`     | dead-tolerant      | "something else finished the job while we waited for saved data"                | ✅ Unchanged. Only ever runs during a continued session's load poll, before any death can have happened. The dead tolerance was written for the death path and is simply not exercised by it any more.                                                                                                                                                                                                                                                                                                                                                                                                 |
| 3   | `CreateFreshCharacterAt`   | dead-tolerant      | "don't build a second character for a player who has one"                       | ✅ **This is the one the window actually widens**, and it is correct because the test was never "did they die recently" — it asks whether the player has a character that can still play, and a corpse never has been one. `CompleteRespawn` relies on it passing with a corpse held. Comment updated in place to say the window is now long.                                                                                                                                                                                                                                                          |
| 4   | `OnPlayerBodySpawned`      | **bare**           | "a character arrived while the stored body was in flight — discard the arrival" | ✅ Yes, and the bare form is _better_ than a dead check here. Unreachable from death (death clears `m_sBodyPersistenceId`, so the stored-body route cannot start, and the pending-set claim gates re-entry). If it were somehow reached with a corpse held, a dead check would hand the restored body over and spawn the player **without a pick** — silently violating the feature's core invariant. The bare test discards instead. Destructive-but-unreachable beats reachable-and-wrong.                                                                                                           |
| 5   | `OnPlayerBodySpawnTimeout` | **bare**           | "gave up on the request — but only spawn if they still have nothing"            | ✅ Yes, conservatively. Same unreachability as #4. With a corpse held it returns without spawning, leaving the awaiting entry to own the outcome.                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| 6   | `CreateAndJoinGroup`       | **bare, inverted** | "retry until the player has an entity to belong to a group with"                | ✅ Yes. **Not on the plan's list** (P6-G1). A corpse satisfies it, so a player who dies within 3 s of a handover now gets their faction/group set while dead instead of on their new character. Harmless: the entity is only a presence signal — everything after it operates on _player-controller_ components, `SetCivilianFaction` and `EnsureOwnGroup` are both idempotent, and the recruit roster the group event triggers respawns recruits at **their own** stored bodies, never at the player's position. The 3 s trigger window is fixed at handover and is **not** lengthened by this phase. |
| 7   | `ReAskRespawnScreen`       | dead-tolerant      | "something else gave them a living character — stop asking"                     | ✅ New in this phase. Must be dead-tolerant: treating the corpse as a character would drop the entry and strand the player in exactly the state the region exists to end.                                                                                                                                                                                                                                                                                                                                                                                                                              |

**The through-line:** every one of these asks _"does this player have a character that can still play?"_,
and a corpse has never been one. That is why widening the window does not break any of them — and why
the three bare tests, which are the ones that would be wrong if a corpse counted as a character, all
happen to sit on paths a death cannot take, and all fail **safe** (decline to spawn / discard) rather
than **wrong** (spawn without a pick) if they ever were reached.

---

## Decisions

### P1-D1 — The screen activates `MapContext` itself, every frame

**The plan did not know this and it is the reason R1 would otherwise have failed.** `MapContext` is not
activated by `SCR_MapEntity` at all — it comes from the **menu preset**. `chimeraMenus.conf` declares
`MenuPreset MapMenu { … ActionContext "MapContext" … }`, and the only two `ActivateContext("MapContext")`
calls in the entire vanilla tree are `SCR_DeployMenuBase.OnMenuUpdate` and `SCR_MapEditorUIComponent`.

A workspace-hosted layout has no preset, so **nothing** would have activated it: the map would have
rendered correctly and then ignored every pan, zoom, cursor and selection input — a failure that looks
exactly like "workspace hosting doesn't work" but isn't. `OVT_RespawnContext.OnActiveFrame` therefore
calls `m_InputManager.ActivateContext("MapContext")` every frame while the map is up.

### P1-D2 — `MapRespawn.conf` carries the cursor module and omits every tool module

The config is a standalone file, not a same-GUID delta, so it inherits nothing. Carried: the layers,
props, descriptor-visibility and descriptor-defaults configs (the last with Overthrow's own delta hiding
vanilla's text/icon descriptors), and **`SCR_MapCursorModule`** — which owns every pan/zoom/cursor and
`MapSelect` listener and is the one module that cannot be omitted.

Deliberately omitted, each freeing an input as a side effect: `SCR_MapUIElementContainer` (⚠️ **the
important one** — `OVT_OverthrowMapUI` _is_ one, and two of them both resolve `UIIconsContainer` while
`OnMapOpen` deletes every child of it, so whichever inits second wipes the other's markers),
`SCR_MapToolMenuUI`, `SCR_MapJournalUI`, `SCR_MapRulerUI`, `SCR_MapTaskListUI`, `SCR_MapDrawingUI`,
`SCR_MapToolInteractionUI`, `SCR_MapMarkersUI`. Also not carried from Overthrow's `MapOverthrow.conf`:
`OVT_MapRestrictedAreas`, `OVT_MapThreatGrid` (already disabled) and `OVT_MapPlayerLocation` (a dead
player has no position to draw).

### P1-D3 — `gamepad0:y` + `keyboard:KC_HOME` for "Respawn at home", not the plan's `KC_H`

The input work was pulled forward from Phase 7 because a `WLib_NavigationButton` with no action draws no
glyph, and "can this be driven on a pad" is the question Phase 1 exists to answer. The full live
`MapContext` surface was enumerated by hand — 41 actions across both confs, inline declarations _and_
`ActionRefs` — which is precisely the repo checker's known blind spot.

### P1-D4 — `OverthrowRespawnContext` deliberately omits `MenuUp/Down/Left/Right` and `MenuSelect`

A considered exception to the usual Overthrow rule, not an oversight. On a gamepad those four directions
**are** the left stick, which is already `MapPanHGamepad`/`MapPanVGamepad`; `MenuSelect` is `gamepad0:a`,
which is already `MapSelect`; `MenuBack` is `gamepad0:b`, which is already `OverthrowCloseInfoPanel`.
Listing them would make one stick both pan the map and move widget focus, and one `A` press both select a
marker and activate a focused widget. Vanilla's own spawn screen agrees — `DeployMenuContext` and
`DeployMenuMapContext` list none of the four and no `MenuSelect`; every affordance is a dedicated,
glyphed action. **On this screen the map cursor is the navigation device.** If the pad test disagrees,
the fix is four lines in the `ActionContext` — but test the double-fire first.

### P3-D1 — the QRF exclusion runs on the **server** too, not just the client's `CanRespawn`

The plan spells the exclusion out only on the four client-side `CanRespawn` overrides.
`CollectEligiblePositions` now applies it as well. Without that, the server would accept a location the
client would never have offered — and DoD **I-2** ("start a QRF on the chosen location, press Respawn
here, land at home with the fallback message") would silently not work. Both ends run the same rule,
which is the whole point of §3.3.

### P3-D2 — `IsPositionInActiveQRF` fails **open** on a missing manager

Returns "not excluded" rather than "excluded". Refusing everything on a partially-replicated client would
hand a dead player an empty screen with no explanation — the worst outcome this feature has. The server
re-validates on arrival, so the cost of being wrong in this direction is a fallback to home, not a wrong
spawn. Rationale is in the `//!` block.

### P3-D3 — `OVT_MapLocationHouse.CanRespawn` deliberately ignores `m_bCanFastTravel`

Unlike its `CanFastTravel` sibling. Whether a house is a fast-travel destination is a different question
from whether you live there. This is the one place the new override is **not** a strict subset of the
existing check, so it is recorded rather than left to be rediscovered.

### P4-D1 — the info panel's distance row is a **third** path, suppressed by the layout

P2-G1 found two distance paths; there are three. `OVT_MapLocationType.SetupIconWidget` and
`OVT_MapLocationElement.UpdateDistance` both honour `m_bShowDistance`, so `0` gates them. But
`OVT_OverthrowMapUI.SetupLocationInfoBase` — the **info panel's** row — never consults the attribute at
all: it calls `GetDistanceFromPlayer()` unconditionally and writes the literal string `"Unknown"` on
`-1`. A dead player would therefore have seen a "Distance: Unknown" row on every panel. It is suppressed
structurally instead: `OVT_MapInfoPanelRespawn.layout` carries no `Distance` widget, so the
`FindAnyWidget` returns null and the whole block is skipped. Same technique as the travel button — the
requirement is met by the layout, not by a code branch.

### P4-D2 — the `.conf` rationale comments were **removed** after the fact

Phase 4 wrote the three-zeros rationale into `OverthrowMapRespawn.conf` as `//` comments. Checked
afterwards: **zero** of the repo's `.conf` files and **zero** of vanilla's carry a comment — the format
is entirely unattested, nothing available can test whether the parser accepts one, and a parse failure
here is invisible to `compile-check.sh` and to both test groups and would ship the respawn map dead. The
comments were stripped from the `.conf`; the identical rationale lives in `OVT_RespawnMapUI.c`'s class
header and above. (A Workbench re-save would have silently stripped them anyway.)

### P5-D1 — the Phase 6 seam is a **method**, not an inline call

`RpcAsk_Respawn`'s validation order (server guard → arrival `Print` → destination allow-list → identity →
hand-off → result) is what Phase 5 owns, and putting the spawn hand-off behind a local
`CompleteRespawn(playerId, destination, targetPos)` method makes that order **immutable across Phase 6**
— Phase 6 replaces one method body and touches nothing else in the file. Until it does, every respawn
returns `SPAWN_FAILED` with an ERROR line naming the phase: loud, not silent.

### P5-D2 — steps 3 and 4 of `RpcAsk_Respawn` return **without** sending a result

A deliberate, documented exception to "every outcome reports". Both run _before_ an identity exists — an
out-of-range destination and an unresolvable owner have no client to report to. Neither is reachable from
Overthrow's own UI (both call sites pass a literal enum), and both leave the player on the screen free to
press again. Matches the `OVT_TravelRequestComponent` template exactly.

### P5-D3 — `AskShowRespawnScreen` carries a server guard the template does not

`SendTravelResult` has no `if (!Replication.IsServer()) return;` because it is only ever reached from an
already-server-guarded handler. `AskShowRespawnScreen` is a **public entry point** that Phase 6 calls, so
a stray client call would otherwise fire an owner-targeted `Rpc()` from a client. Guard added, reason
documented in place.

---

### P6-D1 — the degrade path keeps the one-frame `CallLater`, it does not spawn synchronously

The plan says a missing request component means "call `CreateCharacter(playerId, persId)` immediately".
Taken literally that would be a **synchronous** call from inside `OnPlayerKilled_S`, and the line being
replaced was `GetGame().GetCallqueue().Call(CreateCharacter, ...)` — deferred by one frame, with the
file's own comment saying why: _a new character cannot be handed over on the same frame the old one
died_. "Immediately" means _at t=0 rather than after a timeout_, not _in this stack frame_. The degrade
path therefore reproduces the removed line **exactly**, delay included, so the fallback really is
today's behaviour rather than something subtly worse than it.

### P6-D2 — "not awaiting" answers `NOT_ELIGIBLE`, not `SPAWN_FAILED`

`OVT_RespawnResult.SPAWN_FAILED` is documented as _"the player is still awaiting and must be re-asked"_.
For a duplicate or late ask that is a lie — the claim is gone precisely because it was already honoured.
`NOT_ELIGIBLE` is not a perfect fit either, but it does not make a false promise about the state, and
the commonest way to reach it is a second click from somebody whose first click already worked.

### P6-D3 — an unresolvable UID does **not** consume the claim

`CompleteRespawn` refuses `NO_PLAYER` _before_ the one-shot claim, so a player whose persistent id is
momentarily unresolvable keeps their awaiting entry and their re-ask chain. Consuming the claim on that
branch would spend their only remaining exit on a transient failure.

### P6-D4 — the server resolves the request component per player, never through `OVT_Global`

`OVT_Global.GetRespawnRequests()` resolves the **local** machine's controller. On a listen server that
would hand the host's own component back for every player. `OVT_SpawnLogic.FindRespawnRequests(playerId)`
goes `OVT_Global.GetPlayers().GetController(playerId)` instead, and the t=0 capability check and the
re-ask tick share it — so a check that passes at death is a promise the tick can keep.

---

## Gotchas

### 🔴 P6-G1 — the plan's corpse-window enumeration is wrong in two ways

The plan names **five** `GetControlledEntity()` sites and says **two** omit the dead check. The file has
**seven** (six pre-existing plus this phase's own tick), and **three** of the pre-existing ones omit the
dead check. The missing site is `CreateAndJoinGroup`, which uses the _inverse_ test — it retries while
there is **no** controlled entity, so a corpse now satisfies it. Benign (see P6-A1) but it was not on
the list, and an audit that trusted the list would have missed it.

### 🟡 P6-G2 — `CompleteRespawn` returning OK does not prove a character exists

`CreateFreshCharacter*` returns `void`, so once the claim is consumed a failure inside the spawn (the
player left, or the character prefab would not spawn) leaves a player with no claim, no character, no
re-ask chain, and a client that was told OK. Deliberately not papered over: threading a success value
out would edit the method every non-respawn spawn also uses, and re-arming the claim on a post-hoc
`GetControlledEntity()` probe would flash a failure hint on the **common** path whenever possession had
not registered yet. The residual case needs the player prefab itself to fail to spawn, which strands
players at their first spawn too, by the same route and with the same ERROR line. Recorded rather than
fixed, and worth one deliberate look during the play-test.

### 🟡 P6-G3 — a save taken while a player is awaiting is safe, but only because of three existing guards

`SyncPlayerBodyIds()` runs before every save and iterates connected players' controlled entities — which
is now a **corpse** for minutes rather than one frame. All three writes it makes are already dead-guarded
(`CapturePlayerBodyId`, `CapturePlayerBodyTransform`, `CapturePlayerGearSnapshot` each return early on a
dead character), so death-is-complete-loss survives a save taken mid-choice. `CapturePlayerBodyId`'s own
comment anticipates exactly this ("a save taken in the frame between the death and the respawn") — the
window is now much bigger and the guard is unconditional, so it still holds. Nothing to change; worth
knowing before anyone relaxes one of those guards.

### 🟡 P6-G4 — disconnecting while awaiting now enters a game-mode branch it never used to

`OVT_OverthrowGameMode.OnPlayerDisconnected` opens with `if(controlledEntity)` and its comment states
that _"a player who leaves while dead or in the respawn menu has no controlled entity"_. That is no
longer true — they hold a corpse — so the block now runs on a corpse. It is safe for the same reason as
P6-G3 (all three captures are dead-guarded) and the extra `OVT_PersistenceTracking.Save(corpse)` is
harmless, since the kill hook already marks the corpse to self-spawn as lootable remains. The corpse is
then deleted by vanilla, because `OVT_ReconnectComponent.HandlePlayerDisconnect` explicitly refuses to
claim a dead body. Net result on reconnect: no body id, no last-known position, no gear snapshot ⇒ home,
which is exactly what D-3 intends. **The comment in that method is now stale and should be corrected in
Phase 8.**

### ✅ P5-G1 — a dead player keeps ownership of its controller (source reading, not evidence)

The whole phase rests on this. `OVT_PlayerManagerComponent.AssignControllerOwnership` grants ownership
via `rplComponent.GiveExt(playerController.GetRplIdentity(), true)` — bound to the **connection**, not
the character — and the entity is deleted only by `CleanupPlayerController`, which runs for
**disconnected** players only. So the `RplRcver.Server` ask is accepted from a corpse-less client and
`RplRcver.Owner` sends reach it. **Nothing tests this.** First real proof is Phases 6 + 7 together, on a
dead client whose ask reaches the server.

### 🟡 P5-G2 — arrival→result log pairing depends on adjacency

Two clients are told apart by pairing each arrival `Print` with the result `Print` that follows it, which
works because RPC handlers run to completion on the server's main thread. If anything inside
`CompleteRespawn` ever logs in between, that adjacency weakens and the arrival line would need the
resolved player id added to it.

### 🟡 P4-G1 — R8 is wrong on **both** counts: `gamepad0:x` _and_ `y` are claimed

The plan's R8 says neither is claimed by any `MapContext` action. Hand-verification of the live surface
says otherwise:

- **`gamepad0:x`** is `MapContextualMenu`. It is inert here **only because** `MapRespawn.conf` carries
  neither `SCR_MapRadialUI` nor `SCR_MapDrawingUI` — the only two consumers in the vanilla tree. Bound to
  `OverthrowRespawnHere` anyway (vanilla ships the same overlap for `MenuQuickDeploy`), recorded in the
  conflict checker's `ACKNOWLEDGED` list with the mechanism named and a re-check note.
- **`gamepad0:y`** is vanilla `HintDismiss`, which is in `MapContext`'s `ActionRefs` and therefore live
  on this screen. Harmless — dismissing a hint while respawning home costs nothing — but it means
  pressing Y may visibly dismiss a hint at the same moment.

### 🟡 P4-G2 — `keyboard:KC_RETURN` versus an open chat line is unverified

`ChatToggle` is in `MapContext`'s `ActionRefs`, so a player can open chat on this screen. Whether the
chat context's `ChatSendMessage` consumes Enter before `OverthrowRespawnHere` sees it was not
established. Worth one deliberate try during the play-test: open chat on the respawn screen, type, press
Enter, and confirm you do not also respawn.

### 🔴 P1-G1 — the plan's proposed `keyboard:KC_H` is taken, three times over

`MapContext` already holds `MapToggleShowSettings` (plain `KC_H` click), `HintToggle` (plain `KC_H`
click) **and** `HintContext` (`KC_H` hold 750 ms). Pressing `H` on the respawn screen would have fired
all of them alongside the respawn. Rebound to **`keyboard:KC_HOME`**, whose only vanilla owner is
`CamFreeFly` in `ScriptCameraFreeFlyContext` and is never live here.

### 🟡 P1-G2 — the plan's `gamepad0:x` for "Respawn here" overlaps `MapContextualMenu`

It is inert **only because** `SCR_MapRadialUI` and `SCR_MapDrawingUI` were omitted from
`MapRespawn.conf`. That is a config dependency, not a guarantee. Vanilla ships the same overlap
(`MenuQuickDeploy` on `x`), so it is usable — but if either module is ever added back, `x` double-fires.
After `y`, the only unclaimed plain pad buttons here are `thumb_left` and `shoulder_right` (the latter
inert only because `SCR_MapToolInteractionUI` is omitted). `shoulder_left` is **VON at priority 110** and
is not available.

### 🟡 P1-G3 — the spike screen can fast-travel a living player

`MapRespawn.conf` currently points `m_InfoPanelLayout` at the existing `OVT_MapInfoPanel.layout` so that
marker selection is observable during the spike. **That panel still carries Fast Travel and Bring
Recruits, and they really will teleport you and charge the fare.** Phase 4 replaces it with
`OVT_MapInfoPanelRespawn.layout`, which is where the "no travel affordances" requirement is actually met.

### 🟡 P1-G4 — R11 confirmed from source: one SPAWNSCREEN config only

`SCR_MapEntity.SetupMapConfig` early-returns the currently active config when `mapMode == m_eLastMapMode`,
swapping only `RootWidgetRef`. `OpenMap` then leaves `m_bDoReload` false, so `ActivateComponents` takes
its `else` branch and re-activates the already-loaded instances — `SetActive(true)` refreshes
`m_RootWidget` from `GetMapConfig().RootWidgetRef`, so the reuse is correct. **It is correct only while
Overthrow has exactly one SPAWNSCREEN config**; a second would silently be handed the first one's config
object. Recorded as a `//!` block on `MAP_RESPAWN_CONF`.

### ✅ K4 confirmed from source: no life-state teardown on SPAWNSCREEN

`SCR_MapEntity.OpenMap` installs the `m_OnLifeStateChanged` and `GetOnPlayerDeleted()` hooks **only** for
`EMapEntityMode.FULLSCREEN`. `OnLifeStateChanged` stows the map gadget on any non-ALIVE transition and
`OnPlayerDeleted` closes `ChimeraMenuPreset.MapMenu`. Neither is installed for SPAWNSCREEN — which is
exactly the property this screen needs, since it exists to sit in the state that would trigger them.

### 🟡 P1-G5 — highest unverified risk: `ActionContext` flags

Whether `OverthrowRespawnContext` at `Priority 50 / Flags 4` **coexists with** `MapContext` at
`Priority 50 / Flags 0x6c` or suppresses it could not be determined — the flag enum is not in the
extracted tree. Equal priority means neither outranks the other and `Flags 4` is what every other
Overthrow menu context uses, but the symptom if wrong is a visible map that does not respond. One-line
fix: change to `Flags 0x6c`, or drop `Flags` entirely as `DeployMenuContext` does.

### 🔴 P3-G1 — `out` is a reserved parameter modifier and cannot be a parameter name

The plan writes `CollectEligiblePositions(string persId, notnull array<vector> out)`. That does not
compile. The parameter is named `positions` — same shape, appended to, never cleared, caller owns it.
(Same family as the known `owned`-is-a-keyword trap.)

### 🟡 P3-G2 — a zombie test-harness process survived every kill

The first `run-tests.sh` invocation of Phase 3 timed out at 300 s and left
`ArmaReforgerSteamDiag.exe` **PID 30624** in a state where `tasklist.exe` still lists it (322 MB) but
`taskkill /F /T`, `Stop-Process -Force` and `tools/lib/common.sh --sweep-stale --kill` all report "there
is no running instance of the task". Nine subsequent runs worked normally, so it is **not blocking** —
but it is still listed, `sweep-stale` has kept a pidfile for it, and it may want a reboot or manual
cleanup. Not caused by this feature.

### 🟡 P2-G1 — the plan's `GetLocalControlledEntity` enumeration was incomplete

Six hits remain under `Scripts/Game/UI/Map/`, not the three the plan predicted. All six are
position/gadget-scoped and none is an identity path, so the acceptance criterion holds — but two were
unlisted: **`OVT_MapLocationElement.UpdateDistance`** is a _second_ distance calculation independent of
`OVT_MapLocationData.GetDistanceFromPlayer`, which means Phase 4's `m_bShowDistance 0` is now covering two
code paths rather than one; and `OVT_MapPlayerLocation` draws the living player's own marker and
correctly returns early without an entity.

### 🟡 P2-G2 — the client's persistent-id mapping is written by an RPC, so there is a window after connect

`m_mPersistentIDs` is populated on clients by two routes: `RpcDo_RegisterPlayer` → `SetupPlayer`, which
writes the mapping **before** its `if (!Replication.IsServer()) return;` guard (the client half of
`SetupPlayer` is exactly that write), and `RplLoad` for JIP. Both are sound — but between connect and the
RPC arriving the map is empty and `GetLocalPersistentId()` returns `""`. That is the same fail-closed
behaviour as today, and it is a second reason Phase 6's re-ask tick exists.

Also worth recording: `GetPersistentIDFromPlayerID`'s doc comment claims it "attempts to create the
mapping and calls SetupPlayer". **It does neither** — it is a pure lookup that returns `""` on a miss.
The comment is stale; the behaviour is what a client wants.

### 🟡 P2-G3 — the `GetController()` fallback is dead code until Phase 6

It only fires when there is no controlled entity, and nothing in the tree produces that state until the
death path is deferred. Its first real execution will be on the day Phase 6 lands.

### 🟡 P1-G6 — the spike host is on the player character, and Phase 7 must move it

Phase 1 registers `OVT_RespawnContext` in `OVT_UIManagerComponent.m_aContexts` on
`Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` so the screen can be shown while **alive**
via the `/respawn-screen` chat command. That host is useless to a dead player. Phase 7 moves the context
to `OVT_OverthrowGameMode` (`m_RespawnUIContext`) driven by `OVT_RespawnScreenHandlerComponent` on the
player controller, and must remove this entry.

---

## Session Notes

### 2026-08-10 — `/autorun-feature map/respawn`

Started from an existing plan (`/plan-feature` had already run). Scaffolded `tasks.md` (131 tasks, 27 of
them user-driven) and this file, and re-measured the Phase 0 baselines rather than quoting them.

### 2026-08-11 — Phase 8 (docs and contract records)

Documentation only. Two code edits, both comments, both zero-behaviour: `OVT_MapContext.HideMap()`'s
false consumer claim (K13) and — picked up from **P6-G4** — the now-stale comment in
`OVT_OverthrowGameMode.OnPlayerDisconnected` that asserts a player leaving while dead has no controlled
entity. Gate re-run after both: compile exit 0 / **5964 files**, Fast **54**, All **89**.

**FINDING P8-A — the plan's Q-5 note about `MAP_FRAME_NAME` is wrong.** Phase 8's brief said
`SCR_MapConstants.MAP_FRAME_NAME` is declared with **no consumer** in the extracted vanilla tree. Grep
says it has three: `SCR_MapDrawingUI`, `SCR_MapMarkerBase` and `SCR_MapMarkerEntity`. The true statement
— and the one written into `map/core/context.md` — is that **none of those modules is carried by
`MapRespawn.conf`**, so on the respawn screen the name is defined and never looked up. That is a config
dependency, not a property of the name, and it joins P1-G2/P4-G1 as a thing that changes the moment a
module is added back.

**Q-6 swept clean.** Every `+` line across the eleven changed `.c` files and the whole body of the six
new ones was checked for `file:line` pointers (`.c:NNN`, `:NNN-NNN`): **zero hits**. The pointers that do
exist in `OVT_SpawnLogic.c`, `OVT_OverthrowGameMode.c` and `OVT_OverthrowMapUI.c` are pre-existing
untouched context lines — mostly citations into the vanilla tree — and belong to the epic's standing
`file:line`-rot audit, not to this feature.

### 2026-08-11 — `/autorun-feature map/respawn` complete (Phases 0–8)

Eight phases in one autonomous run, one agent at a time, each phase gated before the next started.
**Final gates, re-measured by the orchestrator after the last phase rather than quoted from any agent:**
compile exit **0 / 5964 files**, Fast **54**, All **89**.

Six new `.c` files (+6 from the 5958 baseline), 10 new Logic cases (44 → 54 Fast, 79 → 89 All), and no
movement in any other tier at any phase boundary — which matters most at Phase 6, where the Campaign and
Persistence tiers exercise the very spawn state being edited, and where the per-tier breakdown was
measured before _and_ after and came back identical.

**Four things the plan had wrong**, each caught during the build and recorded above rather than worked
around: `MapContext` is activated by the **menu preset**, not by `SCR_MapEntity` (P1-D1 — without the fix
the map would have rendered perfectly and ignored every input, and the spike would have "failed" for a
reason unrelated to workspace hosting); `keyboard:KC_H` is taken three times over (P1-G1); risk R8's
claim that neither `gamepad0:x` nor `y` is spoken for is wrong on **both** counts (P4-G1); and
`CollectEligiblePositions(..., array<vector> out)` does not compile, because `out` is a reserved
parameter modifier (P3-G1). The corpse-window audit found **seven** guards where the plan listed five,
and three of the pre-existing ones omit the dead check rather than two (P6-A1).

**Two orchestrator interventions.** The `.conf` rationale comments Phase 4 wrote were **removed**: no
`.conf` file in this repo or in the entire vanilla tree carries a comment, nothing available can test
whether the parser accepts one, and a parse failure there is invisible to every automated gate — the
identical text lives in `OVT_RespawnMapUI.c`'s header instead (P4-D2). And the six regenerated
`localization_Overthrow.<lang>.conf` exports were **inspected, not trusted**: **+22 / −0** in every one,
11 ids in `Ids{}` and 11 matching texts in `Texts{}` in the same order, zero deletions — the additive
shape, not the mismatched-block corruption that has bitten this repo before.

**Nothing in this feature has been executed.** Every gate that passed is a compile-or-logic gate; the
autotest world has no players, so `OnPlayerKilled_S` never fires, no RPC ever crosses a wire, and there
is no UI tier at all. See **Still unverified** above for the single consolidated list, and
**Where to look when it doesn't work** for the three distinguishing failure signatures.

### 2026-08-18 — BUG-182: chosen-location respawns spawned players inside geometry

**Post-ship defect, filed and fixed.** Players picking a location on the respawn screen materialised
inside the FOB truck / building walls: `CollectEligiblePositions` records **raw entity origins** and
`CreateFreshCharacterAt`'s "used as given" contract meant the chosen-location path was the **only**
spawn path that never ran `OVT_Global.FindSafeSpawnPosition`'s `OVT_SpawnPointComponent` query — the
authored points on `OverthrowMobileFOBDeployed.et` / `OVT_RecruitmentTent.et` / `OVT_Camp.et` /
`OVT_BaseController.et` were simply never consulted (the HOME path heals via
`EnsureClearSpawnPosition` and was unaffected). Fix: `CompleteRespawn` now routes `resolvedPos`
through `TryFindSafeSpawnPosition` **after** the eligibility match (the raw origin stays the lookup
key; only the spawn moves), with a loud WARNING fallback to the origin when nothing clear is found.
The stale "re-safety-checking would move the player away" premise in `CreateFreshCharacterAt`'s doc
comment is corrected. Compile 0 / 6059 files; acceptance is manual (see `docs/bugs/BUG-182.md`) —
the bug stays open until the FOB/camp/base/house respawns are observed landing on authored points.

### 2026-08-11 — Phase 9 discharged: the user ran the gate and reported all green

**The feature is complete.** The user ran the §6 Verification Method and reported the whole gate green —
V-3 (Workbench clean load), V-4 (the single-player sweep), V-5 (two clients), V-6 (disconnect while
awaiting), V-7 (gamepad-only) and V-8 (localization). Recorded as **the user's report**, which is the only
form this evidence can take: none of it is observable to `compile-check.sh` or to either test group, and
that was true by design from Phase 0.

**No code change was required to pass it.** The tree at the moment of the play-test was byte-identical to
the tree at the end of Phase 8 — no new files, no commits, and the six localization exports still at
+22/−0 with all 11 ids present. Every "Still unverified" item in the section above is therefore
discharged as _observed correct as built_, not as _fixed after observation_. The three that mattered
most, because each would have invalidated a design decision rather than needing a patch:

- **A1/A2 — the workspace-hosted SPAWNSCREEN map takes input.** This was the feature's one genuine
  unknown and the reason Phase 1 was a spike sequenced first. The `ChimeraMenuBase` fallback recorded in
  Phase 1 was never needed, and **P1-G5's `ActionContext` flags question resolves in favour of
  `Flags 4`** — `OverthrowRespawnContext` at `Priority 50 / Flags 4` does coexist with `MapContext` at
  `50 / 0x6c`. That is now a measured fact rather than an inference, and it is the reusable one: a future
  Overthrow screen can host a vanilla map in a workspace layout, provided it activates `MapContext`
  itself every frame (P1-D1).
- **D — the wire works, and P5-G1 holds.** A dead player really does keep ownership of its
  `OVT_OverthrowController`, so the server-directed ask is accepted from a corpse-less client and
  owner-targeted sends reach it. That was source reading until now, and the whole feature rested on it.
- **I-5 — the persistent id resolves with no controlled entity.** The failure signature to watch for was
  "bases and FOBs draw but no houses or camps"; it did not appear. K3's identity fix does what it was
  written to do, and by extension `OVT_Global.GetLocalPersistentId()` and the `GetController()` fallback
  are now proven for any future dead-player feature (P2-G3's "dead code until Phase 6" is retired).

**Still true, and worth carrying forward** — these are not defects and were not part of the gate:
`gamepad0:x` is inert only while `MapRespawn.conf` omits `SCR_MapRadialUI` and `SCR_MapDrawingUI`
(P4-G1/P1-G2); `MAP_FRAME_NAME` is unconsumed on this screen for the same config-dependent reason
(P8-A); Overthrow may have exactly **one** SPAWNSCREEN config (P1-G4/R11); P6-G2's spawn-failure gap is
documented rather than closed; and `OVT_AdminCommandsComponent`'s chat commands are likely dead in
single player (found out of scope, not fixed, not this feature's to fix).
