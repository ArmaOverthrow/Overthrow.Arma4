# Occupying Core (Faction Manager) - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements — the spend-loop bug cluster (`CheckUpdate`/`SpendResources`) and the unvalidated capture RPCs are the highest-value fixes

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` (1508 L) — manager + data records
- `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c` — per-base controller (flagpole prefab `Prefabs/Controllers/OVT_BaseController.et`, 9 placed in `bases.layer`)
- `Scripts/Game/Controllers/OccupyingFaction/OVT_TowerControllerComponent.c` — empty marker class (towers are pure manager logic)
- `Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c` — save/load
- `Scripts/Game/UserActions/OVT_CaptureBaseAction.c`, `OVT_ManageBaseAction.c`
- `Scripts/Game/GameMode/Systems/Modifiers/*/OVT_OccupyingFactionDeath*Modifier.c` — town modifiers off `m_OnAIKilled`
- Config: `OVT_DifficultySettings.c` (OF tuning block), `OVT_OverthrowConfigComponent.c`

---

## Important Decisions

- **Records + controllers linked by position:** `OVT_BaseData.entId` at discovery; nearest-position matching for persistence restore. Survives ID churn; O(n) lookups.
- **Two unrelated "threat" concepts:** global scalar `m_iThreat` (escalation) vs `GetThreatByLocation()` (spatial score for base ranking + map overlay). They never interact.
- **Garrisons respawn, never restore:** AI self-spawn disabled in `Overthrow.conf`; serializer stores upgrade state / prefab lists; `InitBaseControllers` is the single replay point for new and continued campaigns alike.
- **Hand-rolled positional JIP** (no RplProps); delta updates via reliable broadcast RPCs. `RplLoad` writes faction keys back into the shared config component.
- **Continue-path guard:** deserialize clears `m_bDistributeInitial` so the opening build-out isn't doubled — restored state must be applied before `PostGameStart` runs.

---

## Gotchas & Learnings

- **The spend loop's budget math is dead:** `perBase` never used, `toSpend` never decremented — the top-threat base drains the entire reserve each tick; divide-by-zero when no OF bases remain.
- **`GetBase()` has no null guard** on the marker entity; the serializer author worked around it locally (`FindBaseController`) instead of fixing it.
- **Tower capture = garrison wipe.** No user action, no timer; and towers never flip back to the OF except via specops' 600 s capture.
- **`SetClientBaseFactions` is a one-shot 1 s bet** on JIP + streaming; a lost race leaves wrong client flags forever.
- **Counter-attacks are much rarer than intended:** pick-then-filter wastes rolls on OF-held bases, and the last base index is unreachable (`RandInt` half-open).
- **`RpcAsk_InstantCaptureBase` ships unauthenticated** — the DiagMenu gate is client-side.
- **`m_iThreat` is a float** with an int prefix; `GetThreatLevel()` truncates it, and deployments consume the truncated value.
- The player-count resource ladder (×2/×3/×4/×5/×6 at 4/8/16/24/32) is duplicated verbatim in the QRF controller — change both or neither.
- **Discovery stamps tower/base factions with the config-DEFAULT occupying index** (`GetOccupyingFactionIndex()` computes and caches from `m_sOccupyingFaction = "USSR"` at `Init` time); the save's real faction key is only applied later in `ApplyPersistedOccupyingFaction`. Fixed for both towers AND bases 2026-08-13: any tower/base the save has no record for (map updated after the save) is stamped to the occupying faction in the apply sweep; restored records are never trampled.

---

*This context file was created retrospectively by analyzing existing code.*
