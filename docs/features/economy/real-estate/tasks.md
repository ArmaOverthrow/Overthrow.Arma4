# Real Estate - Task Checklist

**Last Updated:** 2026-08-02
**Progress:** Complete (Existing Feature)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Core functionality implemented (ownership, rentals, homes, warehouses)
- [x] ✅ Integration with existing systems (market money, towns, spawning, persistence)
- [x] ✅ Retrospective documentation created

---

## Fixes

- [x] ✅ Resistance-funds purchases lock the building (community report, fixed 2026-09-01) — `OVT_RealEstateContext.c`: last-house rule no longer blocks resistance-account sales of resistance-owned buildings (server never had that rule); `SetAsHome()` now applies the same resistance-owner upgrade `Refresh()` uses, so the enabled button no longer refuses with NotOwner. Client-only; compile-check OK; play-test owed.

## Future Enhancements

See `implementation.md` Known Issues / Future Enhancements — 10 catalogued bugs plus authority gaps, headlined by the JIP warehouse loop-bounds bug (multiplayer-corrupting), the `UpdateRents` early-return, and client-side money debiting.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
