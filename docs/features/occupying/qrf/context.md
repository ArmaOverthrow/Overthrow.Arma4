# QRF - Context & Decisions

**Last Updated:** 2026-08-20 (CLOSED)
**Current Phase:** CLOSED — legacy
**Status:** 🗄️ CLOSED 2026-08-20 — legacy. `OVT_QRFControllerComponent` still resolves every battle (standard + the `counter-attacks` siege mode) and is consumed as-is by `occupying/objectives`' `StartBattle` module; no further feature work here. Open retrospective debt (JIP payload gaps, live battle rolls back on load) is tracked in the epic's Tech Debt, not here.

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)
- ✅ Player-initiated town uprisings (2026-08-16): auto-trigger removed; uprising starts from a new flag action on the town controller (see tasks.md)
- ✅ Recruits count towards zone control (2026-08-18): resistance AI now scores alongside players; dead/downed characters count for neither side

**What's Next:**
- ⏸️ USER: reposition `OVT_TownController` instances in the world (prefab now has a flagpole model)
- 📋 MP play-test the uprising request over the wire (Rpc arity blind spot)
- 📋 Review for potential improvements (see implementation.md Future Enhancements — free-resources bug and LZ fixes are the highest-value)

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` — the entire QRF (507 lines, one class, server-only)
- `Prefabs/Controllers/OVT_QRFController.et` — bare marker entity + RplComponent, no persistence
- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` — owner: spawn, callbacks, client mirroring, outcome application (`occupying/core`)
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:105-150` — client→server triggers (`RpcAsk_StartBaseCapture`, `RpcAsk_InstantCaptureBase`)
- `Scripts/Game/Configuration/OVT_DifficultySettings.c` — `QRFPointsToWin`, `maxQRF`, `QRFFastTravelMode`
- UI: `OVT_EconomyInfo.c` (HUD), `OVT_MapRestrictedAreas.c` / `OVT_MapContext.c` (map)

---

## Important Decisions

- **Uprisings are player-initiated (2026-08-16):** the quarter-hourly auto-trigger no longer starts battles in occupied towns — it surprised new players. `OVT_StartUprisingAction` on the town controller flag starts them (hidden on villages/resistance towns, disabled with `#OVT-SupportTooLow` at support ≤ `OVT_OccupyingFactionManager.UPRISING_SUPPORT_THRESHOLD` (75)). Suppression battles (resistance town, support < 25%) stay automatic — that's the OF fighting back, not a player surprise. Server path is `OVT_UprisingRequestComponent` on the per-player `OVT_OverthrowController` (comms component is deprecated and deleted on the v1.5 controller-migration branch); the client sends a town id as a lookup key, never a coordinate, and the server re-validates everything.
- **Town controller is now a flagpole:** `OVT_TownController.et` inherits `FlagPole_02_V1_FIA.et` exactly like `OVT_BaseController.et` (same `SlotManagerComponent {55DAE04E55ECE7FA}` override → USSR flag). Flag material tracks the replicated town faction via a 10 s check on every machine (`CheckUpdateFlag` — same `SCR_FlagComponent.ChangeMaterial` pattern as bases). The medical-supplies sign child was kept (jobs use it as the delivery landmark); world instances need repositioning by hand — the entity was invisible before and some sit on roads.
- **Ephemeral battle entity:** one controller entity per battle, spawned at the objective, deleted on resolution; consequences flow only through the manager's `m_OnFinished` callbacks. One QRF at a time (`m_CurrentQRF` singleton slot).
- **Global garrison despawn:** all garrison/patrol/civilian spawn predicates check `!m_CurrentQRF`; the QRF delays its own spawning 15 s so they clear first. The contested base loses its defenders by design (perf headroom).
  - ⚠ **SUPERSEDED IN PART, 2026-08-19 — read this before acting on the bullet above.** The "global" half of that sentence is history: `virtualization/base-defense-migration` moved base defence off base upgrades and onto **deployments**, whose groups live on the engine's proximity lifecycle and honoured no QRF predicate at all — a play-test found tower guards materialising at a contested base mid-battle. The **intent** of the bullet survives; its **scope** does not. The rule as built is now:
    - suppression is **local**, not global — only within the QRF's own `QRF_RANGE` (750 m) of `m_vQRFLocation`. Deployments elsewhere on the map keep being maintained, which is the migration's own improvement and is deliberately preserved;
    - it is gated on **`IsQRFEngaged()`**, not on `m_CurrentQRF` existing. A counter-attack siege is a battle object for up to 31 minutes before a shot is fired, and emptying the objective during `SILENT_DEPLOY`/`MUSTER` would be exactly the tell `occupying/counter-attacks` §3.9 exists to avoid. A **standard, player-initiated battle is engaged from creation**, so nothing about a player's own battle changed;
    - it is scoped to the **occupying faction**. Resistance-faction deployments are never suppressed — this battle's zone-control scoring deliberately counts resistance AI (see the zone-control bullet below), so holding those men back would score the player down for it;
    - it **suppresses materialisation** of groups that are not yet up (`SCR_AIGroup.SetLifecyclePolicy(Manual)`, reversed when the battle ends). Groups **already standing** when the battle reaches them are left alone — the contested base is emptied by the player as it always was, it simply stops being refilled. Forcibly despawning standing groups was considered and deliberately **not** done; see `occupying/counter-attacks/context.md` for the feasibility verdict.
    - Owner: `OVT_DeploymentManagerComponent.TickBattleSuppression()`; the rule itself is the pure `OVT_DeploymentBattleSuppression`. Civilians are unaffected by any of this — they are still the town invoker, unchanged.
- **Zone control is a head count of everyone still standing (2026-08-18):** counted every 10 s inside 220 m. Human players **and** resistance-faction AI (recruits, and any future resistance deployment) score for the resistance — the old players-only model made an all-AI assault lose by default. Player-controlled entities are skipped in the agent loop so a player is never double-counted. Dead and unconscious characters count for neither side (`IsFightingFit`), which also stops a corpse earning the 2 XP/tick. +5/tick still requires zero OF AI within 750 m. Recruits alone can now carry a battle — deliberate: they are forces the player paid for and committed.
- **Not persisted, by omission:** no QRF state in the serializer, controller/troops never persistence-tracked — a load mid-battle cleanly rolls back to "no battle".
- **Survivors stay:** QRF AI is intentionally not cleaned up after battle (commit `e115965`).

---

## Gotchas & Learnings

- **Stale-doc warning cleared 2026-08-18:** BUG-013/025/026/027/031 are all **closed** — QRFs now debit `m_iResources` (with a zero clamp), the LZ trace checks `result > 0 && !trace.TraceEnt`, and the `Goodqrfpos` file-scope globals are gone (replaced by the member `m_vGoodTargetPos`).
- **`SCR_ChimeraCharacter.GetFactionKey()` reads the *affiliated* faction**, which is exactly what `OVT_RecruitManagerComponent.SetRecruitFaction` writes (`m_sPlayerFaction`) — so a faction-key compare is a reliable recruit test. It was CIV before BUG-146.
- **Two replication channels, one live:** the controller's `RplProp m_iPoints/m_iWinningFaction` stop replicating the moment the battle phase starts (BumpMe unreachable); the manager's broadcast RPCs are the real channel the HUD uses.
- **`m_iCurrentQRFBase/Town` are not in the JIP payload** — JIP clients mis-draw map restricted areas during a battle.
- The economy freeze during a QRF is total (no resources/threat/spending/counter-attacks/town checks anywhere) — long battles visibly stall the whole faction.
- Deployment AI within 750 m counts toward the QRF's enemy tally (unintended coupling); deployments also stop evaluating during battles. ⚠ **Updated 2026-08-19:** "stop evaluating" is `IsQRFEngaged()`, not "a battle exists", and it only ever blocked **creating** deployments. Existing occupying-faction deployment groups within 750 m of an engaged battle are now additionally held dormant — see the amendment under *Global garrison despawn* above.

---

*This context file was created retrospectively by analyzing existing code.*

---

## 2026-08-25 — The leadup HUD showed the previous battle's score

**Author:** *"when a battle starts the HUD always shows the points from the last battle during the leadup phase."*

**Nothing ever reset `m_iQRFPoints`.** It has exactly one writer — `OVT_OccupyingFactionManager.UpdateQRFPoints`, called from `OVT_QRFControllerComponent.CheckUpdatePoints` **from inside its `if(m_iTimer <= 0)` block**, so no value is pushed until the countdown has already run out. The controller does zero its own `m_iPoints` in `OnPostInit`, but that copy is not the replicated one `OVT_EconomyInfo.UpdateQRF` reads for the two sliders (`OVT_EconomyInfo.c:381`).

⚠ **The stale value was always a decisive one.** A battle ends *at* the cap (`m_iPoints >= toWin || <= -toWin`), so what sat on screen for the whole leadup was a full ±`QRFPointsToWin` bar for whoever won last time.

**Fix:** `ResetQRFScore()` beside `UpdateQRFPoints`, called from `StartBaseQRF` and `StartTownQRF` at the point the rest of the QRF state is stamped — before `m_bQRFRevealed` is set, so the panel cannot be shown with the old score even for a frame.

⚠ **At start, not at finish.** A finish handler that does not run — a rolled-back save, a campaign teardown, a future caller — would leave the stale value behind again. Opening every battle from a known state cannot.

⚠ **Points only. `m_iQRFTimer` looks like the same bug and is not:** `CheckUpdateTimer` runs on a 1 000 ms call from the controller's own `OnPostInit` and publishes `m_iTimer` on its first tick, so the clock is this battle's within a second. Zeroing it here would need the lead time in the controller's units — **milliseconds**, `m_iTimer` defaults to `120000` — and would buy one second of correctness for a units mistake waiting to happen.

`tools/compile-check.sh` exit 0 (6347 files). Suite not run; play-test owed.
