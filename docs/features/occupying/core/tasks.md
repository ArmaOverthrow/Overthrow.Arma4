# Occupying Core (Faction Manager) - Task Checklist

**Last Updated:** 2026-08-02
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

## Future Enhancements

See `implementation.md` Future Enhancements. Headline items: fix the spend-loop bug cluster (dead `perBase`, undecremented `toSpend`, divide-by-zero), validate the capture RPCs, null-guard `GetBase()`.
⚠️ Note (2026-08-13): BUG-025…031 are all closed — the headline items above may already be fixed; verify against `docs/bugs/` before starting work.


---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
