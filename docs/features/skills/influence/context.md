# Skills: Influence (Trade & Diplomacy) - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements (see implementation.md Known Issues — two client-authority holes are the headline)

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Systems/SkillEffects/OVT_TradeDiscountSkillEffect.c`, `OVT_GivePermissionSkillEffect.c`, `OVT_SupportSkillEffect.c` — the three effects (all `OnPlayerData`-only, all assign)
- `Configs/Player/overthrowSkills.conf` — Trade `:3-56`, Diplomacy `:102-147`
- `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c:531-544` — `GetBuyPrice`, the single `priceMultiplier` read
- `Scripts/Game/UserActions/OVT_ConvertSupporterAction.c` — the single `diplomacy` read
- `Scripts/Game/UI/Context/OVT_VehicleMenuContext.c:170-189` (`Import` check), `OVT_PortContext.c:56-109` (`IllegalImports` catalogue split)
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:83-92` (`RpcAsk_AddSupporters`), `:473-500` (`RpcAsk_ImportToVehicle`)
- `Scripts/Game/Data/OVT_PlayerData.c:43-54, 92-101` — fields + permission API

---

## Important Decisions

- **Effect → derived-record-field → consumer decoupling.** Consumers never know skills exist; they read `priceMultiplier` / `diplomacy` / `permissions` off `OVT_PlayerData`. One writer-skill per field — nothing enforces that, so don't add a second skill writing the same field.
- **Discount is opt-in per call site** via `GetBuyPrice(..., playerId)`; server RPCs re-derive prices rather than trusting client numbers. Call sites that don't pass a player (ports, procurement) silently skip the discount.
- **Permissions re-derive, never replicate.** Each client rebuilds its own permission list via effect replay; there is no revocation API (`ResetSkillEffects()` wholesale-clears is the only removal).

---

## Gotchas & Learnings

- **`RpcAsk_ImportToVehicle` validates nothing server-side** — no permission, proximity, or item check. The permission gate lives entirely in client UI. Fix here, not in the UI.
- **The diplomacy roll is client-side** and `RpcAsk_AddSupporters` accepts caller-supplied `num` unconditionally; `AddSupport()` has no population cap (contrast `TakeSupportersFromNearestTown`, which does guard).
- Trade discount reality vs. player expectation: shop buys only. Not imports (the thing Trade unlocks!), not procurement, not vehicles-via-procurement, not sells, not real estate.
- `"Import"` / `"IllegalImports"` are magic strings shared by one `.conf` and two `.c` files with no constant — a config typo disables the whole permission line silently (no test would catch it).
- Diplomacy default is **0.1** (not 0) — an unskilled player can convert, at 10%; a failed attempt burns the civilian permanently (client-session bool).
- Diplomacy L5 is **1.0** — guaranteed conversion, unbounded supporter farming.
- `OVT_ConvertSupporterAction` derives the player from `GetLocalControlledEntity()` (not `pUserEntity`) with no null check — safe only while `HasLocalEffectOnlyScript()` is true.
- The `Import` button shows on proximity alone and fails with a hint — every sibling button hides itself instead; likely an oversight, not a pattern.
- Procurement pricing exists twice (branch in `GetShopBuyPrice` + inline copy in `RpcAsk_BuyVehicle`) — keep in sync or deduplicate.
- Replay order (ascending levels, assign semantics) is what makes L1+L5's double permission grant and rising discounts land correctly — see `skills/core` for the contract.

---

*This context file was created retrospectively by analyzing existing code.*
