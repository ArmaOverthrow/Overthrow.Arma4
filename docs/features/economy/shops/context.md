# Shops - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (see implementation.md — 13 concrete issues catalogued)

**What's Next:**
- 📋 Highest-value fixes: server-authoritative sell, broken pagination (items unreachable in big shops), restock (lives in market)

**Blockers:**
- Shop stock persistence is blocked on the market's int resource IDs (need a resource-name-keyed format)

---

## Key Files

- `Scripts/Game/Components/Economy/OVT_ShopComponent.c` — the whole per-shop model (108 lines: type, stock map, JIP, broadcast)
- `Scripts/Game/UI/Context/OVT_ShopContext.c` — the menu (buy/sell/pagination, gun-dealer multiplier, procurement list)
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:352-593` — the server transaction block
- `Scripts/Game/Controllers/OVT_TownController.c:203-351` — gun-dealer spawn/stock/registration
- `Configs/System/ShopConfig.conf` + `GunDealerConfig.conf` — declarative inventory rules

---

## Important Decisions

- **Shops never own mutation RPCs** — everything funnels through `OVT_PlayerCommsComponent`; only the broadcast-only `RpcDo_SetInventory` lives on the shop.
- **Seven enum types, three real families:** town item shops (world-query registered), gun dealer (spawned per town), vehicle/procurement (fuel stations register; garages/helipads deliberately don't — no restock, no NPC sales, no town-stock pricing for them).
- **Interaction is on the cashier furniture prop** (child of the shop building), not the shop entity; gun dealers carry the component themselves.
- **Inventories are declarative catalog queries**, not item lists — content flows through without config edits, but stock is keyed by unstable int IDs.

---

## Gotchas & Learnings

- Buy is the pattern to copy (server recomputes price, charges only for delivered items); **sell is entirely client-trusted** — don't replicate that shape.
- The gun-dealer sell multiplier exists only in the UI — nothing server-side enforces it.
- Procurement shops open via `OVT_MainMenuContextOverrideComponent` with a string-typed context name special-cased in `OVT_MainMenuContext` — no user action involved.
- `SHOP_FOOD` is a dead enum value; `PurchaseFailedInventoryFull` is localized but never sent (empty `if` block).
- Registration string-compares `ClassName() == "SCR_DestructibleBuildingEntity"` — a vanilla rename silently unregisters every shop.

---

*This context file was created retrospectively by analyzing existing code.*
