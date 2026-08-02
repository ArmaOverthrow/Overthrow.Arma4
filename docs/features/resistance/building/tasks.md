# Resistance Building - Task Checklist

**Last Updated:** 2026-08-02
**Progress:** Complete (Existing Feature)

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

## Future Enhancements

See `implementation.md` Future Enhancements. Headline items: server-side validation of the place/build RPCs (indices, rules, limits, funds), charge buildables in `BuildItem` instead of the client, fix multiplayer removal (RplId + replicated ownership), and fix the menu pagination arithmetic.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
