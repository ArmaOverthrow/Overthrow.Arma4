# Deployments - Task Checklist

**Last Updated:** 2026-08-02
**Progress:** Complete (Existing Feature)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Module framework (condition/spawning/behavior, prototype-clone, registry)
- [x] ✅ Evaluate→score→create loop with cost/chance/priority/max-instance filters
- [x] ✅ Proximity virtualization + vanilla persistence (self-spawn, eliminated-flag ordering)
- [x] ✅ Three shipped configs (Town Patrol + two vehicle patrols); town-patrol migration from BaseUpgrades
- [x] ✅ Retrospective documentation created

---

## Future Enhancements

See `implementation.md` Future Enhancements. Headline items: fix the `m_mFactionDeployments` leak, set `m_iResourcesInvested`/`m_fThreatLevel` at creation, fix the world-time unit bugs, wire runtime condition evaluation, decide the BaseUpgrades migration's fate.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
