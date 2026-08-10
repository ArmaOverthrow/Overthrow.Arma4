# FOB (Mobile Forward Operating Base) - Task Checklist

**Last Updated:** 2026-08-09\
**Progress:** Complete (100%)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively (carved out of `resistance/core` on 2026-08-09).

- \[x\] ✅ Mobile FOB truck + deployed prefab pair, purchase/upgrade paths
- \[x\] ✅ Deploy/undeploy state machine with async cargo transfer and validation
- \[x\] ✅ Map icons, priority FOB, restricted-area overlay, fast travel
- \[x\] ✅ Garrisons, build/place proximity, set-home respawn, item limits
- \[x\] ✅ JIP replication + idempotent vanilla persistence of records
- \[x\] ✅ Retrospective documentation created

---

## Open Bugs (filed from discovery, 2026-08-09)

Tracked in `docs/bugs/`, all `linkedFeature: resistance/fob` — fix via `/fix-bug <id>` or `/fix-feature resistance/fob`:

- \[x\] ✅ BUG-129 (high, closed 2026-08-09) — deploy leaks the truck's vehicle registration: ghost truck rebuilt at the shop on next login, duplicate FOB markers, wrecked FOB after restart (fixed: registrations retired at all six deletion sites, FOBs excluded from vehicle lifecycle, duplicate-record guard, rebuild clearance check, ownership-map restore; play-test confirmed)
- \[ \] BUG-122 (high) — officer gate client-side only; upgrade path bypass with client-side payment
- \[ \] BUG-124 (high) — undeploy area cleanup has no ownership/association filter
- \[ \] BUG-121 (medium) — failed undeploy leaks a ghost FOB record
- \[ \] BUG-125 (medium) — garrison AI orphaned on undeploy
- \[ \] BUG-123 (medium) — `RpcAsk_SetPriorityFOB` unauthenticated
- \[ \] BUG-126 (medium) — empty-registry nearest-FOB query returns zero vector
- \[ \] BUG-119 (low) — FOBs never get a name
- \[ \] BUG-120 (low) — deploy announcement dropped (missing notification preset)
- \[ \] BUG-127 (low) — undeploy confirmation sent to owner, not actor
- \[ \] BUG-128 (low) — deploy progress shows generic label; `TransferStorageForDeployment` dead

**Filed later, from outside this feature:**
- \[ \] **BUG-139** (medium, filed 2026-08-11 by `map/location-types`) — FOB/camp **garrison never reaches clients**: neither `RplSave`/`RplLoad` (`OVT_ResistanceFactionManager.c:1231-1300`) nor `RpcDo_RegisterFOB`/`RpcDo_RegisterCamp` carries it, and there is no garrison RPC of any kind. **Also a server-side defect**: the three `AddGarrison*` paths insert only into `garrisonEntities`, so the `garrison` prefab list is populated *only* by a save load and `garrison.Count()` reads 0 on the host too until a reload. Persistence is unaffected (the serializer snapshots the live entities). The map panels hide the row at 0 as a mitigation.

Related open bugs owned elsewhere: BUG-109 (occupying — specops inert at FOBs), BUG-116 (persistence — deployed-FOB cargo in scope).

---

## Future Enhancements

See `implementation.md` Future Enhancements for the non-bug backlog (comms→controller migration, persistentId wire keys, per-operation state, dead-code deletion, Logic-tier tests).

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*