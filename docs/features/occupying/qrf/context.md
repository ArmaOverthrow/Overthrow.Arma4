# QRF - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements (see implementation.md Future Enhancements — client-RPC validation and the free-resources bug are the highest-value fixes)

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
