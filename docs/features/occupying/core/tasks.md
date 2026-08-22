# Occupying Core (Faction Manager) - Task Checklist

**Last Updated:** 2026-08-22
**Progress:** Complete (Existing Feature)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Base/tower registries with world discovery and campaign start/continue lifecycle
- [x] ✅ Resource/threat economy with income, spending, specops and counter-attack decisions
- [x] ✅ Base capture flow, tower garrisons/capture, town battle triggers
- [x] ✅ JIP replication + vanilla persistence serializer
- [x] ✅ Retrospective documentation created

---

## Enhancements

- [x] ✅ New radio towers on existing saves default to the occupying faction (2026-08-13) — `ApplyPersistedOccupyingFaction` now stamps any tower the save has no record for with the occupying faction index (after the save's faction key is applied), and falls back to occupying for saved tower records with `faction < 0` (mirroring the bases path). Previously an unmatched tower kept discovery's guess and `CheckRadioTowers` skipped it forever (no garrison). Covered by `OVT_TEST_Persistence_NewRadioTower_DefaultsToOccupyingFaction` (proven red pre-fix: "expected occupying faction 3, it is -1").
- [x] ✅ Same fix for bases (2026-08-13) — a base with no save record (added to the map after the save was written) is likewise stamped to the occupying faction in the apply sweep; restored records are never trampled. Covered by `OVT_TEST_Persistence_NewBase_DefaultsToOccupyingFaction` (proven red pre-fix the same way).

- [x] ✅ **The defense share drips into the deployment pool instead of flooding it (2026-08-22)** — the epic's deferred "drip the defense-share TRANSFER into the pool" idea, built. Income is untouched (still a lump on the four six-hour boundaries, still latched, still replayed by `resistance/sleep` through the same methods); what moved is the TRANSFER. `GainAndSpendResources` now calls `ArmDefenseShareDrip(newResources)`, which flushes whatever the previous window still owed and arms the new share as a debt; `DripDefenseShare()` pays one slice an hour at a jittered minute. Six slices, remainder carried by `OVT_BaseDefenseConversion.DripAmount` (which divides what is STILL owed by the drips STILL to come, so nothing is stranded and the final drip pays out the rest).
  - **Author decisions (2026-08-22):** the money **stays in the reserve** while it is owed (an escrow bucket was the alternative), and the jitter is on **timing only** — every slice is the same size, the minute of the hour is rolled fresh after each drip.
  - ⚠ **Deliberate consequence, not a defect:** the reserve now sits ~80 % fatter for most of each six-hour window, and the reserve is what `OVT_ObjectiveDirectorComponent` reads for `objectiveQRFResourceGate` — so a counter-attack clears its funding gate **earlier** than it did. Flagged to the author before building and accepted.
  - **Replayed by the sleep time skip** on every hour boundary the chronological loop walks, without the jitter (a fast-forward has no "inside the hour" to land in; what it owes is the same *number* of drips).
  - **Persisted at serializer version 4** (three appended ints). A version 1–3 payload restores a debt of **zero**, which is the truthful answer — those campaigns had the whole share credited at the payday, and defaulting them to a full window would pay it twice (BUG-183's family).
  - Coverage: `OVT_TEST_Logic_BaseDefenseConversion_DripPaysOutExactlyTheShare` (the carry, across whole windows), `OVT_TEST_Init_Deployments_DefenseShareDripsIntoThePool` (the wiring: arming moves nothing, six drips conserve reserve+pool at every step and deliver exactly the share, and an abandoned window is settled by the next payday's flush), `OVT_TEST_Persistence_DefenseDripDebtSurvivesTheRoundTrip`. **All 594/601** — the 7 red are pre-existing failures in uncommitted `logistics/resources` and `core/damage` work, unchanged by this feature.
  - ⏸️ **Play-test owed:** nothing has watched the faction actually spend on the new rhythm.

## Future Enhancements

See `implementation.md` Future Enhancements. Headline items: fix the spend-loop bug cluster (dead `perBase`, undecremented `toSpend`, divide-by-zero), validate the capture RPCs, null-guard `GetBase()`.
⚠️ Note (2026-08-13): BUG-025…031 are all closed — the headline items above may already be fixed; verify against `docs/bugs/` before starting work.


---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
