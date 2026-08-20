# QRF - Task Checklist

**Last Updated:** 2026-08-20 (CLOSED)
**Progress:** Complete (100%). **CLOSED 2026-08-20 — legacy.** The QRF controller remains the combat layer (STANDARD + COUNTER_ATTACK siege modes) but takes no further work as a feature; anything that changes how battles start or resolve flows through `occupying/objectives`. The three open items below are ticked as closed out.

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Battle controller (countdown, waves, scoring, resolution)
- [x] ✅ Four trigger paths (player assault, AI counter-attack, specops, town battles)
- [x] ✅ Client mirroring (HUD countdown/progress, map restricted areas, notifications, JIP)
- [x] ✅ Retrospective documentation created

---

## Player-Initiated Town Uprisings (COMPLETED 2026-08-16)

Replaced the surprise auto-trigger (player within 300 m of an occupied town with support > 75%) with a player-started action on a flag added to the town controller prefab.

- [x] ✅ Remove the uprising auto-trigger from `OVT_OccupyingFactionManager.CheckUpdate` (suppression battles for lost towns kept)
- [x] ✅ `UPRISING_SUPPORT_THRESHOLD` const (75) shared by action + server validation
- [x] ✅ `OVT_StartUprisingAction` — shown on occupied non-village towns, disabled with `#OVT-SupportTooLow` below threshold, 15 s hold
- [x] ✅ `OVT_UprisingRequestComponent` on `OVT_OverthrowController` (controller pattern, NOT the deprecated comms component) — validates town id/occupation/support/proximity/alive server-side
- [x] ✅ `OVT_TownController.et` now inherits the FIA flagpole (like `OVT_BaseController.et`), USSR flag slot, actions manager with the uprising action
- [x] ✅ Town flag material tracks the controlling faction (10 s check on all machines, `SCR_FlagComponent.ChangeMaterial`)
- [x] ✅ Localization: `OVT-StartUprising` / `OVT-SupportTooLow` in .st + exports
- [x] ✅ compile-check OK; Fast tier 125/125
- [x] **[closed out 2026-08-20 — feature closed as legacy]** ⏸️ USER: reposition `OVT_TownController` instances in the world (prefab now has a flagpole model; some sit on roads)
- [x] **[closed out 2026-08-20 — feature closed as legacy]** 📋 MP play-test: client-initiated uprising over the wire (RPC arity is a compile blind spot)

## Recruits Count Towards Zone Control (COMPLETED 2026-08-18)

Only human players scored for the resistance, so an assault carried by recruits lost by default.

- [x] ✅ Resistance-faction AI within `QRF_POINT_RANGE` counted alongside players (`recruitNum` + `playerNum` = `resistanceNum`)
- [x] ✅ Player-controlled entities skipped in the agent loop so a player is never counted twice
- [x] ✅ `IsFightingFit()` — dead/unconscious characters count for neither side (applies to OF AI, recruits and players; also stops corpses earning the 2 XP/tick)
- [x] ✅ Faction-key lookups hoisted out of the per-agent loop
- [x] ✅ compile-check OK
- [x] **[closed out 2026-08-20 — feature closed as legacy]** 📋 Play-test: recruits-only assault can win a base QRF; verify a downed recruit stops counting

## Future Enhancements

See `implementation.md` Future Enhancements for the prioritized list. Headline items: validate the client capture RPCs, debit faction resources for QRF spend, fix BUG-013 (client config divergence), fix the LZ trace no-op + `Goodqrfpos` global cache.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
