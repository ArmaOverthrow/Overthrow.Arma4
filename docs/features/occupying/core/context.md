# Occupying Core (Faction Manager) - Context & Decisions

**Last Updated:** 2026-08-22
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- ⏸️ **Play-test the defense-share drip (2026-08-22)** — nothing has watched the faction spend on the new rhythm; the point of the change is *feel*, so a suite cannot close it
- 📋 **These docs are stale** and were written against a 1508-line manager that is now ~2,300 lines. The spend-loop cluster (`SpendResources`, dead `perBase`, divide-by-zero) is **deleted**, `GetBase()` **is** null-guarded, and **BUG-025/026 are both closed** — the "highest-value fixes" this file used to name are all done. The Gotchas below are marked where they no longer hold

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

- **The defense share is a DEBT, dripped hourly, not a transfer (2026-08-22).** `GainAndSpendResources` arms `m_iPendingDefenseTransfer` instead of moving 80 % across in one statement; `DripDefenseShare()` pays a slice an hour at a jittered minute. **The income half deliberately did not move** — `resistance/sleep` replays income through these exact methods, so changing the income cadence changes the replay's granularity with it, which is BUG-183's family (an unpersisted latch paying a sweep twice on load is a repeatable money exploit). The pool is the contended resource, so smoothing its *arrival* lands exactly where the burst behaviour was, and the half carrying the save-format and time-skip hazards is untouched.
  - **Author decision:** the money **stays in the reserve** while owed, and the jitter is **timing only**.
  - ⚠️ **Known, accepted consequence for `occupying/objectives`:** the reserve now sits ~80 % fatter for most of each window, and `OVT_ObjectiveDirectorComponent` reads the reserve for `objectiveQRFResourceGate` — **counter-attacks clear their funding gate earlier than before.** If the objective ramp starts feeling too fast, this is the first thing to look at, not the difficulty fields.
  - **Nothing can be stranded:** `ArmDefenseShareDrip` flushes the previous window before arming the new one, so a window that loses drips to a QRF freeze (`CheckUpdate` returns early for the whole of an engaged QRF), a mid-window load, or a sleep replay costs **timing and never money**.

- **Records + controllers linked by position:** `OVT_BaseData.entId` at discovery; nearest-position matching for persistence restore. Survives ID churn; O(n) lookups.
- **Two unrelated "threat" concepts:** global scalar `m_iThreat` (escalation) vs `GetThreatByLocation()` (spatial score for base ranking + map overlay). They never interact.
- **Garrisons respawn, never restore:** AI self-spawn disabled in `Overthrow.conf`; serializer stores upgrade state / prefab lists; `InitBaseControllers` is the single replay point for new and continued campaigns alike.
- **Hand-rolled positional JIP** (no RplProps); delta updates via reliable broadcast RPCs. `RplLoad` writes faction keys back into the shared config component.
- **Continue-path guard:** deserialize clears `m_bDistributeInitial` so the opening build-out isn't doubled — restored state must be applied before `PostGameStart` runs.

---

## Gotchas & Learnings

- ~~**The spend loop's budget math is dead**~~ — ✅ **GONE (BUG-026 closed).** `SpendResources` and the whole per-base loop were deleted by `virtualization/base-defense-migration`; the defense share now goes to the one deployment pool.
- ~~**`GetBase()` has no null guard**~~ — ✅ **FIXED.** It null-checks the marker and returns null.
- **Tower capture = garrison wipe.** No user action, no timer; and towers never flip back to the OF except via specops' 600 s capture.
- **`SetClientBaseFactions` is a one-shot 1 s bet** on JIP + streaming; a lost race leaves wrong client flags forever.
- **Counter-attacks are much rarer than intended:** pick-then-filter wastes rolls on OF-held bases, and the last base index is unreachable (`RandInt` half-open).
- ~~**`RpcAsk_InstantCaptureBase` ships unauthenticated**~~ — ✅ **BUG-025 closed.** The capture RPCs moved to `OVT_CampaignRequestComponent` and carry no client-supplied position; `InstantCaptureBase` is server-resolved.
- **`m_iThreat` is a float** with an int prefix; `GetThreatLevel()` truncates it, and deployments consume the truncated value.
- The player-count resource ladder (×2/×3/×4/×5/×6 at 4/8/16/24/32) is duplicated verbatim in the QRF controller — change both or neither.
- **Discovery stamps tower/base factions with the config-DEFAULT occupying index** (`GetOccupyingFactionIndex()` computes and caches from `m_sOccupyingFaction = "USSR"` at `Init` time); the save's real faction key is only applied later in `ApplyPersistedOccupyingFaction`. Fixed for both towers AND bases 2026-08-13: any tower/base the save has no record for (map updated after the save) is stamped to the occupying faction in the apply sweep; restored records are never trampled.

---

*This context file was created retrospectively by analyzing existing code.*
