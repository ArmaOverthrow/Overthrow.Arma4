# Towns Gun Dealers - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (pre-dates Beast Mode; spawn tiers reworked in `aa62369` "Improve gun dealer spawning", 2025-07-14)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-03 00:05

---

## Executive Summary

Every town, city and capital (never a village) spawns exactly one gun dealer — an invulnerable, factionless FIA civilian carrying an `OVT_ShopComponent` of type `SHOP_GUNDEALER`. The dealer sells weed (the drug-loop seed) plus a catalog-query-driven arms selection: all pistols/attachments/throwables/explosives/ammo, and **one random weapon per heavy category** (rifle, sniper, MG, launcher) per town — the mechanic that makes visiting different towns' dealers meaningful. Dealer transactions ≥ $1000 fire the paired BlackMarket stability(−5)/support(+5) modifiers. The dealer *is* a shop, but a deliberately second-class one: excluded from the town shop registry, so it escapes NPC demand drain and the town-stock price curve — and also every restock pass except its own weed-only one.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase (`/discover-feature`, 2026-08-02). The feature has already been implemented and shipped.

---

## Goals

### Primary Goals
- Give the resistance a per-town black-market arms source that anchors the early game (`findGunDealer` tutorial job) and the drug-money loop.
- Make dealer inventory data-driven (catalog queries, not item lists) so new/DLC weapons appear without config edits.
- Encourage travel: each town's dealer stocks a different random heavy weapon per category.

### Success Criteria
- [x] One dealer per non-village town, spawned at campaign start/continue, position persisted
- [x] Config-driven inventory (`GunDealerConfig.conf`: 14 catalog rules + weed prefab)
- [x] Shop UI reuse with a difficulty-scaled sell-price multiplier
- [x] Black-market stability/support modifier tie-in on big purchases
- [ ] Weapon/ammo stock ever restocking (only weed restocks — see Known Issues)
- [ ] Dealer icon reliably visible on the map for never-streamed dealers

---

## Current Architecture

### Key Components

| Area | Files | Role |
|---|---|---|
| Spawning owner | `Scripts/Game/Controllers/OVT_TownController.c` (351 L) | `ActivateTown()` → `CallLater(SpawnGunDealer, 0)` for non-villages; 3-tier position selection; stocking; faction neutralisation; economy registration |
| Config class | `Scripts/Game/Configuration/OVT_GunDealerConfig.c` (9 L) | Two arrays: `m_aGunDealerItems` (catalog queries) + `m_aGunDealerItemPrefabs` (explicit prefabs w/ cost+stock) |
| Shipped config | `Configs/System/GunDealerConfig.conf` (74 L) | 14 catalog rules + weed (`cost 75`, stock 25–100) |
| User action | `Scripts/Game/UserActions/OVT_GunDealerAction.c` (69 L) | Opens `OVT_ShopContext`; local-only (`HasLocalEffectOnlyScript`); ~95% duplicate of `OVT_ShopAction` |
| Dealer entity | `Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_GunDealer.et` (78 L) | Civilian FIA character, `EnableDamage 0` (invulnerable), faction `""`, `OVT_ShopComponent { SHOP_GUNDEALER }`, action `#OVT-GunDealer` |
| Economy seam | `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c` | `m_GunDealerConfig` (:58), registry `m_aGunDealers` (`array<RplId>`, :68), `RegisterGunDealer` (:1005), weed restock (:314-328), prefab price seeding (:1225), shop-registry exclusion (:1535), JIP (:1612/:1655) |
| Town data | `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c:34` | `OVT_TownData.gunDealerPosition` — the *only* "town has a dealer" record (position doubles as boolean) |
| Job content | `Configs/Jobs/findGunDealer.conf` + `OVT_IsNearestTownWithDealerJobCondition` + `OVT_GetDealerLocationJobStage` | Tutorial job (jobIndex 2, $50 + 5 XP); `OVT_TownHasDealerJobCondition` is authored but unused by any shipped config (tests + BUG-005 target it) |
| Modifiers | `Scripts/Game/GameMode/Systems/Modifiers/{Stability,Support}/OVT_BlackMarket*.c` | `m_OnPlayerTransaction`, gated `SHOP_GUNDEALER` + `amount >= 1000`; conf: −5 stab / +5 sup, timeout 3600, stackLimit 3 |
| Map | `Scripts/Game/UI/Map/OVT_MapIcons.c` | One `gundealer` icon per registered RplId, with retry/fallback machinery for unstreamed entities |
| Difficulty | `OVT_DifficultySettings.c:72` `gunDealerSellPriceMultiplier` | Easy 0.8 / Normal 0.5 (default) / Hard 0.3 / Extreme 0.2; JSON-overridable; JIP-replicated; persisted by config serializer |
| Persistence | `Scripts/Game/Persistence/Serializers/Components/OVT_TownManagerSerializer.c:27,116,153` | Persists `gunDealerPosition` only — entity + stock are rebuilt/rerolled each session |

### Spawn flow (server, campaign start or continue)

```
OVT_OverthrowGameMode.DoStartGame()
  → TownManager.PostGameStart() → foreach controller: ActivateTown()
      → if (size != VILLAGE) CallLater(SpawnGunDealer, 0)          // TownController:102
SpawnGunDealer():                                                   // TownController:233-338
  1. persisted position reuse   (gunDealerPosition[0] != 0 — X-only check, BUG-005 family)
  2. marked house               (STATIC entity w/ SHOP_GUNDEALER shop — DEAD: no such prefab ships)
  3. random unowned house       (manager's GetRandomUnownedHouseInTown — unguarded GetRandomElement)
  → SpawnEntityPrefab(m_pGunDealerPrefab)  → shop.m_iTownId = townID
  → stock: weed roll + 14 catalog rules (m_bSingleRandomItem = one random pick per heavy category;
           quantity = RandFloatXY(1, GetTownMaxStock(...)) so bigger towns stock deeper)
  → CallLater(SetDealerFaction, 500)  // faction "" so neither side shoots them
  → m_Economy.RegisterGunDealer(id)   // RplId into m_aGunDealers
```

### The dealer is a shop — but excluded from the registry

`FilterShopEntities` (`OVT_EconomyManagerComponent.c:1530-1537`) excludes `SHOP_GUNDEALER` (and characters can't pass the `SCR_DestructibleBuildingEntity` class test anyway). Consequences: not in `m_aAllShops`/`m_mTownShops`, so no `InitShopInventory`, no NPC demand drain, no contribution to `GetTownStock` (dealer stock never moves prices, and its own scarcity term reads unrelated town-shop stock). But `m_iTownId` **is** set, which is what lets the BlackMarket modifiers resolve the town.

### Pricing

Buy price = the normal shop pipeline (`GetShopBuyPrice` has no dealer branch); the Trade skill's `priceMultiplier` applies. The only dealer-specific pricing is `gunDealerSellPriceMultiplier`, applied **exclusively client-side in the UI** (`OVT_ShopContext.c:63-75, 227-230, 304-307`) — sell-button hiding at 0, displayed price, credited price. Server-side there is no dealer sell logic at all (sell is client-authoritative — BUG-020).

### Networking

- **Registry JIP:** `m_aGunDealers` count+ids in economy `RplSave:1612`/`RplLoad:1655` (append without clear; no runtime registration RPC).
- **Entity/stock:** normal engine streaming; stock JIP via `OVT_ShopComponent.RplSave/RplLoad`; runtime deltas via server `RpcAsk_*` → broadcast `RpcDo_SetInventory` per item.
- **`gunDealerPosition` is NOT in the town JIP payload** (`OVT_TownManagerComponent.RplSave:1299-1344` omits it, as it does `areaHeat`) — clients read `vector.Zero` for every town. Survivable only because every current reader is server-side; the player-facing location travels as the replicated `OVT_Job.location`.

### Persistence

Only `gunDealerPosition` (town serializer) and the sell multiplier (config serializer) survive. The entity, its stock, and the registry are rebuilt every session: `ApplyPersistedTowns` restores the position, `SpawnGunDealer` reuses it (tier 1) and **re-rolls all stock**. Net effect: save/load is a full dealer-inventory reroll — the only way weapon stock ever comes back, and the per-town random heavy weapons change on every continue.

---

## Key Technical Decisions

### Decision 1: Spawned entity, not map-placed
**Context:** Per-town randomised stock and location can't ride a placed prefab.
**Implementation:** `SpawnEntityPrefab` at `ActivateTown` time; existence is derived state (`gunDealerPosition`), rebuilt each session.
**Trade-offs:** Simple persistence (one vector), free stock refresh on load; but no designer placement (the marked-house tier that would allow it is dead code) and save/load rerolls are an unearned restock.

### Decision 2: `gunDealerPosition` doubles as location cache and "has a dealer" boolean
**Implementation:** Zero-X test in five sites (`OVT_TownHasDealerJobCondition.c:5`, `OVT_IsNearestTownWithDealerJobCondition.c:7,18`, `OVT_GetDealerLocationJobStage.c:6`, and the spawner's own reuse branch `OVT_TownController.c:237`).
**Trade-offs:** One field, no extra replication; but X-only comparison is the BUG-005 family, and an explicit `bool hasGunDealer` would kill the whole class of defects.

### Decision 3: Catalog queries + `m_bSingleRandomItem` as the travel mechanic
**Implementation:** `FindInventoryItems(type, mode, search)` per rule; heavy categories pick one random survivor (documented to players in the job description: "one random gun of each type, so try different towns").
**Trade-offs:** DLC-proof and genuinely good design; but quantity/pick rolls have an off-by-one (`RandInt(0, Count()-1)` half-open — last entry unobtainable), and rule #12 in the conf duplicates #7 (double-stocked generic ammo; probably meant sniper ammo).

### Decision 4: Second-class shop (registry exclusion)
**Trade-offs:** Immunity from NPC drain and the price curve at the cost of losing every shared restock pass; the weed-only `ReplenishStock` branch (`:314-328`, comment says "Item prefabs only ie weed") was a conscious scope limit that contradicts the localized promise "will sell all ammunition".

### Decision 5: Invulnerable rather than protected by logic
**Implementation:** `EnableDamage 0` on the prefab; 500 ms deferred `SetAffiliatedFactionByKey("")`.
**Trade-offs:** No respawn/cleanup path needed (`m_GunDealerID` is never re-checked); the magic 500 ms timer works around component init order.

---

## Current State

### What's Working
- One dealer per non-village town, position stable across saves, tutorial job wiring, map icon for streamed dealers
- Catalog stocking incl. per-town random heavy weapons; town-size-scaled stock depth
- Black-market modifier pair on ≥$1000 purchases; StrongEconomy correctly excludes dealers
- Difficulty-scaled sell multiplier incl. JSON override, JIP and persistence

### Known Issues (filed)
- **BUG-005** (open, pinned by a deliberate test): X-axis-only dealer check — and discovery found the same defect in **four more sites**, including the spawner's position-reuse branch (a persisted dealer at X=0 gets re-placed on continue, or silently lost)
- **BUG-020**: dealer sell is client-authoritative; the sell multiplier exists only client-side; `m_OnPlayerTransaction` fires for buys only, so fencing loot produces zero black-market heat
- **BUG-024**: shop pagination integer division — dealers are the worst case (catalog stocking exceeds 15 types; items become unreachable in the UI)
- **BUG-054**: dealer weapon/ammo stock never restocks (only weed does); only recovery is the save/load reroll
- **BUG-055**: `GetRandomUnownedHouseInTown` calls `GetRandomElement()` on a possibly-empty array — a fully player-owned town crashes `SpawnGunDealer` at campaign start
- **BUG-056**: `RandInt(0, Count()-1)` half-open off-by-one — the last catalog entry per single-random rule is unobtainable at every dealer

### Technical Debt (unfiled)
- Marked-house spawn tier (tier 2) is dead code — no static prefab sets `SHOP_GUNDEALER`; designers cannot place dealers
- Radius mismatch: dealer house search uses controller `m_iTownRange` (800) but the fallback uses the manager's deprecated 400 m range
- No null checks on the spawned dealer entity/shop/`FindEntityByID` in `SpawnGunDealer`/`SetDealerFaction`
- `m_bIncludeOtherFactionItems` semantics inverted (drops the whole rule, not just third-faction items; masked by defaults; duplicated in `InitShopInventory`)
- `gunDealerPosition` absent from town JIP payload with no marker saying "server-only" — a landmine for any future client-side reader
- Map icon unreachable for never-streamed dealers (fallback cache only fills on the success path)
- `m_aGunDealers` never cleaned; JIP `RplLoad` appends without clearing
- No `Replication.IsServer()` guard anywhere in the spawn chain (server-only by call-site convention)
- `OVT_GunDealerAction` ≈ `OVT_ShopAction` verbatim duplicate; `GetInventoryId` typo → silent resource id 0; `m_ShopType` declared `int` not enum

---

## Future Enhancements

### High Priority
- [ ] Restock the catalog items (BUG-054) — or intentionally embrace the scarcity and fix the job description
- [ ] Fix the X-only check in all five sites, or add `bool hasGunDealer` to `OVT_TownData` (retire BUG-005 + update its pinning test in the same commit)
- [ ] Guard `GetRandomUnownedHouseInTown` (BUG-055) — one-line fix, campaign-start crash class

### Medium Priority
- [ ] `RandInt` off-by-one (BUG-056) and the duplicate ammo rule (#12 → sniper ammo)
- [ ] Server-side sell path with the dealer multiplier (rides the economy epic's BUG-020 fix); fire `m_OnPlayerTransaction` on sells
- [ ] Map-icon fallback for never-streamed dealers (position could ride the registry JIP)

### Low Priority / Nice to Have
- [ ] Revive the marked-house tier (ship a `SHOP_GUNDEALER` static prefab) for designer-placed dealers
- [ ] Merge `OVT_GunDealerAction`/`OVT_ShopAction`; enum-type `m_ShopType`; remove the 500 ms magic delay

---

## Testing

### Current Coverage
- `OVT_TEST_Logic_Jobs.c:123-152` — `TownHasDealer` set/unset (world-free)
- `OVT_TEST_Logic_Jobs.c:169-198` — **deliberately pins BUG-005** (goes red when fixed; update with the fix)
- `OVT_TEST_Logic_Town.c:455-520` — `CopyFrom` copies `gunDealerPosition`, not location/size
- `OVT_TEST_CampaignSuite.c:139-215` — town activation uses `gunDealerPosition != vector.Zero` as the observable for the whole activation chain

### Testing Gaps
- The spawn fallback chain (all 3 tiers + failure path), stocking rolls, faction filters
- Registry JIP round trip; weed restock branch; sell-multiplier application; black-market threshold/gating
- Save/load reroll behaviour; map-icon retry/fallback

---

## Dependencies

### Internal Dependencies
- **towns/core**: `OVT_TownData.gunDealerPosition`, town controllers/activation, `GetRandomUnownedHouseInTown`, `GetTownMaxStock` inputs (population/stability)
- **economy (epic)**: shop component/context, resource DB + pricing pipeline, registry + restock clock, `FindInventoryItems`
- **towns/stability + towns/support**: BlackMarket modifier pair (conf-registered, transaction-event-driven)
- **jobs (epic)**: `findGunDealer` tutorial job + dealer conditions/stage
- **skills (epic)**: Trade `priceMultiplier` on buys
- **core/persistence**: town serializer carries the position; config serializer carries the multiplier

### External Dependencies
- Base-game character/faction/damage components; `SCR_DestructibleBuildingEntity` class test in the registry filter

---

## Notes

**Discovered Information:**
- The dealer's "one random heavy weapon per town" is the deliberate travel mechanic, promised verbatim in the localized job text
- Save/load reroll is the de-facto restock mechanism — undocumented and probably unintended, but currently the only one
- Commit `5a6acce` added weed restocking only; `aa62369` added the (dead) marked-house tier

**Retrospective Assessment:**
- The catalog-query inventory design is the strongest part — data-driven, DLC-proof, and the single-random mechanic is good game design
- The weak layer is lifecycle: position-as-boolean (BUG-005 family), no restock, no respawn/cleanup, unguarded fallbacks — all small fixes clustered in `SpawnGunDealer`
- The sell-side authority gap is an economy-epic problem (BUG-020) that the dealer multiplier makes strictly worse

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature towns/gun-dealers` to begin making improvements.*
