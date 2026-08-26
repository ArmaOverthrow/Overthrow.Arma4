# Market - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (see implementation.md — 16 concrete issues catalogued)

**What's Next:**
- 📋 Highest-value fixes: `ReplenishStock` dead code (restocking has never worked), server-side sell validation, resource-DB checksum

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c` — the 1724-line manager (resource DB, prices, simulation, wallets, registries, JIP)
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` — the client→server RPC gateway for every economy mutation
- `Configs/Pricing/itemPrices.conf` + `vehiclePrices.conf` — the price cascade (broad→narrow, later overrides earlier)
- `Scripts/Game/Persistence/Serializers/Components/OVT_EconomyManagerSerializer.c` — persists treasury + tax only; player money is in `OVT_PlayerManagerSerializer`
- `Scripts/Game/Data/OVT_PlayerData.c` — the actual wallet storage

---

## Important Decisions

- **Resource IDs are array indices** into `m_aResources`, built independently on every machine — client/server ordering must match and nothing validates it. This also blocks persisting shop stock without a name-keyed format.
- **No `RplProp`** — JIP snapshot (`RplSave`/`RplLoad`) + explicit reliable broadcast RPCs.
- **Economy is frozen when no players are online** (`CheckUpdate` early-returns) — deliberate, world state is player-presence-coupled.
- **Price cascade semantics:** later `OVT_PriceConfig` entries override earlier ones; `hidden` drops items; unmatched items fall back to Conflict supply cost (source of BUG-009).
- **Division is float in these formulas** — EnforceScript derives division mode from the target type; the suspected integer-truncation bugs were disproven by measurement (test-coverage findings).

---

## Gotchas & Learnings

- `DoAddResistanceMoney` calls the RPC body directly as a local setter, then streams — reuse pattern, not a bug.
- `OVT_Global.GetServer()` returns the game-mode comms component on the server and the local player's on clients — that's why server code can call the client-facing wrappers.
- The doc comments in the manager are Gemini-generated (`e7978e5`) and have drifted (invoker signatures) — trust the code, not the comments.
- `OVT_EconomyInfoWidgets.c` is dead; the HUD uses `FindAnyWidget("MoneyText")` directly.
- Anti-double-spend latches (`addingMoney`/`takingMoney`) can permanently deadlock a client's money ops if one RPC drops.

### Pricing prefabs outside the catalogue (2026-08-23)

New seam on the manager: `GetBuyPriceForPrefab` / `ResolvePricingResource` / `GetFallbackBasePrice`
(+ `OVT_PrefabItemClassifier` in Utilities) price a prefab that is in no faction ITEM catalogue by its
nearest registered prefab ancestor, else by component classification through the same `itemPrices.conf`
rules. It never registers anything (ids are the wire format). Consumers: the equipped-recruit loadout
quote (see `resistance/recruit-ux`), shop sell (server + client browser), port export price, port/warehouse
browse categories, HC manifest + vehicle quote, vehicle storage identity - id-needing callers use only the
ancestor route (`ResolvePricingResource`), price-only callers use `GetBuyPriceForPrefab`. `BuildResourceDatabase`'s price-config loop was factored
into `ResolveConfiguredPrice`, semantics unchanged; `GetBuyPrice` now goes through `ApplyBuyMargin`.

---

*This context file was created retrospectively by analyzing existing code.*
