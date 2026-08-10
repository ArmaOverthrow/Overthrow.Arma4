# Towns Map Info - Task Checklist

> ## 📦 ARCHIVED 2026-08-10 — superseded by the `map` epic
>
> **Was:** `docs/features/towns/map-info/tasks.md` (feature 5 of the `towns` epic).
> **Successor:** `docs/features/map/` — see `epic-overview.md` and the features `core`,
> `location-types`, `fast-travel`, `legacy-retirement`.
>
> The system these tasks describe — `OVT_MapIcons`, `OVT_MapContext`'s three modes and the legacy
> fast/bus travel paths — was **deleted by `map/legacy-retirement` on 2026-08-10**. **Do not work
> these "Future Enhancements":** BUG-067, BUG-068 and BUG-069 are structurally impossible now (the
> code is gone), and BUG-070 belongs to the `map` epic — it concerns `OVT_MapRestrictedAreas`, which
> was deliberately **retained**. Everything else here was either replaced by the new map or is
> obsolete.

**Last Updated:** 2026-08-03
**Progress:** Complete (Existing Feature)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Vanilla map extension (config delta): overlays, icons, player arrows
- [x] ✅ Town info panel with modifier chips
- [x] ✅ Fast travel (anchors + rules + payment) and bus travel
- [x] ✅ RplId retry/fallback icon subsystem
- [x] ✅ Retrospective documentation created

---

## Future Enhancements

See `implementation.md` Future Enhancements: per-frame validation fix (BUG-067), icon array integrity (BUG-068), close-path unification (BUG-069), drawn-vs-enforced radii (BUG-070), bus-travel parity, chip localisation, deref guards.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
