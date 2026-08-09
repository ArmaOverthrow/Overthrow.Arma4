# Towns Stability - Context & Decisions

**Last Updated:** 2026-08-03
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (`/discover-feature`, 2026-08-02)
- ✅ Top findings filed as BUG-057, BUG-058, BUG-059 (BUG-060 covers the RPC surface under towns/core)

**What's Next:**
- 📋 Review for potential improvements (see implementation.md Future Enhancements)

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Systems/OVT_TownModifierSystem.c` — the shared framework base (this feature owns its docs; towns/support consumes it)
- `Scripts/Game/GameMode/Systems/Modifiers/OVT_ModifierConfig.c` + `OVT_Modifier.c` — config entry + handler base
- `Scripts/Game/GameMode/Systems/Modifiers/OVT_TownStabilityModifierSystem.c` + `Stability/*.c` — the stability flavour + 6 handlers
- `Configs/Modifiers/stabilityModifiers.conf` — 11 entries; **index order is the persisted/replicated id**
- `OVT_TownManagerComponent.c` — owner/transport (see towns/core)

## Important Decisions

- **Stability = `round(clamp(100 + Σ effects, 0, 100))`, always recomputed** — never mutate `town.stability` directly; express every cause as a modifier. The serializer re-derives it on load (saved int is a fallback only).
- **Config index = modifier identity** in saves, RPCs and client UI. `stabilityModifiers.conf` is append-only; reordering remaps every existing save (load drops out-of-range ids with a warning, but *valid wrong* ids are silently accepted).
- **Timeout unit is real seconds** (tick decrements 10 per 10 s); `timeout <= 0` = permanent, never expires, `OnActiveTick` unreachable. PatrolHarassment alone uses the in-game clock for its gate.
- **Positive modifiers are repair, not bonus** — base 100 with a 100 cap means StrongEconomy/MedicalSupplies only show while negatives are active.

## Gotchas & Learnings

- **The tick rebuild drops permanent modifiers** (BUG-057): `timeout <= 0` entries are `continue`d past the rebuild list, so any *other* expiry wipes them — and no remove RPC fires, so client chips desync. Any new permanent modifier inherits this until fixed.
- **Handlers are single shared instances** — `OnTick(town)` is called per town on one object. Never store per-town state in handler fields (BUG-058's `m_iLastCheckedHour`).
- **Handler `OnStart` never runs** (BUG-059): `system.Init()` precedes town discovery. Don't rely on `OnStart` for per-town seeding until the ordering is fixed.
- Omitted `.conf` attributes take `[Attribute()]` defaults — DrugProblems' effect (−5) and five entries' non-stackability are accidents of omission; `flags`' ACTIVE bit is inert (checks commented out).
- `m_OnPlayerTransaction` fires on buys and (since `e82b892`) sells; both handlers ignore `isBuying`, so sells now trigger them too — honour the flag when touching these.
- Logic tests build modifiers with `new` + fixture factories because `new` does not apply `[Attribute()]` defaults — a recorded house rule.
- Persistence tests pick "the first negative-effect modifier" = idx 0 RecentGunfire (the dead entry) — deleting it from the conf would shift every id *and* break that finder.

---

*This context file was created retrospectively by analyzing existing code.*
