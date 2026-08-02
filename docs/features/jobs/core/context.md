# Jobs Core - Context & Decisions

**Last Updated:** 2026-08-02 23:42
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing shipped code)
- ✅ Retrospective documentation created (`/discover-feature`, 2026-08-02)
- ✅ Top 5 discovery findings filed as BUG-037…041 (BUG-005 pre-existing)

**What's Next:**
- 📋 Fix the multiplayer distribution bugs — BUG-037 (tutorial chain dead for the second player on any server) is the headline
- 📋 See implementation.md Future Enhancements

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/OVT_JobManagerComponent.c` — the manager (lifecycle, RPCs, JIP, persistence apply)
- `Scripts/Game/Configuration/OVT_JobConfig.c` — config classes + flags
- `Scripts/Game/GameMode/Systems/Jobs/` — framework bases + 8 conditions + 11 stages
- `Scripts/Game/UI/Context/OVT_JobsContext.c` + `UI/Layouts/Menu/JobsMenu.layout` — the menu
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:901-953` — accept/decline seam
- `Scripts/Game/Persistence/Serializers/Components/OVT_JobManagerSerializer.c` — vanilla persistence
- `Prefabs/GameMode/OVT_OverthrowGameMode.et` — `m_aJobConfigs` (**order = persisted `jobIndex`; append only**)
- `Configs/Jobs/*.conf` — the 8 shipped jobs
- `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Jobs.c` — 4 logic cases (one pins BUG-005)

---

## Important Decisions

- **False-means-advance stage protocol:** `OnStart`/`OnTick` returning false ⇒ skip/advance. Waiting stages override `OnTick` only; side-effect stages instant-advance out of `OnStart`. **This invariant is what makes the no-replay persistence restore correct** — the definitive record lives in `docs/features/core/persistence/context.md`; do not break it in new stages.
- **`jobIndex` is positional** (prefab array position) and persisted; the serializer drops out-of-range/unrestorable records rather than guessing. Append-only discipline on `m_aJobConfigs`.
- **Occupancy derived, counters stored:** occupancy sets are rebuilt from the restored board; lifetime counters (`m_aJobCounts`, `m_mPlayerJobCounts`) persist verbatim.
- **Broadcast RPCs over RplProp** (variable-length board, entity-less players) — same rationale as skills.

---

## Gotchas & Learnings

- **The `new` trap:** `[Attribute()]` defvalues are applied by the config loader, NOT by `new` — logic tests must assign every field explicitly (two conditions use `-1` sentinels and neutral-1 multipliers; see the `OVT_TEST_Logic_Jobs.c` header).
- **`OVT_TEST_Logic_Jobs_DealerCondition_PinsAxisOnlyCheckBug` asserts the WRONG behaviour on purpose** — fixing BUG-005 must flip that test, which is the proof the fix landed.
- Player-allocated jobs are auto-accepted at StartJob (`accepted = owner != ""`), so the menu's Accept/Decline only ever applies to public jobs.
- `m_vCurrentWaypoint` is shared scratch state also written by `OVT_RecruitsContext`; a waypoint at world origin reads as unset.
- Jobs bypass `OVT_NotificationManagerComponent` and use `SCR_HintManagerComponent` directly — the only Overthrow subsystem that does.

---

*This context file was created retrospectively by analyzing existing code.*
