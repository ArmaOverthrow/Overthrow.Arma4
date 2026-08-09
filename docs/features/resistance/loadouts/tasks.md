# Resistance Loadouts - Task Checklist

**Last Updated:** 2026-08-02
**Progress:** Complete (Existing Feature)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Manager + data model (store/index maps, recursive item tree, deterministic ids)
- [x] ✅ Capture/apply engine (attachments, nested containers, quick slots, slot-exact box conservation)
- [x] ✅ UI + user actions on the equipment box; recruit apply-to-one/all handoff
- [x] ✅ Multiplayer seam (RpcAsk endpoints, save/delete broadcasts, JIP metadata)
- [x] ✅ Persistence rebuilt on vanilla persistence (serializer v2, index + contents, idempotent) — 2026-08-02
- [x] ✅ Retrospective documentation created

---

## Future Enhancements

See `implementation.md` Future Enhancements. Headline items: close the free-item paths (equipped weapon/attachments spawned instead of consumed from the box; spawn-mode RPC), validate the RPC seam server-side, capture stowed weapons, surface officer templates to other players, make overwrite-save transactional.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
