# Damage & Destruction (`core/damage`) - Task Checklist

**Last Updated:** 2026-08-21 21:00
**Progress:** 74/74 tasks complete (100%) — 70 build tasks + 4 post-close fix tasks (2026-08-21)

**Epic:** `core` (feature #8) · **Plan:** `implementation.md` · **Scope truth:** `requirements.md` · **Branch:** `v1.5`

> Task ids match the `<phase>.<n>` ids in `implementation.md` §4 — do not renumber them. Per-task acceptance criteria live in the plan's tables and are not repeated here.
> **Agent tiers are set by the plan** (Agent Routing Summary): Phases **1, 4 and 7** are **⚠️ ADVANCED (`component-developer-advanced`)**; Phase 8 is **`help-docs-sync`**; the rest are `component-developer`.
> Every phase must leave `tools/compile-check.sh` at exit 0. **Implementation agents never run `tools/run-tests.sh`** — the orchestrator runs it once per completed phase (`.claude/test-policy.md`).
> Reserved GUID series: `6B70D0000000xxxx`. A collision fails silently.

---

## Phase 1: Engine-behaviour spike (10/10 complete) — ⚠️ ADVANCED (component-developer-advanced)

*Probe subjects: **Bunkers** (already carries the multi-phase component) and **Guard Tower** (active `SCR_DestructibleBuildingComponent`). Output is the S1–S6 decision record in `context.md`; Phases 2–8 are blocked until it exists.*

- [x] ✅ **1.1 — `OVT_StructureDestructionComponent` subclass**
  - Description: `: SCR_DestructionMultiPhaseComponent` + `…ComponentClass`. Members per plan §3.1; `RuinIt`/`RepairIt`/`RestorePhase` start `if(!Replication.IsServer()) return;`. Follow `SCR_DestructionTireComponent` (`:219-251`): cache the intact model in `OnPostInit`, `override GoToDamagePhase` routing phase 0 to `RepairToIntact()`, which sets the phase then `CallLater(ChangeModel, …)` with the cached name. Never rely on `GetOriginalResourceName()`. `IsRuined()` safe on clients and pre-init.
  - File(s): `Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c` (NEW)
  - Estimate: 🔴 3 h
  - **Done 2026-08-20.** `super.GoToDamagePhase(0, …)` turned out to do everything phase 0 needs except the mesh (including cancelling a pending `ChangeModel`), so `RepairToIntact()` calls super and adds only the `CallLater(ChangeModel, …)`. `ChangeModel` is also overridden so an empty cached name clears the object (BD4), and `RuinIt()` refuses when no phase is authored (BD5).
- [x] ✅ **1.2 — `RpcDo_ApplyPhase(int phase, bool withEffects)` broadcast RPC**
  - Description: `[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]`; server path invokes it locally **and** `Rpc()`s it; handler guards `if (!Replication.IsServer())` before driving the phase. Arity hand-audited against the handler signature, recorded in a comment.
  - File(s): `Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c`
  - Estimate: 🟡 1 h
- [x] ✅ **1.3 — Retrofit Bunkers probe prefab**
  - Description: Re-declare `SCR_DestructionMultiPhaseComponent "{5E76C88E922E8914}"` as `OVT_StructureDestructionComponent` if a delta can swap the class; otherwise add ours as a second component and disable the inherited one (record which in `context.md`). `Enabled 1`, `m_bDeleteAfterFinalPhase 0`, one phase with a ruin mesh. Set inherited `RplComponent "{5E76C88E937D49B8}" { Enabled 1 }`.
  - File(s): `PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_bunker_plastic_foundation_camonet.et`
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20 — the class was RETYPED** (BD1); vanilla proves a delta can re-declare an inherited instance GUID under a subclass (`Turret_Base.et:66` → `Tripod_Base.et:70`). `RplComponent "{5E76C88E937D49B8}" { Enabled 1 }` added — S5 confirms it was inactive on the shipped tree.
- [x] ✅ **1.4 — Retrofit Guard Tower probe prefab**
  - Description: Add `OVT_StructureDestructionComponent` **and** re-declare `SCR_DestructibleBuildingComponent "{5D7E937DD0A125D0}" { Enabled 0 }` (mirror `TentUSSR_01_base.et:11-13`). Do not touch line 1.
  - File(s): `Prefabs/Structures/Military/Houses/GuardTower_01/OVT_GuardTower_01.et`
  - Estimate: 🟡 1 h
- [x] ✅ **1.5 — Ruin models for the two probes**
  - Description: Guard Tower → real ruin, prefer prefab `GuardTower_01_Ruin.et` over the bare `.xob` (material remap). Bunkers → fallback `{1C9E0D1CD5A0E4F9}…Bunker_SPS_Ruin.xob`. One of each route deliberately. Judge scale/ground alignment in-game.
  - File(s): the two prefabs from 1.3/1.4
  - Estimate: 🟡 1 h
  - **Done 2026-08-20 — the Guard Tower uses the bare `.xob`, not the ruin prefab** (BD3: `GetModelAndRemapFromResource` does not walk prefab inheritance, and that ruin chain carries no material remap anyway). 🔴 **The Bunkers root carries no model of its own**, so the ruin mesh will be added beside the intact sandbags — see `context.md`; Phase 4 needs a composition strategy.
- [x] ✅ **1.6 — Temporary debug driver: admin chat commands**
  - Description: `SCR_Global.IsAdmin`-gated `/ruin-structure` and `/repair-structure` on `OVT_AdminCommandsComponent`, nearest buildable within 15 m of the caller (server-side from the caller's character origin). The Phase 6 deliverable arriving early.
  - File(s): `Scripts/Game/Components/Controller/OVT_AdminCommandsComponent.c` (or wherever the component lives at HEAD)
  - Estimate: 🟡 1-2 h
- [x] ✅ **1.7 — Answer S1–S6 in `context.md`**
  - Description: Each with observed evidence and the chosen branch; also record the 1.3 class-swap outcome and the 1.5 mesh verdicts.
  - File(s): `docs/features/core/damage/context.md`
  - Estimate: 🟡 1 h
- [x] ✅ **1.U1 — Workbench loads both probe prefabs clean (user-gated — human verification)** — **Play-test green 2026-08-20 (user-confirmed)**
- [x] ✅ **1.U2 — Listen host + one client: `/ruin-structure` swaps the mesh on both machines, `/repair-structure` puts it back on both (user-gated — human verification)** — **Play-test green 2026-08-20 (user-confirmed)**
- [x] **1.G — Gate: compile-check 0 + orchestrator test run** — 2026-08-20: compile-check 0 (6193 files); Fast group 330/331 — the one red (`OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions`: 'Base Fortifications' config authors no slot-gate module) is **pre-existing on HEAD** (Configs/ and Tests/ untouched by this phase; both files last changed in f9b08a38/a62d7b6f).

---

## Phase 2: The destruction API, effects, sound, and sabotage adoption (9/9 complete) — `component-developer`

- [x] ✅ **2.1 — `OVT_StructureDamage` static facade**
  - Description: `Ruin` / `Repair` / `IsRuined` / `IsDestructible` / `Resolve` per §3.1. `Resolve()` checks the entity then walks `GetChildren()`/`GetSibling()` one level (the ramp). `Ruin()`/`Repair()` return false without a component, server-only no-ops elsewhere. Header states the D8 contract.
  - File(s): `Scripts/Game/Utilities/OVT_StructureDamage.c` (NEW)
  - Estimate: 🟡 2 h
  - **Done 2026-08-20.** Also switched Phase 1's temporary admin commands off their inline component lookup and onto the facade (`FindNearestStructure` now returns the structure `IEntity`; `ResolveStructureDestruction()` deleted).
- [x] ✅ **2.2 — `RaiseEffects()`: explosion + smoke particles**
  - Description: `SCR_DestructionCommon.PlayParticleEffect_CompleteDestruction(GetOwner(), m_ExplosionParticle, EDamageType.EXPLOSIVE, true)` then the same for `m_SmokeParticle`; both `[Attribute]` `ResourceName`s with defaults on the component class. Runs on every machine from `RpcDo_ApplyPhase`.
  - File(s): `Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c`
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20.** Defaults were already authored on the component in Phase 1 and both GUIDs verified against the vanilla tree: `{6D89EA548ABDDF25}Particles/Enviroment/Building_Explosion_Debris_Brick.ptc` and `{B0D882F853DE2783}Particles/Enviroment/Building_Explosion_Smoke.ptc` (`Configs/Destruction/Building_FX_Particle/*`).
- [x] ✅ **2.3 — Positional one-shot sound**
  - Description: `SCR_SoundManagerModule.GetInstance(world)` → `CreateAudioSource` at bbox centre → `PlayAudioSource`; `[Attribute] ref SCR_AudioSourceConfiguration` on the class. **Not** `SCR_DestructionUtility.PlaySound`. No error when the config is unset.
  - File(s): `Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c`
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20.** With a fallback (BD8): when a prefab authors no config, the component builds one from vanilla's own bank `{5B79C73C52E6A74A}Sounds/Destruction/Multiphase/Destruction_Multiphase.acp` with event `SOUND_MPD_<m_eMaterialSoundType>`, so no prefab has to author sound. Every bail is silent.
- [x] ✅ **2.4 — Sabotage adoption**
  - Description: In `DemolishNextStructure()` (`:322`) replace `resistance.DestroyPlacedItem(target);` with `if(!OVT_StructureDamage.Ruin(target)) resistance.DestroyPlacedItem(target);`. Nothing else in the method changes.
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_BaseSabotageBehaviorDeploymentModule.c`
  - Estimate: 🟢 0.5 h
  - **Done 2026-08-20.** One line plus its comment; nothing else in the method moved. Gate re-run: `DeleteEntityAndChildren|NavmeshRebuild` still absent from the module.
- [x] ✅ **2.5 — Sabotage re-targeting guard**
  - Description: `CollectTargetCallback` skips structures where `OVT_StructureDamage.IsRuined(entity)` is true, so a ruin is never "demolished" again.
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_BaseSabotageBehaviorDeploymentModule.c`
  - Estimate: 🟢 0.5 h
  - **Done 2026-08-20.** Placed directly after the `IsGearContainer()` exclusion in `CollectTargetCallback`.
- [x] ✅ **2.6 — Init case `OVT_TEST_Init_StructureDamage`**
  - Description: `Ruin/Repair/IsRuined` on null and on a component-less entity return false without erroring; `Resolve()` finds a component on a child. Recorded proof-it-can-fail preamble; no `maxAttempts`.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_StructureDamage.c` (NEW)
  - Estimate: 🟡 1-2 h
  - **Recommended addition from Phase 1:** also spawn a real probe prefab (`E_Sandbag_01_bunker_plastic_foundation_camonet.et` or `OVT_GuardTower_01.et`) and assert the phase round-trip — `IsRuined()` false → `Ruin(entity, false)` → true → `Repair(entity)` → false, with the entity's `RplId` and `OVT_BuildableComponent` owner unchanged throughout. That is the cheapest automated net under S1's mechanism; Phase 1 deliberately wrote no test because the component is engine-constructed and the facade this suite is keyed to did not exist yet.
  - **Done 2026-08-20.** Two cases, and the Phase-1 recommendation WAS feasible in the Init tier (it spawns prefabs elsewhere): case B spawns the real Guard Tower probe, round-trips the phase and asserts the `RplId`, the `OVT_BuildableComponent` instance and its owner id are unchanged, then arranges a parent with `AddChild()` to cover `Resolve()`'s child walk before the ramp retrofit exists. No mesh assertion - `ChangeModel` is deferred through the call queue. Registered by `[Test(suite: OVT_TEST_InitSuite)]`; the suite discovers cases by that attribute, there is no list to edit.
- [x] ✅ **2.7 — Loc keys (`.st` master only)**
  - Description: Any Phase 2 keys into `Language/localization_Overthrow.st`; braces balanced. Never the `.conf` exports.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢 0.5 h
  - **Done 2026-08-20 - nothing to add.** Phase 2 introduces no user-facing string: the effects are particles and a sound, and every new log line is a `Print` for the server console. `Language/localization_Overthrow.st` is untouched.
- [x] ✅ **2.U1 — Listen host + one client: sabotage demolishing a retrofitted structure produces explosion, smoke, sound and a ruin on both machines; per-mission notification fires exactly once (user-gated — human verification)** — **Play-test green 2026-08-20 (user-confirmed)**
- [x] **2.G — Gate: compile-check 0 + orchestrator test run** — 2026-08-20: compile 0 (6195); Fast 332/333 after a hit-zone fix on the Guard Tower (engine drops a hit-zone-less damage manager — S6 corrected in context.md); remaining red is the pre-existing CompositionSlotGate case.

---

## Phase 3: Persistence (6/6 complete) — `component-developer`

- [x] ✅ **3.1 — `OVT_BuildableComponentSerializer` version 1 → 2**
  - Description: Write the phase after the existing three fields; read behind `if (version >= 2)`; restore via `comp.RestorePhase(phase)`. The existing `OVT_NavmeshRebuild.Queue(owner)` line stays first (§3.5). Extend the file header.
  - File(s): `OVT_BuildableComponentSerializer.c` (under `Scripts/Game/Persistence/`)
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20.** Phase read live off the destruction component through `OVT_StructureDamage.Resolve(owner)`; 0 when the prefab was never retrofitted, because the field is positional. Version guard exercised at source level (writer temporarily flipped to a v1 payload, tree compiled clean, reader stops at `if (version < 2) return true;`, reverted); its runtime exercise is 3.U1 against a pre-Phase-3 save, which is a genuine v1 payload.
- [x] ✅ **3.2 — `RestorePhase(int)` on the component**
  - Description: Authority-only; drives `GoToDamagePhase(1,false)` or `RepairToIntact()`; broadcasts with `withEffects = false`; tolerates being called before children finish spawning.
  - File(s): `Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c`
  - Estimate: 🟡 1 h
  - **Done 2026-08-20.** The intact model is cached from the owner's `VObject` **before** the ruin mesh is applied; a restore that arrives while the entity has no model yet is deferred exactly one frame (BD11) and then takes whatever is there, because an empty model is legitimate (BD4).
- [x] ✅ **3.3 — Persistence round-trip: ruin survives save → dirty → reload**
  - Description: Modelled on `OVT_TEST_PersistenceRoundTrip_FuelDepot_LevelSurvivesSave` (`:7959`): build, ruin, save, dirty the live phase to 0, reload, re-find by prefab + radius, assert ruined.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20** as `OVT_TEST_PersistenceRoundTrip_StructureDamage_RuinSurvivesSave`, and it is a REAL round trip rather than the FuelDepot degradation — which needed a third gate seam, `RequestInstanceReload()` (BD10).
- [x] ✅ **3.4 — Persistence round-trip: repaired structure does not revert**
  - Description: Ruin, save, reload, repair, save, reload, assert intact.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 🟡 1 h
  - **Done 2026-08-20** as `OVT_TEST_PersistenceRoundTrip_StructureDamage_RepairSurvivesSave`: both directions in one case (the suite's `JobBoard_*` precedent for re-applying twice), each half dirtied to the opposite state before its reload.
- [x] ✅ **3.U1 — Listen host save → quit → Continue brings a ruin back as a ruin, silently (user-gated — human verification)** — **Play-test green 2026-08-20 (user-confirmed)**
- [x] **3.G — Gate: compile-check 0 + orchestrator test run** — 2026-08-20: compile 0 (6195); **All group 391/392** — both new persistence round-trip cases green; the only red is the pre-existing CompositionSlotGate case from HEAD.

---

## Phase 4: Retrofit the remaining six prefabs (11/11 complete) — ⚠️ ADVANCED (component-developer-advanced)

*Six hand-edited prefabs; a GUID collision fails silently; the Workbench load is the only gate that can catch a mistake. Never touch line 1 of any prefab.*

- [x] ✅ **4.1 — Recruitment Tent + Medical Tent**
  - Description: Add `OVT_StructureDestructionComponent` only; the inherited building component is already `Enabled 0` — do not re-declare it.
  - File(s): `Prefabs/Structures/Military/FOB/OVT_RecruitmentTent.et`, `Prefabs/Structures/Military/FOB/OVT_MedicalTent.et`
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20.** Component only on both; `SCR_DestructibleBuildingComponent "{692CD84F45C9100B}"` confirmed `Enabled 0` at `TentUSSR_01_base.et:11-13` and NOT re-declared. Both roots carry their own `MeshObject` (`TentUSSR_01.xob`), so the swap replaces the canvas rather than adding beside it - the furniture children stay standing, see `context.md`.
- [x] ✅ **4.2 — Garage + Fuel Depot**
  - Description: Add ours; re-declare `SCR_DestructibleBuildingComponent "{5D7AD092A3CB4B41}"` / `"{5DA867F4F045AA84}"` with `Enabled 0`. Fuel plumbing untouched; refuel gate lands in 4.6.
  - File(s): `Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et`, `Prefabs/Structures/Military/FOB/OVT_FuelDepot.et`
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20.** Both GUIDs verified against the chain (`Garage_E_02_base.et:11`, `FuelTank_02_Base.et:15`) and re-declared `Enabled 0`. Fuel plumbing untouched; a ruined depot stops dispensing because the phase change also disables its `SCR_FuelSupportStationComponent` (BD12), which `OVT_FuelUtils` already skips.
- [x] ✅ **4.3 — Helipad**
  - Description: Add ours; it already has its own `RplComponent` (`:62`).
  - File(s): `PrefabsEditable/Auto/Structures/Military/Camps/HelipadImprovised_01/Helipad.et`
  - Estimate: 🟢 0.5-1 h
  - **Done 2026-08-20.** Root carries its own mesh through `HelipadImprovised_US_01_base.et:4-6`; no destruction anywhere in the chain, so nothing to disable. Its `Default_RplComponent.ct` preset is empty (so active by default) and now says `Enabled 1` explicitly.
- [x] ✅ **4.4 — Vehicle Maintenance Ramp (component on the child)**
  - Description: Component on the child at `:71`, plus `SCR_DestructibleBuildingComponent "{67148C0FAD779ACE}" { Enabled 0 }` on that child. Verify `OVT_StructureDamage.Resolve()` finds it from the root.
  - File(s): `Prefabs/Structures/Military/FOB/OVT_VehicleMaintenanceRamp.et`
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20.** Component + the disabled `{67148C0FAD779ACE}` on the child, which is where the mesh, the `RigidBody` and the two support stations already live. `Resolve()` from the root is asserted by the new Init case, which spawns the real prefab.
- [x] ✅ **4.5 — Ruin models for all six**
  - Description: Ramp/Garage/Fuel Depot → their real `…_Ruin.et` prefabs; tents + Helipad → generic fallback (§3.10a). Record each choice and how it reads in `context.md`.
  - File(s): the six prefabs above; `docs/features/core/damage/context.md`
  - Estimate: 🟡 2 h
  - **Done 2026-08-20.** Ramp/Garage/Depot use their real ruin `.xob` (BD3: none of the three `_Ruin.et` prefabs carries its own inline `MeshObject.Object`, so the prefab route would resolve to an empty path). Tents use `DebrisPile_Wood_01.xob` and the Helipad `DebrisPile_Concrete_01_Medium.xob` - BD13. Per-prefab table and how each is expected to read: `context.md`.
- [x] ✅ **4.6 — Phase-0 gates on ruin-inert actions**
  - Description: One shared helper; `CanBeShownScript` false while ruined for: Fuel Depot refuel actions, `OVT_RecruitFromTentAction`, Garage/Helipad shop + parking surfaces (`OVT_MainMenuContextOverrideComponent`, `OVT_ShopComponent`, `OVT_ParkingComponent`), any storage action on a buildable.
  - File(s): `Scripts/Game/UserActions/OVT_FillFuelAction.c`, `OVT_RecruitFromTentAction.c`, `OVT_MainMenuContextOverrideComponent.c`, `OVT_ShopComponent.c`, `OVT_ParkingComponent.c`, shared helper (likely on `OVT_StructureDamage`)
  - Estimate: 🔴 3 h
  - **Done 2026-08-20.** One helper, `OVT_StructureDamage.IsUsable()`, which resolves from the ROOT parent so it works from a child-mounted action. `CanBeShownScript` on `OVT_FillFuelAction`, `OVT_RecruitFromTentAction`, `OVT_BuyEquippedRecruitAction`, `OVT_HealAction` and the three storage actions; `IsOwnerUsable()` on `OVT_MainMenuContextOverrideComponent` read by both find-filters; a refusal in `OVT_ParkingComponent.GetParkingSpot()`; server-side refusals in `OVT_ShopTransactionComponent.ResolveShop()` and `OVT_VehicleRequestComponent.RpcAsk_BuyVehicle()`. Vanilla support stations are switched off by the phase change itself (BD12).
- [x] ✅ **4.7 — Sweep: `m_bDeleteAfterFinalPhase 0` × 8**
  - Description: `grep -n "m_bDeleteAfterFinalPhase 0" Prefabs/ PrefabsEditable/` returns exactly eight matches.
  - File(s): all eight prefabs
  - Estimate: 🟢 0.25 h
  - **Done 2026-08-20.** Exactly 8.
- [x] ✅ **4.8 — Sweep: active `RplComponent` × 8**
  - Description: Every retrofitted prefab has an active `RplComponent`.
  - File(s): all eight prefabs
  - Estimate: 🟢 0.25 h
  - **Done 2026-08-20.** 8 for 8, every one now authored explicitly rather than inherited, so the sweep is a grep and not a chain walk.
- [x] ✅ **4.U1 — Workbench loads all eight prefabs clean (user-gated — human verification)** — **Play-test green 2026-08-20 (user-confirmed)**
- [x] ✅ **4.U2 — Listen host + client: admin command ruins and repairs each of the eight, correct mesh both ways (user-gated — human verification)** — **Play-test green 2026-08-20 (user-confirmed)**
- [x] **4.G — Gate: compile-check 0 + orchestrator test run** — 2026-08-20: compile 0 (6195); `m_bDeleteAfterFinalPhase 0` = 8/8; prefab diff is pure insertion (0 removed lines → line 1 untouched everywhere); **Fast 333/334** — `…_CEveryBuildableIsRetrofitted` green (all eight spawn, are destructible, round-trip ruin→repair); the only red is the pre-existing CompositionSlotGate case.

---

## Phase 5: Repair — pricing, difficulty, seam and the held action (12/12 complete) — `component-developer`

- [x] **5.1 — `OVT_RepairPricing` pure statics**
  - Description: `RepairCost(int baseCost, float buildMultiplier, float repairMultiplier)` (one rounding) and `IsRepairable(int baseCost)` (false for `UNKNOWN_STRUCTURE_COST`). Class comment: money only; logistics plugs in here.
  - File(s): `Scripts/Game/Data/OVT_RepairPricing.c` (NEW)
  - Estimate: 🟢 0.5-1 h
- [x] **5.2 — `repairCostMultiplier` on `OVT_DifficultySettings` + six presets**
  - Description: `[Attribute(defvalue: "0.5", …, category: "Economy")] float repairCostMultiplier;` next to `buildableCostMultiplier`. Author explicitly: Easy `0.5`, Normal `0.5`, Hard `0.75`, Extreme `1`, Insane `1`, plus `Difficulty_TestWorld.conf`. `grep -c repairCostMultiplier Configs/Difficulty/` = 6.
  - File(s): `OVT_DifficultySettings.c`, `Configs/Difficulty/*.conf` (6)
  - Estimate: 🟡 1 h
- [x] **5.3 — `CONFIG_STREAM_VERSION` 4 → 5**
  - Description: Append `repairCostMultiplier` to the difficulty block of `RplSave`/`RplLoad` **after `fuelPricePerLitre`**; bump the constant; add the "Version 5 appended…" header paragraph. Write order == read order.
  - File(s): `OVT_OverthrowConfigComponent.c`
  - Estimate: 🟡 1 h
- [x] **5.4 — `OVT_ResistanceFactionManager`: `FindBuildableForEntity` / `GetRepairCost` / `RepairStructure`**
  - Description: Extract the prefab-name join from `GetStructureCost` (behaviour unchanged, header comment moves with it). `RepairStructure(entity, playerId)`: check `PlayerHasMoney`, `OVT_StructureDamage.Repair`, then `TakePlayerMoney`; `playerId == -1` = free.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c`
  - Estimate: 🟡 2 h
- [x] **5.5 — `OVT_ResistanceRequestComponent.RepairStructure(RplId)` + `RpcAsk_RepairStructure`**
  - Description: Ladder: server guard → `ResolveOwningPlayerId()` → manager null-bail → `ResolveEntity` → not ruined → reject → `CallerIsWithin(…, REPAIR_MAX_DISTANCE)` → `resistance.RepairStructure(entity, playerId)`. Every refusal via `RejectResistanceRequest`. No identity parameter.
  - File(s): `Scripts/Game/Components/Controller/OVT_ResistanceRequestComponent.c`
  - Estimate: 🟡 1-2 h
- [x] **5.6 — `OVT_RepairStructureAction`**
  - Description: Modelled on `OVT_RearmVehicleAction`: `CanBeShownScript` → ruined; `CanBePerformedScript` → `LocalPlayerHasMoney` + `SetCannotPerformReason("#OVT-CannotAfford")`; `GetActionNameScript` → `"#OVT-RepairStructure ($" + price + ")"`; `PerformAction` → controller verb; `HasLocalEffectOnlyScript` true; ~1 s price-cache TTL; guard null `GetDifficulty()` on clients.
  - File(s): `Scripts/Game/UserActions/OVT_RepairStructureAction.c` (NEW)
  - Estimate: 🟡 2 h
- [x] **5.7 — Wire the action into all eight prefabs**
  - Description: `additionalActions` entry with `Duration` (start 20 s) and a sensible `ParentContextList`; six prefabs need a whole `ActionsManagerComponent` (copy `OVT_FuelDepot.et:41-76`), sized to the ruin; Fuel Depot takes only a new entry.
  - File(s): all eight prefabs
  - Estimate: 🔴 3 h
- [x] **5.8 — Logic suite `OVT_TEST_Logic_RepairPricing`**
  - Description: Half at 1.0/0.5; full at 1.0/1.0; the authored ladder × eight real costs; `.5` rounding boundary; zero cost; `IsRepairable(UNKNOWN_STRUCTURE_COST)` false; repair ≤ build at every preset. Proof-it-can-fail preambles; no `maxAttempts`.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_RepairPricing.c` (NEW)
  - Estimate: 🟡 1-2 h
- [x] **5.9 — Init case `OVT_TEST_Init_RepairSeam`**
  - Description: Request component resolves off `OVT_OverthrowController`; every buildables-config entry prices to a positive finite repair cost via the real join.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_RepairSeam.c` (NEW)
  - Estimate: 🟡 1 h
- [x] **5.10 — Loc keys**
  - Description: `#OVT-RepairStructure`, `#OVT-RepairStructure-Repaired` in the `.st` master; English code fallbacks; braces balanced.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢 0.5 h
- [x] ✅ **5.U1 — Dedicated server: a client repairs a ruin, is charged exactly the server-side price, structure returns intact for every connected player (user-gated — human verification)** — **Play-test green 2026-08-20 (user-confirmed)**
- [x] **5.G — Gate: compile-check 0 + orchestrator test run** — 2026-08-20: compile 0 (6199); `repairCostMultiplier` in 6/6 difficulty confs; identity gate clean. First All run went **394/399**: the Guard Tower prefab stopped spawning because a SECOND `ActionsManagerComponent` was added while `GuardTower_01_base.et:146` already carries one (`{5A30CE5EDD9DBDB5}`, the ladder/door template) — engine error `component ActionsManagerComponent cannot be combined with component ActionsManagerComponent`, and the whole entity failed (4 of our cases red). Orchestrator re-declared the inherited GUID with our context/action as a delta (Vehicle_Base.et precedent); re-run **All 398/399**, only the pre-existing CompositionSlotGate red. GUID `6B70D00000000018` is now unused/free.

---

## Phase 6: Admin commands and the gear-survival gate (7/7 complete) — `component-developer`

- [x] ✅ **6.1 — Promote the admin commands to production**
  - Description: `/ruin-structure` (+ `/ruinstructure`), `/repair-structure` (+ `/repairstructure`) in `RegisterChatCommands()` double-alias style; `RplRcver.Server` handlers gated by `SCR_Global.IsAdmin(playerId)`, `LogLevel.WARNING` on refusal, `AdminCommandRefused` notification — shape of `RpcAsk_GiveMoney` (`:269-306`).
  - File(s): `OVT_AdminCommandsComponent.c`
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20.** Both aliases were already registered in the double-alias style and both handlers were already in the `RpcAsk_GiveMoney` shape (`Replication.IsServer()` self-invoke, `ResolveOwningPlayerId()`, `SCR_Global.IsAdmin` + `LogLevel.WARNING` + `AdminCommandRefused`, an audit line on success). What Phase 6 changed is the file's own claim about them: the "TEMPORARY DEBUG DRIVER" banner is replaced by a production header. No behaviour change — verified by diff.
- [x] ✅ **6.2 — Server-side target resolution**
  - Description: Nearest buildable within a fixed radius of the caller's own character origin; handler takes no entity parameter.
  - File(s): `OVT_AdminCommandsComponent.c`
  - Estimate: 🟢 0.5 h
  - **Done 2026-08-20.** Grep-verified: `RpcAsk_RuinStructure()` and `RpcAsk_RepairStructure()` take no parameters at all, and `FindNearestStructure(playerId)` reads the caller's own character origin on the server. No command in the class takes an entity.
- [x] ✅ **6.3 — Storage case of the shared "ruins are inert" helper**
  - Description: Any storage/inventory user action on a buildable is hidden while ruined; verify with a container-carrying buildable created for the test if none ships.
  - File(s): shared helper from 4.6; relevant storage user actions
  - Estimate: 🟡 1 h
  - **Done 2026-08-20.** Verification pass across every user action that touches a storage or an inventory (`OpenInventory` / `SetStorageToOpen` / `SetLootStorage` / `InventoryStorageManagerComponent`, 13 files) plus a walk of all eight buildable prefab chains for a storage component. **No shipped buildable is a container** and no ungated storage action is authored on one. Completion: the generic `SCR_OpenStorageAction` (modded) gained the same `IsUsable()` gate the three `OVT_*StorageAction`s took in 4.6, because it is the action that appears on ANY entity carrying a storage — i.e. the first one a future buildable container would show. Left ungated on purpose: vehicle and body/loot actions (never a buildable) and the loadout box actions (placeable boxes only) — see context.md.
- [x] ✅ **6.4 — Gear-survival assertion**
  - Description: Init or Persistence case: item into storage → ruin → repair → item still there; or the weaker invariant (same `RplId` + same `OVT_BuildableComponent` owner before/after) if no shipped buildable is a container.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` or an Init case
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20.** `OVT_TEST_Init_StructureDamage_DGearSurvivesAPhaseRoundTrip` — the REAL container case, not the weaker fallback: the subject is assembled at runtime from the retrofitted Guard Tower plus an `OVT_AmmoBox_Placed` parented to it, one item inserted into the box, then ruin → repair. Asserts the contents survive both halves, the container is never deleted, the gate closes on the ruin and `OVT_OpenStorageAction` stops offering itself while it lasts. Case B's identity invariant is not repeated.
- [x] ✅ **6.5 — Record the §3.8 decision + seams in `context.md`**
  - Description: Ruin storage closed; seams left for `core/storage` and the occupying epic's ammobox question.
  - File(s): `docs/features/core/damage/context.md`
  - Estimate: 🟢 0.5 h
  - **Done 2026-08-20.** BD21 (the §3.8 decision as built) and BD22 (the two seams) in `context.md`, plus the Phase 6 gotchas and session note.
- [x] ✅ **6.U1 — Dedicated server: an admin ruins/repairs any of the eight from chat; a non-admin is refused and logged (user-gated — human verification)** — **Play-test green 2026-08-20 (user-confirmed)**
- [x] **6.G — Gate: compile-check 0 + orchestrator test run** — 2026-08-20: compile 0 (6199); **Fast 340/341** — `…_DGearSurvivesAPhaseRoundTrip` green on first run; only the pre-existing CompositionSlotGate red.

---

## Phase 7: The occupying faction repairs its own ground (9/9 complete) — ⚠️ ADVANCED (component-developer-advanced)

- [x] ✅ **7.1 — Read-only survey (gates the phase)**
  - Description: Re-verify against HEAD: `OVT_BaseBehaviorDeploymentModule` virtual surface + `super.OnUpdate()`; `CloneModule()` hand-copy; how `m_bDirectorOnly 0` configs are picked (`EvaluateFactionDeployments`, `FindBestDeploymentConfig`, `OVT_DeploymentSelection.SelectNextConfigIndex` — lowest `m_iPriority` wins); `RequestDeploymentCollection()` deferred teardown. Record base-defense config numbers in `context.md` before any code.
  - File(s): `docs/features/core/damage/context.md`
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20.** Written into `context.md` as "The deployment framework, as surveyed at HEAD for Phase 7": the behaviour module's virtual surface and the `super.OnUpdate()` requirement, the hand-copy `CloneModule()` convention, the evaluator path end to end, `SelectNextConfigIndex`'s lowest-priority-wins/ties-to-registry-order rule, the one-frame deferred teardown, and the nine base-defense configs' priority/chance/max-instances numbers (**Parked Vehicles at 10 is the number to beat**).
- [x] ✅ **7.2 — `OVT_BaseRepairBehaviorDeploymentModule`**
  - Description: Mirror of sabotage per §3.7; attributes `m_sModuleName`, `m_fClearRadius`, `m_fSearchRadius`, `m_fMaxBaseDistance`, `m_iHoldSeconds`, `m_iStructuresPerMission`; `super.OnUpdate` first; targets = ruined buildables at this base, cheapest first; repair with `playerId = -1`; `CloneModule()` copies every attribute, no runtime state. No `DestroyPlacedItem`/`DeleteEntityAndChildren`.
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_BaseRepairBehaviorDeploymentModule.c` (NEW)
  - Estimate: 🔴 3-4 h
  - **Done 2026-08-20.** Sabotage's shape with three inversions (base must be ours, targets must be ruined, no director call anywhere). Repairs through `resistance.RepairStructure(entity, -1)`. `grep -n "DestroyPlacedItem\|DeleteEntityAndChildren"` → empty.
- [x] ✅ **7.3 — Pure decision statics**
  - Description: `EvaluateRepair(ticksLeft, aliveInside, enemyPresent, …)` and `IsRepairTarget(...)`, callable on a bare `new` module.
  - File(s): `OVT_BaseRepairBehaviorDeploymentModule.c`
  - Estimate: 🟡 1 h
  - **Done 2026-08-20.** `EvaluateRepair(aliveInside, enemyPresent, inout ticksLeft)` and `static IsRepairTarget(assocId, assocType, baseId, baseFaction, myFaction, isRuined)`, plus two extra statics the plan did not name — `IntervalTicksFrom(fallback, campaign)` and `StructuresPerMissionFrom(fallback, campaign)` — which is what makes the difficulty PRECEDENCE reachable from the Logic tier at all.
- [x] ✅ **7.4 — `Deployment_ObjectiveRepair.conf` + registry entry**
  - Description: Insertion module, repair module authored before any reinforcement, `OVT_ReinforcementBehaviorDeploymentModule { m_bDeleteOnConditionFail 1 }`, `OVT_BaseControlConditionDeploymentModule { m_bRequireControl 1 }`, no `OVT_ObjectiveConditionDeploymentModule`, `OCCUPYING_FACTION` / `BASE` / `m_bDirectorOnly 0`, priority/chance/max-instances behind base-defense. Fresh GUIDs from the reserved series. Register in `overthrowDeployments.conf`.
  - File(s): `Configs/Deployment/Deployment_ObjectiveRepair.conf` (NEW), `Configs/Deployment/Deployment_ObjectiveRepair.conf.meta` (NEW), `Configs/Deployment/overthrowDeployments.conf`
  - Estimate: 🟡 1-2 h
  - **Done 2026-08-20.** Named **"Base Repair Detail"** (the file keeps the plan's mandated path). Insertion → repair → reinforcement → base control; no objective condition, no composition module. `BASE` / `OCCUPYING_FACTION` / `m_bDirectorOnly 0` / priority **15** / chance 100 / max instances **1**. GUIDs `…0038`–`…003E`.
- [x] ✅ **7.5 — Init case `OVT_TEST_Init_ObjectiveRepair`**
  - Description: Config resolves, BASE-typed, `m_bRequireControl 1`, module order, group prefabs for `US` and `USSR`; target filter excludes intact / other-base / resistance-held; hold pauses, never resets. Mirror `OVT_TEST_Init_ObjectiveSabotage.c`.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ObjectiveRepair.c` (NEW)
  - Estimate: 🟡 2 h
  - **Done 2026-08-20.** Four cases: **A** config (registered, valid, BASE, evaluator-selectable, priority behind base defense, module order, `m_bRequireControl 1`, no objective/patrol/composition module, non-anchor source provider, a real group prefab for US and USSR), **B** target filter (intact / other base / resistance-held / camp / FOB / unassociated all refused), **C** the hold pauses and never resets, **D** clone fidelity + live difficulty precedence.
- [x] ✅ **7.6 — Logic case for `EvaluateRepair`**
  - Description: Interval ticks, difficulty precedence over fallback attributes — shape of sabotage's case D.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveRepair.c` (NEW)
  - Estimate: 🟡 1 h
  - **Done 2026-08-20.** Two cases: **A** the full decision ladder (empty / defended / pause-not-reset / run-out / re-arm / zero-interval floor / finished-mission stop), **B** the interval and quota precedence and their floors. No difficulty field was added — repair reads sabotage's (**BD23**), which is server-only and does not ride the JIP stream.
- [x] ✅ **7.7 — Balance numbers + narrow-trigger note in `context.md`**
  - Description: The numbers chosen, why provisional, and the §3.7 note that the trigger surface is narrow on shipped data.
  - File(s): `docs/features/core/damage/context.md`
  - Estimate: 🟢 0.5 h
  - **Done 2026-08-20.** BD23/BD24/BD25, the churn arithmetic behind the numbers, and the honest §3.7 note that on shipped data the common outcome is "there was nothing left to repair".
- [x] ✅ **7.U1 — Listen host: admin-ruin two structures at an occupying-held base; repair deployment selected, team inserts, structures return one per interval, cheapest first (user-gated — human verification)** — **Play-test green 2026-08-20 (user-confirmed)**
  - ⚠ **Withdraw more than 150 m from the base centre and wait.** `m_fClearRadius 150` is the circle a player pauses the work from, by design (plan §3.7's pause-don't-reset interval). Standing at the base watching is the one way to see nothing happen at all.
- [x] **7.G — Gate: compile-check 0 + orchestrator test run** — 2026-08-20: compile 0 (6202); module grep gate clean (no DestroyPlacedItem/DeleteEntityAndChildren/ObjectiveDirector). First Fast run 345/347: `…_CHoldIntervalPausesRatherThanResetting` was red on its own arithmetic (after 3→2→pause, the resumed tick leaves 1, it does not complete) — orchestrator fixed the CASE (added the 2→1 step; module untouched); re-run **Fast 346/347**, only the pre-existing CompositionSlotGate red.

---

## Phase 8: Help & documentation sync (6/6 complete) — `help-docs-sync`

- [x] **8.1 — Field Manual repair entry**
  - Description: What a ruin is, gear survives, half-cost rule rising at harder difficulties. Every sentence cites a file:line in the agent's report.
  - File(s): `Configs/FieldManual/`
  - Estimate: 🟡 1 h
  - **Done 2026-08-20.** New entry **Ruins and Repair** (`{6B70D0000000003F}`, pieces `…40`–`…48`) in `Configs/FieldManual/Categories/FM_Overthrow.conf`, sitting between *FOBs and Building* and *Capturing a Base*. Four sections: what a ruin is and that it survives a save, that a ruin is inert, that the gear inside survives, the 20 s repair action and its difficulty share, and the occupying faction's repair detail. **Also rewrote the stale `OVT-FieldManual_CounterAttacks_Text3`**, which said sabotage losses were permanent with "no rubble, no repair, no salvage". Every claim's file:line is in the string `Comment`s.
- [x] **8.2 — Tutorial popup at a ruin (if it fits the trigger vocabulary)** — **SKIPPED, deliberately**
  - Description: Otherwise skip; if an `ActionContexts` block is needed, grep-count it after editing (conf merges drop `ActionContexts`).
  - File(s): `Configs/Tutorials/`
  - Estimate: 🟡 1 h
  - **Skipped 2026-08-20 (no trigger exists).** `OVT_TutorialEvent` (`Scripts/Game/Configuration/OVT_TutorialTrigger.c:12-45`) has fourteen events and none of them can fire on "a player is standing at a ruin", on a structure being ruined, or on the repair action becoming available: the nearest are `PLAYER_BUILD` (a finished buildable), `PLAYER_ENTER_BASE` (crossing into an occupying-held base's close range) and `MENU_OPENED`. Sabotage raises no tutorial event at all. A popup here needs a new proximity/structure-state trigger, which is **tutorial-system framework and belongs to `new-player-experience/tutorial-content`, not to a docs phase.** Reported as a gap; no `.conf` was touched, so the `ActionContexts` grep-count guard did not apply.
- [x] **8.3 — Public wiki** — **DRAFTED, NOT PUBLISHED**
  - Description: Sabotage page stops saying structures are destroyed permanently; repair section added.
  - File(s): wiki (wikijs MCP)
  - Estimate: 🟡 1 h
  - **2026-08-20: no `mcp__wikijs__*` tool was exposed to this session at all**, not even `wikijs_connection_status` — the MCP server is not connected. This is the same wall `occupying/counter-attacks` T10.3 hit. Nothing was written and nothing was invented. The full paste-ready text is `docs/features/core/damage/wiki-draft.md`: the sabotage-page correction, a new player-facing **Ruins and Repair** page, the `repairCostMultiplier` row for `difficulty`, and one sentence for the FOB/building page. **Still owed.**
- [x] **8.4 — Loc keys (`.st` only)**
  - Description: Keys into `Language/localization_Overthrow.st`; never the `.conf` exports.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢 0.5 h
  - **Done 2026-08-20.** Ten new keys `OVT-FieldManual_Ruins_{Title,Text,Head,Text2,Head2,Text3,Head3,Text4,Head4,Text5}` (`{6B70D00000000049}`–`{6B70D00000000052}`), plus one rewritten value + `Comment` on the existing `OVT-FieldManual_CounterAttacks_Text3`. `.st` braces balanced 1796/1796 (1776 before). No `Language/*.conf` export was touched. ⚠ **None of it renders in-game until the user re-exports in Workbench (8.U1).**
- [x] ✅ **8.U1 — Workbench re-export of the localization `.conf` files; the three surfaces (Field Manual, tutorial, wiki) agree with shipped behaviour (user-gated — human verification)** — **Play-test green 2026-08-20 (user-confirmed)**
- [x] **8.G — Gate: compile-check 0 + orchestrator test run** — 2026-08-20: compile 0 (6202). **Test run skipped** — the phase touched only `Configs/FieldManual/`, the `.st` master and docs; the suites assert nothing there (test-policy §2). Last suite verdicts stand: Fast 346/347 (Phase 7), All 398/399 (Phase 5); the single red is pre-existing on HEAD.

---

## Bugs & Issues

**Active Bugs:**
- (none)

**Bug-report candidates for the orchestrator** (plan §10 — do not file from an implementation agent; dev-branch defects go in feature docs):
- **S5 CONFIRMED-NO (2026-08-20):** the Bunkers prefab ships with no active `RplComponent`. Chain: the Overthrow file has none, its two vanilla ancestors have none, and the grandparent's `RplComponent "{5E76C88E937D49B8}"` resolves to `Prefabs/Props/Core/DestructionMultiPhase_Rpl_Base.ct` = `{ Enabled 0 }`. It is a live buildable (`buildables.conf:53-64`, cost 750). Consequence independent of this feature: no `RplId`, so `RpcAsk_RemovePlacedItem(RplId)` can never name a built bunker — **dismantle is unreachable**; whether it replicates to clients at all in MP is worth asking in the report. Fixed forward in this feature's retrofit, but the defect predates it.
- Vanilla `SCR_CampaignServiceEntityComponent.RepairEntity():71-80` passes a `bool` into `int damagePhase` (listed for reference only).

**Fixed Bugs:**
- (none)

---

## Technical Debt

- [x] ✅ 💳 **Occupying repair balance numbers are provisional** — Priority: Low — closed out 2026-08-20, not done (carried as tech debt in context.md)
  - Description: `m_iBaseCost`, `m_iCostPerGroup`, priority, `m_fChance`, `m_iMaxInstances` authored conservatively in 7.4; belong to the `occupying` epic.
  - Effort: small, once `occupying/base-upgrades` widens the trigger surface.
- [x] ✅ 💳 **Fallback rubble for tents/helipad** — Priority: Low — closed out 2026-08-20, not done (carried as tech debt in context.md)
  - Description: Generic rubble where a canvas tent stood may not read well (4.5). Escalation = Resource Browser session with the user, not new art.

---

## Post-close fix — buildables died to single bullets (4/4 complete, 2026-08-21)

*Trigger: a built Recruitment Tent ruined by stray small-arms fire, no sabotage. Root causes BD31 (16-bit hit-zone cap put every structure past its ruin line from spawn) and BD32 (repair never restored health). User balance call BD33.*

- [x] ✅ **PC.1 — Health under the engine cap + small-arms threshold on the seven sturdy buildables**
  - Description: `m_fBaseHealth`/`m_fPhaseHealth` 100000 → 32000 on Guard Tower, both tents, ramp (child), Garage, Helipad, Bunkers; `DamageThreshold 50` on each hit zone; Bunkers author the full hit-zone block (explosive ×90 → ×1).
  - File(s): the seven prefabs (see context.md per-prefab table)
- [x] ✅ **PC.2 — Repair restores the hit zone's health**
  - Description: `RepairToIntact()` → `GetDefaultHitZone().SetMaxHealth(m_iOriginalHealth, ESetMaxHealthFlags.FULLHEAL)` on the authority (vanilla's `SetHitZoneHealth` proven insufficient).
  - File(s): `Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c`
- [x] ✅ **PC.3 — Fuel Depot made deliberately fragile**
  - Description: 250 + 250, kinetic/fragmentation/explosive/incendiary/fire ×1, collision ×0.
  - File(s): `Prefabs/Structures/Military/FOB/OVT_FuelDepot.et`
- [x] ✅ **PC.4 — Init cases E/F + weapons-path hit helper**
  - Description: E — damage ruin → repair → full health → a rifle round is shrugged off (proven able to fail on both holes). F — live buildables config: a collision hit ruins nothing, five rifle rounds ruin only the depot and leave the rest untouched. Init suite 182/184 (pre-existing CompositionSlotGate red + E before its fix); E green standalone afterwards.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_StructureDamage.c`
- 🎯 **PC.U1 — user play-test** of the fix: tracked in `context.md` → "Needs human verification" (not a task — closed feature).

---

## Testing Tasks

Covered by the per-phase items above (2.6, 3.3, 3.4, 5.8, 5.9, 6.4, 7.5, 7.6) and the user-gated `*.U*` items. The final 11-item dedicated-server MP play-test is in `implementation.md` §7 and tracked in `context.md` → "Needs human verification".

---

## Documentation Tasks

- Phase 8 above. `context.md` is updated at the end of every phase (S1–S6 after Phase 1; 4.5 mesh choices; 6.5; 7.1; 7.7).

---

## Task Status Legend

- [ ] Not started
- [ ] 🔄 In progress
- [ ] ⏸️ Blocked (waiting on something)
- [x] ✅ Completed
- [x] ❌ Cancelled/Won't do

---

## Progress Tracking

### Completed This Session (2026-08-20)
- Scaffolding (earlier session).
- **Phase 1 tasks 1.1–1.7.** Compile-check exit 0; static gates run (root-class gate clean, `m_bDeleteAfterFinalPhase 0` = 2 of the eventual 8, no `DeleteEntityAndChildren`/`NavmeshRebuild` under `Scripts/Game/Components/Damage/`). `tools/run-tests.sh` deliberately not run — orchestrator only.
- Still open in Phase 1: **1.U1**, **1.U2** and the **1.G** gate line.
- **Phase 4 tasks 4.1–4.8.** The other six prefabs retrofitted (12 new GUIDs, `6B70D0000000_0004`–`_000F`), ruin meshes chosen per structure, one shared phase-0 gate (`OVT_StructureDamage.IsUsable`) applied to eleven surfaces, and a third Init case that spawns EVERY buildable in the shipped config and round-trips it. Compile-check exit 0 (6195 files); `m_bDeleteAfterFinalPhase 0` = **8**; active `RplComponent` = **8/8**; root-class gate clean. `tools/run-tests.sh` deliberately not run — orchestrator only. Still open: **4.U1**, **4.U2** and the **4.G** gate line.
- **Phase 3 tasks 3.1–3.4.** Serializer v1 → v2, `RestorePhase()` fleshed out with the cache-before-mesh ordering, two new round-trip cases and the third gate seam they need. Compile-check exit 0 (6195 files). `tools/run-tests.sh` deliberately not run — orchestrator only. Still open: **3.U1** and the **3.G** gate line.

- **Phase 6 tasks 6.1–6.5.** The admin commands promoted to production (documentation only — they were already in the `RpcAsk_GiveMoney` shape), the storage sweep completed with one more gate on the generic `SCR_OpenStorageAction`, and a real container gear-survival case built out of a Guard Tower plus a parented ammo box. Compile-check exit 0 (6199 files). `tools/run-tests.sh` deliberately not run — orchestrator only. Still open: **6.U1** and the **6.G** gate line.

- **Phase 5 tasks 5.1–5.10.** `OVT_RepairPricing` (one rounding), `repairCostMultiplier` on the difficulty ladder (6 confs), `CONFIG_STREAM_VERSION` 4 → 5, the manager's `FindBuildableForEntity`/`GetRepairCost`/`RepairStructure`, the `RepairStructure(RplId)` verb on `OVT_ResistanceRequestComponent`, `OVT_RepairStructureAction`, the action wired into all eight prefabs (24 new GUIDs, `6B70D0000000_0010`–`_0037`), four Logic cases, two Init cases and the two loc keys. Compile-check exit 0 (6199 files); identity gate clean; root-class gate clean; `grep -c repairCostMultiplier Configs/Difficulty/*.conf` = 1 in each of 6. `tools/run-tests.sh` deliberately not run — orchestrator only. Still open: **5.U1** and the **5.G** gate line.

- **Phase 8 tasks 8.1–8.4.** Field Manual entry **Ruins and Repair** (10 pieces, GUIDs `6B70D0000000_003F`–`_0048`) plus the rewrite of the stale `OVT-FieldManual_CounterAttacks_Text3`; ten new `.st` keys (`_0049`–`_0052`), braces balanced 1796/1796; the tutorial popup **deliberately skipped** (no trigger in `OVT_TutorialEvent` can fire at a ruin); the wiki **drafted but not published** (`wiki-draft.md`) because no `mcp__wikijs__*` tool was exposed to the session. Compile-check exit 0. `tools/run-tests.sh` deliberately not run — orchestrator only. Still open: **8.U1** and the **8.G** gate line.
- **2026-08-20 autorun:** Phases 1–8 built (tasks 1.1–8.4 + all 8 gates); remaining = 10 user-gated items (1.U1, 1.U2, 2.U1, 3.U1, 4.U1, 4.U2, 5.U1, 6.U1, 7.U1, 8.U1).
- **2026-08-20 close-out:** user play-test green on all 10 gated items (Workbench load of 8 prefabs, listen-host + dedicated MP checks, save→Continue, GM Neutralize FX/sound/fire, occupying repair detail, loc re-export); feature closed.

### Discovered New Tasks
- **Phase 4 needs a composition strategy for the Bunkers buildable** — its root carries no `MeshObject`, so a root-level phase model is added beside the intact sandbag children instead of replacing them. Vanilla's own answer is a destruction component per child (`Sandbag_01_bunker_plastic_CompositionDestruction.et:11-19`). **The Helipad and the two tents must be checked for the same shape before 4.5 authors them.**
- **A buildable can now be round-tripped through storage by instance** (`OVT_PersistenceManagerComponent.ReapplyEntitySaveData`, BD10). The FuelDepot case above is degraded to a save-only assertion for the lack of exactly that seam — whoever next touches `economy/fuel` can promote it to a full round trip in a few lines. Not done here: it is another feature's case.
- ✅ **Done 2026-08-20** — `m_fDamageThresholdMaximum 50000` is authored on all six, alongside the mandatory `"Additional hit zones"` block from the S6 correction.
- **The Bunkers composition finding did NOT generalise** — the Helipad and both tents carry their own root `MeshObject`, so only the Bunkers root is a composition. Nothing was changed there; the rubble-inside-intact-sandbags result stands and is 1.U2/4.U2's to judge. Full per-prefab check: `context.md`.
- **A ruined structure's vanilla SUPPORT STATIONS are switched off by the phase change** (BD12) — fuel, repair, salvage and heal. `SCR_CheckFuelAction` is deliberately left working on a wrecked depot, and `SCR_BaseSupportStationComponent.SetEnabled()` has no JIP replication of its own, so a client that streams in a ruin can be offered a support-station action the server will refuse. Both recorded in `context.md`; neither is this feature's to fix.

- **A tutorial popup at a ruin needs a NEW tutorial trigger** (8.2). `OVT_TutorialEvent` (`Scripts/Game/Configuration/OVT_TutorialTrigger.c:12-45`) carries no event for structure state or for standing near a specific entity, and sabotage raises none. Adding one is tutorial-system framework and belongs to `new-player-experience/tutorial-content`, not to this feature.
- **The wiki sync (8.3) is owed and blocked on tooling.** `docs/features/core/damage/wiki-draft.md` is paste-ready. The `occupying/counter-attacks` T10.3 draft is owed on the same page set — do both in one pass.
- ✅ **Done 2026-08-20 (play-test fix 1)** — engine-driven phase changes (GM "Neutralize", weapon kills) bypassed our effects and left support stations working. `SetDamagePhase()` is now the single funnel for both; `RpcDo_ApplyPhase` no longer raises them. BD27.
- ✅ **Done 2026-08-20 (play-test fix 2)** — the ruin visual was vanilla's collapse dust played unscaled. Now an explosion + debris one-shot plus a retained fire (120 s) and smoke (600 s), stopped by a repair or by `OnDelete`; the Fuel Depot overrides the explosion with the fuel fireball. BD28.
- ✅ **Done 2026-08-20 (play-test fix 3)** — the destruction sound was inaudible: the `PhasesToDestroyed` and `EntitySize` signals were never set. Set now, and the manager's own audio config is preferred when it has spawned. BD29.
- ✅ **Done 2026-08-20 (play-test fix 4)** — the ruin sound was audible but was only the per-material BREAK event: a close-range crack, when these demolitions are heard from hundreds of metres out. A vanilla big-explosion bank (`Particles_Explosions_TNT_Large.acp` / `SOUND_EXPLOSION`, Fuel_Large on the depot) now plays as the primary layer with the break underneath. BD30.

### Blocked Items
- ✅ Unblocked 2026-08-20 — S1–S6 are answered in `context.md`. Phases 2–8 may start; read the two 🔴 findings in "Discovered New Tasks" first.
- ✅ 2026-08-20 — **1.U1** and **1.U2** confirmed by the user; nothing blocked.

---

## Notes

### Task Estimation
- 🟢 Small (< 1 hour)
- 🟡 Medium (1-3 hours)
- 🔴 Large (> 3 hours)

### Per-phase counts
| Phase | Plan tasks | User-gated | Gate | Total |
|---|---|---|---|---|
| 1 | 7 ✅ | 2 | 1 | 10 |
| 2 | 7 | 1 | 1 | 9 |
| 3 | 4 | 1 | 1 | 6 |
| 4 | 8 ✅ | 2 | 1 | 11 |
| 5 | 10 | 1 | 1 | 12 |
| 6 | 5 | 1 | 1 | 7 |
| 7 | 7 | 1 | 1 | 9 |
| 8 | 4 | 1 | 1 | 6 |
| **Total** | **52** | **10** | **8** | **70** |

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
