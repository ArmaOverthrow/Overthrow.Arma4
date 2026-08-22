# Resistance Building - Task Checklist

**Last Updated:** 2026-08-16
**Progress:** Complete (Existing Feature) — in maintenance

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Config model (`OVT_Placeable`/`OVT_Buildable`) + authored configs (8 placeables, 7 buildables)
- [x] ✅ Place/build UI contexts (paged card menus, ghost previews, rotation/prefab cycling, build camera)
- [x] ✅ Server spawn pipeline (ownership, base association, handler dispatch, XP, navmesh, tracking)
- [x] ✅ Placeable handlers (camp registration, town support modifiers)
- [x] ✅ Item limits (`OVT_ItemLimitChecker` + configurable house/camp/FOB caps)
- [x] ✅ Removal mode (raycast highlight, owner/officer permissions)
- [x] ✅ Vanilla persistence (SelfSpawn configs + component serializers; EPF removed)
- [x] ✅ Retrospective documentation created

---

## Maintenance

- [x] ✅ BUG-048/049/050 (2026-08-02) — server-side validation + actor derivation on the place/build RPCs, server-side buildable charge, `RplId` removal, pagination arithmetic. `implementation.md`'s "High Priority" list is now historical.
- [x] ✅ BUG-178 (2026-08-16) — listen host built one full UI-context set per joined player, so the main (U) menu would not close when starting a placement and the survivors stole the rotate keys. Guarded `OVT_UIManagerComponent.AfterControlledByPlayer` with vanilla's local-controlled-entity discard. Compile OK; Fast 125/125, All 166/166. **Manual two-machine listen-host check still owed** (see the bug's acceptance section).

## Future Enhancements

Open bugs: **BUG-106** (camp `m_bAwayFromBases` rule unreachable), **BUG-160** (place-menu focus highlight too faint on gamepad).

See `implementation.md` Future Enhancements for the remaining medium/low items: cap `FindNearestBase` by per-type radius, delete the dead `m_bAwayFromBases` block + null-guard `GetNearestBase`, rewrite `Placeables/README.md`.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
