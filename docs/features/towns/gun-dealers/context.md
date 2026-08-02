# Towns Gun Dealers - Context & Decisions

**Last Updated:** 2026-08-03
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (`/discover-feature`, 2026-08-02)
- ✅ Top findings filed as BUG-054, BUG-055, BUG-056 (plus BUG-005's four extra sites documented)

**What's Next:**
- 📋 Review for potential improvements (see implementation.md Future Enhancements)

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/Controllers/OVT_TownController.c:233-338` — `SpawnGunDealer` (position tiers, stocking, registration)
- `Scripts/Game/Configuration/OVT_GunDealerConfig.c` + `Configs/System/GunDealerConfig.conf` — inventory rules
- `Scripts/Game/UserActions/OVT_GunDealerAction.c` — opens the shop UI
- `Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_GunDealer.et` — the invulnerable dealer entity
- `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c` — registry, restock (:314-328), exclusion (:1535), JIP

## Important Decisions

- **`gunDealerPosition` doubles as location + "has dealer" boolean** — the X-only test (BUG-005) exists in five sites, including the spawner's own persisted-position reuse branch. Any fix must cover all five or add `bool hasGunDealer` to `OVT_TownData`.
- **Dealer is a shop excluded from the shop registry** — immune to NPC drain and price curves, but also outside every restock pass except the weed-only one.
- **Stock is rerolled on every save/load** — the de-facto (and only) weapon restock mechanism; position persists, inventory does not.
- **`m_bSingleRandomItem` = one random heavy weapon per category per town** — the deliberate travel mechanic, promised in the localized job text.

## Gotchas & Learnings

- The marked-house spawn tier (STATIC entity with `SHOP_GUNDEALER` shop) is dead — no such prefab ships; every dealer uses the random-unowned-house fallback.
- `m_OnPlayerTransaction` fires for **buys only** (`isBuying` hardcoded true at its single invoke site) — black-market modifiers never see sells.
- The sell-price multiplier exists **only client-side** in `OVT_ShopContext`; the server has no dealer sell logic at all (BUG-020's client-authoritative sell).
- The BUG-005 pinning test (`OVT_TEST_Logic_Jobs.c:169-198`) goes red by design when the X-axis check is fixed — update it in the same commit.
- `OVT_TownHasDealerJobCondition` (the class BUG-005/BUG-041 cite) is unused by shipped configs; the live condition is `OVT_IsNearestTownWithDealerJobCondition` with the same defect.

---

*This context file was created retrospectively by analyzing existing code.*
