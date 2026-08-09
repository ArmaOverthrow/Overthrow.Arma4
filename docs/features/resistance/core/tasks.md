# Resistance Core (Faction Manager) - Task Checklist

**Last Updated:** 2026-08-02
**Progress:** Complete (Existing Feature)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Camp lifecycle: register (via building handoff), one-per-player replacement, privacy, delete with object cleanup
- [x] ✅ FOB lifecycle: mobile FOB deploy/undeploy with container transfer, priority FOB, area cleanup
- [x] ✅ Garrisons for bases/camps/FOBs with waypoints and supporter draw-down
- [x] ✅ Officer role: grants (SP/host/admin/config), promotion UI, mod-wide checks
- [x] ✅ JIP replication (RplSave/RplLoad + delta RPCs) + vanilla persistence serializer (idempotent re-apply)
- [x] ✅ Retrospective documentation created

---

## Future Enhancements

See `implementation.md` Future Enhancements. Headline items: add a server-validated `RpcAsk_AddOfficer` (the client Make Officer path is a dedicated-MP no-op), validate-before-spawn in FOB deploy/undeploy (fall-through currently duplicates vehicles), fix the completion-handler unsubscribe/`GetOwner()` cast and per-operation state, and bounds/null-check + server-charge the garrison RpcAsks.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
