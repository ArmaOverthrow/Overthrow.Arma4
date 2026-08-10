# Shops - Task Checklist

**Last Updated:** 2026-08-02
**Progress:** Complete (Existing Feature)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Core functionality implemented (town shops, gun dealers, vehicle/procurement shops)
- [x] ✅ Integration with existing systems (market pricing, towns, jobs, map, modifiers)
- [x] ✅ Retrospective documentation created

---

## Open Bugs (filed 2026-08-11 from `map/location-types`)

Five findings surfaced while building the map's shop and gun-dealer info panels, deliberately not fixed
there. All `linkedFeature: economy/shops` — fix via `/fix-bug <id>` or `/fix-feature economy/shops`:

- \[ \] **BUG-143** (medium) — **everything a gun dealer sells is permanently priced at the maximum scarcity markup.** No town shop stocks a weapon **and** `RegisterGunDealer` never inserts into `m_mTownShops`, so `GetTownStock` is 0 for every dealer item and the scarcity term is pinned at exactly +10% forever — no amount of buying or selling moves it. Affects the whole dealer inventory, not just weapons. **This is why the map's gun-dealer weapon carets ship behind `m_bShowWeaponCarets`, default off.**
- \[ \] **BUG-140** (medium) — three unguarded null derefs in `OVT_EconomyManagerComponent`: `GetTownStock` (`:601-611`), `GetShopByRplId` (`:435-440`), `DistanceToNearestPort` (`:846-858`). Latent until client code asks the economy manager anything positional — which map panels do.
- \[ \] **BUG-141** (medium) — `GunDealerConfig.conf:51-54` omits `m_eItemType`, which defaults to RIFLE (`2`), duplicating the rifle-ammunition rule instead of being the SNIPER_RIFLE one. **Sniper ammunition is never stocked** (confirmed: vanilla types SVD magazines `SNIPER_RIFLE`+`AMMUNITION` on their own prefabs) and rifle magazines are added twice.
- \[ \] **BUG-142** (medium) — `RegisterGunDealer` has no broadcast RPC. Dealers register at *campaign* start, so on a listen server every client connected before the host pressed Start never learns of any gun dealer.
- \[ \] **BUG-144** (low, **design question**) — a dealer's four signature weapons re-roll from the unseeded global RNG on every campaign load; nothing persists them, only `gunDealerPosition`. Three options written up (leave / seed per town / persist).

---

## Future Enhancements

See `implementation.md` Known Issues / Future Enhancements — 13 catalogued issues, headlined by the client-authoritative sell exploit, broken pagination (integer division makes items unreachable), and missing stock persistence.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
