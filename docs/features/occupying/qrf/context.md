# QRF - Context & Decisions

**Last Updated:** 2026-08-16
**Current Phase:** Enhancements
**Status:** ✅ Documented (Existing Feature) + player-initiated uprisings landed

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)
- ✅ Player-initiated town uprisings (2026-08-16): auto-trigger removed; uprising starts from a new flag action on the town controller (see tasks.md)

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
- **Players-only scoring:** zone control counted every 10 s inside 220 m; only human players score for the resistance; +5/tick requires zero OF AI within 750 m.
- **Not persisted, by omission:** no QRF state in the serializer, controller/troops never persistence-tracked — a load mid-battle cleanly rolls back to "no battle".
- **Survivors stay:** QRF AI is intentionally not cleaned up after battle (commit `e115965`).

---

## Gotchas & Learnings

- **Two replication channels, one live:** the controller's `RplProp m_iPoints/m_iWinningFaction` stop replicating the moment the battle phase starts (BumpMe unreachable); the manager's broadcast RPCs are the real channel the HUD uses.
- **File-scope globals** `Goodqrfpos`/`Goodqrfbasepos` cache the first landing zone per QRF — all wave sources collapse to one spawn point, and state leaks across controller instances.
- **The LZ "clear box" trace is a no-op** (`TracePosition` fraction compared `>= 0`), so troops can spawn inside geometry.
- **QRFs never debit `m_iResources`** — the budget math is real but the spend is discarded (`m_iUsedResources` write-only).
- **`m_iCurrentQRFBase/Town` are not in the JIP payload** — JIP clients mis-draw map restricted areas during a battle.
- The economy freeze during a QRF is total (no resources/threat/spending/counter-attacks/town checks anywhere) — long battles visibly stall the whole faction.
- Deployment AI within 750 m counts toward the QRF's enemy tally (unintended coupling); deployments also stop evaluating during battles.

---

*This context file was created retrospectively by analyzing existing code.*
