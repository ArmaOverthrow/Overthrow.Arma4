# Map Location Types - Context & Decisions

**Last Updated:** 2026-08-10
**Current Phase:** Retrospective Documentation
**Status:** ⚠️ Partially Implemented — 10 types shipped, 4 parity gaps outstanding

---

## Quick Status

**What's Done:**
- ✅ Ten location types implemented and configured (Town, Base, RadioTower, House, Warehouse, Shop, FOB, Camp, Port, GunDealer)
- ✅ Per-type fast-travel policy layered over one shared service
- ✅ Retrospective documentation created
- ✅ **Parity checklist derived from the legacy component itself** rather than from the stale design note

**What's Next:**
- 🔴 Close four parity gaps: **G1** Vehicle, **G2** job waypoint, **G3** POI registry, **G4** bus-stop component
- 🔴 Bespoke info panels for the seven types that have none
- 🔴 MP/JIP verification, especially the per-player ownership types

**Blockers:**
- `map/core` should be verified first — its D6 (markers never refresh mid-open) directly determines whether a Vehicle marker can work at all, since vehicles move.

---

## Key Files

| File | Role |
|---|---|
| `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocation*.c` | The ten type subclasses |
| `Scripts/Game/UI/Map/LocationTypes/TypeInfo/OVT_ShopTypeInfo.c` | Nested config object mapping shop type → name + icon |
| `Configs/Map/OverthrowMap.conf` | All ten type entries and their attributes |
| `UI/Layouts/Map/LocationTypes/OVT_MapInfo{Town,Base,RadioTower}.layout` | The only three bespoke info panels |
| `UI/Layouts/Map/LocationTypes/Town/OVT_TownModifierWidget.layout` | Town modifier chips |
| `Scripts/Game/UI/Map/OVT_MapIcons.c` | **Legacy** — the parity checklist lives here (icon names + POI registry) |
| `Scripts/Game/Components/OVT_MainMenuContextOverrideComponent.c` | Self-registering POI source (G3), and the closest precedent for G4 |
| `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c:881` | `GetNearestBusStop` — the descriptor-based discovery G4 replaces |

---

## Important Decisions

**1. One subclass per campaign category, declared in config.**
A type is a `.c` subclass plus an `OverthrowMap.conf` entry plus (optionally) a layout. Ten types shipped without a single change to core map code, which is the pattern working as designed.

**2. Per-type eligibility gate on top of one shared travel service.**
`Base`, `Camp`, `House` and `FOB` each apply their own rule (enemy-held, private, not-your-house) and then defer to `OVT_FastTravelService.CanGlobalFastTravel`. There is exactly one implementation of the global rules — a good starting position for `map/fast-travel`.

**3. Vary appearance inside a type rather than subclassing.**
Two mechanisms exist: `Shop`'s nested `m_aShopTypes` array of `OVT_ShopTypeInfo` (config-authored, enum-keyed name + icon), and `Town`'s `m_sVillageIconName`/`m_sTownIconName`/`m_sCityIconName` attributes selected by size. Prefer the Shop pattern for new variation — it is authored in Workbench and needs no code.

**4. Bus stops become an entity-attached component (epic decision, not yet built).**
Replaces vanilla `MDT_BUSSTOP` descriptor proximity queries so a stop can be attached to anything. Two precedents exist and **G3 and G4 should be designed together** — a single "entity-attached map marker component" could serve both the POI registry and bus stops.

---

## Gotchas & Learnings

- **The stale design note is not the parity checklist.** `docs/archive/OverthrowMapSystem.md` lists nine "next steps" and mentions neither job waypoints nor the POI registry. The real checklist is `OVT_MapIcons.c`'s imageset icon names — `camp`, `gundealer`, `house`, `port`, `tower`, `vehicle`, `warehouse`, `waypoint` — plus its static POI registry. Always derive parity from the code being replaced.
- **`OVT_MapIcons.RegisterPOI` is a static called from live prefabs.** `OVT_MainMenuContextOverrideComponent` ships on garages and maintenance ramps. Deleting the legacy file breaks that call site — a compile-level dependency, not just a visual regression.
- **"Generic info panel" is a misnomer.** `UpdateInfoPanel` returns early when `m_InfoLayout` is empty (`OVT_MapLocationType.c:126-127`). The seven types without a layout contribute *nothing* to `ContentSlot` — they are not falling back to a shared renderer, they are simply blank below the header.
- **`OnLocationClicked` is unreachable** (`map/core` D7). No shipped type overrides it, so nothing is broken today — but the next type author will reasonably assume it works.
- **Three manager-access idioms coexist.** Inherited cached fields (Town, House, Warehouse, Camp, FOB); own shadowing member + lazy resolve (Base, RadioTower); local `OVT_Global.GetEconomy()` per call (Shop, Port, GunDealer). The base class caches seven managers in `Init()` and half the types ignore them.
- **`m_Vehicles` is cached on every type and used by none** — a leftover from the Vehicle type that was never written.
- **`CanFastTravel` is on a hot path** — per element, per zoom change. `House` and `Camp` do a string compare against the player's persistent ID for every marker of their kind.
- **Vehicles are the one type that needs mid-open refresh.** They move; `map/core`'s `OnLocationDataChanged()` refresh hook has no callers. G1 cannot be finished without addressing D6.

---

*This context file was created retrospectively by analyzing existing code.*
