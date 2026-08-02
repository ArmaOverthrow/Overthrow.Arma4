# Recruits - Task Checklist

**Last Updated:** 2026-08-02
**Progress:** Complete (Existing Feature)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Recruit data model, cap, and both acquisition paths (civilian + tent)
- [x] ✅ Vanilla group insertion, commanding, owner-only inventory command, AI civilian-perception integration
- [x] ✅ XP/kills/levels with owner notifications; permadeath; dismissal; rename (single-player)
- [x] ✅ Replication (whole-table JIP + create/remove/update broadcasts)
- [x] ✅ Persistence (versioned record serializer + stored-body round trip via vanilla persistence)
- [x] ✅ Recruit management UI (roster, details, rename, dismiss, show-on-map)
- [x] ✅ Retrospective documentation created

---

## Future Enhancements

See `implementation.md` Future Enhancements for the prioritized list. Headline items: server-validate the recruit RPCs (identity, cost, target, supporter/spawn atomicity), add a rename RPC so renames work in multiplayer, capture the pre-teleport position in fast-travel-with-recruits, fix the `FindRecruitEntity` mid-iteration removal, and de-hardcode the `US`/`USSR` XP faction keys.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
