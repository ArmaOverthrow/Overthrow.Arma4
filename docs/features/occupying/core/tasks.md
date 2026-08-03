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

## Future Enhancements

See `implementation.md` Future Enhancements. Headline items: fix the spend-loop bug cluster (dead `perBase`, undecremented `toSpend`, divide-by-zero), validate the capture RPCs, null-guard `GetBase()`.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
