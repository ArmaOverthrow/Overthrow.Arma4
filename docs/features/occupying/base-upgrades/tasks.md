# Base Upgrades - Task Checklist

**Last Updated:** 2026-08-02
**Progress:** Complete (Existing Feature)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Upgrade framework (priority scheduler, allocation model, threat gates)
- [x] ✅ Eight live upgrade types (patrols, positions, tower guards, checkpoints, compositions ×3, parked vehicles, specops)
- [x] ✅ Proxying virtualization + QRF suppression
- [x] ✅ Replay-based persistence (slotted compositions entity-tracked)
- [x] ✅ Retrospective documentation created

---

## Future Enhancements

See `implementation.md` Future Enhancements. Headline items: fix the resource-accounting cluster (dead clamp, proxied-bank inflation, counter drift), make checkpoints survive load, set `m_Spawned` on composition deserialize, delete the dead TownPatrol class, decide the deployments migration's fate.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
