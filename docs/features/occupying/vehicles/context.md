# Vehicles - Context & Decisions

**Last Updated:** 2026-08-23
**Current Phase:** Phase 7 complete — all seven phases built (59/59). Feature-close gate outstanding.
**Status:** 🟡 In Progress

---

## Quick Status

**What's Done:**
- ✅ **Phase 6 complete (6/6).** `ReportVehicleLoss(vector)` on the occupying faction manager, hooked from ONE place - `OVT_InsertionSpawningDeploymentModule.ReleaseConvoy()`, gated on `m_Truck && !IsTruckOperational()` - which covers the mounted module's HOLDING vehicle-loss branch and the inherited DRIVING one without a second call site, and does not fire from a normal teardown; `OVT_ArmouredSweepBehaviorDeploymentModule` (its own single clock, `m_iSweepMinutes` - Phase 2's "one clock, not two" question resolved by NOT authoring `m_iHoldTicks` on the config); `Deployment_HunterKillerSweep.conf` + its registry entry (registry now carries **27**); `TickHunterKiller()` on the existing 60 s tick, four gates, create-then-debit at one call site; 1 new Init case file (4 cases). `compile-check.sh` exit 0 (6340 files). Suite deferred. **This is the last code phase — only Phase 7 (help/docs/localization) remains.**
- ✅ **Phase 5 complete (8/8).** `m_OnBattleStarted` invoker on the occupying faction manager, published from both `StartBaseQRF` and `StartTownQRF` for both QRF modes; `m_sVehicleRole` + `ReleaseVehicleOwnership()` on the parked-vehicle module (byte-identical G8 fallback verified by construction - the original method body is untouched, just renamed and called from behind the new role check); `OVT_CrewUpOnAlarmBehaviorDeploymentModule` (the ownership-transfer contract: release only after `AdoptVehicle()` succeeds); `Deployment_BaseArmourSortie.conf` + its registry entry; the "Base Parked Armour" registry delta; one `vehicle_crew` group entry + one new group prefab per faction; 1 new Init case file (3 cases + a shared fixture). `compile-check.sh` exit 0 (6338 files). Suite deferred. **One acceptance grep changed the shape of the unsubscribe code** - "present in both cleanup paths" is literal, so both `OnDeactivate()`/`OnCleanup()` carry their own inline `.Remove()` call rather than a shared helper. **One can-fail proof did not go red** - the ownership-transfer reordering fault compiles clean AND produces the same end state, an honest tier limitation recorded in full below.
- ✅ **Phase 4 complete (10/10).** `Deployment_QRFMountedEchelon.conf` + its registry entry, `SendMountedEchelon()` and six helpers on the QRF controller, one call site in **each** mode inside the existing `allocated` accumulation, `DeleteDeployment`-only teardown on `m_OnFinished` **and** on `OnDelete`, 1 new Init case file (5 cases + a shared fixture). `compile-check.sh` exit 0. Suite deferred. **The single reserve debit is still single and still outside the mode branch.** **Four plan defects found — see Phase 4 below; two of them (a slice expression that is negative in ordinary play, against a ladder that reads a negative budget as UNBOUNDED, and pricing a rung against a smaller budget than the module will use) would have been silent money faults.** T4.7 verdict: **zone scoring DOES count a mounted crew — nothing changed.**
- ✅ Planned. `implementation.md` is 7 phases with C1–C9 working-tree corrections verified 2026-08-23 on `v1.5`.
- ✅ Scaffolded: `tasks.md` (59 tasks), this file.
- ✅ **Phase 3 complete (8/8).** `OVT_MobileCheckpointBehaviorDeploymentModule`, `OVT_CheckpointApproachRules`, `Deployment_ObjectiveHarassment_Mounted.conf` + its registry entry, the rung appended last in **four** objective-plan ladders **and in the director's own `HARASSMENT_LADDER` constant** (a fifth edit site the plan does not name), 1 new Logic case file (4 cases), 1 new Init case + 2 extended ones. `compile-check.sh` exit 0. Suite deferred. **Four plan defects found — see Phase 3 below; one of them (a missing town-harassment module) would have deadlocked every town objective at the top of its own ramp.**
- ✅ **Phase 2 complete (12/12).** `OVT_MountedForceSpawningDeploymentModule`, one appended enum value on `OVT_EInsertionState`, `ForceCreateDeploymentFrom(...)`, 5 new Init cases + 1 appended battle-suppression case. `compile-check.sh` exit 0. Suite deferred.
- ✅ **Phase 1 complete (10/10).** `OVT_VehicleLadderRules`, the two registry fields, `vehicleThresholdScale`, six difficulty presets, both faction registries (+ C7 corrections), two prefab deltas, Logic + Init cases. `compile-check.sh` exit 0. Suite deferred per policy.

**What's Next:**
- ✅ **Phase 7 complete (5/5).** Field Manual: an *Armour and Checkpoints* section on the existing "Patrols and Garrisons" page (4 pieces, 4 new `.st` keys). **Tutorial popup SKIPPED** - no trigger for "the player has seen an occupying armed vehicle" exists and authoring one would need new tutorial-framework code (full reasoning in Phase 7 below). Wiki NOT published - no `wikijs` MCP server in the session; the draft is `wiki-draft.md`. Epic overview carries row 7.
- ⏸️ **Feature-close gate**: the deferred **All** suite run, the DoD greps, and the human verification list below.
- ⏸️ **Phases 3, 4, 5 and 6 are play-testable and have not been played.** Their checklists are at the end of each phase's write-up (Phase 6's is owed - see below). Phase 4's steps 2-4 are the first real test of the observer repair on a force that has to drive kilometres unobserved; step 8 is the **only** way `SiegeEchelonAnchor` is exercised at all. Phase 5's step 3 is the only way to catch `AdoptVehicle()` silently falling through to a fresh spawn. Phase 6's own risk is the same class as Phase 4's step 3: `TickHunterKiller()` never drives a vehicle anywhere in the automated tiers, so whether a real hotspot actually buys a sweep, and whether the sweep actually loiters instead of parking once, is entirely unobserved until play-test.

**Blockers:**
- None.

---

## Play-test round 1 (user, 2026-08-23) — three reports, two fixed, one confirmed against the log

Server log: `console (2).log`. Only **one** mounted deployment ever ran in it — the hunter-killer sweep —
which makes every line below attributable.

**1. Too many vehicles, too early — FIXED (tuning).** The ladder shipped with its bottom rung at threat
**0** and `baseThreat` on Normal is **100**, so armed vehicles were available from minute one. The log
proves it: *"role 'armed' at threat 152 resolved to 'light_armed'"*. Every `m_iMinThreat` moved up a step
in both faction registries — bottom 0 → **400**, middle 400 → **900**, top 900 → **1500**.

⚠ **The threshold alone was not enough.** `GetVehiclePrefabFromFaction()` fell back to the authored
`m_sTruckVehicleType` on a ladder miss, so below 400 the three ladder doctrines would have driven an
**unarmed Ural** out and parked it as a mobile checkpoint — the same traffic, minus the gun. New attribute
`m_bWalkWhenNoLadderRung` (defvalue 1) returns an empty `ResourceName` instead, which is one of the
documented roads to walking. Authored **1** on harassment / echelon / hunter-killer, **0** on the base
armour sortie (that one adopts a hull already parked; the ladder never decides whether it drives).

⚠ **A second, pre-existing source of vehicle traffic is visible in the same log and was NOT touched:**
`Light Vehicle Patrol` (`Deployment_VehiclePatrol_Light.conf`, `m_fChance 2`, no threat floor) respawned a
`light_armed` at Chotain twice in 20 minutes. It belongs to `deployments`, not to this feature.

**2. A hunter-killer sweep was sent at a radio tower the player owns — FIXED.** Log: *"Hunter-killer sweep
sent to <6156, 5252> (score 44)"*. `UpdateKnownTargets()` inserts a `BROADCAST_TOWER` target for every
tower the occupying faction does not hold, and `PickHunterKillerTarget()` scored it like any other, so an
undefended tower could out-score everything else and buy a fighting vehicle. Towers are now skipped in the
picker. The objective director's own tower recapture is untouched — that is a specops job and stays one.

**3. 🔴 CONFIRMED, NOT YET FIXED — the mounted force is far bigger than the vehicle carrying it.**
User: *"when I neutralized all of the guys in them a short time later 2 'crewmen' teleported to it and got
into it ... I've also seen some crewmen walking the streets without a vehicle"*.

The arithmetic, all authored:

| | men | source |
|---|---|---|
| `vehicle_crew` | **3** | `OVT_Group_USSR_VehicleCrew.et` — three `Character_*_Crew`, so any of them reads as a "crewman" on sight |
| `light_fireteam` | **4** | `Group_USSR_LightFireTeam.et` (`m_iMinGroupCount/m_iMaxGroupCount 1` on the sweep) |
| `light_armed` UAZ-PKM | **3 seats** | `m_iMaxCapacity 3` in the faction registry |

**Seven men are loaded into a three-seat car.** `SeatRider` cascades PILOT → TURRET → cab → cargo and then
simply returns false; the four it cannot seat stay on their feet. Every one of them is registered at
`RIDING_SPAWN_DISTANCE` (100 km) — *always materialised* — and `TickHold` calls
`NudgeCrewMaterialisation()` → `ForceSpawn(m_iCrewHandle)` **every tick**, so a crew slot the survivor mask
still counts alive but that has not materialised pops into the world beside the parked vehicle and boards
it as a seat frees up. That is the observed teleport, and the log's timeline matches: the sweep arrived
04:54:52, and the crew was not reported wiped until **05:10:00** — fifteen minutes and (by the user's
account) two separate engagements later.

The men on foot are the same arithmetic seen from outside: unseated riders of a `vehicle_crew` group,
carrying the crew group's own MOVE waypoint.

**FIXED — user's call, 2026-08-23: _"it should only be the crewmen, a driver and a gunner ... whatever the
default crew of the vehicle is, and that's across all of the vehicle deployments including QRF unless it's a
truck/insertion."_** A mounted force is now **its crew and nothing else**:

- All four mounted configs carry `m_sGroupType ""`, `m_iMinGroupCount 0`, `m_iMaxGroupCount 0`,
  `m_iCostPerGroup 0`. Nobody can be left unseated because nobody is loaded who does not have a seat.
- The crew stays the authored **3**-man `vehicle_crew` (driver, gunner, commander) — the vehicle's default
  crew, not a cut-down pair.
- The rule extended past this feature, per the same instruction: `Deployment_VehiclePatrol_Light.conf` and
  `Deployment_VehiclePatrol_Heavy.conf` were crewed by a 4-man `light_fireteam` / a `light_patrol` and now
  use `vehicle_crew` too. **Truck and insertion configs are the stated exception and were not touched** —
  the ten configs authoring the 2-man `truck_crew` are unchanged.
- Costs drop with the groups: the hunter-killer sweep is now 150 rather than the 190 the log shows.

⚠ **Two consequences that needed code, not config:**

1. **A crew-only force has nobody left when its crew dies.** The crew is registered under its own owner key
   and no wipe is ever attributed to the module, so without the passengers the deployment would sit in
   `WALKING` with zero men and be **refunded as if intact**. `OVT_MountedForceSpawningDeploymentModule`
   now overrides `DismountAndWalk()` and marks itself eliminated when the crew was the whole force. The
   crew-lost test is taken **before** `super`, because `ReleaseConvoy()` hands the registration back and
   `IsCrewAlive()` answers false for everything afterwards — a force that lost only its *vehicle* still has
   men and must keep its deployment.
2. **The walk fallback became nonsense for these configs.** With no passengers, a ladder miss under
   `m_bWalkWhenNoLadderRung` marches three crewmen to the objective on foot. New
   `OVT_OccupyingFactionManager.CanFieldLadderVehicle(role = "armed")` answers "has the campaign escalated
   far enough for the ladder to answer anything" (unbounded budget — each module still checks its own
   ceiling), and gates all three runtime dispatchers: `TickHunterKiller()` (gate 2b),
   `OVT_QRFControllerComponent.SendMountedEchelon()` (a `RefuseEchelon`), and the director's
   `CanSendObjectiveDeployment()` for `Objective Harassment (Mounted)` only, under a new
   `REFUSAL_LADDER_LOCKED`, asked **before** the pool question. Below threat 400 the harassment ramp simply
   tops out at rung 4, which is the pre-`vehicles` behaviour; rungs 1-4 are untouched, so no ramp stalls.

---

## ⚠ Test suites are DEFERRED for this whole feature

The user is running **Workbench / play-test sessions on `v1.5` for the duration of this build** (instruction, 2026-08-23). `tools/run-tests.sh` launches a real Reforger client that **steals desktop focus for ~15–20 s** and returns **INDETERMINATE (exit 2)** — not red, not green — when a Workbench session is concurrent.

So, for every phase of this feature:

- **Per-phase gate = `tools/compile-check.sh` exit 0 only.** Announced, not silent.
- **The All group `{6A6E2A002F53A581}` runs once, at the very end**, as the feature's single regression gate (`tasks.md` → *Feature-close gate* G-2).
- A phase that closes with only compile-check green is honestly reported as **"suite deferred"**, never as "tests pass".

This is `.claude/test-policy.md` §2's *Defer the gate* clause, applied for the whole feature rather than one phase. The cost is that a defect introduced in Phase 1 may not surface until the Phase 7 run — accepted deliberately, and the reason every phase's Init/Logic cases are still **written** on schedule even though they are not **executed** until the end.

---

## 🔴 RISK TO THE PREMISE (raised 2026-08-23, mid-build) — unobserved vehicles may not be simulated

Raised by a user play-test on `v1.5` during this feature's build. Full diagnosis in
`docs/features/occupying/objectives/context.md` → *"Play-test 2026-08-23"* and its CORRECTION section.

**In one line:** an insertion transport at Chotain — a spawn point the user has watched work "a hundred times"
— never moved at all, with a crew that was alive, materialised, AI-active and correctly pinned at LOD 9. The
only variable was that **nobody was watching**: nearest player 1931 m, no GM camera.

**The load-bearing evidence:** `VehicleWheeledSimulation.ForceEnableSimulation()` exists as an engine proto,
documented *"Forcibly enables simulation of vehicle, only meant for cinematics, not to be used in any game
logic!"*, with **zero callers in the whole vanilla script tree**. A proto that exists only to force vehicle
simulation on proves vehicle simulation is conditionally **off**. The governing budget is
`DYNAMICSIM_LASTLOD_DISTANCE`, defaulting to ~1000 m; the stall was at 1931 m.

**Three cheaper explanations are already dead** and must not be re-opened — the crew/spawn gate (the log
reports full health), the LOD pin (`SCR_AIDecideBehavior.s_aUpdateIntervals` saturates at 2.0 s from LOD 2, so
pinning deeper than `maxLod - 1` buys nothing), and AI comms (`SCR_AICommsHandler.CanBypass` at LOD ≠ 0 is a
success short-circuit, not a block).

### Why this feature cares more than any other

Every mounted deployment here is **specifically designed to drive unobserved**:

| Rung | What it needs to do unobserved | What it degrades to |
|---|---|---|
| Mounted harassment (P3) | drive out, park, **relocate** every few minutes | parks once and never moves |
| **QRF mounted echelon (P4)** | **really drive from a base ~2 km away so it can be ambushed (F4)** | never arrives; the force walks |
| Armour sortie (P5) | drive one leg out of the base | crews up and sits |
| Hunter-killer sweep (P6) | loiter around a hotspot (P6) | does not sweep |

Every one of those degrades to the **walk fallback**, which is *correct behaviour* (G4/F8) and therefore
**silent**. The feature would look like it works and quietly not deliver G2 ("AT has a job") or F4.

### Stance for the build — UNCHANGED, and deliberately so

**Nothing in the plan changes and no phase is re-scoped.** The mounted spine, the ladder, the accounting and
the fallback are all correct either way, and the fix — if the experiment confirms it — is a
`DYNAMICSIM_LASTLOD_DISTANCE` value on four vehicle prefabs this feature already owns same-GUID deltas for
(`Ural4320_transport`, `BTR70`, and P1's new `BRDM2` / `LAV25`). That is a prefab-property edit in exactly the
place the friction tuning already lives, not a code change.

⚠ It is a **Workbench** edit: `DYNAMICSIM_LASTLOD_DISTANCE` has **0 hits as text** across both trees, so it
cannot be authored or verified by grep.

**Owed:** the decisive experiment (GM camera at ~900 m vs ~1200 m from a driving convoy — does it stop on
crossing ~1 km and resume on approach?) before this feature's play-test, so the play-test is not spent
rediscovering it.

### ✅ ROOT CAUSE FIXED 2026-08-23 — and G6 is INTENTIONALLY BROKEN

The lever turned out **not** to be `DYNAMICSIM_LASTLOD_DISTANCE` (that is a *character* prefab property
governing AI agent LOD, the axis already proved irrelevant, with 0 text hits in either tree — the paragraph
above is struck). It is BI's own documented one: `ObserversSystem.InsertObserverSP`, *"Temporary observers can
keep distant entities simulated, so be mindful of their lifetime."* A **Game Master camera is an observer**,
which is precisely why the convoy always drove when watched.

`OVT_InsertionSpawningDeploymentModule` now parks core's entity observer on the transport while it drives
(`HoldTruckSimulated()`, called from `EnsureConvoy()`) and drops it at `ReleaseConvoy()`, behind a new authored
off-switch `m_bTransportIsObserver` (default **on**). **Every mounted deployment here inherits it**, including
the adopted-hull branch — the add deliberately sits in `EnsureConvoy()` rather than in `SpawnTruck()` for
exactly that reason. Full write-up, leak audit and test limits:
`docs/features/occupying/objectives/context.md` → *"✅ FIXED 2026-08-23"*.

⚠ **This breaks G6 on purpose.** `git diff` on the insertion module is no longer "one appended enum value" —
it is that plus an attribute, a method, two call sites, three clone-count corrections and a header rewrite. The
user was consulted and left the call to the implementer, and the break is recorded here rather than discovered
later. **DoD Q1's grep will now legitimately fail and must be RE-STATED, not "repaired"**: the honest form of
the guarantee is "this feature's own phases appended one enum value; the observer repair is a separate,
documented change landed alongside it". Nothing about D1 (subclass, do not refactor) changes.

⚠ Two of this feature's own files were touched by the repair, because `CloneModule` is not chained:
`OVT_MountedForceSpawningDeploymentModule.CloneModule()` now copies **28** fields, not 27 (the parent's ten
became eleven), and `OVT_TEST_Init_MountedForce.c`'s clone case asserts the new one.

⚠ The mounted `HOLDING` state keeps the observer for **the deployment's** lifetime rather than the drive's,
because `ReleaseConvoy` is not reached until teardown. Defensible — a parked echelon is manned and may be
ordered to relocate — but it is this feature's call to make deliberately, not the repair's.

---

## Decisions carried in from the plan

The full rationale lives in `implementation.md` §5 (D1–D10). The load-bearing ones for anyone touching this code:

- **D1 — Subclass, do not refactor.** `OVT_MountedForceSpawningDeploymentModule : OVT_InsertionSpawningDeploymentModule`. The convoy path is three play-test rounds and seven diagnosed faults deep; copying or extracting it restarts that clock. `git diff` on the insertion module is **one appended enum value** for the entire feature (G6/Q1).
- **D4 — The vehicle price is a budget, not a receipt.** `GetResourceCost()` runs off the config *template*, before a deployment exists and therefore before the faction is known, so the rung cannot be resolved at pricing time. The module charges the authored `m_iTruckCostOverride` and then **refuses any rung dearer than it**. A config that wants a BTR-70 has to author ≥ 120.
- **D6 — An echelon is created with zero invested and torn down with `DeleteDeployment`.** `RecallDeployment` and `CollectDeployment` both credit `AddFactionResources` — the **pool** — while a QRF wave debits `m_iResources`, the **reserve**. Collecting an echelon would create money across two ledgers (C6).
- **D8 — Battle suppression needs no whitelist, and that is fragile.** `SuppressForcesAroundBattle` skips any handle whose spawn distance is *strictly wider* than the global ring, and riding crews register at `RIDING_SPAWN_DISTANCE = 100000`. The mounted module is exempt **only for as long as it does not call `DropPassengersToGlobalRing()`**. One call throws the exemption away and pins the force dormant inside its own battle. T2.11 is the mechanical guard.
- **D9 — Nothing about live vehicles is persisted.** A restored mounted deployment walks. No serializer, no version bump.
- **D10 — Reachability is bounded, not solved.** No A→B land-reachability query exists anywhere in the tree; every water check is a **point** test. The echelon uses the authored `landIsolated` flag plus `FindNearestRoadSpawn` at both ends. A source with a road at both ends may still be unreachable across water — the crew stalls, the stuck test fires, **the force walks**. Degradation, not failure.

### D11 (new, 2026-08-23) — a HOLDING mounted force KEEPS its transport observer for the deployment's life

The observer repair drops the observer in `ReleaseConvoy()`, this file's single audited teardown. For an
ordinary insertion that is the end of the drive. For a **mounted** force, `ReleaseConvoy` is not reached until
the deployment ends — so the observer's lifetime becomes the **deployment's**, not the drive's. The repair
agent flagged this as this feature's call. **It is deliberate and it stays.**

**Why.** Every mounted doctrine here has to *act* while nobody is watching, and that is the whole point of the
feature:

- the mobile checkpoint **relocates** to a new approach on its clock;
- the gunner has to man a turret and engage;
- the hunter-killer **loiters** on patrol waypoints around a hotspot;
- the armour sortie **drives a leg** out of its base.

Dropping the observer on arrival would re-introduce the exact bug for the second half of every mounted
mission — the vehicle would arrive, park, and then be unable to move or fight until a player walked up to it.
That is worse than the original defect, because it would look like correct behaviour.

**The cost, stated.** An observer keeps everything near it simulated. Three of the four doctrines are already
bounded — the sweep by `m_iHoldTicks`, and the QRF echelon and the armour sortie by the battle's end
(`DeleteDeployment` on `m_OnFinished`). **The mobile checkpoint is the only indefinite one**, and it is bounded
in practice by the harassment ladder's own instance caps. `m_bTransportIsObserver` is the off-switch if a
play-test shows the cost is real, and it is authored per-config.

**Owed at play-test:** watch server frame time with two QRF echelons plus a checkpoint live, and confirm the
observers are gone after the battle ends (the repair's Init case proves the removal happens on teardown; it
cannot prove the teardown is reached).

---

## Working notes

### Phase 1

**Status:** ✅ Complete. `tools/compile-check.sh` exit 0 (6328 files). Suite deferred per the whole-feature policy above.

**Files:**
- NEW `Scripts/Game/Data/OVT_VehicleLadderRules.c` — the four pure statics, no `OVT_Global`/`GetGame()`/`World`/`Entity` identifier anywhere (verified: `grep -n "OVT_Global\|GetGame()\|World\|Entity"` on the file is empty).
- TOUCH `Scripts/Game/Faction/OVT_Faction.c` — `m_sLadderRole` / `m_iMinThreat` on `OVT_FactionVehicleEntry`; `OVT_FactionVehicleRegistry.ResolveLadderRung(...)`; `OVT_Faction.ResolveVehicleForRole(...)`.
- TOUCH `Scripts/Game/Configuration/OVT_DifficultySettings.c` — `float vehicleThresholdScale`, category `"Occupying Faction"`, `defvalue "1"`.
- TOUCH `Configs/Difficulty/Difficulty_{Easy,Normal,Hard,Extreme,Insane,TestWorld}.conf` — `vehicleThresholdScale` authored explicitly in all six.
- TOUCH `Configs/Factions/{USSR,US}_OverthrowData.conf` — see rung table below.
- NEW `Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et(.meta)`, `Prefabs/Vehicles/Wheeled/LAV25/LAV25.et(.meta)` — same-GUID deltas.
- NEW `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_VehicleLadder.c` — 4 cases (ScaledThreshold, RungUnlocked, RungAffordable, PickRung), world-free.
- NEW `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_VehicleLadderResolution.c` — a new small file rather than an append, since `OVT_TEST_Init_VehiclePriceSpecificity.c`'s subject (`OVT_EconomyManagerComponent` price specificity) is a different concern from faction-registry ladder resolution.

**The rung table as actually authored** (all role `armed`; `light_armed` / `heavy_armed` kept their names per G9):

| Faction | Entry | Prefab GUID | `m_iMinThreat` | `m_iCost` | crew / capacity |
|---|---|---|---:|---:|---|
| USSR | `light_armed` | `{0B4DEA8078B78A9B}` UAZ469_PKM.et | 0 | 25 | 2 / 3 |
| USSR | `medium_armed` **NEW**, entry GUID `{6BC1100000000001}` | `{254289B9C09904AB}` BRDM2.et | 400 | 70 | 2 / 4 |
| USSR | `heavy_armed` | `{C012BB3488BEA0C2}` BTR70.et | 900 | **120** | 3 / **10** |
| US | `light_armed` | `{F6B23D17D5067C11}` M151A2_M2HB.et | 0 | 25 | 2 / 3 |
| US | `heavy_armed` | `{3EA6F47D95867114}` M1025_armed_M2HB.et | 400 | **70** | 3 / **4** (explicit, matches the old default) |
| US | `heavy_armor` **NEW**, entry GUID `{6BC1100000000002}` | `{0FBF8F010F81A4E5}` LAV25.et | 900 | 120 | 3 / 9 |

This matches implementation.md §3.2's table exactly, including the two GUIDs (`{254289B9C09904AB}` BRDM2, `{0FBF8F010F81A4E5}` LAV25), which were cross-checked against the vanilla `Vehicles_EntityCatalog_{USSR,US}.conf` catalog entries (the extracted vanilla tree carries no `.meta` files, so the catalog's `m_sEntityPrefab` GUID is the only independent confirmation available) and matched.

**Name-asymmetry rationale (G9/D3).** USSR's new top rung is `medium_armed` (BRDM-2, threat 400 — it sits BELOW the existing `heavy_armed` BTR-70 at 900) while US's new top rung is `heavy_armor` (LAV-25, threat 900 — it sits ABOVE the existing `heavy_armed` M1025 at 400). The names read asymmetrically because they are: USSR already had a rung at the top of its ladder (`heavy_armed`/BTR-70) so the new entry had to slot in underneath it, while US's existing `heavy_armed` (M1025) is a mid-tier gun-truck and the new entry is the faction's first real armour, so it earns the "heavy" word instead. Renaming either existing entry was ruled out — `Deployment_VehiclePatrol_*` and `Deployment_BaseHeavyPatrol` (three shipped configs) consume `light_armed`/`heavy_armed` by literal name, and the ladder resolves by role (`m_sLadderRole "armed"`) and threshold, never by name, so the asymmetry has no functional cost.

**C7 corrections, with before values (confirmed against the working tree before editing):**
- USSR `heavy_armed` (BTR-70) authored **no** `m_iCost` and **no** `m_iMaxCapacity` before this phase — confirmed at `USSR_OverthrowData.conf:67-71` (pre-edit), so it took `OVT_FactionVehicleEntry`'s attribute defaults: `m_iCost` defvalue `"50"` and `m_iMaxCapacity` defvalue `"4"`. A BTR-70 (10-position hull) therefore cost the same as a UAZ-PKM plus 25 and only carried 4 of its 10 seats. Now explicit: `m_iCost 120`, `m_iMaxCapacity 10`.
- US `heavy_armed` (M1025_armed_M2HB) also authored **no** `m_iCost` before this phase — confirmed at `US_OverthrowData.conf:68-73` (pre-edit); it took the same `defvalue "50"`. C7's text in implementation.md only calls out USSR by name, but the working tree showed the identical defect on the US side, and implementation.md's own rung table (§3.2, "US heavy_armed ... 70 (was default 50)") already anticipated it — so this phase's fix matches what the table asked for. `m_iMaxCapacity` was already implicitly 4 (defvalue) and is now authored explicitly at the same value, per the task's "explicit `m_iCost` and `m_iMaxCapacity` on both `heavy_armed` entries" instruction. Now explicit: `m_iCost 70`, `m_iMaxCapacity 4`.

**Nothing in the plan proved wrong against the working tree this phase.** The Ural4320_transport.et tuning recipe, the BTR70.et same-GUID delta pattern, the `GetVehiclePrefabByName` shape, and the `OVT_TEST_LogicFixture` / `OVT_TEST_LogicSuite` conventions all matched implementation.md's description exactly.

### Phase 2

**Status:** ✅ Complete (12/12). `tools/compile-check.sh` exit 0 (6330 files). Suite deferred per the whole-feature policy above.

**Files:**
- TOUCH `Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` — **one appended enum value and its one-line doc comment. Nothing else, for the whole feature (G6/Q1).** The literal diff is `FINISHED` → `FINISHED,` plus a blank line, one `//!` line and `HOLDING`.
- NEW `Scripts/Game/GameMode/Deployments/Modules/OVT_MountedForceSpawningDeploymentModule.c`
- TOUCH `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c` — `ForceCreateDeploymentFrom(...)` + `ApplyMountedSourceOverride(...)`.
- NEW `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_MountedForce.c` — 5 cases + a probe subclass + a small fixture class.
- TOUCH `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_DeploymentBattleSuppression.c` — 1 appended case (the C3/D8 guard).
- **`git diff Configs/` carries nothing from this phase.** It is not empty, but everything in it is either Phase 1's (six difficulty presets, two faction registries) or a **concurrent session's** (`Configs/System/GunDealerConfig.conf`, `Configs/System/ShopConfig.conf`, written at 02:43 while this phase was compiling, plus `Language/`, `Prefabs/Props/` and `economy/shops` docs). No config authors the mounted module until Phase 3.

**T2.1 survey verdict: everything in C1/C2 and in the task list is exactly where the plan says it is.** Verified against the working tree at the start of the phase:

| Claim | Plan | Working tree |
|---|---|---|
| `OnInsertionArrived` | `:1893` | `:1894` (the `//!` block opens at `:1891`) — the hook is empty and is documented as *"THE HOOK A SUBCLASS OVERRIDES"* |
| `CompleteInsertion` | `:1861` | `:1861`, `protected` |
| `OnUpdate` | `:1053` | `:1053`, an **if-chain** with three tests and no `default` — `HOLDING` falls through all three as a no-op in the parent, as C2 predicted |
| `GetVehiclePrefabFromFaction` | `:3219` | `:3219`, `protected` |
| `CloneModule` | `:3304-3337` | `:3304`, 13 + 10 lines, not chained |
| `ReleaseConvoy` | `:1956` | `:1956` |
| `DropPassengersToGlobalRing` | `:3052` | `:3052` |
| Suppression skip | `OVT_DeploymentManager.c:565-568` | `:567` is the condition line (`spawnDistance <= 0 \|\| spawnDistance > GetGlobalSpawnDistance()`); the comment block above it is `:559-566` |

Nothing had moved. No re-baseline was needed.

#### The ownership split for a mounted force

Identical to the insertion module's, with one addition and one change of duration:

| | |
|---|---|
| **Owned** | the vehicle (deleted at teardown unless a player claimed it, or collected on the abandoned countdown) — **except** a hull handed in by `AdoptVehicle()`, which belongs to whoever handed it over until they release their own ownership; the crew's registration, under its own owner key, never counted as part of the force; the waypoints (`AIGroup.AddWaypoint()` does not take ownership); **one slot of the faction's convoy cap, released at arrival and not held for the length of the hold** |
| **Borrowed** | the passenger groups; the ladder; `vehicleThresholdScale`; the live threat figure |
| **Changed duration** | the vehicle is held for the life of the deployment rather than for the length of a drive. That is exactly why `CompleteInsertion()` still calls `ReleaseReservation()`: a mounted force that kept its convoy slot would hold it for minutes or hours, and enough of those stop the faction driving anywhere for the rest of the campaign. |

#### Why the force is never put back on the global proximity ring at arrival

`SuppressForcesAroundBattle` (`OVT_DeploymentManager.c:567`) skips any handle whose **resolved spawn distance is strictly wider than the world's global ring**. Riding crews and riding passengers register at `RIDING_SPAWN_DISTANCE = 100000`, so a mounted force is *already* exempt — with no whitelist, no flag and no per-module opt-in anywhere in the tree (C3/D8). **The whole protection is therefore a prohibition**, and its failure mode is silent: a QRF echelon that reached its battle and then dropped its passengers to the global ring would be pinned dormant on the next 1 Hz suppression pass, materialise nobody, and deliver an empty vehicle that is still paid for.

`grep -n "DropPassengersToGlobalRing" …OVT_MountedForceSpawningDeploymentModule.c` → **0 hits**, comments included.

**And the parent's own sweep is deliberately left alone.** Two speculative guards were written during this phase and then **removed** after tracing their real callers:

1. **An `EnsureGroups()` override re-asserting the riding ring after the parent's unconditional drop.** `EnsureGroups()` is reached during a live `HOLDING` from exactly one place — the deployment manager's records-restored fan-out (`OnVirtualRecordsRestored`, `:380`) — and the reinforcement rebuy does **not** use it (`OVT_InfantrySpawningDeploymentModule.Reinforce` calls `ConvergeGroups` directly). A records restore **re-creates the group entities from core's own payload**, so a force that was seated is not seated any more: the men who were in the vehicle no longer exist. Re-asserting the riding ring on their replacements would permanently materialise a squad standing in a field, one per deployment, for the rest of the campaign. The parent's drop is the right answer there, and it is D9's answer — **a restored mounted deployment walks**.
2. **An `IsForceMounted()` override returning true in `HOLDING`.** It gates *both* registration seams **and** the seating pairing, and the pairing (`AdoptPassenger`, `:2431`) is written against `DRIVING`. Widening only the ring half would register a reinforcement beside a parked vehicle, never seat it, and never let it go dormant — the same permanent-materialisation defect. A reinforcement bought into a hold therefore registers **at the source, on the ordinary ring, and walks in**, which is what men arriving after the vehicle has stopped look like anyway.

Both removals are recorded in the module header so the next reader does not re-add them.

#### T2.9 — the activation-ordering answer: **activation is NOT synchronous with creation.**

Traced through the working tree:

```
CreateDeployment (:1868)
  └─ InitializeDeployment (OVT_DeploymentComponent.c:55)
       ├─ RegisterDeployment
       ├─ per module: CloneModule() → AddModule() → module.Initialize(this) → OnInitialize()
       │     ⚠ OnInitialize() is `{}` on OVT_BaseDeploymentModule:121 and NOTHING in the tree overrides it
       │       (grep "OnInitialize" over Scripts/Game/GameMode/Deployments/Modules/ → 2 hits, both in the base)
       └─ CallLater(UpdateDeployment, 8000-12000 ms, repeating)      ← staggered 0.8-1.2x

UpdateDeployment (:237)  … first fires 8-12 s LATER, on a different frame
  └─ (after ticking every module) if (!m_bActive) ActivateDeployment()   ← :277
        └─ spawning module Activate() → OnActivate() → EnsureGroups() → DecideInsertion() → EnsureSourceResolved()
```

So the first convergence is a **whole update interval** after `CreateDeployment` returns. **`SetSourceOverride` applied on the returned component is in place with ~8-12 s of margin, and no change to `CreateDeployment` was needed** — `ForceCreateDeploymentFrom` creates, then walks the created component's **runtime** modules (`GetSpawningModules()`, which returns the clones, never the config's templates) and applies the override. `SetSourceOverride` still warns and refuses if it is handed a source after one has been resolved, so a future caller that gets the ordering wrong says so in the log instead of silently moving an origin the force is already registered against.

#### 🔴 Plan defect found: `RequestDeploymentCollection` is not reachable from a spawning module

T2.6 specifies "`m_iHoldTicks` expired → `RequestDeploymentCollection`". `RequestDeploymentCollection` is **`protected` on `OVT_BaseBehaviorDeploymentModule` (`:295`)**, and the mounted module is a **spawning** module (`OVT_InfantrySpawningDeploymentModule` line). It is not reachable, and reaching around it would mean copying the whole exfiltration rule — the collection latch, `TickExfiltration`, `IsPlayerWatchingDeployment`, the one-frame `CallLater` deferral and `StandDownDeploymentForce` — a second copy of a policy with its own play-test history (*"a squad may not evaporate in front of a player"*, author 2026-08-21).

**Resolution taken:** the hold clock counts and **latches**, exposed as `bool IsHoldExpired()`. Nothing in the mounted module collects anything. The behaviour module riding on the deployment is what acts on it — which costs Phase 6 nothing, because T6.2 already has `OVT_ArmouredSweepBehaviorDeploymentModule` calling `RequestDeploymentCollection("its sweep is over")` on its own clock. **Phase 6 should either poll `IsHoldExpired()` from the sweep behaviour or drop `m_iHoldTicks` from `Deployment_HunterKillerSweep.conf` and keep one clock, not two.**

#### Two smaller notes for later phases

- **`m_sTruckVehicleType` must stay authored on every mounted config.** The parent's `SpawnTruck()` (`:729`) refuses on an empty one **before** it asks for a prefab, so an empty string is not "use the ladder" — it is "never put a vehicle on the road at all". It is both the ladder's fallback and the parent's non-empty gate.
- **A `m_iTruckCostOverride` of 0 refuses every rung**, because `RungAffordable` treats a negative budget as unbounded and a zero one as "only something free would do". That is an authored consequence, not a bug, and it degrades to the named vehicle type.

#### Can-fail proofs

Running a suite is the orchestrator's job and the suites are deferred for this whole feature, so each proof below is a **fault injected into the subject and compiled**: every one exited `tools/compile-check.sh` with **0**, which is the point — none is a script error, so nothing in the toolchain would stop any of them shipping. Every subject was restored and the tree recompiled clean (exit 0, 6330 files) afterwards.

| Case | Fault | Expected red |
|---|---|---|
| `…MountedForce_ArrivalHoldsTheForceAboard` | **A1** `m_eState = HOLDING` → `RETURNING` in `CompleteInsertion()` | "arrival must leave a mounted force HOLDING" |
| | **A2** `ReleaseReservation();` deleted from `CompleteInsertion()` | "arrival must hand the convoy slot back" |
| | *(the ring claim's fault cannot be injected without writing the prohibited call, so it is proven the other way round: the `m_bDismountOnArrival` arm of the same case takes the parent's path and reads back the global ring, so the two readings differ inside one run)* | |
| `…MountedForce_CloneCarriesEveryAttribute` | **CL1** `clone.m_sVehicleRole = …` deleted (one of the four own) | "lost m_sVehicleRole — every mounted force silently reverts to a soft-skinned truck" |
| | **CL2** `clone.m_sTruckVehicleType = …` deleted (one of the parent's ten) | "lost m_sTruckVehicleType" |
| | **CL3** `clone.m_eImportance = …` deleted (one of the grandparent's thirteen) | "lost m_eImportance" |
| `…MountedForce_HoldingFallsBackToTheMarch` | **H1** `DismountAndWalk("its vehicle was destroyed");` in `TickHold()` replaced with a bare `return;` | "a mounted force that has lost its vehicle must fall back to the march" |
| `…MountedForce_LadderPicksARungInsideItsBudget` | **L1** the budget argument changed from `m_iTruckCostOverride` to `-1` | "a budget of 0 must answer no rung at all" |
| | **L2** the empty-role early return deleted from `GetVehiclePrefabFromFaction` | "with no ladder role authored, the module must fall back to the named vehicle type" |
| `…MountedForce_RuntimeSourceBeatsTheAuthoredProvider` | **S1** the override branch deleted from `EnsureSourceResolved()` | "a runtime source must be preferred over the authored provider" |
| | **S2** the late-offer guard in `SetSourceOverride()` neutralised (condition → `false`) | "a source handed in after one has been resolved must be ignored" |
| `…DeploymentBattleSuppression_TheRidingRingExemptsAMountedForce` | **B1** the riding registration's `spawnDistanceOverride` changed from `RIDING_SPAWN_DISTANCE` to `SPAWN_DISTANCE_GLOBAL` — precisely what a stray restore of the ordinary ring does to a live mounted force | "a mounted force's riding registration must resolve to a ring STRICTLY WIDER than the world's global one" |

**What the new cases could NOT prove, stated honestly:**

- **Nothing here runs the battle-suppression pass.** `SuppressForcesAroundBattle` and `TickBattleSuppression` are `protected`, and the 1 Hz tick that calls them is installed in `PostGameStart()`, which an Init-tier world never reaches. T2.11 establishes every input the pass reads against live objects and quotes the manager's own skip condition back at them; it cannot observe a pin being applied or skipped.
- **Nothing here drives a real convoy.** No case builds a deployment (the module's first convergence would put a real armed vehicle on a real road and register groups at a 100 km ring with the autotest camera inside it), so the drive, the seating, the LOD pin, the stuck test and the arrival trigger are all inherited-and-unexercised. `CompleteInsertion` is reached through a test-local subclass, not by arriving.
- **"Passengers still seated" is asserted as "still on the riding ring", not as "still in a compartment."** There is no vehicle and there are no agents in the fixture.
- **MP, JIP and save/reload are untouched** by any tier, as everywhere else in this tree.
- **The whole suite is deferred** to the end of the feature per the policy above, so none of these eleven proofs has been observed going red — only compiled.

#### Play-test notes owed for this module (Phase 3 is the first phase that can run one)

Nothing authors this module until Phase 3, so it cannot be play-tested on its own. When Phase 3's config lands, the two things to watch that no automated tier covers are (a) that the vehicle actually **drives** rather than degrading to the walk fallback — see the RISK TO THE PREMISE section above — and (b) that the crew stays **manned and awake** through the hold, which is what `TickHold`'s per-tick `HoldRidersActive()` re-assert exists for.

### Phase 3

**Status:** ✅ Complete (8/8). `tools/compile-check.sh` exit 0 (6335 files). Suite deferred per the whole-feature policy above.

**Files:**
- NEW `Scripts/Game/Data/OVT_CheckpointApproachRules.c` — the pure approach arithmetic (bearing folding, angular separation, the distance band, the roll-to-index pick, the previous-bearing exclusion). `grep -n "OVT_Global\|GetGame()\|World\|Entity"` on the file is **empty**.
- NEW `Scripts/Game/GameMode/Deployments/Modules/OVT_MobileCheckpointBehaviorDeploymentModule.c` — the behaviour module. **Nothing in `OVT_MountedForceSpawningDeploymentModule` was changed for it**; it works entirely through that module's Phase 2 public seams.
- NEW `Configs/Deployment/Deployment_ObjectiveHarassment_Mounted.conf(.meta)`
- TOUCH `Configs/Deployment/overthrowDeployments.conf` — one entry, `"Objective Harassment (Mounted)"`. The registry now carries **23**.
- TOUCH `Configs/Objective/Objective_TownOffensive.conf` (Harassment `:43`, ForwardBase `:144`) and `Configs/Objective/Objective_BaseOffensive.conf` (Harassment `:30`, ForwardBase `:116`) — the rung appended **last** in all four `m_aLadder` blocks.
- TOUCH `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c` — **a fifth edit site the plan does not name**; see the defect below.
- TOUCH `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ObjectiveOperations.c` — case A extended, a checkpoint clone check added to the clone-fidelity case, one new case + a probe subclass.
- NEW `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_CheckpointApproach.c` — 4 cases, world-free.
- Comment-only refreshes where the tree said "four rungs": `OVT_SendDeploymentObjectiveOperation.c`, `OVT_TEST_Init_ObjectiveModules.c`, `OVT_TEST_Init_ObjectiveOperations.c`.

**GUIDs claimed** (re-grepped at the start of the phase: `6BC1` had only Phase 1's two `6BC11…` hits, so the whole `6BC10…` series was free):

| GUID | What |
|---|---|
| `{6BC1000000000001}` | `Deployment_ObjectiveHarassment_Mounted.conf` (the `.meta` Name) |
| `{6BC1000000000002}` | its `OVT_MountedForceSpawningDeploymentModule` |
| `{6BC1000000000003}` | that module's `OVT_ObjectiveAnchorSourceProvider` |
| `{6BC1000000000004}` | its `OVT_TownHarassmentBehaviorDeploymentModule` |
| `{6BC1000000000005}` | its `OVT_MobileCheckpointBehaviorDeploymentModule` |
| `{6BC1000000000006}` | its `OVT_ReinforcementBehaviorDeploymentModule` |
| `{6BC1000000000007}` | its `OVT_ObjectiveConditionDeploymentModule` |
| `{6BC1000000000011}` | the `overthrowDeployments.conf` registry entry |

#### 🔴 Four plan defects found against the working tree

**1. The ladder has FIVE edit sites, not four.** `OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER` (`:133`) is a **code constant**, and it is not decoration:

- `IsObjectiveOperationConfig()` (`:2038`) matches a tracked deployment's config name against it to decide whether the deployment is an *operation* — which is what holds the phase's idle clock open while its men walk, and what makes its teardown a **recall** rather than a write-off. A rung in the `.conf` and not in the constant "fails safe to not-an-operation", i.e. it silently does neither.
- Three shipped Init cases assert the two lists are **equal, element by element** — `OVT_TEST_Init_ObjectiveModules.c:544`, `OVT_TEST_Init_ObjectiveFOB.c:1945` — and two more iterate the constant to check every rung's phase span (`OVT_TEST_Init_ObjectiveFramework.c:656`, `OVT_TEST_Init_ObjectiveFOB.c:2041`).

So the constant gained the fifth rung too. **T3.5 as written would have shipped a rung the director does not recognise and reddened three existing cases.**

**2. The mounted config must author `OVT_TownHarassmentBehaviorDeploymentModule`, and T3.3's module list omits it — this is a DEADLOCK, not a tidiness point.** The ladder index **saturates at the top rung** (`OVT_ObjectivePhaseRules.HarassmentLadderIndex`), so from the fifth success onwards *every* harassment operation is the mounted one. The forward-base phase opens on the town's support falling, and support only falls when a harassment operation completes its hold and stacks the `ObjectiveHarassment` modifier — which is that module's job and nothing else's. A mounted rung without it would have stalled every town objective permanently at the top of its own ramp. (`OVT_TEST_Init_ObjectiveModules_HarassmentConfigAsksForDifficulty` iterates the ladder and requires the module with the `-1` sentinel, so it would also have gone red — but the red would have been the symptom, not the fault.)

It also settles the module ordering: **mission behaviour first, checkpoint second, reinforcement third**, per the rule three config headers state and nothing enforces.

**3. `m_sGroupType "light_fireteam"` collides with rung two, and the tree asserts distinctness.** `OVT_TEST_Init_ObjectiveOperations`' ladder case refuses two rungs that field the same group ("rung N is not an escalation"). The plan's choice is right — **the fifth rung escalates by VEHICLE, not by group** — so the assertion was restructured rather than deleted: the distinctness rule now applies to the infantry rungs only, and the mounted rung answers four *stronger* claims in its place (it is last; its vehicle budget actually resolves an armed rung in **both** shipped registries at threat 0; it authors a ladder role and a non-empty `m_sTruckVehicleType`; it carries a checkpoint behaviour with a sane band, authored before the reinforcement module). Weakening an assertion without replacing it would have been the wrong trade.

**4. `m_iAllowedLocationTypes TOWN|BASE` has no textual form in a `.conf`.** There is no space-separated or pipe-separated multi-flag example anywhere in either tree; the one piece of evidence that exists is vanilla writing a combined flags field **numerically** (`m_eInventoryItemTypes 131382`, `Prefabs/.../*.et`), with single-valued fields written by name (`m_eFuelNodeTypes IS_FUEL_STORAGE`). So it is authored as **`m_iAllowedLocationTypes 3`** (TOWN 1 | BASE 2). ⚠ **This is the one line in the phase that only Workbench can confirm** — `compile-check.sh` does not parse `.conf`. It is also inert in practice: the config is `m_bDirectorOnly 1` and every ladder site authors `m_iRequiredTargetKind 1` (TOWN), so the mask is never consulted by the evaluator. If Workbench shows it unresolved, `TOWN` alone is a safe fallback and matches what the four infantry rungs already author.

#### Design decisions taken in this phase

**P3-1 — The checkpoint does NOT answer `BuildVirtualPlan()`, and that is the decision.** A `OVT_VirtualWaypointPlan` is *registration-time input only* (`virtualization/core/api.md`, "Plan semantics you must respect"): core builds the waypoints once, owns them, and re-derives nothing when a group moves. A plan is therefore asked for **at the source, before any checkpoint exists**, and could never be moved afterwards — while the entire point of this module is that the checkpoint moves. So the force keeps the insertion module's own fallback (a cycling march onto the objective with a long hold), which is also what the tree already demands: the shipped ladder case fails any rung authoring a patrol behaviour precisely because a plan of its own would "pre-empt the insertion module's cycling march onto the deployment position".

**Consequence, stated rather than hidden: a relocation moves the vehicle and its crew, not the dismounted infantry.** Men already on the ground hold the plan they were registered with and push on into the town. That is coherent — the vehicle and its gun ARE the checkpoint, the fireteam is the harassment — but it is not what a reader of T3.1 would assume, and changing it would mean re-planning a registered group, which the frozen core API forbids.

**P3-2 — The passengers' registration ring is deliberately NOT restored when they dismount.** The insertion module pairs `DisembarkPassengers()` with `DropPassengersToGlobalRing()`. This module must not, and the reason is **D8**: a mounted deployment is exempt from `SuppressForcesAroundBattle` only while its handles resolve to a ring strictly wider than the global one. **Phase 4 authors this same module on the QRF's mounted echelon, at a live battle** — a drop to the global ring there would pin the force dormant and deliver an empty vehicle, silently. The price is that the checkpoint's men stay materialised for the deployment's life; it is already being paid, because **D11** keeps an engine observer on the transport for exactly that long anyway. A save/load resets it: a restored mounted deployment walks (D9) and the parent's own ring sweep puts the re-created groups back on the ordinary ring.

**P3-3 — The vehicle is never moved by transform, only driven.** "Park facing along the road segment" is delivered by **arriving along it**: the checkpoint point is a road point, the crew drives to it on the road, and a vehicle that drove there is pointing the way it came. Rotating or teleporting an occupied, physics-simulated hull desynchronises the rigid body from the entity node (`OVT_BaseSpawningDeploymentModule.SpawnEntity`'s own note) and throws its occupants. `m_vCheckpointAngles` still carries the road's heading and is used to lay the dismounted men out **along** the road rather than in a circle.

**P3-4 — The crew is identified structurally, not by a flag.** `GetSpawnedEntities()` reports the force, the vehicle **and** the crew; `CollectRegisteredHandles()` reports only the force's handles, because the crew is registered under a separate owner key on purpose (`CREW_KEY_SUFFIX`). The crew is therefore *the one group the mounted module reports that is not one of its passengers*. No new seam was needed on Phase 2's file.

**P3-5 — The crew is the only group this module may give a waypoint to, and it may because its plan is null.** `EnsureCrew()` registers the transport crew with a **null** plan, deliberately, so core owns no waypoints on it. Every other group of the deployment is plan-driven and is never touched. Whatever the crew was doing is **detached (not deleted)** before a new order is added: the insertion module's landing-zone waypoint is still on the queue at HOLDING — arrival is judged by that module's radius test, not by the waypoint completing — so an order merely appended behind it might never run. The entity still belongs to the insertion module, which deletes it on its own teardown.

**P3-6 — The shipped config authors `m_iRelocateMinutes 4`, not the attribute default of 8.** The attribute default *is* 8, as T3.1 specifies. The config authors 4 because a harassment operation is collected by its own hold: `objectiveHarassmentHoldSeconds` is 240 s on Easy down to 90 s on Insane, plus the drive, so an eight-minute clock would in practice never fire and §6's play-test step 3 ("watch one relocation") would be unobservable. A clock longer than the mission is dead authoring.

**P3-7 — The park has a patience bound.** A drive that never arrives (blocked road, water in the way, a crew the LOD system stopped simulating) would otherwise leave the infantry aboard forever. After `PARK_PATIENCE_TICKS` (12 updates, ~2 min) the checkpoint is declared to be wherever the vehicle actually is and the doors open. Worse placement beats a force that never gets out.

#### Can-fail proofs

The suites are deferred for this whole feature, so — as in Phase 2 — each proof below is a **fault injected into the subject and compiled**. Every one exited `tools/compile-check.sh` with **0**, which is the point: none is a script error, so nothing in the toolchain would stop any of them shipping. Every subject was restored and the tree recompiled clean (exit 0, 6335 files).

| Case | Fault | Expected red | Injected? |
|---|---|---|---|
| `…MountedCheckpointApproachChooser` | **M1** the road test in `ChooseApproach()` replaced with an unconditional insert of the sampled probe point | "a search radius of zero must refuse every approach" | ✅ compiled clean |
| | **M2** `m_fCheckpointBearing` replaced with `NO_PREVIOUS_BEARING` in the `ChooseBearingIndex()` call | "a relocation came back to the bearing it just left" | ✅ compiled clean |
| `…CloneFidelity` (checkpoint half) | **CL** `clone.m_iRelocateMinutes = …` deleted | "the mobile checkpoint clone dropped m_iRelocateMinutes" | ✅ compiled clean |
| `…ARampConfigsResolveAndAreOrdered` | **A6** `"Objective Harassment (Mounted)"` moved above `"(Heavy)"` in `HARASSMENT_LADDER` | "has to be the LAST one" | reasoned, not injected |
| | **A7** `m_iTruckCostOverride` on the mounted rung dropped to 10 | "answers NO rung for role 'armed' at threat 0" | reasoned, not injected |
| | **A8** the checkpoint module removed from the mounted rung's `.conf` | "authors no mobile checkpoint behaviour" | reasoned, not injected |

⚠ A6–A8 are **`.conf` faults, and `compile-check.sh` does not parse `.conf`** — injecting them proves nothing a compile can see, so they are recorded as reasoned rather than dressed up as tested. The Logic cases' faults are all arithmetic and are covered by the boundary values in the cases themselves (a roll of exactly 1, an empty candidate list, a separation across north).

⚠ **The clone case's probe values are all different from what `new` produces.** `m_iRelocateMinutes` is **7**, not 0, precisely because a probe value equal to a fresh field would make a dropped copy line and a correct one read identically.

#### What the new cases could NOT prove, stated honestly

- **Nothing here drives a vehicle, parks one, or dismounts anybody.** No case builds a deployment — the mounted module's first convergence would put a real armed vehicle on a real road and register groups at a 100 km ring with the autotest camera inside it — so `TickPark`, `TickDismount`, `PlaceForce`, `IssueDrive`, `DetachForeignWaypoints` and `StandDownCheckpoint` are **entirely unexercised**. Everything asserted about them is a reading of the code, not an observation.
- **The waypoint hand-off is unproven in both directions.** That the crew's plan is null (so core owns no waypoints on it), that detaching the insertion module's landing-zone order does not break its own teardown, and that an appended move order is actually executed are all read off the sources. A play-test is the first thing that can see any of it.
- **The rotation half of the Init case is conditional on the map.** If the test world's first town offers only one usable approach in the 150–300 m band, every relocation is legally refused and the exclusion rule is never exercised against a real second choice. The case prints a diagnostic saying so rather than passing silently. The refusal half (a search radius of zero refuses everything) is unconditional.
- **Nothing asserts that the ramp reaches rung 5.** The saturation arithmetic is pinned in the Logic tier and the rung table in the Init tier, but "a town objective actually escalates to a mounted operation" is a play-test.
- **The `.conf` is unverified by any gate.** `compile-check.sh` does not read it. The module list, the GUIDs, the numeric location mask and the four ladder edits are all Workbench/play-test items.
- **MP, JIP and save/reload are untouched**, as everywhere else in this tree.

#### Note for Phase 4 (the QRF mounted echelon)

`OVT_MobileCheckpointBehaviorDeploymentModule` is reusable as T4.2 assumes, with two things to author deliberately:

1. **`m_iRelocateMinutes 0`** — already the plan's intent. With 0 the module parks once and never moves, which is what a standoff ring wants.
2. **The band still applies.** The module drives the vehicle to a road point `m_fApproachMinDistance`..`m_fApproachMaxDistance` **from the deployment position**. With `m_fLZStandoffDistance 0` the deployment position *is* the standoff point, so an echelon authored with the shipped 150–300 m band would move 150–300 m off its own ring slot. Author a small band (or the same band deliberately) rather than inheriting this one.

Also: the passenger-ring prohibition (P3-2) is what makes the echelon work at a live battle. It is a prohibition in this file too, and it has no mechanical guard here — `grep -n "RestoreGlobalSpawnRing\|SPAWN_DISTANCE_GLOBAL" …OVT_MobileCheckpointBehaviorDeploymentModule.c` → **empty** is the whole of it.

#### T3.8 — Play-test checklist for this phase

Run on a fresh Normal campaign. Debug affordances: `/give-resources`, a raised time multiplier, and — only if the ladder needs forcing — a **temporary** `vehicleThresholdScale 0.05` in `Difficulty_Normal.conf`, **reverted before committing**.

1. **Workbench first, before play.** Open `Configs/Deployment/overthrowDeployments.conf`. **Expect: 23 entries**, with `"Objective Harassment (Mounted)"` expanding into five modules in this order — mounted force, town harassment, mobile checkpoint, reinforcement, objective condition — and **no unresolved attribute**. ⚠ Check `m_iAllowedLocationTypes` specifically: it is authored as the number `3` and this is the only place that can confirm it resolves to TOWN + BASE.
2. Open `Configs/Objective/Objective_TownOffensive.conf` and `Objective_BaseOffensive.conf`. **Expect:** a **five**-entry `m_aLadder` ending in `"Objective Harassment (Mounted)"` in **both** the Harassment and the ForwardBase phase of each — four places.
3. **Take a town and let the harassment ramp run.** Watch the log for the rung names. **Expect:** four infantry operations, then the mounted one, and every operation after that is the mounted one. → F2, and the saturation claim.
4. **Watch the mounted operation arrive.** **Expect** in the log, in order: `Mounted force '…': role 'armed' at threat N resolved to '…'`, then `arrived at … with N group(s) still aboard`, then `Mobile checkpoint '…': setting up on the approach at bearing B, D m out`, then `is set at … - it reached the approach it was sent to`. ⚠ **If the third line is followed by `it stopped making progress towards the approach`, the drive is not working** — that is the RISK TO THE PREMISE section above, and the observer repair is what it tests. → F2, G2.
5. **Walk to the checkpoint.** **Expect:** an armed vehicle on a road 150–300 m out from the town, its gunner still in the turret, and its infantry on the ground spread up and down the road. **Then expect them to walk into the town** — that is the harassment half and is correct (P3-1).
6. **Wait four minutes and watch one relocation.** **Expect:** `is moving from the approach at bearing B1 to the one at bearing B2`, with B1 and B2 at least 60 degrees apart, and the vehicle actually driving. A town with one road in will log nothing and stay put — check the log for `no approach to the objective has a road within …` before calling it a failure. → F2.
7. **Destroy the checkpoint vehicle.** **Expect:** `Mobile checkpoint … stood down: its force is no longer riding a vehicle`, the mounted module's own `its vehicle was destroyed` line, and the surviving infantry still fighting at the objective. **Nothing may be left standing in a wreck.** → F3, F8.
8. **Let the operation finish its hold.** **Expect:** the town's support falls (the `ObjectiveHarassment` modifier stacks), the harassment success counter advances, and the deployment is collected once no player is within `baseCloseRange` — with no orphan vehicle and no crew left standing where it was. ⚠ This is the deadlock check from defect 2: **if support never moves after the fifth rung, the mounted rung's harassment module is not firing.**
9. **Watch server frame time with a checkpoint live** (D11's owed measurement). The transport carries an engine observer for the deployment's life, and now so does a squad standing next to it.
10. **Save mid-operation, quit, Continue.** **Expect:** the force on the ground and walking, no ghost vehicle, no checkpoint log after the load. → D9.

### Phase 4

**Status:** ✅ Complete (10/10). `tools/compile-check.sh` exit 0 (6336 files). Suite deferred per the whole-feature policy above.

**Files:**
- NEW `Configs/Deployment/Deployment_QRFMountedEchelon.conf(.meta)` — two modules: the mounted force and the mobile checkpoint. No reinforcement module and no condition module (see the module-list note below).
- TOUCH `Configs/Deployment/overthrowDeployments.conf` — one entry, `"QRF Mounted Echelon"`. The registry now carries **24**.
- TOUCH `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` — the whole of the phase's code: six constants, two runtime fields, `SendMountedEchelon()` + six helpers, `TearDownEchelons()`, three accessors, one call site in each mode, an `OnDelete()` override, and two visibility widenings (below).
- NEW `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_QRFMountedEchelon.c` — 5 cases + a shared fixture class.
- TOUCH `docs/features/occupying/epic-overview.md` — the reachability limitation as a second bullet under *Land reachability*.

**GUIDs claimed** (re-grepped at the start of the phase: `6BC1` held only Phase 1's two `6BC11…` and Phase 3's `6BC1000000000001`-`0007` and `0011`, so everything from `0021` up was free):

| GUID | What |
|---|---|
| `{6BC1000000000021}` | `Deployment_QRFMountedEchelon.conf` (the `.meta` Name) |
| `{6BC1000000000022}` | its `OVT_MountedForceSpawningDeploymentModule` |
| `{6BC1000000000023}` | its `OVT_MobileCheckpointBehaviorDeploymentModule` |
| `{6BC1000000000024}` | that mounted module's `OVT_NearestControlledBaseSourceProvider` |
| `{6BC1000000000031}` | the `overthrowDeployments.conf` registry entry |

#### T4.1 survey verdict: NOTHING HAS MOVED, and the single debit is still single and still outside the mode branch

Verified against the working tree at the start of the phase. A concurrent session is active in this
tree, so every line the plan names was re-read rather than trusted:

| Claim | Plan | Working tree |
|---|---|---|
| `SendWave` | `:747` | `:747` |
| `SpendWholeBudgetInOnePass` | `:831` | `:831` |
| `SpawnTroops` | `:875` | `:875` |
| `GetLandingZone` | `:1074` | `:1074` |
| `BuildSiegeRing` | `:707` | `:707` |
| the four index-parallel spawn arrays | `:31-40` | `:30`, `:31`, `:32` and `m_aSpawnRingSlots` at `:42`, under its own ⚠ header |
| the scoring loop | `:468-596` | `CheckUpdatePoints` opens at `:468`; the agent walk is `:491-527`; `m_OnFinished.Invoke()` is `:590` |
| `m_OnFinished` | — | declared `:24`, invoked once at `:590`, subscribed twice by the occupying faction manager (`:1251`, `:1315`) **after** `Start()` |
| **the single reserve debit** | `:800-806` | **`:803`**, inside the `:796-806` block, and **outside the mode branch** — the `if (COUNTER_ATTACK) … else … ` ends at `:790` and neither arm returns early. `grep -c "m_OccupyingFaction.m_iResources -="` was **1** before the phase and is **1** after it. |

No re-baseline was needed. The unrelated concurrent work in the tree (shops, storage, props, `Language/`,
high command) touches no QRF, deployment or vehicle file; the observer repair to
`OVT_InsertionSpawningDeploymentModule.c` / `OVT_DeploymentManager.c` / the FOB module is the expected one
documented in `objectives/context.md` and is untouched by this phase.

#### The reachability limitation, verbatim (D10)

> There is no A→B land-reachability query anywhere in the engine or in this campaign, and this feature
> invents none. It refuses a source whose own base record carries `landIsolated`, and a source or a
> standoff point with no road within `ECHELON_ROAD_SEARCH_M` (200 m). **A source with a road at both
> ends that is nonetheless unreachable across water still passes both tests.** The crew then stalls,
> the insertion module's stuck test fires, and **the force dismounts and walks**. That is a
> degradation, not a failure — and because it is exactly the designed fallback (G4/F8) it is
> **silent**. Two further limits: the `landIsolated` flag is read **only when the nearest base record
> is within 50 m of the source**, because two of the QRF's wave sources are not bases at all (the
> occupying faction's forward operating base, and the no-bases-left placeholder) and a distant base's
> flag says nothing about either — for those the road test alone decides; and the flag is **authored
> per map**, so a community map that never sets it gets no first test at all.

Written into `epic-overview.md` → *Land reachability* as a second bullet (T4.10).

#### The four-parallel-array hazard, and how this phase stays clear of it

`m_aSpawnQueue` / `m_aSpawnPositions` / `m_aSpawnTargets` / `m_aSpawnRingSlots` are addressed by the
**same index**, and `SpawnFromQueue` takes index 0 off each of them. `BuildSiegeRing` runs **after**
`SendWave()` returns and lays out **exactly one ring slot per queued group**, from the final queue
length. So an echelon that appended itself to the queue would take an infantry group's ring slot and
shift every later group onto somebody else's — with no symptom beyond groups standing in the wrong
place.

**The rule this phase follows is therefore a prohibition, and it is mechanical:** `SendMountedEchelon`
and every one of its helpers touch none of the four. An echelon is a **deployment**, not a queued
group; it has no slot and needs none. `m_aEchelons` is deliberately declared apart from the four, with
a header saying so, and the acceptance grep for the phase is that no spawn-array write appears
anywhere inside `SendMountedEchelon`.

⚠ The one place the ring geometry IS reused is `SiegeEchelonAnchor`, which calls
`OVT_QRFSiege.RingSlotOffset(m_iEchelonsSent, ECHELON_CAP_PER_BATTLE, ECHELON_STANDOFF_M)` — a **read**
of the same pure helper `BuildSiegeRing` uses, on its own count, at its own radius. The count is the
**cap** and not the live number of echelons, for the reason that helper's own header gives: a count
that grows collapses the ring, because "slot 0 of 1" and "slot 0 of 2" are the same direction.

#### Delete, never collect, and why (C6/D6)

There are two ledgers and they are not the same one:

| Ledger | Field | Who moves it |
|---|---|---|
| **The reserve** | `OVT_OccupyingFactionManager.m_iResources` | a QRF wave, at the **one** debit in `SendWave` outside the mode branch |
| **The pool** | the deployment manager's per-faction map, credited by `AddFactionResources` | the deployment framework's own collect and recall teardowns, and the occupying faction's income |

An echelon is paid for out of the **reserve** and stood up with `resourcesInvested = 0`. Collecting or
recalling it would credit the **pool** for money the reserve spent — the faction would end every battle
richer, once per echelon, for ever, with no symptom a player or a play-test could ever see.

So: **`DeleteDeployment` only.** The rule is stated three times on purpose — in the ledger header above
`SendMountedEchelon`, at the teardown itself, and as an acceptance grep
(`grep -n "CollectDeployment\|RecallDeployment"` over the QRF controller must be **empty**). ⚠ The two
method names are deliberately **not spelt out anywhere in that file, in code or in prose**, so that the
grep is a mechanical guard rather than a reading exercise; the file says "the deployment manager's
COLLECT and RECALL teardowns" instead and points at that manager's own headers.

Teardown is subscribed in `OnPostInit` — **not** by the caller, because the occupying faction manager
inserts its own `OnQRFFinished*` handler **after** `Start()` and that handler deletes the entity, so a
teardown registered later than it would run against a component that no longer exists. `Insert` does
not de-duplicate and `OnPostInit` runs exactly once, so there is exactly one subscription;
`OnDelete()` removes it and runs the teardown again as a belt, which is safe because
`TearDownEchelons()` clears its own list.

#### 🔴 T4.7 verdict: ZONE SCORING DOES COUNT A MOUNTED CREW. Nothing was changed.

`CheckUpdatePoints` (`:468`) walks `GetGame().GetAIWorld().GetAIAgents()`, casts each agent's controlled
entity to `SCR_ChimeraCharacter`, keeps those whose `GetFactionKey()` matches the occupying key, drops
anything `IsFightingFit` refuses, and then measures `vector.Distance(character.GetOrigin(), objective)`.
Four readings, four verdicts:

1. **The faction key.** A mounted crew is spawned from the occupying faction's own group prefabs and
   carries the occupying faction key like any other man it fields. ✅
2. **`IsFightingFit`.** It asks `CharacterControllerComponent.GetLifeState() != ALIVE` and
   `IsUnconscious()`. **Neither says anything about compartments.** A seated man is alive and conscious
   and is not excluded. ✅
3. **`GetOrigin()` while seated** — the one that could have been fatal, because a character in a
   compartment is parented to the vehicle and a LOCAL position would make every mounted crew measure as
   thousands of metres from the objective, silently. **It is a world position.** The decisive evidence
   is vanilla's own capture-zone logic: `SCR_SeizingComponent.EvaluateEntityFaction`
   (`ArmaReforger/scripts/Game/GameMode/Components/SCR_SeizingComponent.c:302`) reads
   `char.GetOrigin()` of a character it has just confirmed `IsInVehicle()` and feeds it straight to
   `SCR_TerrainHelper.GetHeightAboveTerrain` — a world-space query — and excludes seated characters
   only for being too **high**. Vanilla counts seated occupants towards zone control by exactly this
   reading. ✅
4. **Materialisation.** A mounted force never leaves the riding ring (`RIDING_SPAWN_DISTANCE`
   = 100 000), so its men are always materialised and are always real agents — which is the same
   property (D8/P3-2) that exempts them from battle suppression. ✅

**Verdict: no change is owed to `qrf`, and none was made.** The one edit T4.7 justified was making
`IsFightingFit` **public**, exactly as `CheckUpdatePoints` and `CheckUpdateTimer` in the same file
already are and for the same reason, so that the answer is measured by an Init case rather than
asserted in prose. That is a visibility change and nothing else.

⚠ **One honest caveat about what "counts" means at 450 m.** `ECHELON_STANDOFF_M` is 450, and the
scoring loop has **two** rings:

- `QRF_POINT_RANGE` = **220 m** is the head-count ring — the one whose enemy total is compared against
  the resistance's to push `m_iPoints` one way or the other. **A crew holding a 450 m standoff is
  outside it** and does not contribute to that comparison.
- `QRF_RANGE` = **750 m** is the "is the enemy anywhere near" ring. A standoff at 450 m **is** inside
  it, and being inside it is what denies the resistance the `m_iPoints += 5` fast push it gets when
  `enemyTotal == 0`.

So DoD **F4's** phrase *"their crews score in zone control"* is true in the second sense and not the
first: an echelon at its standoff **denies the walkover** but does not itself contest the centre —
until it, or its dismounted infantry, moves in. That is coherent with what a standoff is, and it is
recorded here rather than left for somebody to discover as a bug. Anyone who wants an echelon to
contest the centre should lower `ECHELON_STANDOFF_M` below 220, not change the scoring.

#### 🔴 Plan defects found against the working tree

**1. `allocate - allocated` is almost always ZERO OR NEGATIVE, so §3.5's budget expression would have
made the echelon unreachable — and worse than unreachable.** §3.5 writes the call site as
`allocated += SendMountedEchelon(base, qrfpos, allocate - allocated)`. But the infantry loop is
`while(allocated < allocate && ii < 6) allocated += SpawnTroops(...)` and `SpawnTroops` returns a whole
group's price (`8 * baseResourceCost`), so it **exits the moment `allocated >= allocate`** — the
overshoot is the normal case and the exact hit is the best case. `allocate - allocated` is therefore
`<= 0` essentially always.

That is not merely "no echelon ever". **`OVT_VehicleLadderRules.RungAffordable` treats a NEGATIVE
budget as UNBOUNDED**, so on the first ladder call that reached it, a negative slice would have bought
the most expensive rung in the registry at exactly the moment the wave had run out of money.

**Resolution taken:** the slice is `EchelonSlice(allocated)` = `m_iResourcesLeft - allocated`, clamped
at zero — what the **wave** has left after this source's infantry, which is the figure §3.5's prose
("a slice of the wave budget") actually describes. The clamp is documented as the second line of
defence rather than the only one, because the budget gate below refuses anything under the config's
vehicle budget and would catch a negative anyway.

**2. Pricing against the remaining slice would have let the wave charge for a UAZ and field a BRDM.**
T4.3 says "ladder resolve inside `budget`". But **the module resolves the ladder AGAIN when it spawns**,
8–12 s later, against `m_iTruckCostOverride` and nothing else (D4) — the QRF's slice is not an input it
has. So resolving here against a *smaller* number gives a *cheaper* rung than the one that will
actually drive, and the reserve is short by the difference on every echelon, silently.

**Resolution taken:** the gate is the config's **vehicle ceiling**, not the cheapest rung —
`if (budget < vehicleBudget) refuse` — and the ladder is then asked with `vehicleBudget`. Both
resolutions ask the ladder the identical question and get the identical answer, so what is charged is
what is fielded. The ceiling and the role are **read off the config's own module template**
(`FindEchelonMountedTemplate`) rather than restated as constants, so the two can never drift. The cost
in practice is nil: the slice at a source is in the hundreds and the ceiling is 90.

**3. `GetNearestBase(source)` answers at ANY distance, and two of the QRF's wave sources are not bases.**
T4.3 says "`GetNearestBase(source).landIsolated` false". `SendTroops` builds the source list from base
records, **plus** the occupying faction's forward operating base, **plus** — when the faction holds
nothing else — a placeholder at `qrfpos + "250 0 100"`. `GetNearestBase` has no radius, so for those
two the flag consulted belongs to some unrelated base and would refuse and accept for reasons that have
nothing to do with the place the vehicles set out from.

**Resolution taken:** the flag is read **only when the nearest record is within `ECHELON_SOURCE_MATCH_M`
(50 m) of the source**. Otherwise the record does not describe this source and the road test alone
decides. Recorded in the limitation above and in the epic.

**4. `RequestDeploymentCollection` is not what tears an echelon down — nothing collects it at all, and
that had to be checked rather than assumed.** Phase 3's config needed
`OVT_TownHarassmentBehaviorDeploymentModule` or the objective ramp deadlocked, so the same question was
asked here: does the echelon config need an analogous module to be torn down and accounted for
correctly? **It does not, and the reasoning is the interesting part.** `OVT_BaseBehaviorDeploymentModule`
only ever collects when a *derived* module calls its protected `RequestDeploymentCollection`, and
`OVT_MobileCheckpointBehaviorDeploymentModule` never does (`grep` on that file: zero hits). The other
two collectors in the tree are `OVT_ReinforcementBehaviorDeploymentModule` (`:636`) and
`OVT_TownHarassmentBehaviorDeploymentModule` (`:200`), and **neither is authored on this config** — the
reinforcement module deliberately so, per T4.2. The director's own teardown paths match a config name
against `HARASSMENT_LADDER` and this config is not in it. So **the QRF is the only thing that ever
takes an echelon down**, which is exactly what D6 requires: adding a behaviour module that could
collect one would have been the money-creating bug, not the fix for it. The config authors **no**
condition module for the same reason — a condition failure would delete a deployment the battle still
believes it owns, and the battle's own `m_OnFinished` (plus `OnDelete`) is the single teardown.

#### Design decisions taken in this phase

**P4-1 — The counter-attack standoff ignores the source, and that is the encirclement doing its job.**
In `COUNTER_ATTACK` the anchor is `RingSlotOffset(m_iEchelonsSent, ECHELON_CAP_PER_BATTLE, 450)` — slot
0 due north, slot 1 due south — regardless of which base each echelon drives from. One of the two will
therefore usually be told to hold a bearing on the far side of the objective from its own source, and
will drive around the battle to get there. That is what a ring is; F4 asks for exactly it ("in a
counter-attack siege they sit on ring bearings and block the road"). The cost is a longer drive with
more chances to stall, and a stall degrades to the walk fallback like every other one.

**P4-2 — A wet ring slot is walked inward but is NEVER fallen back to the objective's centre.**
`BuildSiegeRing`'s last resort for an infantry slot that is still in the sea after eight inward steps
is the objective itself, which is dry by definition. An echelon may not do that: a vehicle standing
**on** the objective is not a standoff, it is an assault, and it would put an armed hull in the middle
of the battle the standoff exists to stay out of. `SiegeEchelonAnchor` refuses instead, and the wave
sends infantry only.

**P4-3 — The standard-mode standoff is pulled in to half the separation when the source is close.**
`target + normalize(source - target) * 450` puts the point **behind** the source whenever the source is
nearer than 450 m, and the echelon would set off away from the battle. It is clamped to half the
separation, which always leaves a real drive and is what "stand off towards the source" means when the
source is next door.

**P4-4 — The mobile checkpoint's approach band is authored 0–50 m, not the shipped 150–300.** Phase 3's
note for this phase called it: the band is measured from the **deployment position**, and with
`m_fLZStandoffDistance 0` the deployment position **is** the standoff point, so the shipped band would
move the echelon 150–300 m off its own ring slot. `m_fRoadSearchRadius` is 80 (down from 120) for the
same reason. `m_iRelocateMinutes 0` parks it once and never moves it.

**P4-5 — `m_eImportance HIGH`, unlike mounted harassment's NORMAL.** An echelon exists for the length of
one battle and has to be awake for it; the AI budget starving a force that is standing in a firefight
is the failure this attribute exists to prevent. It is what `Deployment_ObjectiveSabotage`,
`Deployment_ObjectiveTowerRecapture` and both tower configs author for the same reason.

**P4-6 — The config authors a source provider even though the runtime override always wins.**
`OVT_NearestControlledBaseSourceProvider` with no distance limit. `ForceCreateDeploymentFrom` applies
`SetSourceOverride` on every mounted module before the first convergence, so the provider is never
consulted on the QRF's own path — but `EnsureSourceResolved` **registers nothing at all** and warns when
there is neither an override nor a provider, and "registers nothing" is the one failure mode the
insertion module exists to prevent. A future caller that reaches this config through plain
`ForceCreateDeployment` gets a walking force instead of no force.

**P4-7 — `OnDelete` now cancels this component's own pending calls, which is a small pre-existing hole
being closed in passing.** `SendWave` schedules a follow-up wave 4–8 minutes out whenever a wave left
resources unspent, and **nothing removed it**: a battle that finished before it fired left a call queued
against a component whose entity had been deleted. The two repeating ticks are already removed on the
winning path; removing all three in `OnDelete` costs nothing and closes the same hole for every other
way this entity can die (including the way the new Init cases kill it).

**P4-8 — Two visibility widenings on the QRF controller, both with precedent in the same file.**
`IsFightingFit` and `SendMountedEchelon` are public "ONLY SO THAT THE INITIALISATION TIER CAN DRIVE
ONE", which is the note `CheckUpdatePoints` and `CheckUpdateTimer` already carry verbatim. Neither is
called from outside the component in the campaign. `SendWave` was **not** widened; the conserved-total
case drives `Start()`, the campaign's own entry point.

#### Can-fail proofs

The suites are deferred for this whole feature, so — as in Phases 2 and 3 — each proof below is a
**fault injected into the subject and compiled**. Every one exited `tools/compile-check.sh` with **0**,
which is the point: none is a script error, so nothing in the toolchain would stop any of them
shipping. Every subject was restored and the tree recompiled clean (exit 0, 6336 files).

| Case | Fault | Expected red | Injected? |
|---|---|---|---|
| `…ReachabilityGateRefusesALandIsolatedSource` | **R1** the `&& nearest.landIsolated` term deleted from `IsEchelonSourceReachable` | "a land-isolated source must send no echelon" | ✅ compiled clean |
| `…BudgetGateRefusesASliceUnderTheVehicleBudget` | **B1** `if(budget < vehicleBudget)` relaxed to `if(budget <= 0)` — the plan's own wording | "a slice of 89, one under the config's vehicle budget, must send no echelon" | ✅ compiled clean |
| `…CapBoundsABattleAndTheCostIsTheRungCost` | **C1** the cap test `>=` changed to `>` | "a battle may send exactly 2 echelons: the third committed N" | ✅ compiled clean |
| `…AWaveDebitsTheReserveOnceAndNeverThePool` | **D1** `m_OccupyingFaction.m_iResources -= cost;` added inside `SendMountedEchelon` — the second debit, written where a second debit is habitually written | "the reserve must fall by exactly what the wave committed, once" (and the acceptance grep goes to **2**) | ✅ compiled clean |
| | **D2** `DeleteDeployment` swapped for the manager's collect teardown | "tearing an echelon down must credit nothing" (and the acceptance grep stops being empty) | ✅ compiled clean |
| `…ZoneScoringCountsASeatedCrew` | *(no fault can be injected: the case measures the ENGINE's answers — `GetAIAgents`, `GetOrigin` on a parented entity, `GetFactionKey` — not Overthrow code. It is a verdict-taker, not a regression guard, and its failure messages say which feature owns the fix if the answer ever changes.)* | | |

⚠ **One injected fault is deliberately NOT covered by any case, and the reason is worth writing down.**
Removing `EchelonSlice`'s zero clamp compiles clean and changes nothing observable, because the budget
gate refuses anything under the vehicle ceiling and a negative number is under it. The clamp is
therefore genuinely second-line, and the honest statement is that the ladder is protected from a
negative budget by the **gate**, with the clamp as a guard for a future caller. The case that would
catch its removal does not exist because the defect it would catch does not currently exist.

⚠ The `.conf` faults (a missing module, a wrong `m_iTruckCostOverride`, the numeric location mask) are
**not** compile-visible — `compile-check.sh` does not parse `.conf`. Two of the three are covered
mechanically by the budget case, which reads the shipped registry for the config, its mounted module,
its ladder role and its vehicle budget and fails by name if any is missing. The location mask is not,
and is a Workbench item.

#### What the new cases could NOT prove, stated honestly

- **Nothing here drives a vehicle anywhere.** The two cases that create real echelons create them and
  tear them down inside the same frame; a deployment's first convergence is a whole 8–12 s update
  interval later, so no vehicle is ever spawned, no crew is ever materialised, no convoy slot is ever
  taken and nothing ever arrives at a standoff. Everything asserted about the drive is a reading of
  Phase 2's code.
- **The COUNTER_ATTACK branch of the standoff is completely unexercised.** Every case runs a STANDARD
  controller, so `SiegeEchelonAnchor`, the ring-slot bearing, the ocean walk-in and its refusal are
  read-only claims. Driving them would mean a siege fixture; the geometry they call
  (`OVT_QRFSiege.RingSlotOffset`) is already pinned in the Logic tier by
  `OVT_TEST_Logic_QRFSiege.c`, and the walk-in is copied line-for-line from `BuildSiegeRing`.
- **Nothing proves the echelon is not pinned by battle suppression at a live battle.** That claim rests
  entirely on D8/P3-2 — the force never leaves the riding ring — and its mechanical guard is Phase
  2's `…TheRidingRingExemptsAMountedForce`, which itself cannot observe the suppression pass running
  (it is `protected` and installed in `PostGameStart`, which an Init world never reaches).
- **The teardown is proven to DELETE, not proven to be REACHED from a real finish.** The conserved-total
  case calls `TearDownEchelons()` itself. That `m_OnFinished` really fires it at the end of a real
  battle is the `OnPostInit` subscription plus the invoke at `:590`, read and not run.
- **Three of the five cases can be legally reduced to a diagnostic by the world.** If the test map
  offers no occupying base that is its own nearest record, ≥ 250 m from the fixture objective and with
  a road within 200 m, the reachability, budget and cap cases print why and assert only their
  world-free halves. Each says which half it lost. This is deliberate: the alternative is a case that
  reddens on the map rather than on the code.
- **The seated-crew case can be reduced the same way**, by a world with no armed rung, no named
  character, or a compartment that refuses the request. It takes three fixed frame hops and does not
  poll; if the man is not in the seat by the third, it says so and measures nothing.
- **The suites have not been run.** Deferred for the whole feature per the policy above, so none of
  these five cases has been observed going green **or** red — only compiled, with five of its six
  assertions' faults injected and compiled.
- **MP, JIP and save/reload are untouched**, as everywhere else in this tree. An echelon is a
  battle-scoped deployment and a live battle is deliberately rolled back on load (D9), but nothing
  tests that.

#### Play-test checklist for this phase

Run on a fresh Normal campaign, after Phase 3's checklist. Debug affordances: `/give-resources`, a
raised time multiplier, and — only if the ladder needs forcing — a **temporary**
`vehicleThresholdScale 0.05` in `Difficulty_Normal.conf`, **reverted before committing**.

1. **Workbench first.** `Configs/Deployment/overthrowDeployments.conf` — **expect 24 entries**, with
   `"QRF Mounted Echelon"` expanding into **two** modules (mounted force, mobile checkpoint) and no
   unresolved attribute. ⚠ Check `m_iAllowedLocationTypes`: it is authored as the number **127** (all
   seven flags) and Workbench is the only place that can confirm it resolves. It is inert in practice —
   the config is `m_bDirectorOnly 1` and is only ever reached by `ForceCreateDeploymentFrom`.
2. **Attack an occupying base that has another occupying base 1–3 km away with a road between them.**
   Expect in the log: `Mounted echelon 1 of 2 is driving from … to a standoff at … for N resources`,
   then Phase 2's `Mounted force '…': role 'armed' at threat N resolved to '…'`. → F4.
3. **Drive out along the road towards the source base.** Expect to meet an armed vehicle actually
   driving, late — after the first infantry wave. ⚠ **If it never moves, that is the RISK TO THE
   PREMISE section above**, and the observer repair is what it tests.
4. **Let it arrive.** Expect it to stop ~450 m from the objective on a road, its infantry to dismount
   around it, its gunner to stay up, and the log to say
   `Mobile checkpoint '…' is set at … - it reached the approach it was sent to`. → F4, G2.
5. **Count them.** At most **two** for the whole battle, however many sources the faction has and
   however long the battle runs. → the cap.
6. **Watch the war chest.** `/give-resources`-free: note the occupying faction's reserve before the
   battle and after the wave line `Wave complete: N`. Expect it to fall by **exactly N, once**. → F5.
7. **Win or lose the battle and then look for the vehicles.** Expect
   `The battle is over: N mounted echelon(s) deleted` and **no armed vehicle left standing** anywhere
   near the objective, and expect the deployment **pool** not to jump. → F5.
8. **Start a counter-attack siege on a town you hold** (the occupying faction's own objective ramp, or
   force one). Expect the echelons to sit on ring bearings ~450 m out rather than on the line from
   their base, and to be road-blocking. → F4, P4-1. ⚠ **This is the only way to exercise
   `SiegeEchelonAnchor` at all** — no automated case reaches it.
9. **Fight one at its standoff.** Expect the crew to hold ground the moment the fight comes to them,
   and note that at 450 m they are **not** contesting the centre (the T4.7 caveat above). Walk them in
   and they do.
10. **Attack a base whose only occupying neighbour is across water.** Expect
    `No mounted echelon: …` at VERBOSE and an ordinary infantry battle — no vehicle stuck on a beach.
    → D10.
11. **Save mid-battle, quit, Continue.** Expect the battle rolled back and no ghost echelon. → D9.

#### Note for Phase 5 (crew-up on alarm)

Three things this phase settled that the sortie inherits:

1. **`ForceCreateDeploymentFrom(config, position, factionIndex, sourcePosition, resourcesInvested)` is
   the seam**, and `SendMountedEchelon` is the worked example: resolve the config by name through
   `FindConfigByName`, read the role and the vehicle ceiling **off the config's own module template**
   rather than restating them, create, and keep the created component's **EntityID** rather than a
   pointer.
2. **The delete-not-collect rule is the QRF's, NOT the sortie's, and the difference is which ledger paid.**
   An echelon is bought out of the **reserve** by a wave, so it must never be collected. The armour
   sortie is created by a deployment module and — if T5.4 debits the **pool** at its own call site, as
   T6.4 does — may be collected normally, because the refund and the debit are then the same ledger.
   ⚠ Decide that explicitly and write it at the call site; copying `TearDownEchelons`' rule without its
   reason would write off men who could be paid back for.
3. **`OnDelete` on the QRF controller now cancels its own pending `SendWave`/tick calls.** If T5.4
   subscribes a behaviour module to `m_OnBattleStarted`, remember the same class of hazard: `Insert`
   does not de-duplicate and a subscription outliving its subscriber is a call into nothing.

And one thing to check rather than assume, because Phase 3 and Phase 4 both found the plan wrong about
it: **whether the sortie's config needs a behaviour module to be torn down at all.** For the echelon
the answer was no — the mobile checkpoint never calls `RequestDeploymentCollection` and no other
collector is authored, which is what makes the QRF the only thing that can end it. For a sortie that
outlives its battle the answer may be different.

### Phase 5

**Status:** ✅ Complete (8/8). `tools/compile-check.sh` exit 0 (6338 files). Suite deferred per the whole-feature policy above.

**Files:**
- TOUCH `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` — `ref ScriptInvoker<vector> m_OnBattleStarted`, invoked once at the tail of `StartBaseQRF` and once at the tail of `StartTownQRF`, both after `m_bQRFActive`/`m_vQRFLocation` are set and the reveal RPCs have gone out.
- TOUCH `Scripts/Game/GameMode/Deployments/Modules/OVT_ParkedVehicleSpawningDeploymentModule.c` — `m_sVehicleRole` (empty default, G8), `ResolveLadderPrefab()` + `ResolveNamedVehiclePrefab()` (the original `ResolveVehiclePrefab()` body, moved verbatim and unedited), `ReleaseVehicleOwnership(Vehicle)`, `PlantParkedVehicle(IEntity)` (test-only seam), `CloneModule()` +1 line.
- NEW `Scripts/Game/GameMode/Deployments/Modules/OVT_CrewUpOnAlarmBehaviorDeploymentModule.c` — the alarm subscription, `SendSortie()`, the `TickSortieTeardown()` poll, `CloneModule()`.
- NEW `Configs/Deployment/Deployment_BaseArmourSortie.conf(.meta)`.
- TOUCH `Configs/Deployment/overthrowDeployments.conf` — one registry delta ("Base Parked Armour" on `Deployment_BaseParkedVehicles.conf`) and one plain registration ("Base Armour Sortie"). The registry now carries **26**.
- TOUCH `Configs/Factions/{USSR,US}_OverthrowData.conf` — one `vehicle_crew` group entry each.
- NEW `Prefabs/Groups/OPFOR/OVT_Group_USSR_VehicleCrew.et(.meta)`, `Prefabs/Groups/BLUFOR/OVT_Group_US_VehicleCrew.et(.meta)` — same-shape siblings of the shipped `OVT_Group_USSR_SniperTeam.et` / `Group_US_Sniper.et` pattern, three copies of the faction's own `Character_{USSR,US}_Crew.et` (the same character the LAV-25's own turret authors as its default occupant — cross-checked against `Configs/EntityCatalog/{US,USSR}/Characters_EntityCatalog_*.conf` and three to five independent references each, since the extracted vanilla tree carries no `.meta` files to check against directly).
- NEW `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_CrewUpOnAlarm.c` — 3 cases + a shared fixture class.

**GUIDs claimed** (re-grepped at the start of the phase: `6BC1` held Phase 1's `6BC11…0001`–`0002`, Phase 3's `6BC10…0001`–`0007`/`0011`, Phase 4's `6BC10…0021`–`0024`/`0031`, so everything from `0041` up was free; the `6BC13…` prefab-component series had zero hits anywhere, exactly as §3.1 predicted):

| GUID | What |
|---|---|
| `{6BC1000000000041}` | `Deployment_BaseArmourSortie.conf` (the `.meta` Name) |
| `{6BC1000000000042}` | its `OVT_MountedForceSpawningDeploymentModule` |
| `{6BC1000000000043}` | that module's `OVT_NearestControlledBaseSourceProvider` |
| `{6BC1000000000044}` | its `OVT_MobileCheckpointBehaviorDeploymentModule` |
| `{6BC1000000000051}` | the `overthrowDeployments.conf` registration of the sortie config |
| `{6BC1000000000052}` | the `overthrowDeployments.conf` registry-delta entry, "Base Parked Armour" |
| `{6BC1000000000053}` | that delta's `OVT_CrewUpOnAlarmBehaviorDeploymentModule` |
| `{6BC1100000000003}` | USSR `vehicle_crew` faction group entry |
| `{6BC1100000000004}` | US `vehicle_crew` faction group entry |
| `{6BC1300000000001}` | `OVT_Group_USSR_VehicleCrew.et` (the `.meta` Name) |
| `{6BC1300000000002}`–`{…0007}` | that prefab's own component instances |
| `{6BC1300000000011}` | `OVT_Group_US_VehicleCrew.et` (the `.meta` Name) |
| `{6BC1300000000012}`–`{…0017}` | that prefab's own component instances |

Two component GUIDs on both new group prefabs are **deliberately not minted**: `SCR_EditableGroupComponent` (`{50D291F6C83FA532}`) and its `SCR_EditableGroupUIInfo` (`{5298E609432D192D}`) are the exact same literal GUIDs every other group prefab in this tree carries — vanilla's own `Group_USSR_Transport.et`/`Group_US_Transport.et`, and Overthrow's own shipped `Group_USSR_Sniper.et`/`OVT_Group_USSR_SniperTeam.et`/`Group_US_Sniper.et` all repeat them verbatim. Minting fresh ones here would have made this pair of files the only group prefabs in either tree that do not.

#### The ownership-transfer contract (T5.8)

A parked vehicle and a mounted force are two different modules, and each deletes what it believes it owns at its own teardown (or, for the parked module today, does not — see the honest gap below). Handing the same `Vehicle` to both at once is a double free; handing it to neither is an orphan. The whole contract is one rule, enforced by ordering and nothing else:

1. resolve the vehicle through `OVT_ParkedVehicleSpawningDeploymentModule.GetSpawnedEntities()` — it is still owned there;
2. stand up the sortie deployment and find its `OVT_MountedForceSpawningDeploymentModule`;
3. call `AdoptVehicle()` on **that** module. Only once it returns `true` does the sortie own the hull;
4. **only then** call `OVT_ParkedVehicleSpawningDeploymentModule.ReleaseVehicleOwnership()`, which removes the id from the parked module's own list — its `GetSpawnedEntities()` stops reporting it and its (currently nonexistent, see below) teardown stops touching it.

If step 3 fails — no mounted module on the resolved config, or it already holds a hull — the sortie deployment is deleted and step 4 never runs. Nothing is ever released speculatively, so a failed hand-off always leaves the vehicle exactly where it was: parked, and still owned by the module that has owned it the whole time.

**🔴 The double-delete hazard, and the honest gap this phase found while chasing it.** Tracing "what deletes the parked vehicle at teardown" to write the contract's own guarantee turned up that `OVT_ParkedVehicleSpawningDeploymentModule` authors **no `OnCleanup()` override at all** — `OVT_DeploymentComponent.DestroyDeployment()` calls `module.Cleanup()` on every spawning module, but the parked module's own `Cleanup()` (inherited, empty) never touches `m_aSpawnedEntities` or deletes the vehicle entity. In the shipped game this is masked by the same fact the module's own class header already states — "that config authors no reinforcement module, and that is what keeps it alive… the deployment persists for the life of the campaign" — so `DestroyDeployment()` on a parked-vehicle deployment essentially never happens. It is a pre-existing gap, not something Phase 5 introduced, and fixing it is out of this phase's scope (T5.2 asked for `ReleaseVehicleOwnership`, not a teardown rewrite) — but it means the test fixture below has to delete its own fixture vehicle by hand rather than trusting the deployment's teardown to do it, and a future phase or bugfix that adds a real `OnCleanup()` to this module must delete only entities still in `m_aSpawnedEntities` — which `ReleaseVehicleOwnership` already guarantees a handed-off hull is not.

#### The invoker's two publication points (T5.1/C9)

`m_OnBattleStarted` is invoked once at the tail of `StartBaseQRF` and once at the tail of `StartTownQRF`, both **after** `m_bQRFActive`/`m_vQRFLocation` are set and the mode's own RPCs have gone out — the same point Phase 4's survey confirmed `m_OnFinished` is subscribed at the counterpart teardown. Unlike `m_OnQRFTownChanged` (C9), it fires for **both** base and town QRFs and for **both** `STANDARD` and `COUNTER_ATTACK` — a garrison crewing up in secret is exactly what a silent siege should provoke, and a base battle has no town id to gate on in the first place.

#### 🔴 Plan defects found against the working tree

**1. The acceptance grep for "present in both cleanup paths" is literal, and a shared helper would have failed it.** The plan's own shape (`SubscribeToBattles()`/`UnsubscribeFromBattles()`, one call site each in `OnActivate()`/`OnDeactivate()`/`OnCleanup()`) is exactly how Phase 4's `TearDownEchelons()` is called from two places — but that precedent calls a shared *teardown* method, not `.Remove()` directly, and nothing in Phase 4's own acceptance criteria greps for the removal call by name. Phase 5's own acceptance criterion (`grep -n "m_OnBattleStarted.Remove" … → present in both cleanup paths`) is stricter and literal: a `grep -n` against a shared helper method finds the text **once**, not twice. `OnDeactivate()` and `OnCleanup()` therefore each carry their own inline `occupying.m_OnBattleStarted.Remove(OnBattleStarted);`, independently, rather than through `UnsubscribeFromBattles()` (which stays only as `SubscribeToBattles()`'s activation-side counterpart, itself not grepped for). Costs four extra lines; buys a mechanical guard that survives a future reader deleting one call site while believing the other still covers it.

**2. `ForceCreateDeploymentFrom`'s `position` parameter needs no standoff arithmetic here, unlike the echelon's.** Phase 4's `SendMountedEchelon` computes its own standoff point because its config authors `m_fLZStandoffDistance 0` — the deployment position **is** the stopping point (P4-1..P4-4). The sortie does the opposite: `m_fLZStandoffDistance 200` is authored on the mounted module itself, so `SendSortie()` passes the **raw battle location** straight through as `position`, and the module's own inherited "stop N metres short of the destination" logic supplies the pullback. Passing a pre-computed standoff point here (mirroring the echelon) would have pulled back **twice** — once in the crew-up module's own arithmetic, once in the mounted module's `m_fLZStandoffDistance` — and parked the sortie much further from the battle than 200 m.

**3. The mobile-checkpoint's approach band cannot use either shipped preset without re-deriving Phase 4's own P4-4 reasoning.** `OVT_MobileCheckpointBehaviorDeploymentModule.ChooseApproach()` samples road points a band **from `m_ParentDeployment.GetPosition()`** — the raw, un-pulled-back battle location, not wherever the mounted module's `m_fLZStandoffDistance` actually stopped the vehicle. The harassment config's 150–300 m band assumes the deployment position **is** the objective (correct there); the echelon's 0–50 m band assumes the deployment position **is** the standoff (correct there, because its own `m_fLZStandoffDistance` is 0). Neither assumption holds for the sortie, where the deployment position is the battle and the vehicle stops 200 m short of it under its **own** module's pullback. `Deployment_BaseArmourSortie.conf` therefore authors a **third** band, `150`–`250`, centred on the 200 m standoff rather than on either precedent, so the checkpoint's own road search happens near where the vehicle already arrived rather than trying to re-drive it another 150–300 m closer to (or, worse, past) the fight. This is a documented judgement call, not a mechanically-guarded one — nothing in either test tier drives a real vehicle to check the two mechanisms actually agree.

**4. `OVT_InfantrySpawningDeploymentModule` had never been authored with `m_iMinGroupCount 0` / `m_iMaxGroupCount 0` anywhere in the tree, and it had to be checked rather than assumed.** T5.5 says "a crew group" (singular) for the sortie's mounted module, and `m_sTruckCrewGroup` (the driving/gunning crew) is the correct home for it — `EnsureCrew()` registers it independently of vehicle spawning, so it crews the **adopted, previously-empty** hull exactly as F6 requires. That leaves the module's OWN passenger side (`m_sGroupType`, the dismounting infantry every OTHER mounted config authors) with nothing to put there, since F6 describes only a crewed vehicle, not a squad riding along. `CalculateGroupCount()`'s `Math.Clamp(numGroups, m_iMinGroupCount, m_iMaxGroupCount)` clamps cleanly to `0` when both bounds are `0`, and the spawn loop that would consult `m_sGroupType` never runs at that count — read from the source, not observed, since no Init case in this tree's automated tiers ever builds a live deployment far enough to converge one. Recorded as a design decision below rather than hidden in the `.conf`.

#### Design decisions taken in this phase

**P5-1 — The sortie's own "force" is entirely its crew; there is no separate dismounting squad.** `m_iMinGroupCount 0` / `m_iMaxGroupCount 0` / `m_sGroupType ""`, and `m_sTruckCrewGroup "vehicle_crew"` carries the whole fighting complement. A vehicle destroyed under this crew still falls back to `DismountAndWalk` exactly as every other mounted config's does, but with zero registered passenger groups the "march" is empty — the crew (registered separately, under `CREW_KEY_SUFFIX`) is not part of `m_aHandles` and is not marched anywhere. This is an accepted, honestly-recorded degradation specific to a zero-passenger mounted config: the men do not evaporate (D9's fallback still applies to the module's own groups, of which there are none to lose), but nothing walks them home either. No automated tier can observe this — it would require a live vehicle destruction on a converged deployment.

**P5-2 — `m_iTruckCostOverride 0` on the sortie's mounted module, and what that costs.** The hull is adopted, never bought through this config's own ladder resolution — `RungAffordable` treats a budget of `0` as "only something free would do" (D4's own documented consequence), so `GetVehiclePrefabFromFaction`'s ladder branch always refuses and falls to the named fallback, `m_sTruckVehicleType "light_armed"`. That fallback is authored defensively, matching P4-6's own reasoning ("a future caller that reaches this config through plain `ForceCreateDeployment` gets a walking force instead of no force") — it is never consulted on `CrewUpOnAlarmBehaviorDeploymentModule`'s own path, because `SpawnTruck()`'s adopt branch returns before either the ladder or `m_sTruckVehicleType` is ever asked.

**P5-3 — What ends a sortie: a poll on `m_bQRFActive`, not a subscription to the QRF's own `m_OnFinished`.** T5.5's own prompt ("does this config need an analogous module to be torn down correctly, and does anything leak if the battle ends first?") was answered the same way Phase 4 answered it for the echelon — by checking rather than assuming. The echelon's answer (nothing collects it; the QRF alone tears it down) does not transfer: the QRF controller **owns** its echelons for exactly the life of its own battle, but the crew-up module lives on the base's near-permanent parked-armour deployment, which outlives any one battle by design. Subscribing `CrewUpOnAlarmBehaviorDeploymentModule` to the **live QRF controller's own** `m_OnFinished` was considered and rejected: it would mean holding a component reference across an entity that the occupying faction manager itself deletes (`SCR_EntityHelper.DeleteEntityAndChildren(m_CurrentQRF.GetOwner())`) the moment the battle resolves, and unsubscribing from a possibly-already-deleted component on this module's own teardown is exactly the kind of dangling-reference hazard `EntityID`-not-pointer exists to avoid elsewhere in this tree. A poll avoids it entirely: the faction runs **at most one** QRF at a time (`if(m_CurrentQRF) return;` in both `StartBaseQRF`/`StartTownQRF`), so "no battle active" (`!occupying.m_bQRFActive`) correctly means "the battle that provoked this sortie is over," checked every `OnUpdate()` once a sortie is out, and the sortie is `CollectDeployment()`'d — not deleted — the moment that is true. **Collect, not delete, because of which ledger paid**: the sortie is bought from the deployment **pool** at `SendSortie()`'s own create-then-debit choke point (`SubtractFactionResources`, matching `OVT_ObjectiveDirectorComponent.CreateObjectiveDeployment` and T6.4's own dispatcher, not the QRF's reserve-funded, zero-invested D6 pattern), so collecting it back into the same pool is the correct, conserved teardown rather than the money-creating bug D6 exists to prevent for the echelon.

**The honest gap, stated rather than hidden.** The poll is coarse on two axes: two battles starting back to back with no gap between them would leave a sortie uncollected until the *next* gap rather than the one that actually ended its own fight (a delayed collection, never a leak — the pool is never touched twice); and the poll only runs because `CrewUpOnAlarmBehaviorDeploymentModule.OnUpdate()` runs, which requires the base's own parked-armour deployment to still exist. If that deployment is torn down by something else — a base changing hands, a campaign reset — while its sortie is still out, nothing is left watching, and the sortie's own config authors no reinforcement or condition module of its own to fall back on. This mirrors the base deployment's own documented shape (near-permanent, "persists for the life of the campaign") rather than contradicting it, and is recorded here rather than discovered later.

#### Can-fail proofs

The suites are deferred for this whole feature, so — as in Phases 2–4 — each proof below is a **fault injected into the subject and compiled**. Every one exited `tools/compile-check.sh` with **0**, which is the point: none is a script error, so nothing in the toolchain would stop any of them shipping. Every subject was restored and the tree recompiled clean (exit 0, 6338 files) afterwards.

| Case | Fault | Expected red | Injected? |
|---|---|---|---|
| `…ParkedVehicle_CloneCarriesTheLadderRole` | `clone.m_sVehicleRole = m_sVehicleRole;` deleted from `CloneModule()` | "the parked module's clone lost m_sVehicleRole" | ✅ compiled clean |
| `…CrewUpOnAlarm_FiresOnceInsideRadiusNotOutside` | the `> m_fAlarmRadius` early return deleted from `OnBattleStarted()` | "a battle … out … must not send a sortie" | ✅ compiled clean |
| `…CrewUpOnAlarm_OwnershipTransferLeavesExactlyOneOwner` | `parked.ReleaseVehicleOwnership(vehicle);` moved to BEFORE `mounted.AdoptVehicle(vehicle)` | 🔴 **did not go red** — see below | ✅ compiled clean, reasoned through |

⚠ **The third injection is the honest finding of the phase, not a clean win.** Moving the release earlier does not delete or invalidate the `Vehicle` entity — `ReleaseVehicleOwnership` only removes an id from the parked module's own tracking array — so `AdoptVehicle()` still succeeds afterwards, and the case's own assertions (which read the **end state**: the parked module reports zero, the sortie's mounted module reports the same entity) are satisfied either way. **This case proves the end state, not the ordering that is supposed to guarantee it.** The ordering itself — release only after `AdoptVehicle()` returns `true` — is a correctness argument recorded in the class header and verified by reading, not by a test that can distinguish "correct because ordered right" from "correct because nothing exercised the window." A case that could tell the two apart would need to fail `AdoptVehicle()` deliberately (a second module already holding a hull, or no mounted module on the config) and then assert the vehicle is STILL owned by the parked module — which the budget-gate-style refusal cases elsewhere in this tree do for other modules, and which this phase did not add for lack of a cheap way to make `AdoptVehicle()` fail on a freshly-created clone without first breaking the fixture some other way.

#### What the new cases could NOT prove, stated honestly

- **Nothing here drives a vehicle, parks one, or dismounts anybody.** As in every prior phase, no case's fixture ever reaches a real convergence — `ForceCreateDeployment`'s clones exist synchronously but `OnActivate()`/`EnsureGroups()` are a whole 8–12 s update interval away, and no test step spans one. `TickPark`, the checkpoint's own approach-band reconciliation (defect 3 above), and the crew actually driving the 200 m leg are all unexercised.
- **The reordering fault above cannot be told apart from correct code by this tier**, as detailed in the can-fail table.
- **`TickSortieTeardown()`'s poll is read, not run.** No case's fixture ever ticks (`OnUpdate()` is never called by the framework inside one test step), so nothing here observes a sortie actually being collected when `m_bQRFActive` goes false — that is a play-test item, not an automated one, and the honest gap above (a base deployment torn down mid-sortie) is unreachable by any tier.
- **P5-1's zero-passenger `DismountAndWalk` behaviour is unexercised.** No case destroys a vehicle in `HOLDING`; the consequence recorded in P5-1 is read off `TickHold()`/`DismountAndWalk()`'s existing, already-tested (Phase 2) code, not observed against this config's own zero-group shape.
- **The two new group prefabs are unverified by any gate.** `compile-check.sh` does not parse `.et`; the GUID cross-references (three to five independent hits per character prefab) are the only corroboration available in an extracted tree with no `.meta` files on vanilla resources, and Workbench is the only thing that can confirm either prefab resolves, previews correctly, or spawns a group with three live compartment-ready characters.
- **MP, JIP and save/reload are untouched**, as everywhere else in this tree. D9 covers a live sortie the same way it covers every other mounted force — nothing new persists, and a restored campaign never has one.

#### Play-test checklist for this phase

Run after Phase 3's and Phase 4's checklists, on a fresh Normal campaign. Debug affordances as before: `/give-resources`, a raised time multiplier, and — only if the ladder needs forcing — a **temporary** `vehicleThresholdScale 0.05` in `Difficulty_Normal.conf`, **reverted before committing**.

1. **Workbench first.** `Configs/Deployment/overthrowDeployments.conf` — expect **26** entries; the "Base Parked Armour" delta expands with the parked module's role/cost overrides and the crew-up module; "Base Armour Sortie" expands with the mounted module (`m_bAdoptExistingVehicle 1`) and the mobile checkpoint. Open both faction `.conf`s — expect a `vehicle_crew` group entry each, resolving against its new prefab with no unresolved attribute.
2. **Scout a base at threat ≥ 400 with the roll in its favour** (`m_fChance 60`). Expect a parked armed vehicle, uncrewed, exactly as F6 describes (unchanged from the existing parked-vehicles behaviour — this phase adds nothing observable until the base is attacked).
3. **Attack that base.** Expect the log to show `Crew-up '…': base at … crewed its parked vehicle and sortied it towards … for 60 resources`, then the mounted module's own `Mounted force '…': role 'armed' … resolved to '…'` line — but note it should describe the ADOPTED hull, not a freshly spawned one; watch for a **second** vehicle appearing beside the first, which would mean `AdoptVehicle()` silently fell through to `SpawnTruck()`'s normal path. → F6, ownership contract.
4. **Watch the vehicle drive to its standoff.** Expect it to stop roughly 200 m short of the battle, on a road, with its crew still aboard and the gun manned. → P5-2, the mobile-checkpoint band (defect 3).
5. **Attack a DIFFERENT base.** Expect the first base's parked armour (if any is left, or a different base entirely) to do nothing — the geofence should refuse silently at VERBOSE. → F6's negative case.
6. **Let the battle end.** Expect the log to show the sortie collected (`Crew-up '…': the battle is over - its sortie has been collected`) and no orphan armoured deployment on the map afterwards. → P5-3.
7. **Save mid-sortie, quit, Continue.** Expect the sortie's force on the ground and walking (or, per P5-1, simply gone if it has no passengers to march — watch specifically for this), no ghost vehicle, no crew-up log after the load. → D9, P5-1.

### Phase 6

**Status:** ✅ Complete (6/6). `tools/compile-check.sh` exit 0 (6340 files). Suite deferred per the whole-feature policy above.

**Files:**
- TOUCH `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` — `ReportVehicleLoss(vector)`, `TickHunterKiller()` + `IsHunterKillerStillLive()` + `PickHunterKillerTarget()`, `m_HunterKillerDeployment` + `m_bHunterKillerActive`, three new constants (`HUNTER_KILLER_CONFIG_NAME`, `HUNTER_KILLER_THREAT_FLOOR`, `VEHICLE_LOSS_DEDUP_RADIUS_M`), one call site inside `CheckUpdate()`.
- TOUCH `Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` — **thirteen lines inside `ReleaseConvoy()`, nothing else** (see "what was added" below). This is on top of the observer repair's own diff, which this phase did not touch further.
- NEW `Scripts/Game/GameMode/Deployments/Modules/OVT_ArmouredSweepBehaviorDeploymentModule.c`.
- NEW `Configs/Deployment/Deployment_HunterKillerSweep.conf(.meta)`.
- TOUCH `Configs/Deployment/overthrowDeployments.conf` — one entry, `"Hunter Killer Sweep"`. The registry now carries **27**.
- NEW `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_HunterKillerSweep.c` — 4 cases + a shared fixture class.

**GUIDs claimed** (re-grepped at the start of the phase: `6BC1` held Phase 1's `6BC11…0001`–`0004`, Phase 3's `6BC10…0001`–`0007`/`0011`, Phase 4's `6BC10…0021`–`0024`/`0031`, Phase 5's `6BC10…0041`–`0044`/`0051`–`0053` plus `6BC11…0003`–`0004` and `6BC13…0001`–`0007`/`0011`–`0017` — exactly as the task brief predicted, so everything from `0061` up was free):

| GUID | What |
|---|---|
| `{6BC1000000000061}` | `Deployment_HunterKillerSweep.conf` (the `.meta` Name) |
| `{6BC1000000000062}` | its `OVT_MountedForceSpawningDeploymentModule` |
| `{6BC1000000000063}` | that module's `OVT_NearestControlledBaseSourceProvider` |
| `{6BC1000000000064}` | its `OVT_ArmouredSweepBehaviorDeploymentModule` |
| `{6BC1000000000071}` | the `overthrowDeployments.conf` registry entry |

#### T6.6 — why the evaluator could not be used (C5), restated at the new call site

`OVT_DeploymentManagerComponent.CollectSeedCandidates()` and `FindDeploymentCandidates()` — the ordinary 30 s evaluator's whole notion of "where to build something" — only ever produce **named-location** positions: a town centre, a base marker, a port, an airfield, a radio tower, a checkpoint. A hunter-killer sweep is centred on wherever a vehicle was just destroyed, which is none of those — it is an arbitrary point on the map that happens to coincide with a known target's `location`. There is no location-type flag that means "an ambush site", and inventing one would not help: the evaluator's candidate list is built by walking authored markers, not by walking `m_aKnownTargets`. So the only way this deployment is ever created is a **deliberate caller** that already knows the position — exactly the shape Phase 4's QRF echelon and Phase 5's base armour sortie established before it. `Deployment_HunterKillerSweep.conf` is `m_bDirectorOnly 1` for the same reason those two are: the evaluator must never pick it up on its own.

**The create-then-debit choke point, restated at `TickHunterKiller()`:**

```
int cost = config.GetTotalResourceCost();
if (deployments.GetFactionResources(factionIndex) < cost) return;      // afford gate — nothing spent
OVT_DeploymentComponent created = deployments.ForceCreateDeployment(config, best.location, factionIndex, cost);
if (!created) return;                                                  // refused — nothing spent
deployments.SubtractFactionResources(factionIndex, cost);              // debited only AFTER a real create
```

This is the same shape as `OVT_ObjectiveDirectorComponent.CreateObjectiveDeployment` (`:1225`) and `OVT_CrewUpOnAlarmBehaviorDeploymentModule.SendSortie()`: `cost` is read exactly once, from `GetTotalResourceCost()`, and reused unchanged as both the affordability question and the amount debited, so the two numbers can never drift apart. `ForceCreateDeployment` (not `ForceCreateDeploymentFrom`) is enough here — unlike the echelon and the sortie, this config authors its own source provider (`OVT_NearestControlledBaseSourceProvider`) and needs no runtime override, because `TickHunterKiller()` has no "this base" to hand in, only a hotspot.

#### The one-clock decision (Phase 2's own open question, resolved)

**Decision: one clock, and it is `OVT_ArmouredSweepBehaviorDeploymentModule.m_iSweepMinutes`, converted to update ticks with the exact `UPDATE_SECONDS`/`SECONDS_PER_MINUTE` arithmetic `OVT_MobileCheckpointBehaviorDeploymentModule.m_iRelocateMinutes` already uses.** `Deployment_HunterKillerSweep.conf` authors `m_iHoldTicks 0` on the mounted module — the class's own documented default, "0 holds indefinitely" — so the mounted module's hold never expires on its own, and `IsHoldExpired()` is never read anywhere in this phase.

**Why not the alternative** (poll `IsHoldExpired()` from the sweep behaviour, keep `m_iHoldTicks` authored on the config to match `m_iSweepMinutes`): it would put the **same duration in two places** a tuner could edit independently — the mounted module's raw update-tick count and the sweep module's minutes — run through **two different pieces of arithmetic**, with nothing anywhere to notice the two had drifted apart. A config author who changed `m_iSweepMinutes` from 12 to 20 without also recomputing `m_iHoldTicks` (72 → 120) would get a sweep whose behaviour module thinks it has 20 minutes but whose mounted module force-latches `IsHoldExpired()` at the old 12-minute mark — silently, because nothing reads that latch to notice. The chosen shape has exactly one number a tuner can get wrong (`m_iSweepMinutes`), in exactly one file, converted by exactly one piece of arithmetic that was already play-tested (by construction — it is a copy of the checkpoint's own) rather than invented fresh.

**One clock is enforced two ways, not just documented:** `m_iHoldTicks 0` in the shipped config (so the mounted module's own hold never expires), and no executable statement in `OVT_ArmouredSweepBehaviorDeploymentModule` ever calls `IsHoldExpired()` — the name appears only inside this file's own `//!` header, discussing and rejecting the two-clock alternative, so a plain `grep` finds it but a reading of the code (everything outside a `//!` line) does not.

#### 🔴 What was added to `OVT_InsertionSpawningDeploymentModule.c`, exactly

Thirteen lines, all inside `ReleaseConvoy()`, before its first pre-existing statement:

```cpp
if (m_Truck && !IsTruckOperational())
{
	OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
	if (occupying)
		occupying.ReportVehicleLoss(m_Truck.GetOrigin());
}
```

(plus a six-line doc comment above it). **This is deliberately ONE hook, not two**, even though T6.1's own wording names two things — "the mounted module's HOLDING/DRIVING vehicle-loss branch" and "`ReleaseConvoy` when the vehicle was destroyed". Tracing both named branches shows why one hook covers both: the mounted module's own `TickHold()` (HOLDING) calls `DismountAndWalk("its vehicle was destroyed")` on vehicle loss, and the inherited `TickDrive()` (DRIVING) calls `DismountAndWalk("its transport was destroyed")` on the identical test — and `DismountAndWalk()` itself calls `ReleaseConvoy(reason, false)` unconditionally. Both roads to a destroyed vehicle already funnel through this one method before this phase touched it; gating on `IsTruckOperational()` there, rather than on either caller's `reason` string, is what makes the hook correct for both without a second copy.

**Gated on the truck's own state, not on `reason`, and that is what keeps it out of teardown.** `OnCleanup()`'s own `ReleaseConvoy("the deployment is over", true)` — the literal case T6.1 says must not report anything — reaches the same hook, but with an intact vehicle `IsTruckOperational()` answers true and nothing fires. `m_Truck &&` first, separately: `FallBackToWalking()` calls `ReleaseConvoy` on a convoy that never got a truck spawned at all, and a force that never had a vehicle has not lost one.

**Broadened beyond "mounted" on purpose.** The hook lives in the shared base class, so it also fires for a **plain** insertion's transport destroyed on the drive out or the way home — not only a mounted force's own hull. This was a deliberate choice, not an oversight: T6.1's own final clause — "and from `ReleaseConvoy` when the vehicle was destroyed" — names no restriction to mounted configs, and there is no reason the occupying faction should learn less from an ordinary supply truck's death than from an armed vehicle's. `ReportVehicleLoss()`'s own dedup means this costs nothing extra when a mounted loss and a plain-insertion loss happen to coincide.

**G6/Q1 remains legitimately broken by the observer repair, and this phase adds one more line item to the honest restatement of it** (see the "✅ ROOT CAUSE FIXED" section above): `git diff Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` now carries the observer repair AND this phase's thirteen lines. Nothing about D1 (subclass, do not refactor) changes, and the acceptance grep for this phase's own diff — the thirteen lines above, isolated — is the one that matters here.

#### `ReportVehicleLoss()` — the target-type choice

Typed `OVT_TargetType.CAMP`, an **existing** enum value, not a new one. `OVT_TargetType` has no member for "a vehicle died here", and this phase does not add one: `OVT_TargetType` is consulted only by `if`-branches (`GetThreatByLocation()`, `UpdateKnownTargets()`), never by an exhaustive `switch`, so a new member would have been syntactically safe — but every existing member names a persistent enemy **holding** (a base, a tower, a FOB, a camp, a warehouse), and a vehicle wreck is not one. CAMP was picked over WAREHOUSE (the lowest weight, 5 — too faint to reliably clear `HUNTER_KILLER_THREAT_FLOOR` on its own) and over BASE/FOB/TOWER (each names something more specific and more permanent than a wreck) as the closest existing fit: `THREAT_WEIGHT_ENEMY_CAMP` (20) is exactly the weight `HUNTER_KILLER_THREAT_FLOOR` (15) was set below, so a fresh, undeduplicated loss clears the floor on its own at its own exact position — see the floor's own comment for the arithmetic.

#### 🔴 Plan defects and findings against the working tree

**1. `Deployment_QRFMountedEchelon.conf` authors `m_sTruckCrewGroup "truck_crew"` for an ARMED vehicle, and `truck_crew` mans no gun.** Traced while deciding this config's own crew group: `truck_crew` (`USSR_OverthrowData.conf:51-55`, and the US mirror) is "two drivers to crew a transport on a live insertion" — no gunner position — while `vehicle_crew` (Phase 5's own addition, `:56-61`) is "driver, gunner and commander... mans the gun". Phase 4's echelon config authors the former despite its mounted module carrying `m_sVehicleRole "armed"`, which means the QRF's own mounted echelon likely stands at its standoff with an unmanned turret. **This phase does not fix it** — the echelon config belongs to Phase 4, and re-tuning it here would be an undocumented change to a file this phase has no acceptance criterion over. `Deployment_HunterKillerSweep.conf` authors `m_sTruckCrewGroup "vehicle_crew"` deliberately, precisely because a hunter-killer's whole purpose is to be a real threat that can shoot back — see the design decision below. Recorded here so the echelon's own gap is not silently repeated a third time on a future config.

**2. `HUNTER_KILLER_THREAT_FLOOR`'s first value (25) would have made a lone reported vehicle loss unable to trigger its own dispatcher.** T6.4 specifies only that a floor must exist, not its value. The first number chosen (a round 60 % of `THREAT_WEIGHT_ENEMY_BASE`) sat ABOVE `THREAT_WEIGHT_ENEMY_CAMP` (20) — the weight `ReportVehicleLoss()` inserts at — which would have meant a vehicle destroyed in genuinely empty country (nothing else known nearby) could never buy its own sweep, contradicting the feature's own summary ("losing a vehicle... buys one bounded armoured sweep"). Caught before it shipped, not by a test — no case pins the floor's own tuning value, only its existence as a gate — but by re-reading `GetThreatByLocation()`'s weight table against the type `ReportVehicleLoss()` uses before picking a number. Fixed to 15, documented as "under `THREAT_WEIGHT_ENEMY_CAMP`" in the constant's own comment rather than as a bare number, so the relationship cannot silently drift if either constant is retuned alone.

#### Design decisions taken in this phase

**P6-1 — The sweep's own mounted module authors `m_sTruckCrewGroup "vehicle_crew"`, matching the sortie rather than the echelon.** See defect 1 above. `m_iCostPerGroup` and the rest of the mounted module's attributes otherwise mirror `Deployment_QRFMountedEchelon.conf`'s own shape (both are director-only, both source from `OVT_NearestControlledBaseSourceProvider`, both carry a single `light_fireteam` passenger group) — the crew group is the one deliberate departure.

**P6-2 — `m_fLZStandoffDistance 0`.** The dispatcher's own `ForceCreateDeployment(config, best.location, ...)` call hands the hotspot straight through as the deployment's position; with a zero standoff the mounted module drives directly to it rather than pulling back, which is correct for a sweep whose whole job is to loiter AT the hotspot rather than hold a line short of it (contrast the echelon's own standoff, which exists because a battle is dangerous to drive into, and the sortie's, which exists because the battle it is answering already has other forces converging on it).

**P6-3 — `Patrol`/`Defend` waypoints are issued directly on the crew, never through `BuildVirtualPlan()`, for the identical reason `OVT_MobileCheckpointBehaviorDeploymentModule` gives in its own header.** A plan is registration-time input only; a sweep that roams needs to keep moving after registration, which only a direct waypoint on the (null-plan) crew group can do. `OVT_ArmouredSweepBehaviorDeploymentModule` duplicates the checkpoint's `ResolveCrewGroup`/`ClearOwnedWaypoints`/`DetachForeignWaypoints` shape rather than sharing code with it — the same trade every sibling behaviour module in this tree makes (P3-4/P3-5's own precedent), because the two modules' waypoint TYPES differ (`SpawnPatrolWaypoint`/`SpawnDefendWaypoint` vs `SpawnMoveWaypoint`) and a shared base would have to parametrise that difference for exactly two callers.

**P6-4 — A road search that fails falls back to a `Defend` waypoint at the hotspot itself, never to "no order at all".** `ChooseSweepPoint()` samples `SWEEP_SAMPLES` (8) bearings, starting at a random one so repeated failures do not always try the same directions first, and gives up only after all eight find no road within `ROAD_SEARCH_RADIUS_M` (150 m) of a point inside `m_fSweepRadius`. A hotspot with no road anywhere nearby still gets a vehicle sitting there with a gun manned, which is strictly better than a vehicle with no order at all — the same "worse placement beats a force that never gets out" argument P3-7 makes for the checkpoint's own patience bound.

#### Can-fail proofs

The suites are deferred for this whole feature, so — as in every prior phase — each proof below is a **fault injected into the running tree and compiled**, not a suite run. Every one exited `tools/compile-check.sh` with **0**, which is the point: none is a script error, so nothing in the toolchain would stop any of them shipping. Every subject was restored and diffed byte-identical against its pre-fault copy, and the tree recompiled clean (exit 0, 6340 files) after every single one.

| Case | Fault | Expected red | Injected? |
|---|---|---|---|
| `…RefusesAtEachGate` (gate 1) | `if (m_bHunterKillerActive && IsHunterKillerStillLive())` replaced with `if (false)` | "a live sweep already out must not be replaced by a new one" | ✅ compiled clean |
| `…RefusesAtEachGate` (gate 2) | `if (IsQRFEngaged()) return;` deleted from `TickHunterKiller()` | "a battle in progress must not be interrupted by a hunter-killer sweep" | ✅ compiled clean |
| `…RefusesAtEachGate` (gate 3) | `if (score < HUNTER_KILLER_THREAT_FLOOR)` relaxed to `if (score < 0)` | "an empty hotspot with nothing known nearby (score 0) must not buy a sweep" | ✅ compiled clean |
| `…RefusesAtEachGate` (gate 4) | `if (deployments.GetFactionResources(factionIndex) < cost) return;` deleted | "a pool of 0 against a config that costs N must not buy a sweep" | ✅ compiled clean |
| `…SpendsExactlyOnce…` | `SubtractFactionResources` moved to BEFORE `ForceCreateDeployment` | (see below — did **not** go red) | ✅ compiled clean, did not redden |
| `…ReportVehicleLossDeduplicates` | the dedup distance guard replaced with `if (false)` | "a second loss at the SAME spot must not add a second known target" | ✅ compiled clean |
| `…ArmouredSweep_CloneCarriesEveryAttribute` | `clone.m_iSweepMinutes = m_iSweepMinutes;` deleted from `CloneModule()` | "the sweep clone lost m_iSweepMinutes" | ✅ compiled clean |
| *(no case — see below)* | `ReleaseConvoy()`'s `if (m_Truck && !IsTruckOperational())` replaced with `if (false)` | *(nothing reachable by any tier — see below)* | ✅ compiled clean |

⚠ **Two faults are honestly recorded as NOT proven red, or not provable at all, rather than dressed up as tested:**

1. **The debit-reorder fault on `TickHunterKiller()` did NOT go red.** Moving `SubtractFactionResources` to before `ForceCreateDeployment` compiles clean and produces the **same observable pool figure**, because the fixture's `ForceCreateDeployment` call always succeeds (a real, resolvable world position) in the world this suite runs against — there is no refusal for the reorder to expose. The create-then-debit ORDER is a correctness argument for the refusal case, not something this tier's assertions (which read the pool only before and after the whole call) can distinguish from "debited unconditionally, which happened not to be tested against a refusal today." This is the same class of gap Phase 5's ownership-reorder finding recorded for `SendSortie()`, and it is recorded in the case's own doc comment as well as here.
2. **`ReleaseConvoy()`'s hook cannot be reached by any case in this tree at all.** No test in this whole feature drives a real convoy to a real destruction — every prior phase's own notes say the identical thing (a deployment's first convergence is a whole 8-12 s update interval after creation, and no Init-tier test step spans one). The fault above was injected and compiled by hand against the running tree to prove the toolchain would not catch its removal, then reverted; no case exists, and none could be written on this tier, that would have gone red on its own.

#### What the new cases could NOT prove, stated honestly

- **`TickHunterKiller()` never actually drives a vehicle to a hotspot.** Every gate case is entirely synthetic (fabricated `m_aKnownTargets`, a spawned-but-never-`Start()`-ed QRF controller for the engaged gate, a manually zeroed pool) and the success case's own deployment is torn down inside the same frame it is created — activation, and therefore the mounted module's first convergence, is a whole update interval away. Whether a hotspot really buys a sweep that really drives there and really loiters is a play-test question, listed below.
- **`OVT_ArmouredSweepBehaviorDeploymentModule`'s own tick — `ChooseSweepPoint`, `IssueSweepPoint`, `TickWaypointClock`, `TickSweepClock`, `StandDownSweep` — is entirely unexercised.** No case builds a live deployment far enough to reach `OnUpdate()` even once; everything asserted about the module's runtime behaviour (clone fidelity aside) is a reading of the code, not an observation.
- **`ReleaseConvoy()`'s hook, as detailed above, is unreachable by this or any prior phase's tier.**
- **The `.conf` is unverified by any gate.** `compile-check.sh` does not parse it. The module list, the two GUIDs, `m_sTruckCrewGroup "vehicle_crew"`, and every numeric attribute are Workbench/play-test items.
- **MP, JIP and save/reload are untouched**, as everywhere else in this tree. D9 covers a live sweep the same way it covers every other mounted force — nothing new persists, and a restored campaign never has one.

#### Play-test checklist for this phase

Run after Phase 3's, Phase 4's and Phase 5's checklists, on a fresh Normal campaign. Debug affordances as before: `/give-resources`, a raised time multiplier, and — only if the ladder needs forcing — a **temporary** `vehicleThresholdScale 0.05` in `Difficulty_Normal.conf`, **reverted before committing**.

1. **Workbench first.** `Configs/Deployment/overthrowDeployments.conf` — expect **27** entries; `"Hunter Killer Sweep"` expands into two modules (mounted force, armoured sweep) with no unresolved attribute.
2. **Destroy an occupying vehicle** — the mobile checkpoint, a QRF echelon, or a base armour sortie, anywhere. Expect the log to show `A vehicle was lost at … - the location is now a known target`.
3. **Wait for the next 60 s tick with nothing else competing** (no battle in progress, the deployment pool holding at least the config's cost — check the log at VERBOSE for `no rung/pool/floor` refusals if nothing happens within a couple of minutes). Expect `Hunter-killer sweep sent to … (score N) for 60 resources`, then the mounted module's own `Mounted force '…': role 'armed' at threat N resolved to '…'` line.
4. **Watch it arrive and loiter.** Expect it to drive to the hotspot (not stand off from it — P6-2), then relocate to a new patrol point roughly every 90 s, its gun manned throughout (`vehicle_crew` — P6-1). ⚠ If it never moves at all, that is the RISK TO THE PREMISE section above and the observer repair is what it tests.
5. **Destroy a second vehicle at least 200 m away while the sweep is out.** Expect no second sweep (gate 1) — the log should show nothing new until the first one's clock runs out.
6. **Let the 12-minute clock run out.** Expect `its sweep is over`, the deployment collected once no player is within `baseCloseRange`, and no orphan vehicle or crew left behind.
7. **Save mid-sweep, quit, Continue.** Expect the force on the ground and walking (D9), no ghost vehicle, no sweep log after the load.

### Phase 7

**Status:** ✅ Complete (5/5). No script touched, so `compile-check.sh` was not required and was not run.
Suite still deferred per the whole-feature policy; the **All** run remains the feature-close gate.

**Files:**
- TOUCH `Language/localization_Overthrow.st` — **4 new keys**, inserted as four `CustomStringTableItem`
  blocks immediately after `OVT-FieldManual_OccupyingForces_Text7`. Item GUIDs `{6B5A11C00000010D}` …
  `{6B5A11C000000110}`, continuing this page's own `6B5A11C00000010x` series (the highest previously used
  was `…10C`). Brace/quote balance re-verified after the edit (1218 `{` / 1218 `}`, no unterminated
  string).
- TOUCH `Configs/FieldManual/Categories/FM_Overthrow.conf` — four pieces (`SCR_FieldManualPiece_Header`
  + three `SCR_FieldManualPiece_Text`), GUIDs `{6B5A11C00000000D}` … `{6B5A11C000000010}`, appended to the
  **existing** "Patrols and Garrisons" entry after the *Defending a Base* block and before *Radio Towers*.
- NEW `docs/features/occupying/vehicles/wiki-draft.md` — unpublished player-facing page draft.
- TOUCH `docs/features/occupying/epic-overview.md` — row 7, the build-order entry, one dependency bullet,
  the Status/Last-Updated line and the rollup sentence.

⚠ `Configs/Language/` **does not exist in this tree** — the acceptance grep `git diff Configs/Language/`
is vacuous and errors with *"no such path in the working tree"*. The real runtime exports are
`Language/localization_Overthrow.<lang>.conf`. They were **not touched by this phase** (a concurrent session
had already modified all seven before Phase 7 started; that diff is unchanged by this work).

#### T7.3 — the tutorial popup was SKIPPED, deliberately, and this is the gap to report

There is **no trigger for "the player has seen an occupying armed vehicle"** and there is no way to author
one without new framework code. `OVT_TutorialEvent` (`Scripts/Game/Configuration/OVT_TutorialTrigger.c:12-44`)
is a closed catalog of **fourteen** events: eleven raised on the server from an existing manager
`ScriptInvoker` and three raised client-locally. None of them is about seeing anything, and the nearest
candidates all fail:

- `PLAYER_ENTER_BASE` (`:43`) fires on crossing into a base's restricted radius and carries **no payload**,
  so it cannot distinguish a base that has parked armour from one that has not (the delta authors
  `m_fChance 60` at `overthrowDeployments.conf:66`, so most bases will not).
- `BASE_CONTROL_CHANGE` / `TOWN_CONTROL_CHANGE` have no acting player and are proximity fan-outs.
- `OVT_TutorialTrigger.Matches` (`:105-127`) filters on `m_iValue` and an exact `m_sFilter` string only; a
  new event value plus a server-side observer that decides "this player can see an armed occupying vehicle"
  would be **new tutorial-framework capability**, which is explicitly out of scope for this agent and belongs
  to the tutorial-system feature.

Per the phase brief ("if wiring a reliable trigger would require new script, say so and skip the popup"), no
popup was authored. The Field Manual section is the deliverable, and it is complete without one.

#### T7.1 — the citation list. Every sentence written in this phase traces to one of these.

Each `.st` entry also carries the same citations inline in its `Comment`, so a translator or a later editor
can re-check a claim without this file.

| Claim as written | Source |
|---|---|
| Three rungs of armed vehicle per faction, role `armed` | `Configs/Factions/USSR_OverthrowData.conf:66-92`, `Configs/Factions/US_OverthrowData.conf:66-95` |
| USSR: UAZ469_PKM threat 0 / cost 25; BRDM2 threat 400 / cost 70; BTR70 threat 900 / cost 120 | `USSR_OverthrowData.conf:67-73`, `:76-82`, `:85-91` |
| US: M151A2_M2HB threat 0 / cost 25; M1025_armed_M2HB threat 400 / cost 70; LAV25 threat 900 / cost 120 | `US_OverthrowData.conf:68-74`, `:77-84`, `:87-93` |
| "A cannon or a heavy machine gun" on the top rung | LAV-25 (25 mm) and BTR-70 (KPVT) are the two authored top-rung prefabs above |
| Each rung costs several times the one below | 25 → 70 → 120, same lines. Numbers deliberately not quoted in-game, matching this page's existing rule (`OccupyingForces_Text` Comment) |
| A mounted force only fields what its own budget covers | D4 + `OVT_VehicleLadderRules.RungAffordable`; the authored ceilings are `Deployment_ObjectiveHarassment_Mounted.conf:27` (70) and `Deployment_HunterKillerSweep.conf` (`m_iTruckCostOverride 90`) |
| Difficulty moves the whole scale: ×2.0 Easy … ×0.25 Insane | `Configs/Difficulty/Difficulty_Easy.conf:26` 2.0, `Difficulty_Normal.conf:22` 1.0, `Difficulty_Hard.conf:27` 0.5, `Difficulty_Extreme.conf:27` 0.35, `Difficulty_Insane.conf:27` 0.25, `Difficulty_TestWorld.conf:12` 1.0 |
| Heavy rung at 1800 on Easy, 225 on Insane (wiki draft only) | 900 × 2.0 and 900 × 0.25, `OVT_VehicleLadderRules.ScaledThreshold` |
| Checkpoint parks 150–300 m out on an approach road, relocates every 4 minutes | `Configs/Deployment/Deployment_ObjectiveHarassment_Mounted.conf:44-47` |
| It only exists against the **current** objective, harassment through forward-base | same file `:57-61` (`m_sFromPhase "Harassment"`, `m_sThroughPhase "ForwardBase"`, `m_fMaxDistanceFromObjective 600`) |
| The force dismounts around it, the crew stays aboard so the gun stays manned | `OVT_MobileCheckpointBehaviorDeploymentModule.c:333` and `:366` |
| Parked armour: one vehicle, unmanned, from mid-campaign, not at every base | `Configs/Deployment/overthrowDeployments.conf:48-67` — `m_iVehicleCount 1`, `m_iCostPerVehicle 120`, `m_iMinimumThreatLevel 400`, `m_fChance 60`, `m_bFreeAtGameStart 0`, no crew group on the parked module |
| It answers a battle at **its own** base and not one elsewhere | `OVT_CrewUpOnAlarmBehaviorDeploymentModule` authored with `m_fAlarmRadius 750` at `overthrowDeployments.conf:58-63` |
| The sortie takes a firing position short of the shooting and does not roam | `Configs/Deployment/Deployment_BaseArmourSortie.conf:38-41` (150 / 250 / `m_iRelocateMinutes 0`) |
| Up to two armed vehicles are driven to any one battle | `OVT_QRFControllerComponent.c:118` `ECHELON_CAP_PER_BATTLE = 2`, enforced `:1031` |
| They come from a holding of its own, by road | `:127` `ECHELON_ROAD_SEARCH_M = 200`, refusals at `:1163` and `:1244`; land-isolated refusal per D10 |
| They stop short of the fighting | `:123` `ECHELON_STANDOFF_M = 450`. **Not quoted in-game** — internal constant |
| Stopping the vehicle leaves the men on foot | `OVT_MountedForceSpawningDeploymentModule.c:333` `DismountAndWalk("its vehicle was destroyed")`, `:339` for a dead crew. Feature goals G4/F8 |
| A vehicle lost in the open marks that ground | `OVT_OccupyingFactionManager.ReportVehicleLoss` (≈`:2282`), inserted through the same `m_aKnownTargets` path as every other known target, deduplicated against the nearest existing one |
| Another **may** be sent to work the area over for about twelve minutes | `TickHunterKiller()` `:2321` on the existing 60 s tick (`OF_UPDATE_FREQUENCY` `:289`, called `:1834`); `Configs/Deployment/Deployment_HunterKillerSweep.conf` — `m_iMinimumThreatLevel 300`, `m_iBaseCost 60`, `m_fSweepRadius 400`, `m_iSweepMinutes 12`. **"May" is load-bearing**: four refusal gates (one live sweep, no engaged battle, a threat-score floor, pool affordability) |
| Live vehicles are not saved; a restored force walks (wiki draft only) | D9 |
| Reachability is bounded, not solved (wiki draft only) | D10, and the epic's *Land reachability* bullet added by Phase 4 |
| "Anti-tank starts paying for its weight around the middle of a campaign" | **The one interpretive gloss.** It renders `m_iMinThreat 400` (the mid rung, against a top rung at 900) as "the middle of a campaign" and quotes no number. It claims nothing about damage models or launcher availability, and it is written as a statement rather than an instruction (tone rule: inform, never instruct) |

**Two things were deliberately NOT written**, because nothing in the tree supports them: any statement that
armour appears on a schedule or at a stated point in a campaign (it is a threat threshold crossed with a
budget, and both move), and any statement about what a given launcher does to a given hull.

#### What Phase 7 could NOT do

- **The wiki was not touched.** No `wikijs` MCP server was attached to this session, so nothing was searched,
  created or updated on https://wiki.armaoverthrow.com. `wiki-draft.md` carries the pre-publication checklist
  (search before creating; a section on an existing occupying-faction page is preferred over a new page).
- **No screenshot or tile image.** The new section sits on the existing page, which uses
  `UI/Textures/FieldManual/Tiles/default_ui.edds`; no new tile is needed. A dedicated armour page later would
  need one.
- **The four new keys are invisible in game until the user re-exports localization from Workbench.** The
  `.st` master is the only editable file; `Language/localization_Overthrow.<lang>.conf` are build output.

---

## Session log

**2026-08-23 — `/autorun-feature occupying/vehicles`**
- Resolved into the `occupying` epic as feature 7. `implementation.md` (84 KB, 7 phases) already existed; no re-plan.
- Scaffolded `tasks.md` + `context.md`. Recorded the whole-feature suite deferral above.

---

## 🔴 Cross-phase fix (orchestrator, 2026-08-23) — two mounted configs were fielding an UNARMED crew

Phase 6's report flagged that `Deployment_QRFMountedEchelon.conf` authored `m_sTruckCrewGroup "truck_crew"`.
Checking all four mounted configs found **two** with the defect:

| Config | Phase | Was | Now |
|---|---|---|---|
| `Deployment_ObjectiveHarassment_Mounted.conf` | 3 | `truck_crew` | **`vehicle_crew`** |
| `Deployment_QRFMountedEchelon.conf` | 4 | `truck_crew` | **`vehicle_crew`** |
| `Deployment_BaseArmourSortie.conf` | 5 | `vehicle_crew` | unchanged |
| `Deployment_HunterKillerSweep.conf` | 6 | `vehicle_crew` | unchanged |

`truck_crew` is authored in both faction registries as *"Two drivers to crew a transport on a live
insertion"* — driver and co-driver, **no gunner**. `vehicle_crew` (Phase 5, cost 15) is *"Driver, gunner and
commander to crew an armed vehicle."*

**Why this mattered more than a wrong number.** It defeats the feature's two headline goals silently:

- **G2 — "AT has a job."** An armed vehicle with nobody on the gun is not a threat, so there is no reason to
  carry AT and no reason to stalk it.
- **F2** literally reads *"its infantry dismounts to a perimeter, **its gunner stays up**"* — and T3.1's own
  task text says *"leave the crew aboard so the gun stays manned."* With `truck_crew` there **is no gunner to
  leave aboard**.
- **F4** wants an echelon that fights at its standoff ring. Two drivers do not.

Nothing would have crashed, no test would have gone red, and the play-test symptom would have been the vague
"the armour turned up but felt harmless" — the hardest class of defect to trace back to a config line.

⚠ The seven **infantry** insertion configs (`ObjectiveSabotage`, `ObjectiveFOB`, `ObjectiveFOBGarrison`,
`ObjectiveHarassment`, `ObjectiveTowerRecapture`, `TowerRecaptureUnrest`, `BaseRepair`) correctly keep
`truck_crew` and were **not** touched — they carry a transport, not a weapon.

**The generalisable lesson for this module system:** `m_sTruckCrewGroup` is inherited from the *insertion*
module, where "truck crew" is the right default. Every config built on `OVT_MountedForceSpawningDeploymentModule`
must override it, and nothing in the type system or the test tier enforces that. A future mounted doctrine will
hit this again. Worth an Init case asserting every config whose modules include the mounted class authors a
crew group with a gunner — filed as a follow-up, not built here.
