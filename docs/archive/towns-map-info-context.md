# Towns Map Info - Context & Decisions

> ## 📦 ARCHIVED 2026-08-10 — superseded by the `map` epic
>
> **Was:** `docs/features/towns/map-info/context.md` (feature 5 of the `towns` epic).
> **Successor:** `docs/features/map/` — see `epic-overview.md` and the features `core`,
> `location-types`, `fast-travel`, `legacy-retirement`.
>
> This document describes Overthrow's **legacy** map: the hand-rolled `OVT_MapIcons` icon layer, the
> three flag-based modes inside `OVT_MapContext` (map info / fast travel / bus travel), the
> "Map Info" and "Fast Travel" main-menu rows, and the client-side travel paths. **That code no longer
> exists.** It was deleted by `map/legacy-retirement` on **2026-08-10** (`OVT_MapIcons.c`, three
> layouts and their `.meta` files, the `OVT_MapIcons` config block, `OVT_MapContext` stripped
> 591 → 79 lines, and four unvalidated `RequestFastTravel*` RPCs removed from
> `OVT_PlayerCommsComponent`).
>
> **Read this only as history.** Its `file:line` pointers, line counts and code descriptions are no
> longer resolvable. Of the bugs it records: **BUG-067, BUG-068 and BUG-069 are structurally
> impossible** against the current code — the classes that could exhibit them are deleted. **BUG-070
> is still live** and belongs to the `map` epic: it concerns `OVT_MapRestrictedAreas`, which
> `legacy-retirement` deliberately **retained** and did not touch.

**Last Updated:** 2026-08-03
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (`/discover-feature`, 2026-08-02)
- ✅ Top findings filed as BUG-067, BUG-068, BUG-069, BUG-070 (BUG-053 pre-existing; BUG-013 confirmed fixed)

**What's Next:**
- 📋 Review for potential improvements (see implementation.md Future Enhancements)
- ⚠️ Check the unmerged `new-map` branch before large investments — it redesigns towns as first-class map locations

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/UI/Context/OVT_MapContext.c` (515 L) — modes, town panel, fast/bus travel
- `Scripts/Game/UI/Map/OVT_MapIcons.c` (835 L) — per-entity icons, POI registry, RplId retry/fallback
- `Scripts/Game/UI/Map/OVT_MapCanvasLayer.c` + `OVT_MapRestrictedAreas.c` + `OVT_MapThreatGrid.c` — canvas overlays (grid shipped disabled)
- `Configs/Map/MapFullscreen.conf` — the **delta override at vanilla's GUID** (the whole extension mechanism)
- `Prefabs/Characters/.../Character_Player.et:37-43` — context config (layout, chip colors)

## Important Decisions

- **Extension by config delta at the vanilla GUID** — Overthrow's modules/components are *appended* to vanilla's map config. Don't replace the file; don't add a custom map entity.
- **Gamepad is inherited, not implemented** — vanilla `MapSelect` binds `gamepad0:a`; there is no Overthrow map ActionContext and `m_sContextName` is empty. (Consistent with the project's gamepad-navigation memory: don't flag this as a controller blocker.)
- **Chips render from client-loaded modifier configs + replicated id/timer lists** — the deliberate seam; it couples the client to config index order (see towns/stability).
- **Threat grid ships disabled** — re-enabling requires threat replication (server-only today → empty for clients) *and* fixing the 2601-allocations-per-frame draw.

## Gotchas & Learnings

- Two close paths do different things: input `MapExit` closes everything; the engine's `OnMapExit` invoker skips `CloseLayout()` and bus mode (BUG-069). Any new mode must be cleared in **both**.
- `BaseWorld.GetWorldTime()` returns **milliseconds** — the seconds-valued intervals make validation run per frame (BUG-067). Check units on any timer compared against it.
- `OVT_MapIcons`' three parallel arrays must be appended atomically *after* widget creation succeeds; any `continue`/skip desyncs everything after it (BUG-068).
- `OVT_UIContext.Init` contains a self-assignment where `m_Config = OVT_Global.GetConfig()` was intended — **`m_Config` is null in every context**; always fetch via `OVT_Global.GetConfig()`.
- Ocean clicks select the nearest town (`GetNearestTown` is unbounded, click-only — no hover anywhere).
- Icon color defaults to black in `MapIcon.layout` — every icon path must `SetColor`.
- The camp/FOB icon loops hardcode `"FIA"`; the correct source is `m_sPlayerFaction` (used correctly two files over).
- `m_vCurrentWaypoint` (job waypoint icon) is a client-written scratchpad on a replicated manager, shared with the recruits context, never cleared.

---

*This context file was created retrospectively by analyzing existing code.*
