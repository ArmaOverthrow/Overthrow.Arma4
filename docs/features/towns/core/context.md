# Towns Core - Context & Decisions

**Last Updated:** 2026-08-03
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (`/discover-feature`, 2026-08-02)
- ✅ Top findings filed as BUG-055, BUG-060, BUG-061, BUG-062

**What's Next:**
- 📋 Review for potential improvements (see implementation.md Future Enhancements)

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c` (1579 L) — town data, discovery, tick, queries, RPC/JIP, persistence apply
- `Scripts/Game/Controllers/OVT_TownController.c` (350 L) — authoring attributes, civilians, dealer spawn, QRF geometry
- `Scripts/Game/Persistence/Serializers/Components/OVT_TownManagerSerializer.c` — location-matched restore
- `Worlds/MP/OVT_Campaign_Eden_Layers/towns.layer` — the 20 authored Everon towns
- `Prefabs/GameMode/OVT_OverthrowGameMode.et:197-209` — manager + modifier systems wiring

## Important Decisions

- **Town identity = array index, everywhere.** Wire ID, JIP order, shop-map key, job key, controller key. Clients build their own list from the same world query; nothing validates alignment. Treat any change to discovery order as save- and MP-breaking.
- **Stability derived / support absolute** — the serializer recomputes stability from restored modifiers but restores support raw (relative recalc would double-apply). This asymmetry is documented at the source and must be preserved.
- **`Rpc(RpcAsk_X)` executes locally + synchronously on the server** (settled empirically in dev-ops `test-coverage/findings.md:1000-1016`). The "RPC-only" mutators (`AddStabilityModifier`/`AddSupportModifier`) are therefore correct — do not "fix" them by adding a local call (it would double-apply).
- **Manager `Init` runs before persistence deserialize** (EOnInit vs AFTER_ENTITY_FINALIZE) — towns always exist before restore; `ApplyPersistedTowns` matches by nearest location and is idempotent.
- **Peaceful flips are villages-only; QRF battles are towns/cities-only** — a clean, deliberate partition with the occupying epic.

## Gotchas & Learnings

- Discovery runs on clients too — the `Replication.IsServer()` guard is deliberately placed *after* `InitializeTowns()`. Moving it breaks every client.
- The filter-as-visitor query idiom (filter side-effects a member array, returns false, callback is null) underpins town discovery itself — engine changes to filter semantics are a silent kill.
- `SUPPORT_FREQUENCY = 6` fires on the 7th tick — the support/population cadence is 70 s, not 60.
- `m_iSupportCounter`-style shared scratch (`m_Houses`, `m_CheckTown`) makes query helpers re-entrancy-unsafe.
- The legacy map-marker path half-works: it spawns controllers but never fills `m_TownControllers` (BUG-061) — assume authored controllers are mandatory.
- `OVT_TownData.CopyFrom` is dead production code with a stale test doc-comment claiming it's the persistence seam (it isn't anymore).
- `areaHeat` is write-only (wanted system increments it; nothing reads it) yet persisted and tested — an unfinished undercover feature.
- Debug builds don't compile: `OVT_OccupyingFactionManager.c:890` references `closestTown.name` (no such field) inside `#ifdef OVERTHROW_DEBUG`.

---

*This context file was created retrospectively by analyzing existing code.*
