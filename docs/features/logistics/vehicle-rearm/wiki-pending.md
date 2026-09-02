# Wiki sync pending - logistics/vehicle-rearm

**Written:** 2026-09-01 (Phase 7, help-docs-sync)
**Reason:** the `wikijs` MCP server is not configured in this session (`.mcp.json` registers only
`beast-mode-discord`, and no `mcp__wikijs__*` tool is available), so https://wiki.armaoverthrow.com
could not be searched, read or edited. Nothing was attempted against the live wiki.

Everything below is fact-checked against the tree at the same time as the in-game text, and every
claim carries the same citation as its Field Manual counterpart. The in-game Field Manual page
"Re-arming Vehicles" is the short form of this; **the two must agree on names, numbers and
behaviour.**

---

## 1. Search first

Before creating anything, run `wikijs_search_pages` for: `rearm`, `re-arm`, `helipad`, `storage`,
`ports`, `vehicles`, `helicopters`. Paths on this wiki are flat-ish (`recruits`, `difficulty`,
`custom-maps-porting-guide`), so the likely existing homes are `storage`, `ports`, `vehicles`,
`fobs`, `building`. **Update in place; do not create a near-duplicate.**

## 2. New page (only if no rearm page exists): `rearming-vehicles`

Player-facing voice, no class names or GUIDs.

> ### Re-arming Vehicles
>
> Every armed vehicle carries a Re-arm action, from an armed jeep to an APC to a helicopter
> gunship. It is one action and one full restock: every turret magazine and every rocket pod the
> vehicle has comes back full together, and there is no way to refill part of it.
>
> #### Ammunition before money
>
> A re-arm spends storage before it spends money. It looks in the vehicle's own storage first, then
> in every storage within twenty five metres that you are allowed to draw from, for the magazines
> the vehicle's weapons take, and anything it finds there is spent instead of bought. When storage
> covers the whole re-arm the action reads **Re-arm (from storage)**, costs nothing and works
> anywhere, so a crew that hauled its own ammunition is not tied to a base.
>
> #### Paying for the rest
>
> Only what storage cannot cover is bought, and the price is that same share of the full one, so
> half the magazines covered is half the bill. That bought part is the only part with a place
> attached to it: the vehicle has to be standing at a built Helipad or a built Garage at a base the
> resistance holds, or within a hundred metres of a deployed FOB. Anywhere else with something
> still to buy, the action shows blocked and names the helipad, garage or FOB it wants.
>
> #### Carrying the ammunition
>
> Vehicle weapon ammunition can be imported at a port and moved from one storage list to another,
> but it never comes out into anyone's hands: taking it out reports it as missing and it stays on
> the list. That is exactly where a re-arm looks for it, so a crate of it on the list of a truck
> parked alongside counts for as much as a crate on the vehicle's own.

Tags to send with the update (the wikijs update call needs `tags` present): `vehicles`, `combat`,
`logistics`.

## 3. Surgical edits to existing pages

### `storage` (or wherever "what has storage" lives)

Stale sentence to replace, in whatever wording that page uses:

- **Was:** an illegal or armed vehicle has no storage at all, and neither does a helicopter.
- **Now:** an illegal or armed vehicle keeps a smaller hold of a hundred items; helicopters have
  storage on the same terms; one civilian Mi-8 carries a hold with no limit at all.

Add, under "taking things out":

- Vehicle weapon ammunition cannot be taken out into an inventory. It is reported as missing and
  stays on the list, which is where a re-arm can still reach it.

### `ports`

Stale sentence to replace:

- **Was:** the vehicle needs storage of its own, so an illegal or armed one cannot trade at a port
  at all.
- **Now:** the vehicle trades out of its own storage, so what it can take in at once is whatever
  room that storage has left: three hundred items in an ordinary car, a hundred in an illegal or
  armed one, no limit in a truck.

### `vehicle-repair` (if it exists)

No change needed. Repair and re-arm are separate actions with separate rules; the repair page's
structure ranges (Ramp and Garage ~12 m, Helipad ~20 m) are about repair only and must **not** be
reused as the re-arm site radius.

---

## Citations (same set the `.st` comments carry)

| Claim | Source |
|---|---|
| Re-arm on every armed vehicle | `Prefabs/Vehicles/Core/Vehicle_Base.et:78-88` - the only prefab declaring `OVT_RearmVehicleAction`, seven-name context union |
| One full restock | `OVT_VehicleRearmUtils.PerformRearm` - every magazine to `GetMaxAmmoCount()`, every rocket pod reloaded |
| Own storage first, then nearby | `OVT_VehicleRearmUtils.CollectRearmStores`, `OVT_StorageUtils.CollectStores`/`PlayerMayDrawFrom` |
| Twenty five metres | `OVT_StorageRequestComponent.c:212` `m_fHolderRadius` default 25 |
| Free and anywhere when covered | `OVT_RearmVehicleAction.c:100-102` (cost <= 0 returns true before any site/funds test) |
| "Re-arm (from storage)" label | `OVT_RearmVehicleAction.c:123-129`, `#OVT-RearmVehicle_FromStorage` |
| Pro-rata price | `OVT_VehicleRearmRules.ProratedCost` |
| Helipad / Garage / FOB | `OVT_VehicleRearmUtils.IsAtRearmSite`; `SITE_SEARCH_RADIUS` 20, `FOB_SEARCH_RADIUS` 100, types `"Helipad"` / `"VehicleGarage"` (`OVT_VehicleRearmUtils.c:53-62`) |
| Blocked reason wording | `#OVT-Rearm_NeedsSupplyPoint` |
| Ammunition never leaves a ledger | `OVT_StorageRequestComponent.c:1884` guard + `OVT_PrefabUtils.IsItemHiddenInInventory` |
| Armed vehicle capacity 100 | `OVT_StorageRules.ResolveAutoCapacity` + `OVT_StorageComponent.c:73` `m_iArmedVehicleCapacity` |
| Helicopter storage | `Prefabs/Vehicles/Core/Helicopter_Base.et:8-12` |
| Unlimited civilian Mi-8 | `Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_civ_base.et` |

**Do not put a dollar figure on a re-arm.** The base is scaled by the difficulty's vehicle price
multiplier, so any number would be wrong at most settings.
