# Skills: Influence (Trade & Diplomacy) - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Early Overthrow Reforger port (pre-2025)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02 23:15

---

## Executive Summary

The two "influence" skills shape how the player interacts with towns and the economy. **Trade** (5 levels) discounts shop buy prices 2%→15% and grants the `Import` permission at level 1 and `IllegalImports` at level 5, gating the port import menu and its extended catalogue. **Diplomacy** (5 levels) sets the chance (20%→100%) that the "Convert supporter" action on a civilian succeeds, feeding town support — the currency behind donation income, recruitment, and patrol suppression. The skill framework itself is documented in `skills/core`; this feature covers the three effect classes, their config, and their consumers.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Trade: cheaper shopping and access to port imports (legal, then illegal)
- Diplomacy: convert civilians into town supporters more reliably

### Success Criteria
- [x] Trade discount applies to shop buy prices (server re-derives; client and server agree)
- [x] `Import`/`IllegalImports` permissions gate the port UI and its catalogue
- [x] Diplomacy chance drives supporter conversion
- [ ] Server-side authorization on the import and add-supporters RPCs (currently client-trust — see Known Issues)

---

## Current Architecture

### Key Components

| File | Role |
|---|---|
| `Scripts/Game/GameMode/Systems/SkillEffects/OVT_TradeDiscountSkillEffect.c` | `OnPlayerData`: `player.priceMultiplier = 1 - m_fDiscount` |
| `Scripts/Game/GameMode/Systems/SkillEffects/OVT_GivePermissionSkillEffect.c` | `OnPlayerData`: `player.GivePermission(m_sPermission)` (idempotent) |
| `Scripts/Game/GameMode/Systems/SkillEffects/OVT_SupportSkillEffect.c` | `OnPlayerData`: `player.diplomacy = m_fSupportChance` |
| `Configs/Player/overthrowSkills.conf` | Trade `:3-56` (discounts .02/.05/.08/.10/.15; `"Import"` @L1, `"IllegalImports"` @L5); Diplomacy `:102-147` (chance .2/.4/.6/.8/**1.0**) |
| `Scripts/Game/Data/OVT_PlayerData.c` | `priceMultiplier` (:45, default 1), `diplomacy` (:51, default **0.1**), `permissions` + `HasPermission`/`GivePermission` (:53-54, :92-101) — all DERIVED, `[NonSerialized()]` |

All three effects override only `OnPlayerData` (none use `OnPlayerSpawn`); all assign rather than accumulate, so ascending level replay leaves the highest level's value.

### Consumers — Trade discount (`priceMultiplier`)

Exactly **one read** in the codebase: `OVT_EconomyManagerComponent.GetBuyPrice(id, pos, playerId)` at `:539` — applied only when `playerId > -1`. Coverage of the buy paths:

| Path | Discounted? |
|---|---|
| Shop buys — display + purchase (`OVT_ShopContext`, `RpcAsk_Buy`), incl. gun dealer & vehicle shops | ✅ yes |
| Procurement-shop branch of `GetShopBuyPrice()` (`:589`) and its inline duplicate in `RpcAsk_BuyVehicle` | ❌ no |
| Port imports (`OVT_PortContext` display + `RpcAsk_ImportToVehicle` — raw `GetPrice(id)`) | ❌ no — the skill that unlocks importing doesn't discount it |
| Sell prices (`GetSellPrice` has no player param) | ❌ never — Trade cannot improve what a player is paid |
| Real estate (`OVT_RealEstateManagerComponent.GetBuyPrice(IEntity)`) | ❌ no (unrelated overload) |

So the localized description ("%1% Discount when buying items at shops") is literally accurate — and narrower than players likely assume.

### Consumers — Permissions

- **`Import`** — `OVT_VehicleMenuContext.Import()` `:170-189`: no permission → close + `#OVT-CannotImport` hint. The button itself is shown on port *proximity* alone (`:93-105`), unlike sibling buttons which hide when unavailable.
- **`IllegalImports`** — `OVT_PortContext.c:56-109`: with it, the catalogue is `GetAllNonOccupyingFactionItems()` (everything non-enemy, including items no shop stocks); without, only standard shop-type inventory (vehicles excluded).
- Purchase path: `OVT_PortContext.Buy()` → `RpcAsk_ImportToVehicle` (`OVT_PlayerCommsComponent.c:473-500`) spawns items into the nearest vehicle's storage.

### Consumers — Diplomacy (`diplomacy`)

Exactly **one read**: `OVT_ConvertSupporterAction.c:9` — `RandFloat01() < player.diplomacy` on a user action present on every civilian (gated: not a recruit, alive, conscious). Success → `OVT_Global.GetServer().AddSupporters(pos, 1)` → `RpcAsk_AddSupporters` → `OVT_TownManagerComponent.AddSupport(pos, 1)`. Either way `MarkAsPerformed()` burns that civilian for the client session (no retry, no cooldown; at the 0.1 default, ~90% of attempts consume a civilian for nothing).

What a supporter is worth: donation income scales with `town.support` (doubled above 75 stability), supporters are the recruitment currency (camps and garrison groups spend them 1:1 with population), and support ≥ 75 suppresses town patrols.

### Permission system (as exercised here)

Untyped strings in a `[NonSerialized()]` array; `GivePermission` is idempotent (the guard exists because effects replay); **no `RevokePermission` exists** — the only removal is wholesale `ResetSkillEffects()`. Permissions are never replicated as values: each client re-derives its own by effect replay (JIP `RplLoad` → `OnPlayerDataLoaded`; live level-ups via `RpcDo_SetPlayerSkill`). No registry/constants tie the config strings to their two consumers.

---

## Key Technical Decisions

### Decision 1: Skills communicate through derived player-record fields
**Context:** Consumers (economy, user actions, UI) shouldn't know about the skill system.
**Implementation:** Effects write scalars/permissions onto `OVT_PlayerData`; consumers read the record.
**Trade-offs:** Clean decoupling and replay-safe persistence; but each field supports exactly one writer-skill, and permission strings are compile-time-unchecked magic values.

### Decision 2: Discount applied inside `GetBuyPrice` behind an opt-in `playerId`
**Context:** Some price queries are player-less (economy simulation, town stock).
**Implementation:** `playerId > -1` opts a call site into the personal multiplier; server RPCs re-derive the price rather than trusting the client's number.
**Trade-offs:** Server-authoritative pricing for shop buys; but every call site must remember to pass the player, and several (ports, procurement, vehicles-inline) don't — inconsistently, not clearly by design.

---

## Current State

### What's Working
- Shop-buy discounts end-to-end (display and server-side charge agree)
- Port gating by both permissions; catalogue widening at Trade L5
- Supporter conversion driven by diplomacy level; replay/JIP-safe derivation

### Known Issues (verified during discovery)
1. **`RpcAsk_ImportToVehicle` has no server-side authorization at all** (`OVT_PlayerCommsComponent.c:473-500`) — permission, port proximity, and item legality are checked only in client UI. A crafted client can spawn any catalogue item (including occupying-faction gear) into a vehicle at base price. Most serious finding.
2. **`RpcAsk_AddSupporters` is client-authoritative** (`:88-92`) — the diplomacy roll happens client-side and `num` is caller-supplied; supporters are real economy. The roll belongs server-side.
3. **Diplomacy L5 = 1.0** → guaranteed conversion; combined with (2) and `AddSupport()` having no population cap, support can exceed population until the next `RecalculateSupport()` clamps it.
4. Imports ignore the Trade discount (see table) — intentional-or-miss, undocumented.
5. `OVT_ConvertSupporterAction.c:7-9` null-derefs `player` and uses `GetLocalControlledEntity()` instead of `pUserEntity` — safe today only because the action is local-effect-only.
6. Failed conversions permanently burn the civilian (client-side bool; doesn't survive despawn or apply to other clients).

### Technical Debt
See `context.md` for the full list — includes the unregistered permission strings, the missing revocation path, duplicated procurement pricing, and the `Import` button visibility inconsistency.

---

## Future Enhancements

### High Priority
- [ ] Server-side checks in `RpcAsk_ImportToVehicle`: `HasPermission("Import")`, `IllegalImports` for restricted items, port proximity, importable-item validation
- [ ] Move the diplomacy roll server-side; validate/clamp `RpcAsk_AddSupporters` (and cap `AddSupport` at population)

### Medium Priority
- [ ] Decide and document: should imports get the Trade discount? Should port UI show the player's price?
- [ ] Cap Diplomacy L5 below 1.0 (e.g. 0.75-0.9)
- [ ] Permission constants (`OVT_Permissions` or statics on `OVT_PlayerData`) + startup validation that config permission strings are known keys

### Low Priority / Nice to Have
- [ ] A sell-price multiplier as a natural Trade mid-level effect (the skill's XP source is selling, but the skill never improves sells)
- [ ] Conversion retry/cooldown design instead of permanent per-civilian burn
- [ ] Deduplicate procurement pricing (`GetShopBuyPrice` branch vs `RpcAsk_BuyVehicle` inline copy)

---

## Testing

### Current Coverage (Tier A, mutation-proven)
- `OVT_TEST_Logic_Skills_EffectsWriteOnlyTheirOwnField`: fresh defaults (`priceMultiplier` 1, `diplomacy` 0.1, empty permissions); trade 0.25 → 0.75; support 0.35 → 0.35; permission grant; cross-field spill guard (`FindSpill()`)
- `OVT_TEST_Logic_Skills_GivePermissionIsIdempotent`: triple grant → one entry; distinct second permission; negative `HasPermission`

### Testing Gaps
- No test that `priceMultiplier` reaches an actual price (`GetBuyPrice`'s multiplier branch only exercised with `playerId = -1`)
- No test of the diplomacy roll / `AddSupport`; no test pinning `"Import"`/`"IllegalImports"` as the strings the UI checks (a config typo silently disables the Trade permission line)
- No `ResetSkillEffects()`+replay test; no multi-level replay-ordering test; no config-integrity test (5 monotonic levels, non-empty permission strings)

---

## Dependencies

### Internal Dependencies
- `skills/core` — effect base class, replay/reset lifecycle, permission storage on `OVT_PlayerData`
- `OVT_EconomyManagerComponent` (pricing), `OVT_TownManagerComponent` (support), `OVT_PlayerCommsComponent` (RPC seams), port/vehicle/shop UI contexts

---

## Notes

**Retrospective Assessment:**
- The effect→record→consumer decoupling works well and both skills are replay-safe by construction
- The consistent theme of the findings is the server boundary: both influence skills' payoffs run through RPCs that don't re-check what the UI checked

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature` to begin making improvements.*
