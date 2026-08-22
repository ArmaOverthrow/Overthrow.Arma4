# Real Estate - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (see implementation.md — 10 bugs + authority gaps catalogued)

**What's Next:**
- 📋 Highest-value fixes: JIP warehouse loop-bounds bug (corrupts joining clients with ≠1 warehouse), `UpdateRents` early-return, server-side validation

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c` — the manager (ownership, warehouses, homes, prices, JIP)
- `Scripts/Game/GameMode/Managers/OVT_OwnerManagerComponent.c` — generic position-keyed ownership base (four hand-maintained maps)
- `Scripts/Game/Persistence/Serializers/Components/OVT_RealEstateManagerSerializer.c` — vanilla serializer (stores position-key strings verbatim; idempotent appliers)
- `Scripts/Game/UI/Context/OVT_RealEstateContext.c` — buy/sell/rent/set-home menu
- `Prefabs/GameMode/OVT_OverthrowGameMode.et:84-140` — the six building-type configs

---

## Important Decisions

- **Buildings are keyed by position string** (`GetOrigin().ToString(false)`), not EntityID/RplId — the only session-stable identity. The RplId approach was tried and reverted (see `OVT_RplOwnerManagerComponent`, still used by vehicles).
- **Home is not real-estate state** — it's `OVT_PlayerData.home`, persisted by the player manager; players can be homed where they don't own.
- **Warehouse `id` ≡ array index** — the serializer deliberately doesn't persist it to defend the invariant; warehouse records materialize implicitly on first ownership of a warehouse-typed building (10 m proximity match).
- **Load ordering is load-bearing:** the starting-home pool is built in `DoPostLoad` *after* deserialization so restored ownership is honoured.

---

## Gotchas & Learnings

- `GetNearestBuilding` uses a 40 m radius (5 m for renters) — dense urban blocks can resolve the wrong building; two ownable buildings within range is unsupported.
- The literal string `"SCR_DestructibleBuildingEntity"` appears in 6 places — a vanilla rename silently kills the feature.
- Money is debited **client-side** before the ownership ask, and the asks are unvalidated — the shops buy flow is the correct pattern to align with.
- `isPrivate`/`isLinked` warehouse flags are persisted and replicated but have **no setter anywhere** — unfinished feature (camps have the wired equivalent).
- `SetBuildingHome` and `TeleportHome` exist with zero callers; `SetAsHome` uses the player's standing position instead.

### `SetBuildingHome` is now DELETED, not merely uncalled (2026-08-14, core/controller-migration P3)

Phase 3 of `core/controller-migration` migrated real estate's eight client→server requests off
`OVT_PlayerCommsComponent` onto the new `OVT_RealEstateRequestComponent`
(`Scripts/Game/Components/Controller/OVT_RealEstateRequestComponent.c`). **`SetBuildingHome` /
`RpcAsk_SetBuildingHome` were not migrated — they were deleted** (migration plan §3.7 / D6).

Why: zero callers repo-wide, and as a network endpoint it was completely unvalidated — it let any
client set *any* player's home to *any* replicated entity on the map, from anywhere. Carrying an
unvalidated endpoint through ten migration phases so that this feature *might* one day use it was the
exact debt the migration exists to clear.

**What this feature owes when the `IsHome`/`SetAsHome` fix lands** (`implementation.md:175`): re-add a
*validated* `SetBuildingHome` as one more handler on `OVT_RealEstateRequestComponent` — caller resolved
via `ResolveOwningPlayerId()` (never a parameter), building resolved server-side or checked against the
caller's position, and an `IsOwner` gate. It is a one-method job on a component that already exists;
nothing was lost.

`TeleportHome` is untouched by the migration and remains a zero-caller method on this feature's manager.

---

*This context file was created retrospectively by analyzing existing code.*
