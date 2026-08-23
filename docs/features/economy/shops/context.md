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

### Shop-config catalogue sweep (2026-08-23)

Cross-referenced the four faction `InventoryItems_EntityCatalog_*.conf` files (1.8.0.10) against
`ShopConfig.conf` + `GunDealerConfig.conf` + `itemPrices.conf` (script in scratchpad, semantics mirror
`FindInventoryItems` + `BuildResourceDatabase`). Config-only fixes:
- **Clothes** now sell `HANDWEAR` (gloves were never listed — the type post-dates the config).
- **Electronics** now sell `RADIO_BACKPACK` with `m_sFind "Radio_"` (vanilla types the FIA deployable tent as RADIO_BACKPACK; the find keeps it out).
- **General** now sells the jerrycan (`EQUIPMENT` + `"Jerrycan_"`) — it was SUPPORT_STATION-mode collateral of the BUG-098 fix.
- **Gun dealer:** `{647C9C52CE140B08}` was a typeless (=RIFLE) duplicate of the rifle-ammo rule; it now reads `SNIPER_RIFLE AMMUNITION` (SVD 10rnd sniper mag). Added one `m_bSingleRandomItem` `WEAPON_VARIANTS` roll each for RIFLE / SNIPER_RIFLE / MACHINE_GUN (M203/GP-25/suppressed/camo rifles, SVD PSO, M21 ART II, RPK-74N, PKMN were unreachable — the WEAPON rules never match `WEAPON_VARIANTS`).

Deliberately still unsold (design, not oversight): tripod/mortar parts, sandbags, barbed tape (EQUIPMENT/SUPPORT_STATION), mortar shells + ballistic tables, helicopter rocket pods and M242 boxes, the FIA tent. Play-test owed (shop stock is rolled on world init — needs a fresh campaign or the 07:00 restock).

---

*This context file was created retrospectively by analyzing existing code.*
