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

---

## Change 2026-08-25 — mortars/tripods priced, and two catch-alls that had never worked

User report: mortars were importable at the port for **$50 a part** (three parts = $150 a mortar), and
"they dont have a price config atm".

**Why $50.** `Configs/Pricing/itemPrices.conf`'s very first entry hides item type **`MORTARS`**, mode
`SUPPORT_STATION`. But the CARRYABLE parts are typed **`EQUIPMENT`/`SUPPORT_STATION`** in the vanilla
entity catalogs — `MORTARS` is only ever the assembled support-station entry — so that rule has never
matched a mortar part. They fell through to the bare `EQUIPMENT` catch-all, whose `cost` attribute
DEFAULTS to 50. Nobody authored $50; it is the attribute default showing through.

**Priced by PATH, not by type+mode.** `ResolveConfiguredPrice` ignores type and mode entirely when
`m_sFind` is set, and `EQUIPMENT`/`SUPPORT_STATION` also covers jerrycans, sandbags, barbed tape and
repair/rearming kits — so a type+mode rule would have swept those up too.

| `m_sFind` | cost | effect |
|---|---|---|
| `Items/Equipment/Mortars/` | 4000 | all six parts — **$12,000** per working mortar |
| `Items/Equipment/Tripods/` | 2500 | NSV/M2/M60/PKM gun + tripod parts — **$5,000** per HMG |

Mortar SHELLS appear in no inventory catalog, so there is no cheap-ammo hole behind this.

**🔴 The second defect, found on the way: `ResolveConfiguredPrice` has NO `break`, so the LAST matching
rule wins.** That is deliberate — the `cost` attribute's own description is *"will override any above
this one"* — which makes a bare rule (no `m_sFind`, matching every item of its type) a **position**
bug wherever it sits below the specific rules it is meant to back. Two did:

| catch-all | was resetting | authored | was actually |
|---|---|---|---|
| `EQUIPMENT` (cost defaulted to 50) | `Binoculars_` / `Radio_` / `Watch_` | 100 / 75 / 45 | 50 / 50 / 50 |
| `HEAL` cost 5 | `MorphineInjection_` / `SalineBag_` / `Tourniquet_` | 10 / 8 / 6 | 5 / 5 / 5 |

Both moved to the TOP of `m_aPrices`. Six prices change as a result — that is a real gameplay change
and it was the user's call, not a silent tidy-up. `hidden` is unaffected either way: a hidden rule
`return`s the moment it matches, so its position never mattered.

**Verified by simulating `ResolveConfiguredPrice` against the real file** (mortar 4000, tripod 2500,
binoculars 100, radio 75, watch 45, flashlight 15, morphine 10, saline 8, tourniquet 6, bandage 5,
medical kit HIDDEN, repair kit HIDDEN, jerrycan/sandbag untouched at 50). Config-only change, so there
is nothing for `compile-check.sh` to say and no `.st` key was added.

**⚠ NO COMMENT WAS LEFT IN THE .conf.** A `//` line was written and then removed: no Overthrow config
and no vanilla config contains one, so there is no evidence the BaseContainer parser tolerates them and
a Workbench save could eat the file. The ordering rule is documented here instead — **a new bare rule
must go at the top of `m_aPrices`, not the bottom.**

**Owed:** eyeball a port screen after deploy — mortar rows should read $4,000, and binoculars $100.

---

## 2026-08-25 — The map garage sold only legal vehicles; the buildable one sells everything

**Author:** *"in a garage on the map (not built) inside a base the procurement screen is showing a BRDM-2 ... also the rest of the vehicles are only legal ones, a garage is supposed to have all vehicles apart from OF ones. check the configs on the garage deltas we have compared to our buildable garage to see whats missing"* — then, on the BRDM: *"it might also be in the FIA configs which is fine, FIA vehicles are allowed"*, and *"I dont see non-occupying faction illegal vehicles like I would if I built one."*

**The script was never the problem.** `OVT_ShopContext.CollectProcurementVehicles` already asks for `GetAllNonOccupyingFactionVehicles(vehicles, true)` — includeIllegal **true**. It then filters that list by *the parking types this site actually has*, and that is where the two garages diverge:

| Prefab | Parking spot types |
|---|---|
| `Garage_E_02.et` (buildable) | CAR, LIGHT, CAR, **TRUCK**, **HEAVY** |
| `GarageMilitary_E_01_base.et` (on the map) | CAR, CAR, CAR, CAR |

And in `vehiclePrices.conf`, **every** armed/illegal vehicle is authored `PARKING_LIGHT` or `PARKING_HEAVY` (`M151A2_M2HB`, `M1025_armed`, `UAZ469_UK59`, `UAZ469_PKM`, `BTR70`, `LAV25`). A garage with only `PARKING_CAR` bays therefore filters out the entire illegal catalogue — exactly the reported symptom, and nothing to do with legality or faction logic.

**Fix:** the map garage's four bays are retyped in place to CAR / LIGHT / HEAVY / TRUCK, matching the buildable's *coverage*. Retyped rather than added to, deliberately: adding bays would mean inventing offsets that cannot be verified outside Workbench, and coverage is what governs the shop list. One consequence worth knowing: that garage now parks one of each class instead of four cars.

**The BRDM-2 was two separate things, and only one was a bug.**
- Faction: **not a bug.** `BRDM2_FIA.et` is in the FIA catalog and `BRDM2.et` in the USSR one — different prefabs. The player saw the FIA variant, which is allowed. `ItemIsFromFaction` was working.
- Parking: **a bug.** `m_sFind "BRDM2"` authored no `parking`, and `OVT_VehiclePriceConfig.parking` defvalues to `0` = `PARKING_CAR` — so an armoured car was classed as a car and appeared in a car-only garage. Now `PARKING_HEAVY`, consistent with the other armour (`BTR70`, `LAV25`). ⚠ Safe for storage: `OVT_StorageRules.ResolveAutoCapacity` only branches on `PARKING_TRUCK`, so CAR→HEAVY changes no capacity.

⚠ **Twelve more entries silently inherit `PARKING_CAR` from that same defvalue** (`M1025.et`, `M997`, `M998_`, `M151A2`, `S1203`, `UAZ452`, `UAZ469`, `S105_`, and the `_Conflict`/`_Arsenal`/`_engineer`/`_command`/`_arsenal`/`_ammo`/`_repair` suffix rules). For the civilian cars that is correct; the list is recorded here so the defvalue is not mistaken for an authored decision next time.

⚠ **Only these two prefabs have `m_bProcurement 1`** — verified by sweeping every prefab carrying an `OVT_ParkingComponent`. The sheds, fuel station and village houses have parking but no shop, so they are unaffected.

`tools/compile-check.sh` exit 0 (6349 files). Workbench load + play-test owed: confirm the four bays read back correctly and that the illegal vehicles now list.
