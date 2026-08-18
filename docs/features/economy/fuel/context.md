# Fuel - Context & Decisions

**Last Updated:** 2026-08-18 (Amendment A2)
**Current Phase:** Complete + Amendments A1/A2 (pending wiki sync, loc re-export, play-test)
**Status:** 🔍 Ready for Review

---

## Quick Status

**What's Done:**
- ✅ Plan (`implementation.md`, 2026-08-18) — all vanilla line refs verified against 1.8.0.10
- ✅ Dev docs scaffolded
- ✅ Phase 1 (8/8): `OVT_FuelPricing` (+`MIN_TICK_COST` const), `OVT_FuelChargeLedger`, `fuelPricePerLitre` knob + presets + `CONFIG_STREAM_VERSION` 3→4 + struct mirror, `OVT_FuelSourceComponent`, 4 Logic cases (each red-proven via recorded inverted-assertion edits). Gate: All group 283/284 — the 1 red was the documented `Setup_Checkpoint` 500 ms I/O flake ([[movement-built]]), green on targeted re-run.

- ✅ Phase 2 (8/8, ADVANCED): modded `SCR_FuelSupportStationComponent` (3 hooks + charge routine), `OVT_FuelUtils` core half, modded `SCR_RefuelAtSupportStationAction` (price suffix + reason), `OVT_CANNOT_AFFORD_FUEL = 550`, 3 loc keys + `CannotAffordFuel` broadcast preset, degradation guards. Zero RPCs (grep-verified). Gate: All 284/284 green.

- ✅ Phase 3 (6/6, ADVANCED): `OVT_FuelDepot.et` (+ meta), buildables.conf entry #8, `OVT_FuelDepotHandler`, `SCR_FuelManagerComponentSerializer` on the buildables persistence group, 2 loc keys, 1 persistence case (**degraded to save-only — see session notes**). Gate: `compile-check.sh` exit 0; `check-placeables.py --strict` resolves the depot's chain and PROVES `RplComponent Enabled 1` from `FuelTank_02_Base.et`. Model re-based on user feedback after the phase — see the 16:05 amendment.

- ✅ Phase 4 (4/4): modded `SCR_SupportStationManagerComponent.OVT_GetSupportStationsOfType` (copies the internal `m_mSupportStations` bucket), `OVT_FuelUtils.FindFuelSourcesCovering` / `FindFuelSourcesNear` + `CollectFuelStations` helper, doxygen audit (one gap: the ledger ctor), loc audit clean (5/5 keys present). 1 new Logic case. Gate: `compile-check.sh` exit 0.

- 🟡 Phase 5 (1/2): in-game help done (new Field Manual page **Fuel** + new **fuel-depot-built** tutorial popup, 10 new loc keys). Public wiki sync BLOCKED on wiki auth - see session notes.

- ✅ Amendment A2 (3/3, ADVANCED network, user play-test): Fill now scopes to the tank whose action you hold (vanilla's own rule), a third Fill instance on the truck cargo context so fuel moves depot -> truck -> depot, depot capacity 10,000 L. Wire is now `RplId` + `m_iFuelTankID`.

- ✅ Amendment A1 (4/4, ADVANCED network, user request): fast **Fill Tank** hold action. `OVT_FuelFillPlan` + `OVT_FuelPricing.ComputeFillPlan` (1 new Logic case, red-proven), new controller component `OVT_FuelRequestComponent` (one `RplId` on the wire, server re-derives everything), `OVT_FillFuelAction` on the vehicle base `fuel_cap` context and the depot's `default` context, `OVT_FuelUtils` fill-discovery half, 4 loc keys, 1 Field Manual sentence. Vanilla Refuel untouched. Gate: `compile-check.sh` exit 0.

**What's Next:**
- 📋 Human: manual play-test steps A–H in `implementation.md` **and step I in `tasks.md`** (I4/I5/I6 rewritten for A2) (the real gate — fuel/MP/prefabs unreachable by the harness); `localization_Overthrow.en-us.conf` re-export in Workbench (14 new keys render raw until then)
- 📋 Wiki (when auth returns): the new `fuel` page must also describe the fast **Fill Tank** action, matching `OVT-FieldManual_Fuel_Text` word for word on the facts.
- ⏸️ Phase 5 T5.2: public wiki sync — blocked on wiki auth (wikijs MCP unavailable; fallback token is list-only). Owed pages + drafted copy recorded in tasks.md T5.2.
- Final gates this session: All 285/285 (after Phase 3), Fast 234/234 (after Phase 4), Phase 5 suite skipped (docs/config/loc only), compile-check exit 0 at end.

**Blockers:**
- None

---

## Key Files

### New (this feature)
- `Scripts/Game/Data/OVT_FuelPricing.c` — pure fee math (+ `ComputeFillPlan`, A1)
- `Scripts/Game/Data/OVT_FuelFillPlan.c` — litres + cost of one fast fill (A1)
- `Scripts/Game/Components/Controller/OVT_FuelRequestComponent.c` — the ONLY RPC in this feature (A1)
- `Scripts/Game/UserActions/OVT_FillFuelAction.c` — the fast "Fill Tank" hold action (A1)
- `Scripts/Game/Data/OVT_FuelChargeLedger.c` — fractional charge accumulator (never settled, never persisted — D8)
- `Scripts/Game/Components/OVT_FuelSourceComponent.c` — free-source marker + HC discovery surface
- `Scripts/Game/Components/SupportStation/Modded/SCR_FuelSupportStationComponent.c` — the three money hooks
- `Scripts/Game/UserActions/Modded/SCR_RefuelAtSupportStationAction.c` — price label + reason string
- `Scripts/Game/Components/SupportStation/Modded/SCR_SupportStationManagerComponent.c` — registry accessor
- `Scripts/Game/Utilities/OVT_FuelUtils.c` — fuel-source API for high-command
- `Prefabs/Structures/Military/FOB/OVT_FuelDepot.et` — the buildable depot
- `Scripts/Game/GameMode/Placeables/OVT_FuelDepotHandler.c` — server-side build backstop

### Touched
- `OVT_DifficultySettings` + `Configs/Difficulty/*.conf` — `fuelPricePerLitre`
- `OVT_OverthrowConfigComponent` — RplSave/RplLoad + `CONFIG_STREAM_VERSION` 3→4
- `Configs/Resistance/buildables.conf` — depot entry
- `Configs/Systems/Persistence/Overthrow.conf` — fuel serializer on buildables group
- `Language/localization_Overthrow.st`
- `Scripts/Game/Utilities/OVT_FuelUtils.c` — A1 added the fill-discovery half (`FindBestFillSource`, `HasFillSourceInRange`, `GetRefuelableCapacity`, `GetProvidableFuel`, `GetStationFuelManager`, `IsStationMountedOn`); A2 replaced the slot-walking half with the scoping trio (`GetOwnFuelManager`, `GetPrimaryFuelTankId`, `FindFuelManagerByTankId` + `ManagerHasTankId`) and deleted `CollectFillableManagers` / `GetRefuelableCapacityOfEntity`
- `Prefabs/GameMode/OVT_OverthrowController.et` — `OVT_FuelRequestComponent` (A1)
- `Prefabs/Vehicles/Core/Vehicle_Base.et` — `OVT_FillFuelAction` on `fuel_cap` (A1)
- `Prefabs/Vehicles/Core/Vehicle_Fuel_Tank_Base.et` (+ meta) — **NEW same-GUID delta**, `OVT_FillFuelAction` on `supportStation_fuel` (A2.2)
- `Prefabs/Structures/Military/FOB/OVT_FuelDepot.et` — `OVT_FillFuelAction` on `default` (A1)
- `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ControllerSeam.c` — roster entry for the new component (A1)

---

## Important Decisions

All eleven decisions (D1–D11) live in `implementation.md` § Key Technical Decisions — per-litre charging (D1, user override of requirements.md's per-time fee), charge at every static station (D2), depot filled via vanilla refuel action (D3), no use-time ownership gate (D4), structural free-source detection (D5), refusal through `IsValid` (D6), non-players refuel free (D7), never-settled ledger (D8), price ≤ 0 kill switch (D9), stream v4 (D10), nothing added to the economy god object (D11). Record only *new* decisions here.

**D12 — The fast fill's money limit REDUCES the fill; it never refuses it. (Amendment A1, 2026-08-18.)**
`OVT_FuelPricing.ComputeFillPlan` takes the minimum of what the target needs, what the source has and
what the balance buys, and a player who cannot afford a full tank gets the fuel their money buys at the
price shown on the label before they commit. Refusing instead would have made the action useless to
exactly the player who most needs a partial tank, and pre-charging would take money for fuel that never
arrived. There is deliberately **no ledger here** (contrast D8): a fill is one transaction, not a stream
of 0.5 s ticks, so the sub-dollar problem the ledger exists for does not arise — the cost rounds up
inside the affordable branch (safe, because the balance it is bounded by is an int) and equals the
balance exactly when money is the binding limit.

**D13 — The fast fill prefers a FREE source over a higher-priority paid one. (Amendment A1.)**
Vanilla's `GetClosestValidSupportStation` ranks on station priority alone, which would let a
`VERY_HIGH` commercial pump shadow a `HIGH` Fuel Depot built near it and charge a player for fuel the
resistance already hauled and paid for (implementation.md R13). Because this action prices the fill up
front it can afford the better rule: free first, then priority, then distance. It is strictly
player-favourable, it is confined to this action, and it leaves the vanilla trickle's ranking alone.

**D15 — One press fills ONE tank: the one whose action you are holding. (Amendment A2.1, user play-test.)**
Not every tank on the vehicle. This is vanilla's own rule copied verbatim — `SCR_RefuelAtSupportStationAction.Init`
resolves the action owner's own `SCR_FuelManagerComponent` and nothing else, which is why vanilla's
BaseRefuel on `fuel_cap` fills a truck's chassis while `Refuel_CargoTank.conf` on the cargo part fills
its cargo tank. A1's hierarchy-wide fill was a real defect, not a preference: it drained a full depot
to 21% in one press. A manager's several nodes still fill together (a Ural's 300 L main + 60 L reserve
are two nodes of one tank), because that is the tank the player is standing at.

**D16 — A tank is named on the wire by its authored `m_iFuelTankID`, not by an RplId. (Amendment A2.2.)**
A truck's cargo tank is a real entity but `Vehicle_Fuel_Tank_Base.et` carries no `RplComponent`, so it
cannot be addressed across the network at all. The request therefore names the networked ROOT plus the
tank id — prefab data, identical on both machines, distinct across a vehicle, and the same key vanilla
uses to scope its own refuel action (`m_aFuelTankIDs`). The server resolves it from scratch, bounded to
that root, so the id can only ever pick a tank on the entity it already range-checked. See
`OVT_FuelRequestComponent`'s header for the full abuse analysis.

**D14 — The fill endpoint is its own controller component. (Amendment A1.)**
`OVT_FuelRequestComponent`, not `OVT_ShopTransactionComponent`. The rearm precedent's own comment
explains why re-arm stayed on the shop component — it is a stop-gap the logistics epic will delete —
and neither that nor "money for goods" applies here: a fill from a fuel truck or the depot moves no
money at all, the feature is permanent, and `resistance/high-command` will want the same seam.
`OVT_TowerSabotageComponent` is the granularity precedent.

---

## Gotchas & Learnings

(from planning, to honour during build)
- `OnFuelAddedToVehicleServer` can receive `node == null` (backup-flow path) and vanilla early-returns — charge BEFORE `super`, never conditional on the node (R2).
- Do NOT use `GetActionStringParam` for the price — it suppresses the fill % readout (implementation.md § Three hooks).
- No `#ifndef DISABLE_FUEL` wrappers — the vanilla station/manager classes are unconditional.
- `m_fInitialFuelTankState` is litres, not a fraction; 0 = empty AND makes an untouched depot serialize to nothing (R4).
- `m_bIgnoreSelf 1` on the depot station or it refuels itself from itself.
- Depot needs its own `UserActionContext "default"` — `BaseRefuel.conf`'s `fuel_cap` context doesn't exist on a static tank (R12).

(from Phase 3, learned during build)
- `ReapplyLatestSaveData()` — the round-trip suite's ONLY load seam — re-applies the GAME MODE entity's record and nothing else (`request.Instances = {owner}`). No test in this harness can reload a buildable, a vehicle or a placeable. Plan for that before promising a round-trip case for any per-entity state.
- `OVT_TEST_PersistenceSuite` runs BEFORE `OVT_TEST_PersistenceRoundTripSuite` in `OVT_TestGroup_All.conf`, and the round-trip capability gate asserts `HasSaveGame()` is FALSE before its own first save. **Never take a save from the Tier-D suite** — it turns the whole All group red, and its header already forbids it.
- `BuildItem(..., -1)` / `PlaceItem(..., -1)` reach `OVT_SkillManagerComponent.GiveXP(-1, …)`, which had no null guard: `GetPlayer("")` is null and the deref aborts the invoker mid-`BuildItem`, before `OVT_PersistenceTracking.Track()`. Guarded in Phase 3. Any future server-initiated build/place path depends on that guard.

(from Amendment A1, learned during build)
- 🔴 **A vehicle's big fuel tank is usually NOT on the vehicle.** `Ural4320_tanker.et` / `M923A1` attach a `Vehicle_Fuel_Tank_Base.et` part through a `RegisteringComponentSlotInfo`, and that PART owns the 4,500 L `SCR_FuelManagerComponent` **and** the vehicle's fuel support station. `vehicle.FindComponent(SCR_FuelManagerComponent)` finds only the chassis tanks from `Vehicle_Base.et` (~40 L on the base, 300+60 on a Ural, 306 on an M923A1). **Know which one you want.** For "the tank this action is on", `FindComponent` on the action owner is CORRECT and is what vanilla does — see D15; A1 got this wrong and drained a depot. Only reach for `SCR_FuelManagerComponent.GetAllFuelManagers`, which walks the slot manager, when you genuinely mean every tank on the vehicle.
- The vanilla fuel-truck station is `m_fRange 9 / m_vOffset 0 0 -2.3` (the concrete part overrides the 7 m base), which is more forgiving than the depot's 7 m for the truck→depot fill.
- `m_bIgnoreSelf` is only consulted by the vanilla `SCR_BaseSupportStationComponent.IsValid`. Any query that goes round that path (like the fill's source search) must exclude self itself.
- The vanilla vehicle engine node authors no `m_eFuelNodeType`, so it defaults to `CAN_RECEIVE_FUEL` only — an engine tank can never be drained as a source, which is why draining a truck only ever touches its cargo tank.

---

## Testing Approach

Per `implementation.md` § Testing Strategy: 4 Logic-tier cases (Fast), 1 persistence round-trip case (with documented Tier-D fallback), the rest play-test gated (manual steps A–H). Suites: All group after phases 1–3, Fast after 4, skipped for 5.

---

## Session Notes

### 2026-08-18 — Amendment A2 complete: fill scoping, cargo context, 10,000 L depot

Straight off the user's A1 play-test: filling at a fuel truck's fuel cap drained a freshly filled
5,000 L depot to 21%. A1 filled **every** manager in the vehicle's hierarchy, so one press pulled the
chassis tanks *and* the cargo tank — Ural-4320 300+60 + 4,500, M923A1 306 + 4,550, i.e. the ~3,960 L
that matches the observed remainder exactly. User directive: "it should only fill the one you are
doing the action on."

- 🔴 **A2.1 — the fix is to copy vanilla, which already had this right.** `SCR_RefuelAtSupportStationAction.Init`
  resolves `pOwnerEntity.FindComponent(SCR_FuelManagerComponent)` — the action owner's OWN manager, no
  slot walk. That is precisely why vanilla's BaseRefuel on `fuel_cap` tops up the chassis while
  `Refuel_CargoTank.conf`, authored on the cargo-tank CHILD entity's `supportStation_fuel` context,
  tops up the cargo tank. A1's `GetAllFuelManagers` was the deviation. `OVT_FuelUtils.GetOwnFuelManager`
  is now the single scoping rule and its doc comment says why it must never grow a slot walk again;
  `CollectFillableManagers` / `GetRefuelableCapacityOfEntity` / `AddFuelToManagers` are deleted rather
  than left around as a trap. **A manager can still hold several nodes and they all fill** — a Ural's
  fuel cap fills its 300 L main and 60 L reserve together, because those are two nodes of the one tank
  you are standing at.
- **A2.2 — new wire, forced by the fact that a cargo tank is not addressable.** `Vehicle_Fuel_Tank_Base.et`
  carries **no `RplComponent`**, so no `RplId` can ever name it. The request is now
  `RpcAsk_FillFuel(RplId rootId, int fuelTankId)`: the root is `GetOwner().GetRootParent()` (the truck;
  the depot resolves to itself) and the tank is named by the authored `m_iFuelTankID` — prefab data,
  identical on both machines, distinct across a vehicle (Ural chassis 1 and 2, cargo 10) and *vanilla's
  own* scoping key (`m_aFuelTankIDs` / `CheckIfFuelNodeIsValid`). `OVT_FuelUtils.FindFuelManagerByTankId`
  resolves it server-side, **bounded to the resolved root**; the client only sends an id that resolves
  back to its own manager, else the action hides. Abuse ceiling: a spoofed id can fill the cargo tank
  from the chassis fuel cap — a tank the same player could fill anyway by walking three metres, at the
  same range, from the same source, at the same price, out of their own wallet, and never on another
  entity. Scoping is a gameplay rule; the money/entity/distance/source boundaries are untouched.
- **A2.2 landed as a BASE delta, not per-prefab, and vanilla proves it works.** `Vehicle_Fuel_Tank_Base.et`
  authors `SCR_RefuelAtSupportStationAction {5E2CE357444CF3AF}` targeting `supportStation_fuel` while
  declaring **no `ActionContexts` whatsoever** — the contexts live in `Ural4320_fuel_tank.et` and
  `M923A1_fuel_tank.et`, which are the only two prefabs in the game that declare that context (all
  CIV/FIA/MERDC variants derive from them). So a base-authored action does bind to a derived-declared
  context, and one same-GUID delta covers every fuel truck. Meta GUID is vanilla's `{6B0F3D4B5193FD07}`
  and the header reproduces vanilla's verbatim — the convention `Vehicle_Base.et` and `Helicopter_Base.et`
  already follow here.
- **A2.3 — depot 5,000 → 10,000 L** (user: two full truck deliveries). Sites synced: the prefab's
  `MaxFuel`, the persistence case's `EXPECTED_MAX_FUEL` **and its header sentence**, the Field Manual
  depot paragraph, the tutorial body, both of their fact-check Comments, `implementation.md`'s depot
  anatomy and two `tasks.md` references. `m_fInitialFuelTankState` stays 0 and `SAVED_FUEL` 1234 is
  still well inside the tank. The 5,000s left alone are all **truck** claims, not depot claims.
- ⚠️ **The user shortened `OVT-Build_FuelDepot_Description` to "Stores and delivers fuel" mid-session**
  (`LastChanged "Aaron Static"`). Left exactly as they wrote it; only its stale fact-check Comment was
  rewritten, and the capacity number now lives only in the Field Manual and tutorial strings.
- 🔴 **TWO LOC KEYS WERE SILENTLY LOST BY A WORKBENCH RE-SAVE, and are restored.** The user re-saved
  `localization_Overthrow.st` from Workbench during the session. That fixed the long-standing missing
  closing brace on `{6B09A1C0E4D50121}` (`OVT-GMIcon_Tooltip_NoData`) — and, because `OVT-StartUprising`
  and `OVT-SupportTooLow` had been parsed as **nested inside** that unclosed item, the re-save dropped
  them. Both are live (`Prefabs/Controllers/OVT_TownController.et:46` and
  `OVT_StartUprisingAction.c:39`), so they would have rendered as raw keys. Restored verbatim from HEAD
  as top-level items with their original GUIDs, alphabetically placed, with a Comment recording the
  cause. **The file's brace delta is now 0** (it was 1 at HEAD) and an Id-set diff against HEAD shows
  nothing else missing. Lesson worth keeping: an unbalanced brace in a `.st` is not cosmetic — the next
  Workbench save deletes whatever the broken item swallowed.
- Gate: `tools/compile-check.sh` exit 0 (6143 files). Rpc arity re-checked by hand against the new
  two-parameter handler.

### 2026-08-18 — Amendment A1 complete: the fast "Fill" action

Built by `network-engineer-advanced` on user request after play-test found the vanilla trickle unusable
on 5,000 L tanks. **The vanilla Refuel path and every Phase 2 modded class are untouched** — this is
purely additive, and `grep -n "Rpc(" ` over the Phase 2 files still returns nothing.

- **The endpoint is a NEW controller component, `OVT_FuelRequestComponent`, not `OVT_ShopTransactionComponent`.**
  The rearm precedent states its own reason for staying on the shop component: re-arm is a stop-gap the
  logistics epic will delete, so a whole component for one RPC pair was not worth it. Neither half holds
  here — the fast fill is permanent, and **a fill from a fuel truck or the depot moves no money at all**,
  so calling it a shop transaction would be wrong in the common case. `resistance/high-command`'s
  auto-refuel tick is a named future consumer of the same seam. Wired on `OVT_OverthrowController.et` as
  `{6B4E1F6000000001}` and added to the Init-tier roster case, which is the only gate that catches a
  controller component that was written but never put on the prefab.
- **Wire encoding: one `RplId` naming the target, and nothing else.** No source, no litres, no price, no
  identity. The caller is `ResolveOwningPlayerId()` off the controller entity the RPC arrived on. Server
  re-derives all eleven inputs from scratch. One `Rpc(` call site in the whole amendment, hand-checked
  against its handler's arity (the compile-check blind spot).
- 🔴 **A FUEL TRUCK'S CARGO TANK IS A SEPARATE ENTITY, and this nearly shipped broken.** `Ural4320_tanker.et`
  attaches `Ural4320_fuel_tank.et` through a `RegisteringComponentSlotInfo`, and THAT part carries the
  4,500 L `SCR_FuelManagerComponent` (`m_eFuelNodeType 11`, `m_iFuelTankID 10`) plus the truck's fuel
  support station. A plain `FindComponent(SCR_FuelManagerComponent)` on the truck finds only the ~40 L
  ENGINE tank from `Vehicle_Base.et`. Both the client gates and the server transfer therefore go through
  `SCR_FuelManagerComponent.GetAllFuelManagers` (wrapped as `OVT_FuelUtils.CollectFillableManagers`),
  which walks the slot manager. **[SUPERSEDED BY A2.1 — that was the wrong fix and both helpers are
  deleted; see the A2 note above. The finding about where the cargo tank lives still stands.]** **Any future code that asks "what tanks does this vehicle have?" must do
  the same** — the naive answer is silently wrong on exactly the vehicle that matters.
- **Source ranking deliberately departs from vanilla: FREE FIRST, then priority, then distance.**
  `GetClosestValidSupportStation` ranks on priority alone, so a Fuel Depot (HIGH) near a commercial pump
  (VERY_HIGH) would be shadowed and the player charged for fuel they already own — R13 by construction.
  Because this action prices the fill up front it can simply prefer the source that costs nothing.
- **Self-exclusion is ours, not `m_bIgnoreSelf`.** That prefab flag only affects the vanilla `IsValid`
  path. `OVT_FuelUtils.IsStationMountedOn` compares owner, both root parents and shared roots, so the
  depot cannot fill itself out of itself and a truck cannot top up its own cargo tank from its own cargo
  tank.
- **Cost model (new decision D12, below).** `OVT_FuelPricing.ComputeFillPlan` takes the minimum of need,
  source stock and wallet, and the money limit REDUCES the fill rather than refusing it. In the
  affordable branch the cost rounds UP, which is provably safe because the balance it is compared
  against is an `int`; in the money-bound branch the cost IS the balance. Either way the caller may take
  it without re-checking — but `PlayerHasMoney` is asserted before the take anyway, because
  `TakePlayerMoneyPersistentId` floors at 0 and would report an overdraw as a success.
- **Charged for what ARRIVED, not for what was planned** (`CostForDelivered` scales down and floors), so
  a target whose nodes moved between the capacity read and the transfer is never billed the difference.
- **Label has three states**, and the third is not cosmetic: paid-with-something-to-sell shows
  `Fill Tank ($X)` with the CLAMPED figure (a player with $12 at a $1/L pump reads $12, not the $60 a
  full tank costs); a resolved free source shows `Fill Tank (Free)`; a full tank / dry pump / empty
  wallet shows the bare name, because `($0)` reads as free and would contradict the reason string under
  it.
- **Gates:** HIDDEN when the entity has no fuel manager or nothing covers it (this is what keeps it off
  every car on the map); VISIBLE-with-reason for tanks-full, source-dry and broke-at-a-paid-source. The
  dry case costs a second registry walk on purpose — an empty depot must not look like no depot.
- **Replication is inherited, not added.** The transfer is `SCR_FuelNode.SetFuel` on the server, which is
  literally the same call in the same place as vanilla's `OnExecutedServer`; money rides the existing
  economy stream. No RplProp, no JIP state, no new replicated field, no `CONFIG_STREAM_VERSION` bump.
  ⚠️ The one thing no gate proves is that a REMOTE client sees the gauge move — vanilla's own refuel
  depends on the same engine behaviour, so a failure here would be a vanilla failure, but it is play-test
  step I9.
- **Prefab wiring.** Vehicle: `Prefabs/Vehicles/Core/Vehicle_Base.et`, entry `{6B4E1F1000000010}` +
  UIInfo `{...11}`, on the **`"fuel_cap"` context**. The Sell-All *mechanism* was mirrored (same delta
  prefab, same `additionalActions` block, `VisibilityRange -1`) but not its door contexts — a fuel action
  belongs where the vanilla Refuel already is, and `fuel_cap` is where every fuelable vehicle has a point.
  No `Sort Priority` is set, so vanilla Refuel keeps its place in the list. Depot:
  `OVT_FuelDepot.et`, entry `{6B4E1F100000000E}` + UIInfo `{...0F}`, on its own `"default"` context
  (the depot has no `fuel_cap`). `Duration 5` on both — the `OVT_RearmVehicleAction` hold shape.
- Loc: 4 keys `{6B4E1F300000000D..10}` inserted **alphabetically** before `OVT-FlipVehicle`; the
  pre-existing unbalanced brace at `{6B09A1C0E4D50121}` was left alone and the file's brace delta is
  unchanged at 1. The broke reason reuses `#OVT-Refuel_CannotAfford`. Field Manual: one sentence into
  `OVT-FieldManual_Fuel_Text`, every clause cited in that key's Comment.
- Gate: `tools/compile-check.sh` exit 0 (6143 files). New Logic case `OVT_TEST_Logic_Fuel_FillPlan`
  red-proven by inverting the source-clamp assertion (compile-check exit 0 with the inversion, then
  restored). Suites left to the orchestrator.

### 2026-08-18 16:05 — Phase 3 amendment: depot model swapped on user feedback

- **`OVT_FuelDepot.et` re-based from the FIA mobile water tank to `{2D92D7E09B3424BC}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_02/FuelTank_02_green.et`** (user's choice). Root class `GenericEntity` -> `SCR_DestructibleBuildingEntity`; root ID `F0DBA538AC2A0552` -> `51BA89020930D32D` (mirrors the immediate parent, the same rule `OVT_MedicalTent.et` follows). **Resource GUID `{6B4E1F1000000001}` and all twelve component GUIDs unchanged**, so buildables.conf, the persistence config and the test case needed no edit at all.
- New chain: `OVT_FuelDepot.et <- FuelTank_02_green.et <- FuelTank_02_Base.et <- Building_Base.et`. Supplies MeshObject, `RigidBody ModelGeometry 1 / Static 1`, `RplComponent Enabled 1` (from `FuelTank_02_Base.et`, proven by `check-placeables.py --strict`), `SCR_DestructibleBuildingComponent`, plus — new vs the water tank — a native vanilla `Persistence` component, `Occluder`, `WorldSubsceneComponent`, `Hierarchy`. **Zero collisions**: nothing in the chain declares a fuel manager, support station, actions manager, sound or editable-entity component, so all seven of ours are genuine additions.
- **Do NOT use the sibling `FuelTank_02_Pump_green.et`.** `FuelTank_02_Pump_Base.et` attaches a full vanilla `FuelStation_E_01_tankpistol` as a child at `socket_pump` — a 50,000 L `FuelSupportStation_FuelPump.ct` station at priority VERY_HIGH, which our modded charge path treats as a PAID pump. It would shadow our free HIGH station (R13 by construction) and hand the depot a second, charging source. The plain green variant is a pure prop and is the correct base.
- The new base brings the depot INTO LINE with the other FOB buildables rather than out of it: medical tent, recruitment tent and garage all derive `Building_Base.et` too, so the native `Persistence` component is the norm here, not a novelty.
- **Destruction behaviour genuinely changed and is NOT engineered around** (reported, not fixed): the water-tank variant authored `SCR_DestructionMultiPhaseComponent Enabled 0`, i.e. the depot was indestructible. The new chain uses `SCR_DestructibleBuildingComponent : DestructibleBuildingComponent_Metal_Small.ct` (MaxHealth 10000, DamageThreshold 100, persistent ruins) which SWAPS the entity for `FuelTank_02_green_Ruin.et` — a bare `StaticModelEntity` carrying none of our components. So a destroyed depot stops dispensing, loses its stored fuel, and is no longer an `OVT_BuildableComponent` entity at all; whether it comes back across a reload is untested and belongs to play-test. Upside: the station now finds a real `SCR_DamageManagerComponent` (`SCR_DestructibleBuildingComponent` extends it) and subscribes to `GetOnDamageStateChanged`, so a destroyed depot correctly reports `DESTROYED_STATION` and de-registers from the support-station registry - which the indestructible water tank never exercised.
- Action point re-derived for the new geometry: `Offset 0 1.3 -1.9`, `Radius 2.5`. Derived from `FuelTank_02_Base.et`'s interior query box (`m_vCenter Offset 1.5692 -0.6313 0`, `m_vScale 9 7 2.8`): long axis X ~9 m, shell half-width ~1.4 m in Z, top ~2.9 m. The point sits ~0.5 m clear of the long side (clean line of sight, not inside the mesh) at chest/valve height, mid-length so the station's 7 m range stays symmetric.
- **Range note for play-test, new with this model.** `SCR_BaseSupportStationComponent.IsInRange` measures station position -> ACTION position. Dispensing: the car's own fuel-cap point must be within 7 m of the depot ORIGIN, so on a 9 m tank park BROADSIDE, not off an end. Filling: the truck's station must be within 7 m of OUR action point on the -Z side, so park the truck on the same side the player stands. `m_fRange` stays 7 per the plan; if E15 finds it tight, that is the knob.
- ⚠️ **The one thing no gate proves: the base GUID `{2D92D7E09B3424BC}`.** `FuelTank_02_green.et` is referenced by nothing in the extracted reference tree and the extracted tree has no `.meta` files, so the GUID cannot be cross-checked - it comes from the user. `check-placeables.py` resolves parents by PATH, not GUID, so its green verdict does not cover it. If the GUID is wrong the failure signature is the depot failing to spawn: the new persistence case reports "BuildItem() built no Fuel Depot at ...".

### 2026-08-18 15:20 — Phase 3 complete

- Built by `component-developer-advanced`. GUID families minted (all grep-clean against this tree AND `../ArmaReforger`): prefab + components `{6B4E1F1000000001..0D}`, buildables entry + handler `{6B4E1F2000000001..02}`, loc `{6B4E1F3000000001..02}`. The persistence serializer reuses `{64C6E14228B31061}` verbatim, which is the file's own convention (already twice at `:108`/`:131`).
- **T3.6 landed as the plan's DOCUMENTED FALLBACK, not the full round trip, and the reason is structural, not timing.** `OVT_PersistenceManagerComponent.ReapplyLatestSaveData()` asks the persistence system for exactly ONE instance — the game mode entity (`request.Instances = {owner}`) — and its own doc comment excludes "world entities, characters, vehicles, placeables". A depot is a separate tracked root, so no re-application can ever restore it. The case builds → asserts the two prefab facts the vanilla serializer silently depends on → fills to 1234 L → real save → re-finds by sphere query → asserts the level. The reload half is owed to manual step F18.
- **The fallback case lives in `OVT_TEST_PersistenceRoundTripSuite`, NOT `OVT_TEST_PersistenceSuite`** as the plan sentence says. Two blockers: that suite's header forbids saves outright, and it is listed BEFORE the round-trip suite in `OVT_TestGroup_All.conf`, so a save there would break the round-trip capability gate's fresh-session precondition and turn the All group red. `LegacyBaseUpgrades_*` is the standing precedent for a save-taking, reload-free case in the round-trip suite.
- 🔴 **`BuildItem(..., -1)` was a null dereference before this phase and had no live caller.** `m_OnBuild` → `OVT_SkillManagerComponent.GiveXP(-1, xp)` → `GetPersistentIDFromPlayerID(-1)` = `""` → `GetPlayer("")` = null → `player.xp`. That aborts the invoker and takes `BuildItem()` down with it BEFORE `OVT_PersistenceTracking.Track()`, so a server-initiated build would never have been saved. Guarded `GiveXP` and its identical twin `TakeXP` with the economy manager's own `if(!player) return;` pattern (`DoTakePlayerMoney:1228`). Required to make the specified test path work at all.
- Prefab notes for play-test: the action context is the FIRST thing to tune if the Refuel action does not appear (R12) — current offset/radius and the geometry they were derived from are in the 16:05 amendment above (the water-tank values this bullet used to quote are superseded, as is its "indestructible" claim). The depot's station leaves `m_eFactionUsageCheck` at 0 (BaseFuelSupportStation.ct never sets it), so anyone may draw from it — which is D4 by construction.
- ⚠️ **Pre-existing, NOT introduced here:** `Language/localization_Overthrow.st` is missing one closing brace — `{6B09A1C0E4D50121}` (`OVT-GMIcon_Tooltip_NoData`) never closes, so `OVT-StartUprising` and `OVT-SupportTooLow` are nested inside it. Present at HEAD (brace count 816/815 before this phase, 821/820 after — delta is exactly the two keys added). Left alone; belongs to whoever owns the GM-icons work.

### 2026-08-18 13:55 — Phase 2 complete
- Built by `component-developer-advanced`, one orchestrator-directed amendment: the out-of-money toast (`m_bNotifyOutOfMoney`, renamed from `m_bLastChargeClamped`) now fires on the **ordinary run-dry path** too — the run-dry test uses the same `GetTickCost(price, m_fPendingActionDuration)` helper as the gate, so toast and gate refusal cannot disagree. Original spec only toasted on a concurrent-debit race, which made DoD F4 unobservable in SP.
- persId resolution: `OVT_PlayerManagerComponent.GetPersistentIDFromControlledEntity` (`:551`) — returns "" for AI/recruits (D7 for free); backing map populated client-side too (SetupPlayer + RplLoad), which is what makes the client gate answerable.
- `LocalPlayerHasMoney` is declared `int` — consume via `if(!...)`, never return it as bool (precedent `OVT_RearmVehicleAction.c:83`).
- Loc text deliberately reads "You can no longer afford fuel - refuelling stopped" (not "ran out of money") — the gate strands ~one tick's worth of dollars (R10), so "ran out" would be a shipped lie. Comment in the .st says don't revert.
- New loc keys render raw until the `en-us.conf` Workbench re-export (known follow-up, in Needs-human-verification).
- JIP note: before config RplLoad lands, client price reads 0 → label briefly lacks the suffix, local gate passes, server still charges correctly. Expected window.

### 2026-08-18 13:00 — Phase 1 complete
- Built by `component-developer`. Two notes worth keeping:
  - **`OVT_ConfigManagerSerializer` was deliberately NOT extended** — the save blob's hand-rolled difficulty field list (`vehiclePriceMultiplier` at `:73/:113/:142`) does not carry `fuelPricePerLitre`. A loaded save re-reads the price from the resolved preset/config, not from the save. Fine today; revisit only if per-campaign price pinning is ever wanted.
  - **`EstimateTickCost` applies the $1 floor even for a 0-litre tick** at non-zero price — conservative-gate direction, matches spec ("floor of 1; zero price → 0").
- Gate: All 283/284; single red = documented Setup_Checkpoint I/O flake, green on targeted single-case re-run.

### 2026-08-18 12:45
- `/autorun-feature economy/fuel` (via Discord). Scaffolded docs from the existing plan; starting Phase 1 with `component-developer`.

---

### 2026-08-18 — Phase 4 complete

- The registry accessor reads `m_mSupportStations` (the protected map on the vanilla manager) directly rather than calling the protected `GetArrayOfSupportStationType`. That vanilla accessor **rebinds its out parameter to the manager's own array** (`m_mSupportStations.Find(type, supportStations)`), so a caller ends up holding a live view of the registry — and it also inserts an empty bucket for any type it is asked about. Walking the map and inserting into the caller's array avoids both.
- `FindFuelSourcesCovering` uses the station's own `GetRange()` and `GetPosition()` (owner position + authored offset); `FindFuelSourcesNear` uses the caller's radius. Both clear the caller's array on **every** return path — a reused array must never hand high-command last tick's stations. That clear contract is the new Logic case (`OVT_TEST_Logic_Fuel_SourceQueryEmptiesResults`); the "a pump 5 m away IS found, with a non-zero price" half needs a loaded world and stays play-test gated (DoD I4).
- Loc audit: all five keys (`OVT-Refuel_PriceFormat`, `OVT-Refuel_CannotAfford`, `OVT-Msg-CannotAffordFuel`, `OVT-Build_FuelDepot`, `OVT-Build_FuelDepot_Description`) present in `localization_Overthrow.st` and each referenced from code/configs. The `en-us.conf` re-export remains the standing follow-up.

---

### 2026-08-18 — Phase 5 (in-game half complete, wiki blocked)

- **Field Manual:** new page **Fuel** in the *Money and Trade* category, after Gun Dealers (`Configs/FieldManual/Categories/FM_Overthrow.conf`, entry `{6B4E1F4000000001}`, pieces `{6B4E1F4000000002..08}`, tile reuses the existing `default_ui.edds` - no new art). Four paragraphs: paid static stations and the rate on the label, running out of money, the free paths, the depot. Keys `OVT-FieldManual_Fuel_Title|Text|Head|Text2|Head2|Text3|Head3|Text4`, GUIDs `{6B4E1F3000000003..0A}`.
- **Tutorial:** new popup `Configs/Tutorials/fuelDepotBuilt.conf` (+ `.meta` `{6B4E1F5000000001}`, page `{...02}`, trigger `{...03}`), registered as `{6B4E1F5000000004}` on `Prefabs/GameMode/OVT_OverthrowGameMode.et`. Trigger `PLAYER_BUILD` with `m_sFilter "Fuel Depot"` - the filter is `OVT_Buildable.m_sName` verbatim (`OVT_TutorialManagerComponent.OnBuild:317-324`), so it must stay in sync with `Configs/Resistance/buildables.conf:90`. Deep-links to the new FM page via `m_sFieldManualTitleKey "#OVT-FieldManual_Fuel_Title"`. Keys `OVT-Tutorial_FuelDepotBuilt_Body|Title`, GUIDs `{6B4E1F300000000B..0C}`.
- **Deliberate gap:** the tutorial trigger catalogue (`OVT_TutorialTrigger.c:12-45`) has no refuel/vehicle/support-station event, so "fuel now costs money" gets no popup. In game it is carried by the price suffix on the Refuel action, the greyed-out `#OVT-Refuel_CannotAfford` reason, the one-shot toast and the new FM page. Adding a `PLAYER_REFUEL` event belongs to the tutorial-system feature, not here.
- 🔴 **Wiki sync could not run.** The `wikijs` MCP tools were not exposed to the sync session, and the token in `Overthrow.Wiki.MCP/wiki-js-mcp/.env` is scoped to `pages.list` only: `pages.single` returns `PageViewForbidden 6013`, so nothing can be read or written with it. Owed wiki work, with page ids: create `fuel` (no existing page covers it - the flat list has no vehicles/fuel page); add a **Fuel Depot** bullet to the "advantages of owning a base" list on `base` (id 11); add `fuelPricePerLitre` to the Economy table on `difficulty/settings` (id 53) and to the preset pages `difficulty/easy` (49), `difficulty/normal` (50), `difficulty/hard` (51), `difficulty/extreme` (52). Numbers to publish: Easy 0.5, Normal 1, Hard 1.5, Extreme 2.5, Insane 4, 0 disables charging. The wiki text must match `OVT-FieldManual_Fuel_*` word for word on the facts.
- Pre-existing `.st` brace imbalance (`{6B09A1C0E4D50121}`, noted in the Phase 3 note) is still there and was left alone; the delta from this phase is exactly balanced.
- Gate: `tools/compile-check.sh` exit 0 (6140 files).

---

*Update this file at the end of each work session.*
