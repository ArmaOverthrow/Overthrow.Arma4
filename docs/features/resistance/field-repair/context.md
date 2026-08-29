# Field Repair — Context & Decisions

**Epic:** resistance
**Last Updated:** 2026-08-29
**Current Phase:** All 7 phases built — Ready for Review
**Status:** 🟢 Ready for Review (play-test + suite run owed)

---

## Quick Status

**What's Done:**
- ✅ Requirements written (user, 2026-08-29)
- ✅ Implementation plan complete — 7 phases, 14 decisions (D1–D14), 14 risks (R1–R14)
- ✅ Feature scaffolded: `tasks.md`, this file
- ✅ **Phase 1 complete (4/4).** The modded class is written and compile-check is 0. Task 1.4 **overturned the plan's premise** and let Phase 5 shrink — see F2 below. Feature is now **37 tasks**.

**What's Next:**
- 🔄 **Phase 2** — un-hide `RepairKit_` (2.1 **first**), then stock it at general stores + gun dealers
- 📋 Phase 3 — a wrench in the starting car
- 📋 Phase 4 — widen the ramp zone to 12 m

**Blockers:**
- ⚠️ **The Reforger autotest suites cannot be run this session.** Claude Code's auto-mode classifier refuses `tools/run-tests.sh` (both `tools/run-tests.sh …` and `bash tools/run-tests.sh …`). `tools/compile-check.sh` and `tools/check-shop-coverage.py` both work and are being used as the gates. **The user must run the suites, or add a Bash permission rule.** Not blocking implementation.
- ~~Phase 5 blocked on task 1.4~~ — **unblocked**, see F2.

---

## The one-sentence version

Reforger already ships complete field repair — every vehicle including helicopters carries
`SCR_RepairAtSupportStationAction`, and the vanilla wrench is the gadget those actions want. Overthrow
switched it off twice by accident: the wrench charges **supplies** (which Overthrow has none of), and
`itemPrices.conf` marks `RepairKit_` **`hidden 1`**, which drops it from the resource database
entirely so no shop can ever list it. Half A fixes both. Half B turns three resistance structures into
100%-repair zones.

---

## Key Files

### Core Implementation
- `Scripts/Game/Components/SupportStation/Modded/SCR_RepairSupportStationComponent.c` — **(new)** the whole of D1: one `override AreSuppliesEnabled() → false`
- `Configs/Pricing/itemPrices.conf:37-41` — rule `{65CCF4EB3DDBBBD0}`; `hidden 1` → `cost 150`
- `Configs/System/ShopConfig.conf` — new `SHOP_GENERAL` rule `{6A9F1E4A00000001}`
- `Configs/System/GunDealerConfig.conf` — new rule `{6A9F1E4A00000002}`
- `Scripts/Game/GameMode/Managers/OVT_VehicleManagerComponent.c:210-239` — `SpawnStartingCar()`, the wrench insertion point
- `Prefabs/Structures/Military/FOB/OVT_VehicleMaintenanceRamp.et` — range 4.5 → 12, as a **delta** on GUID `{5EA88835DBD208B7}`
- `Prefabs/Structures/Military/FOB/OVT_Helipad.et` — **(new)** `{6A9F1E4A00000010}`, range 20
- `Prefabs/Structures/Military/FOB/OVT_Garage.et` — **(new)** `{6A9F1E4A00000011}`, range 12
- `Configs/Resistance/buildables.conf` — repoint Helipad `{5EE4FAB564EA1AA1}` and Garage `{5D98AC7AC353F540}`
- `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_FieldRepairEconomy.c` — **(new)**

### Related / read-only
- `Prefabs/Structures/Military/FOB/OVT_FuelDepot.et` — **the wrapper pattern to copy** (inheritance, components on the root)
- `Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c:389-405` — `ApplySupportStationState()` walks owner + one level of children only
- `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_ShopCivilianStock.c` — the BUG-098 guard and the house style for Phase 6
- `tools/check-shop-coverage.py` — Phase 2's real gate
- `Configs/Systems/Persistence/Overthrow.conf:195-216` — the `OVT_BuildableComponent` / `SelfSpawn 1` buildable rule

---

## Important Decisions

The full set (D1–D14) lives in `implementation.md` — "Key Technical Decisions". The four that shape
day-to-day work:

### D1: One modded class, scoped to repair stations
`AreSuppliesEnabled() → false` on `SCR_RepairSupportStationComponent` covers the handheld wrench, the
vehicle-mounted repair box and the industrial ramps in one file, with no prefab deltas. Rearm, medical
and fuel stations are **different classes** and keep vanilla behaviour — which is what makes the
"world fuel pump still costs money" play-test step a meaningful leak check. Rejected: a same-GUID
delta on the wrench (would leave repair trucks broken) and disabling `SUPPLIES` game-mode-wide (blast
radius = the entire resource system).

### D2: Keep the vanilla 50% cap on the handheld wrench
`RepairKit_01_base.et` sets `m_fMaxHealScaled 0.5`. **No config change implements this decision — the
work is to not touch it.** Field repair makes a vehicle usable, never good; the cap is what makes the
repair structures worth building. A later contributor is likely to read it as a bug (R14), which is
why DoD criterion 2 asserts it as a *requirement*.

### D10: Inherit, do not child-embed, in the new wrappers
The ramp embeds the vanilla ramp as a child; `OVT_FuelDepot.et` inherits and puts everything on the
root. **Follow the fuel depot.** Concrete reason: `ApplySupportStationState()` walks the owner and one
level of children, so a station on the root with a destruction component on a child would never be
switched off on ruin.

### D11: The 50%-vs-100% fallback needs no code
`GetClosestValidSupportStation` asks the station manager first and only falls back to the held gadget
when the manager found nothing (`PrioritizeHeldGadget()` is false for repair). Zones register `HIGH`
with full heal; the wrench is `LOW` with `m_fRange -1` and never registers. Inside a zone → 100%,
outside → 50%. **Do not write fallback logic.**

---

## Gotchas & Learnings

### 1. A `hidden` price rule is a silent, total failure
`ResolveConfiguredPrice` returns immediately on a `hidden` match, and `BuildResourceDatabase` then
`continue`s at `:1738` **before** inserting the item into `m_aResources` / `m_aResourceIndex` /
`m_aEntityCatalogEntries`. An item with no resource id cannot be priced, stocked, listed, bought or
sold — and nothing is logged. **Lesson:** verify the un-hide *independently* of the shop rules.
"The wrench appears in a shop" confirms both halves; "it doesn't" tells you nothing about which broke.

### 2. Prefab deltas must re-declare the inherited GUID
A same-GUID component entry is a **delta**; a fresh GUID authors a **second** component. This has cost
this project a full session before (a duplicate `ActionsManagerComponent` made a later component
unreachable), and `compile-check.sh` cannot see it. Task 4.1 re-declares `{5EA88835DBD208B7}` and its
base template path for exactly this reason (R6).

### 3. A supply-starved zone would suppress the wrench fallback entirely
`GetClosestValidSupportStation:122` skips the held-gadget branch when
`reasonInvalid == NO_SUPPLIES` — so a zone in range but out of supplies leaves the player with **no**
repair at all, which is worse than no zone. Two independent guards: D1's override, and task 5.3's
rule that neither new wrapper carries an `SCR_ResourceComponent` (R7).

### 4. `m_bIncludeSupportStationItems` cuts both ways
The catalog files the wrench as `EQUIPMENT`/`SUPPORT_STATION`. The new shop rules must leave the flag
**true** or `FindInventoryItems:1866` drops the entry. But the `SHOP_ELECTRONIC` rule must keep it
**false** — that flag is the shipped BUG-098 fix (deployable parts on civilian shelves).

### 5. The wrench is required even inside a zone
`SCR_BaseUseSupportStationAction.CanBeShownScript():156-158` hides the action outright when no
matching gadget is held, regardless of any station in range. This is user intent and a DoD criterion —
**not a defect to report** during play-test.

---

## Server Authority & Networking

**Zero new RPCs, zero new persisted records, zero new replicated properties, zero new UI, zero new
localization keys.** If an `RpcAsk`, an `RplProp` or a serializer appears in this feature's diff, the
design has been abandoned — the epic's dominant defect class is client-computed effects paid through
unlinked money RPCs (BUG-042/043/047/048/051/053).

- Repairs are validated and applied by vanilla's `OnExecutedServer`, untouched.
- The starting-car wrench is spawned and inserted **server-side only**; `SpawnStartingCar` is already
  called from `OVT_OverthrowGameMode.c:1241` on the server. Add no authority check and no RPC.
- Purchase and sale ride `OVT_ShopTransactionComponent` (`:340-395`), which already handles spawn,
  insert, charge and refund-on-failure (D12).
- The car's storage contents replicate through the vanilla inventory stack; the zones are ordinary
  components on ordinary entities.

---

## Testing Approach

### Headless gates (run freely)
- `tools/compile-check.sh` — after every edit, exit 0
- `tools/check-shop-coverage.py` — **Phase 2's primary gate**; it independently re-derives
  "registered but reachable by no shop rule" and `RepairKit` is not in its `DEFAULT_IGNORES`

### Automated — Campaign tier only
`OVT_TEST_Campaign_FieldRepairEconomy` (new) + `OVT_TEST_Campaign_ShopCivilianStock` (existing).
**No Logic-tier case** — there is no pure function in this feature (D14).

### Not covered by any automated tier, and known to be so
Any repair action executing; the 50% cap and the 100% zone heal; the zone ranges and the in/out
fallback (the feature's headline behaviour); the starting-car insertion; both new prefabs loading,
building and persisting; multiplayer / JIP. All manual — see the play-test checklist in `tasks.md`.

---

## Next Steps

### Immediate
1. Phase 1 — write the modded class (1.1–1.3)
2. Phase 1 task 1.4 — resolve the Helipad prefab question; file a bug if branch (b)
3. Phase 2 — un-hide the price rule **first**, then both shop rules, then `check-shop-coverage.py`

### After Half A
4. Phase 4 — the ramp range widening (smallest change that proves the zone mechanism)
5. Phase 5 — ⚠️ advanced: the two new wrapper prefabs

### Future / deferred
- A visible zone boundary ring (`SCR_SupportStationAreaMeshComponent`) — candidate follow-up, not this feature
- Retrofitting helipads and garages built in existing saves — accepted cost (D7/R9)

---

## Open Questions

- [ ] **Q:** Does the Helipad buildable actually produce a structure today, given that
      `{9DA31028409EDE7E}...Helipad.et` exists nowhere in the reference extraction?
      **A:** _(task 1.4)_
- [ ] **Q:** Does a 12 m bounding-box zone reach a vehicle parked **inside** the enclosed
      `Garage_E_02` building?
      **A:** _(task 5.8)_
- [ ] **Q:** Are built helipads and garages persisted at all today?
      **A:** _(task 5.8 — expectation is **no**, and the wrapper fixes it as a side effect)_

---

## Session Notes

### 2026-08-29 — scaffold
- Resolved `resistance/field-repair` as an epic-nested feature; loaded the resistance epic context.
- `implementation.md` was already complete (917 lines) — status flipped to **In Progress**, no re-planning.
- Broke the 7 phases into **39 granular tasks**; carried the plan's blocking dependency
  (Phase 5 ⏸️ on task 1.4) and its advanced-agent routing (Phase 5 → `component-developer-advanced`,
  Phase 7 → `help-docs-sync`) into `tasks.md`.
- Next session should start with Phase 1.

---

*Update this file at the end of each work session. Run `/update-feature resistance/field-repair` before compacting conversations.*

---

## Findings recorded during the build

### F1 — The ramp's area-mesh is a GRANDCHILD, and it is editor-only (Phase 4, task 4.4)
`RampVehicle_01_metal_base.et` **does** carry an `SCR_SupportStationAreaMeshComponent`
(`{5EA88835E729A664}`, `m_fRadius 4.5`) — but not on the entity that holds the repair station. It sits
on a nested `GenericEntity` (ID `5EA88835EFCCD074`, `coords 0 0 2`) alongside
`SCR_EditableEntityVisibilityChildComponent` and `SCR_ShowHideInEditorComponent`, with
`m_bHideInWorkbench 1` and the `VirtualArea_01_Focused.emat` material.

Two consequences:
1. It is a **Game Master / editor** visualization, not a ring ordinary players see — so it is not the
   answer to R10, and the "no visible boundary" tech-debt item stands regardless.
2. Matching it to 12 (D5's radius==range rule) means overriding a **grandchild** inside a vanilla
   prefab from Overthrow's wrapper: the child `SCR_DestructibleBuildingEntity` `59CF196E7B413D5F`,
   then the nested entity `5EA88835EFCCD074` inside it.

The repair station itself is confirmed at `RampVehicle_01_metal_base.et:34` —
`SCR_RepairSupportStationComponent "{5EA88835DBD208B7}" : "{F9A7B3AA0BE419B3}...BaseRepairSupportStation.ct"`,
`HIGH` / `m_fRange 4.5` / `m_bUseRangeBoundingBox 1` / `m_vOffset 0 0 2` — exactly as the plan states,
and its `SCR_ResourceComponent` already lists `SUPPLIES` under `m_aDisabledResourceTypes`, so D1's
override is a genuine no-op for the ramp.

`OVT_VehicleMaintenanceRamp.et` currently carries **no** repair-station delta on that child, so task
4.1 is adding the first one.

### F2 — Phase 5's premise was wrong: both structures are ALREADY Overthrow prefabs (task 1.4)
The plan said the Helipad buildable pointed at a GUID/path that "exists nowhere", and that both the
Helipad and the Garage spawned **bare vanilla** prefabs needing new Overthrow wrappers. Both claims are
false, verified directly:

| | What the buildable actually spawns | Already carries |
|---|---|---|
| **Helipad** `{5EE4FAB564EA1AA1}` | `PrefabsEditable/Auto/Structures/Military/Camps/HelipadImprovised_01/Helipad.et` — **Overthrow-authored**, `.meta` GUID `{9DA31028409EDE7E}`, matching `buildables.conf` byte-for-byte. Committed in `922b4611`. | `OVT_BuildableComponent`, `OVT_StructureDestructionComponent` (**root**), `RplComponent Enabled 1`, an `ActionsManagerComponent` with a `"repair"` context, `OVT_ParkingComponent {PARKING_HELI}`, `OVT_ShopComponent`, editable + preview components |
| **Garage** `{5D98AC7AC353F540}` | `Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et` — **Overthrow's own same-GUID, same-path delta** over vanilla, `{80A5B37A1472B084}` | `OVT_BuildableComponent {"VehicleGarage"}`, `OVT_StructureDestructionComponent` (**root**), `RplComponent Enabled 1`, a `"repair"` action context, 5 parking spots, map marker, shop |

**Why the plan got it wrong, and the trap to keep:** the planning pass searched only the Reforger
reference extraction and never grepped Overthrow's own `PrefabsEditable/`. Worse, **the extraction
contains zero `.meta` files** (0 of 17 466), so any "find the `.et.meta` carrying this GUID" step fails
there for *every* asset — which is almost certainly how it concluded the GUID existed nowhere. Tally
for the record: of 28 `PrefabsEditable/Auto/...` references in `Configs/`, **0** are unresolvable; of
all 219 `.et` references in `Configs/`, exactly 1 miss, and it is unrelated (`PaperMap_01_folded.et`).

**This was already known.** `docs/features/core/damage/implementation.md:768` states verbatim that the
Helipad reference is not dangling and is an Overthrow-authored file. The field-repair plan regressed on
a prior finding. **No bug filed** — there is no defect.

**Consequences, all favourable:**
- Phase 5 drops from 8 tasks to 6 — no new prefabs, no `.et.meta`, no hand-minted GUIDs, no
  `buildables.conf` repoint. The reserved GUIDs `{6A9F1E4A00000010}` / `{6A9F1E4A00000011}` are void.
- **D7 and D10 are moot** — there is no wrapper to author, so no wrapper-pattern choice to make.
- **R9's accepted cost evaporates.** Same prefab identity means structures in *existing saves* gain the
  repair zone too.
- **The persistence question is answered, not open.** Both carry `OVT_BuildableComponent`, so both are
  already matched by `Configs/Systems/Persistence/Overthrow.conf:195-216` (`SelfSpawn 1`). No
  before/after save comparison needed.
- **The destruction hazard is satisfied by construction.** `OVT_StructureDestructionComponent` is on
  the **root** of both, so a root-mounted station is on the same entity and
  `ApplySupportStationState()` will disable it on ruin — DoD 14's ramp behaviour now extends to all
  three zones for free.
- One genuine leftover, **not ours to fix**: `Helipad.et` carries a dead `EPF_PersistenceComponent`
  block, one of 15 such prefabs flagged post-EPF-migration at `core/damage/implementation.md:765`.

### F3 — A latent robustness gap in the build path (noticed, deliberately not fixed)
`OVT_ResistanceFactionManager.FinishBuild:866` spawns via `OVT_WorldUtils.SpawnEntityPrefabMatrix`
(`OVT_WorldUtils.c:615`, which does `Resource.Load` with no null guard) and then dereferences the
result at `:869` — `entity.FindComponent(OVT_BuildableComponent)` — before the existing
`if (!buildableComp)` warning can fire. A genuinely bad prefab path would therefore VM-error rather
than warn. At least it fails *loudly*, and before `TakePlayerMoney` at `:893`, so no money is lost.
**Out of scope for this feature.** Worth a one-line null guard in some future pass.

### F4 — The autotest suites are unavailable this session
Claude Code's auto-mode classifier refuses `tools/run-tests.sh` (tried both directly and via `bash`).
This is a known, recurring environment limitation, not a defect in the test tree. The gates actually
used for every phase of this build were `tools/compile-check.sh` and `tools/check-shop-coverage.py`.
**The suites — Campaign tier especially, which is where Phase 6's new case lives — have not been run
and must be run by the user before this feature is considered verified.**

### F5 — `CallLater` rejects `EntityID` arguments (Phase 3)
The obvious way to make a deferred call safe against a deleted entity is to pass an `EntityID` rather
than an `IEntity` and re-resolve it on arrival. **The engine refuses:**

```
ScriptCallQueue: argument 'vehId': Pointer inherited type 'EntityID' is not supported
```

Pass the `IEntity` directly and null-check it on arrival — which is the established house idiom
anyway (`OVT_InventoryManagerComponent.c:237`, `:592`, `:602`, `:708`, `:747` all do exactly this).
Costs one compile-check iteration if you don't know it.

### F6 — Phase 3's retry, and why the ownership split matters
`AddStartingEquipment` spawns the wrench **once** and, on a same-frame insertion failure, schedules
`RetryAddStartingWrench` **without deleting it** — the retry owns the entity from that moment. The
retry is the *only* place a delete can happen, in either of two mutually exclusive branches (vehicle
gone / insertion failed again), and it is a one-shot `CallLater(..., false, ...)`. So every path ends
in exactly one of {inserted, deleted}: never an orphan wrench at the car's origin, never two wrenches,
never a delete that races the retry. The naive version — deleting on the first failure and *then*
retrying — destroys the wrench before the retry can use it, which is why the split is written down.

**3.7, settled:** no persistence work belongs here. `UntrackTransient` is used in this codebase only
to stop a **root-level self-spawning** entity being double-claimed (`OVT_TownController.c:313`,
`OVT_TownVehicleSourceConfig.c:91-96`, `OVT_HighCommandManagerComponent.c:1271`). The wrench is not
root-level — it is an inventory item inside the vehicle's own storage, and it rides the same
serialization that already carries vanilla's two pre-slotted field dressings in that very trunk
(`UAZ469_base.et:694-701`) with no Overthrow code at all.

### F7 — D5's "a mismatched radius lies about the range" premise is WRONG for this component (Phase 4)
The plan's D5 requires `m_fRadius == m_fRange` wherever an `SCR_SupportStationAreaMeshComponent` is
present, on the reasoning that a mismatch makes the visible ring understate the real range. Reading the
vanilla component shows that is not how it works:

```
override float GetRadius()
{
    if (!m_SupportStationComponent)
        return m_fRadius;
    return m_SupportStationComponent.GetRange();   // the LIVE range wins
}
```
`SCR_SupportStationAreaMeshComponent.c:21-28`. `m_fRadius` is only the **fallback** for when no station
is found. So the ramp's ring draws at the real **12 m** without touching it.

There *is* a warning path — `GetSupportStation():39-43` logs `LogLevel.ERROR` about a radius/range
mismatch — but it is guarded by
`supportStation.GetOwner() != owner && supportStation.GetOwner() != owner.GetParent()`. The mesh sits
on grandchild `5EA88835EFCCD074`, whose **parent** is `59CF196E7B413D5F`, the very entity carrying the
station. The guard is therefore false and **nothing is logged**. No error spam to mistake for a defect
during play-test.

**Conclusion: leaving the ramp's area mesh at `m_fRadius 4.5` is correct, not merely acceptable** — it
costs nothing, avoids a two-level-deep delta into a vanilla prefab, and the ring is right anyway.
Independently, the ring is **GM-only**: `SCR_ShowHideInEditorComponent` (default
`SHOW_IN_UNLIMITED_EDITOR`) clears `EntityFlags.VISIBLE` at init and only restores it while a Game
Master has the unlimited editor open, and the component additionally sets `m_bHideInWorkbench 1`. No
ordinary player sees it in any mode.

**Do not "fix" D5 on the new Helipad/Garage zones either** — neither carries an area mesh at all, so
there is nothing to match, and adding one would only ever be visible to a GM.

### F8 — `check-shop-coverage.py`'s summary line does NOT catch a single-shop regression
The plan makes this tool Phase 2's "real gate" on the grounds that it is sensitive to exactly the
changes this feature makes. **That is only half true, and the half that fails matters.**

| Change | Summary line (`404 sellable, 372 reachable, 0 unreachable`) | `--mode all` per-item shop column |
|---|---|---|
| Un-hide reverted (`cost 150` → `hidden 1`) | **drops to 403/371** — caught | caught |
| `SHOP_GENERAL` rule removed | **unchanged** — MISSED | `GUN_DEALER,SHOP_GENERAL` → `GUN_DEALER` |
| `SHOP_GUNDEALER` rule removed | **unchanged** — MISSED | → `SHOP_GENERAL` alone |

The reason: the checker's "reachable" means **reachable by *any* shop rule**, so as long as one of the
two new rules survives, the counts are identical. So the green `404/372/0` this feature shipped on
proves the wrench is reachable *somewhere*, **not** that it is reachable at *both* intended shops.

**What actually covers that claim is Claim B of `OVT_TEST_Campaign_FieldRepairEconomy`** — which has
not been executed (F4). Until the suites run, "sold at general stores *and* gun dealers" rests on
reading the two config rules, nothing more. Use `--mode all` if you need a headless check of it.

**Also worth knowing — the three claims are not independent under revert 1.** Reverting the un-hide
makes `FindInventoryItems` return nothing, so the case trips its `MAX_POLLS` backstop and returns
*before* Claims B and C ever run. Revert 1 therefore **masks** B and C rather than failing them
separately. That is precisely why Claim A is asserted first and independently of any shop rule, and
why "the wrench appears in a shop" was never an acceptable substitute for asserting
`IsRegisteredResource` directly.

---

## Outstanding — what is NOT verified

This feature is **code-complete and every headless gate is green**, but a large share of its behaviour
is only provable in game, and the one automated tier that covers any of it could not be run.

### 🔴 Blocked, needs the user
1. **The autotest suites were never run** (F4). `tools/run-tests.sh` is refused by Claude Code's
   auto-mode classifier. This means **`OVT_TEST_Campaign_FieldRepairEconomy` has never executed** —
   it compiles, its accessors are verified against source, but it has not been seen to pass, and the
   standing BUG-098 guard `OVT_TEST_Campaign_ShopCivilianStock` has not been re-run either.
   → Run the **All** group (this feature touched economy and campaign state).
2. **Task 6.5's red-then-green demonstration is owed** — see 6.5. Static reasoning only.
3. **A Workbench localization re-export is required** before the Field Manual page renders. The `.st`
   master has the 8 new keys; the generated `.conf` exports were correctly left untouched, so the page
   currently shows raw keys in game.
4. **The wiki sync (7.4) is deferred** — no wikijs MCP server was attached.

### 🟡 Play-test owed (the feature's headline behaviour is manual-only by nature)
The full checklist is in `tasks.md`. The items that carry real risk rather than routine confirmation:
- **The starting-car wrench** (A) — the deferred-retry path has never run. Exactly one wrench, none on
  the ground, and again on a *second* new campaign.
- **The 50% / 100% split and the in-out fallback** (B2, E1–E3) — this is the whole UX, delivered by
  vanilla's priority ordering with no code of ours. If D11's reading of
  `GetClosestValidSupportStation` is wrong, this is where it shows.
- **The garage interior** (E5) — 12 m reasoned sufficient from the bounding box, not measured.
- **The fuel-pump regression** (F) — the one check that proves the modded class did not leak past
  repair stations into fuel/medical/rearm.
- **A ruined structure stops repairing** (E6) — now claimed for all three zones, verified by
  construction only.
- **Multiplayer / JIP: entirely untested.**
