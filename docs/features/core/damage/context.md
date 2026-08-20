# Damage & Destruction (`core/damage`) - Context & Decisions

**Last Updated:** 2026-08-20 21:30
**Current Phase:** Closed
**Status:** ✅ Complete — play-test green 2026-08-20

**Epic:** `core` (feature #8) · **Plan:** `implementation.md` (8 phases) · **Scope truth:** `requirements.md` · **Branch:** `v1.5`

---

## Quick Status

**What's Done:**
- ✅ Requirements (2026-08-19) and plan (`implementation.md`, 8 phases, D1–D18)
- ✅ **Phase 1 (1.1–1.7 + 1.G)** — `OVT_StructureDestructionComponent` + `RpcDo_ApplyPhase`, both probe prefabs, S1–S6 record (session note below)
- ✅ **Phase 2 (2.1–2.7 + 2.G)** — `OVT_StructureDamage` facade, `RaiseEffects()`, sabotage ruins instead of deleting (session note below)
- ✅ **Phase 3 (3.1–3.4 + 3.G)** — serializer v2, silent `RestorePhase()`, two REAL round-trip cases (session note below)
- ✅ **Phase 4 (4.1–4.8 + 4.G)** — all eight buildables retrofitted, ruin mesh per structure, shared `IsUsable()` gate on eleven surfaces (session note below)
- ✅ **Phase 5 (5.1–5.10 + 5.G)** — `OVT_RepairPricing`, `repairCostMultiplier` on the difficulty ladder, stream v5, repair verb/RPC, the 20 s held action on all eight (session note below)
- ✅ **Phase 6 (6.1–6.5 + 6.G)** — admin commands promoted to production, storage sweep, gear-survival case (session note below)
- ✅ **Phase 7 (7.1–7.7 + 7.G)** — `OVT_BaseRepairBehaviorDeploymentModule` + 'Base Repair Detail' deployment, six cases (session note below)
- ✅ **Phase 8 (8.1, 8.3 drafted, 8.4 + 8.G; 8.2 skipped by design)** — Field Manual *Ruins and Repair*, Counter Attacks rewrite, ten `.st` keys, `wiki-draft.md` (session note below)

**What's Next:**
- Nothing — closed. Remaining non-blocking debt: wiki publication from `wiki-draft.md` (wikijs MCP not connected), tutorial trigger for 'at a ruin' (framework, other feature), occupying repair balance numbers provisional, tents/helipad generic rubble, FM tile image. Git: everything uncommitted — the user commits.

**Blockers:**
- None (closed 2026-08-20).

---

## Key Files

### Core Implementation (tick as they land)
- ✅ `Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c` — the one modded subclass: `RuinIt` / `RepairIt` / `RestorePhase` / `IsRuined` / `RpcDo_ApplyPhase` / `RepairToIntact` / `CacheIntactModel` / `ChangeModel` override; `RaiseEffects()` filled in Phase 2 (Phases 1–3)
- ✅ `Scripts/Game/Components/Controller/OVT_AdminCommandsComponent.c` — production `/ruin-structure` (+ `/ruinstructure`) and `/repair-structure` (+ `/repairstructure`) (task 1.6; on the facade since 2.1; promoted in 6.1/6.2)
- ✅ `PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_bunker_plastic_foundation_camonet.et` — probe 1 (task 1.3/1.5)
- ✅ `Prefabs/Structures/Military/Houses/GuardTower_01/OVT_GuardTower_01.et` — probe 2 (task 1.4/1.5)
- ✅ `Scripts/Game/Utilities/OVT_StructureDamage.c` — static facade `Ruin` / `Repair` / `IsRuined` / `IsDestructible` / `Resolve` (Phase 2)
- ✅ `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_StructureDamage.c` — two Init cases (Phase 2)
- ✅ `Scripts/Game/Persistence/Serializers/Components/OVT_BuildableComponentSerializer.c` — version 2: the damage phase, appended (Phase 3)
- ✅ `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` — `StructureDamage_RuinSurvivesSave` + `StructureDamage_RepairSurvivesSave`, their shared fixture, and the gate's third seam (Phase 3)
- ✅ `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c` — `ReapplyEntitySaveData()`, the per-instance re-application those cases reload with (Phase 3, BD10)
- ✅ `Scripts/Game/Data/OVT_RepairPricing.c` — pure `RepairCost` / `IsRepairable` (Phase 5)
- ✅ `Scripts/Game/UserActions/OVT_RepairStructureAction.c` — held, priced action; root-parent walk, ~1 s price cache (Phase 5)
- ✅ `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_RepairPricing.c` — four Logic cases (Phase 5)
- ✅ `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_RepairSeam.c` — two Init cases (Phase 5)
- ✅ `Configs/overthrowBroadcastMessages.conf` — the `RepairedStructure` notification preset (Phase 5, BD17)
- ✅ `Scripts/Game/GameMode/Objectives/Modules/OVT_BaseRepairBehaviorDeploymentModule.c` — the maintenance mirror of sabotage: `EvaluateRepair` / `IsRepairTarget` / `IntervalTicksFrom` / `StructuresPerMissionFrom` / `CollectTargets` / `RepairNextStructure` / `CloneModule` (Phase 7)
- ✅ `Configs/Deployment/Deployment_ObjectiveRepair.conf` (+ `.conf.meta`) — "Base Repair Detail", registered in `Configs/Deployment/overthrowDeployments.conf` (Phase 7)
- ✅ `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ObjectiveRepair.c` — four Init cases (Phase 7)
- ✅ `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveRepair.c` — two Logic cases (Phase 7)
- Edits: `OVT_ResistanceFactionManager.c`, `OVT_ResistanceRequestComponent.c`, `OVT_AdminCommandsComponent.c`, `OVT_BaseSabotageBehaviorDeploymentModule.c`, `OVT_BuildableComponentSerializer.c` (v1 → v2), `OVT_DifficultySettings.c`, `OVT_OverthrowConfigComponent.c` (`CONFIG_STREAM_VERSION` 4 → 5), `Configs/Difficulty/*.conf` (6 files), `Language/localization_Overthrow.st`
- ✅ Prefabs (all eight Overthrow-owned, edits only — see plan §3.1/§3.10 and the per-prefab table below): `OVT_GuardTower_01.et`, `OVT_RecruitmentTent.et`, `OVT_MedicalTent.et`, `OVT_VehicleMaintenanceRamp.et` (component on the **child**), `OVT_FuelDepot.et`, `Garage_E_02.et`, `E_Sandbag_01_bunker_plastic_foundation_camonet.et`, `Helipad.et` (Phases 1 + 4)
- ✅ Phase-0 gates (Phase 4, task 4.6): `Scripts/Game/UserActions/OVT_FillFuelAction.c`, `OVT_RecruitFromTentAction.c`, `OVT_BuyEquippedRecruitAction.c`, `OVT_HealAction.c`, `OVT_OpenStorageAction.c`, `OVT_LoadStorageAction.c`, `OVT_UnloadStorageAction.c`; `Scripts/Game/Components/OVT_MainMenuContextOverrideComponent.c`, `OVT_ParkingComponent.c`; `Scripts/Game/UI/Context/OVT_MainMenuContext.c`, `Scripts/Game/UI/HUD/OVT_EconomyInfo.c`; `Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c`, `OVT_VehicleRequestComponent.c`

- ✅ `Scripts/Game/UserActions/Modded/SCR_OpenStorageAction.c` — the generic container action, gated on `IsUsable()` (Phase 6, task 6.3)
- ✅ `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_StructureDamage.c` — case **D**, the gear-survival container case (Phase 6, task 6.4)

### Related Files
- `docs/features/core/damage/implementation.md` — the plan (architecture §3, phases §4, decisions D1–D18, DoD §6, testing §7, risks §10)
- `docs/features/core/damage/requirements.md` — scope truth
- `docs/features/core/damage/tasks.md` — granular checklist (ids = plan task numbers)
- `docs/features/core/epic-overview.md` — epic rollup
- Vanilla reference: `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Game/Destruction/SCR_DestructionMultiPhaseComponent.c`, `SCR_DestructionTireComponent.c`, `SCR_DestructionUtility.c`, `SCR_DestructionCommon.c`

**Reserved GUID series:** `6B70D0000000xxxx` (verified unused 2026-08-19, re-verified 2026-08-20). Every new `.conf`/`.et`/`.meta` object takes the next value; a collision fails silently. Record each GUID used below as it is taken.

| GUID | Taken by | Where |
|---|---|---|
| `6B70D00000000001` | `SCR_DamagePhaseData` (Bunkers ruined phase) | `E_Sandbag_01_bunker_plastic_foundation_camonet.et:21` |
| `6B70D00000000002` | `OVT_StructureDestructionComponent` (Guard Tower) | `OVT_GuardTower_01.et:14` |
| `6B70D00000000003` | `SCR_DamagePhaseData` (Guard Tower ruined phase) | `OVT_GuardTower_01.et:20` |
| `6B70D00000000004` | `OVT_StructureDestructionComponent` | `OVT_RecruitmentTent.et` |
| `6B70D00000000005` | `SCR_DamagePhaseData` (ruined phase) | `OVT_RecruitmentTent.et` |
| `6B70D00000000006` | `OVT_StructureDestructionComponent` | `OVT_MedicalTent.et` |
| `6B70D00000000007` | `SCR_DamagePhaseData` (ruined phase) | `OVT_MedicalTent.et` |
| `6B70D00000000008` | `OVT_StructureDestructionComponent` | `Garage_E_02.et` |
| `6B70D00000000009` | `SCR_DamagePhaseData` (ruined phase) | `Garage_E_02.et` |
| `6B70D0000000000A` | `OVT_StructureDestructionComponent` | `OVT_FuelDepot.et` |
| `6B70D0000000000B` | `SCR_DamagePhaseData` (ruined phase) | `OVT_FuelDepot.et` |
| `6B70D0000000000C` | `OVT_StructureDestructionComponent` | `Helipad.et` |
| `6B70D0000000000D` | `SCR_DamagePhaseData` (ruined phase) | `Helipad.et` |
| `6B70D0000000000E` | `OVT_StructureDestructionComponent` | `OVT_VehicleMaintenanceRamp.et` (**the child**) |
| `6B70D0000000000F` | `SCR_DamagePhaseData` (ruined phase) | `OVT_VehicleMaintenanceRamp.et` (**the child**) |

| `6B70D00000000010`–`6B70D00000000011` | `SCR_SimpleMessagePreset` + its UI info (`RepairedStructure`) | `Configs/overthrowBroadcastMessages.conf` |
| `6B70D00000000012`–`6B70D00000000013` | `OVT_RepairStructureAction` + `UIInfo` (entry on the existing manager) | `OVT_FuelDepot.et` |
| `6B70D00000000014`–`6B70D00000000017` | `UserActionContext` "repair" + `PointInfo` + action + `UIInfo` (entry on the existing manager) | `OVT_MedicalTent.et` |
| `6B70D00000000018`–`6B70D0000000001C` | whole `ActionsManagerComponent` + context + `PointInfo` + action + `UIInfo` | `OVT_GuardTower_01.et` |
| `6B70D0000000001D`–`6B70D00000000021` | same five | `OVT_RecruitmentTent.et` |
| `6B70D00000000022`–`6B70D00000000026` | same five, on the **root** | `OVT_VehicleMaintenanceRamp.et` |
| `6B70D00000000027`–`6B70D0000000002B` | same five | `E_Sandbag_01_bunker_plastic_foundation_camonet.et` |
| `6B70D0000000002C`–`6B70D00000000030` | same five | `Garage_E_02.et` |
| `6B70D00000000031`–`6B70D00000000035` | same five | `Helipad.et` |
| `6B70D00000000036`–`6B70D00000000037` | `CustomStringTableItem` ×2 (`OVT-RepairStructure`, `-Repaired`) | `Language/localization_Overthrow.st` |
| `6B70D00000000038` | the `.conf` file GUID | `Configs/Deployment/Deployment_ObjectiveRepair.conf.meta` |
| `6B70D00000000039`–`6B70D0000000003A` | `OVT_InsertionSpawningDeploymentModule` + `OVT_NearestControlledBaseSourceProvider` | `Deployment_ObjectiveRepair.conf` |
| `6B70D0000000003B`–`6B70D0000000003D` | `OVT_BaseRepairBehaviorDeploymentModule`, `OVT_ReinforcementBehaviorDeploymentModule`, `OVT_BaseControlConditionDeploymentModule` | `Deployment_ObjectiveRepair.conf` |
| `6B70D0000000003E` | the registry entry | `Configs/Deployment/overthrowDeployments.conf` |
| `6B70D0000000003F`–`6B70D00000000048` | the **Ruins and Repair** Field Manual entry + its 9 pieces | `Configs/FieldManual/Categories/FM_Overthrow.conf` |
| `6B70D00000000049`–`6B70D00000000052` | `CustomStringTableItem` ×10 (`OVT-FieldManual_Ruins_*`) | `Language/localization_Overthrow.st` |

Next free: `6B70D00000000053`. ⚠ **`6B70D00000000018` was reserved above and never actually taken** — the Guard Tower's action block uses `…0019`–`…001C` (four objects, not five). Verified free 2026-08-20; available to whoever needs one next. Every value above was re-verified unused across `Prefabs/`, `PrefabsEditable/`, `Configs/`, `Scripts/`, `Worlds/` **and the vanilla reference tree** before it was taken (2026-08-20). The Bunkers component deliberately does **not** take a new GUID — it re-uses the inherited instance GUID `{5E76C88E922E8914}` (see S4 / the 1.3 note).

---

## Spike answers S1–S6

*Phase 1 (task 1.7) MUST fill these in — each with the observed evidence and the chosen branch. Nothing downstream may start until all six are answered. Question text and the pre-recorded fallbacks are in `implementation.md` §4 Phase 1.*

### S1 — ANSWERED (source evidence; in-game confirmation owed for collision)

**Q:** Does the `SCR_DestructionTireComponent`-style `RepairToIntact()` put the intact model back — geometry, materials and collision?

**Answer:** Source-evidence answer: **yes for geometry and materials; collision is conditional and is the one thing that can fail silently.** In-game confirmation owed (see "Needs human verification").

**Evidence:**
- The hole is exactly as the plan described: `SCR_DestructionMultiPhaseComponent.c:227-231` returns before the `CallLater(ChangeModel, …)` because `GetDamagePhaseData(0)` is null by design (`:67-73`), and `GetOriginalResourceName()` (`:141-147`) is read by nothing.
- **But `super.GoToDamagePhase(0, …)` does everything else phase 0 needs** — `RegenerateNavmeshDelayed` (`:189`), `SetDamagePhase(0)` (`:211`), `SetTargetDamagePhase(1)` (`:212`), the `m_fNextPhaseHealth` recompute (`:202-209`) and `callQueue.Remove(ChangeModel)` (`:218-221`). So `RepairToIntact()` calls super and then schedules only the mesh change super refused to, rather than re-implementing the phase bookkeeping the way `SCR_DestructionTireComponent.ReturnToInitialDamagePhase` (`:222-235`) does.
- Materials: `ChangeModel` (`:237-266`) → `SCR_Global.GetModelAndRemapFromResource` → `SCR_DestructionUtility.SetModel(owner, modelPath, remap)` (`:141-153`) → `owner.SetObject(model, remap)`. We hand it the name cached from the owner's own `VObject` at `OnPostInit`, so what comes back is the resource that was there before.
- 🔴 **Collision is the risk.** `SetModel` ends `if (!phys.UpdateGeometries()) phys.Destroy();` (`SCR_DestructionUtility.c:150-152`). If the RUIN model has no physics geometry the entity's physics is destroyed **at ruin time**, and the repair then runs with `GetPhysics()` null so `UpdateGeometries()` is never called again — a visually perfect, walk-through structure. Guard Tower looks safe: ruins get `RigidBody { ModelGeometry 1; Static 1 }` from `Prefabs/Structures/Core/BuildingRuin_base.et:14-17`, which only makes sense if `GuardTower_01_Ruin.xob` carries geometry. The Bunkers root has no `RigidBody` anywhere in its chain, so it has no physics to lose.
- Two facts the plan did not have: (a) super early-returns at `:186-187` when already in the requested phase, so repairing an intact structure is a real no-op; (b) an **empty** cached model is legitimate (see the 1.5 note on the Bunkers composition), so `ChangeModel` is overridden to translate empty into `owner.SetObject(null, string.Empty)` — the vanilla idiom at `SCR_DestructibleBuildingComponent.c:1450`.

**Branch chosen:** the planned one. `override GoToDamagePhase` routes phase 0 into `RepairToIntact()`; the protected **`ChangeModel`** is used, not the `SCR_DestructionUtility.SetModel` fallback — it was not awkward (a subclass reaches a protected member), and keeping `ChangeModel` as the scheduled function is what lets super's own `callQueue.Remove(ChangeModel)` cancel a pending repair when a later phase change arrives. `GetOriginalResourceName()` is never called.

### S2 — ANSWERED (source evidence; two-machine confirmation owed)

**Q:** Does the broadcast RPC reach a second machine and drive the phase there, **in both directions**?

**Answer:** Source-evidence answer: **the message is legal, correctly shaped and arity-audited; whether it lands on a second machine is a listen-host test.** In-game confirmation owed, both directions.

**Evidence:**
- The base class already hosts a `[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]` (`RPC_ReplicateMultiPhaseDestructionState`, `:420-459`), so this component is a proven legal RPC host.
- Why ours rather than vanilla's, confirmed: `ReplicateDestructibleState` (`:462-474`) sends nothing without hit info, and the receiver only calls `GoToDamagePhase` inside `if (phaseData)` — null for phase 0 — so vanilla structurally cannot deliver a repair.
- **Arity hand-audited** (BUG-090 class, recorded in a comment at the call site): handler is `RpcDo_ApplyPhase(int phase, bool withEffects)`, two primitives; all three `Rpc()` call sites (`RuinIt`, `RepairIt`, `RestorePhase`) pass exactly two.
- Server self-invoke: each of the three calls the handler locally **and** `Rpc()`s it; the handler's `if (!Replication.IsServer())` guard stops the phase being driven twice on the server, and the effects branch runs everywhere.
- 🔴 **Precondition not satisfied on one probe before this phase's edit** — a broadcast needs an active `RplComponent` on the owner. See S5.

**Branch chosen:** the planned component-hosted broadcast. The S2 fallback (moving the message onto `OVT_ResistanceRequestComponent` as a `(RplId, int, bool)` verb) was **not** taken and stays available.

### S3 — ANSWERED (source evidence; "nothing misbehaves" confirmation owed)

**Q:** Does `SCR_MPDestructionManager.GetInstance()` become non-null in an Overthrow session, and does anything misbehave during the startup window before it does?

**Answer:** Source-evidence answer: **yes, it auto-spawns, and nothing this feature calls needs it either way.** In-game confirmation owed for the "nothing misbehaves" half.

**Evidence:**
- `Prefabs/GameMode/OVT_OverthrowGameMode.et:279` carries `SCR_DestructionManagerComponent "{5DFEE7C5F4445327}"`.
- `SCR_DestructionManagerComponent.c:17-29`: `EOnInit` requires a non-proxy `RplComponent` on the game mode, then `CallLater(SCR_MPDestructionManager.InitializeDestructionManager, delay)` with `delay = 500`, or `10000` when `RplSession.Mode() == RplMode.Dedicated`.
- `SCR_MPDestructionManager.c:131-147`: `InitializeDestructionManager` spawns `MPDestructionManager.et` when `s_Instance` is null.
- Nothing written in Phase 1 reaches `GetInstance()`. The one attribute that could route into `SCR_DestructionUtility.PlaySound` is `m_eMaterialSoundType` (authored `BREAK_ROCK` / `BREAK_METAL` on the probes), and it is only read from vanilla's weapons-damage path (`:332`, `:356`, `:376`) and vanilla's own broadcast receiver — neither of which we drive — and `PlaySound` hard-returns without the manager (`SCR_DestructionUtility.c:60-63`) rather than erroring.

**Branch chosen:** the pre-recorded one — **depend on nothing**. **No Eden layer was edited**, and Phase 2 is already bound by D5/D6 to `SCR_DestructionCommon.PlayParticleEffect_CompleteDestruction` and `SCR_SoundManagerModule`, neither of which touches the manager.

> **Note added 2026-08-20 (BD29), history kept as written:** the S3 answer above is right that the manager auto-spawns, and the Phase 2 code that followed it read as though it never would. It does. `RaiseSound()` now *prefers* the manager's own audio configuration when `GetInstance()` is non-null and keeps ours for the startup window — and the real defect was never the config but the two missing signals.

### S4 — ANSWERED (source evidence; Workbench load owed)

**Q:** Can `OVT_StructureDestructionComponent` and `SCR_DestructibleBuildingComponent` coexist on one entity, and does `Enabled 0` in a delta really deactivate the latter?

**Answer:** **`Enabled 0` in a delta is proven vanilla practice.** On the Bunkers probe the coexistence question was removed entirely by *retyping* the inherited component instead of adding a second one; on the Guard Tower the two are authored side by side with the building component disabled, so only one damage manager is live. Whether **two live** damage managers work is still unknown and still does not matter.

**Evidence:**
- Disable precedent: `TentUSSR_01_base.et:11-13` ships `SCR_DestructibleBuildingComponent "{692CD84F45C9100B}" : "…DestructibleBuildingComponent_base.ct" { Enabled 0 }`, and that file is itself a delta over `Prefabs/Structures/Core/Building_Base.et`.
- 🟢 **Retype precedent — a delta CAN re-declare an inherited component's instance GUID under a SUBCLASS**, and vanilla does it in this exact family: `Prefabs/Weapons/Core/Turret_Base.et:66` declares `SCR_DamageManagerComponent "{51ACD09C524A7924}"`; its direct child `Prefabs/Weapons/Core/Tripod_Base.et:70` re-declares the *same instance GUID* as `SCR_DestructionMultiPhaseComponent`, and `Prefabs/Weapons/Core/Mortar_Base.et:122` does the same. Two more GUIDs behave identically (`3EBB276D48AFFF41` → `SCR_RotorDamageManagerComponent`; `141326E9FD94FE40` → `SCR_Helicopter`/`SCR_WheeledDamageManagerComponent`). Found by scanning every `ClassName "{GUID}"` pair in the vanilla `Prefabs/` tree for a GUID carrying more than one class name.
- The Guard Tower's root class contributes nothing that could argue with our component: `SCR_DestructibleBuildingEntity`'s entire body is inside `#ifdef ENABLE_BUILDING_DESTRUCTION` (`SCR_DestructibleBuildingEntity.c:52`), which is defined nowhere in the shipping tree, so at runtime it is an empty `Building` subclass.

**Branch chosen:** retype on Bunkers, add-and-disable on the Guard Tower. The "host our component on a child entity" fallback was not needed.

### S5 — ANSWERED (confirmed NO, from the prefab chain — shipped defect)

**Q:** Does a built **Bunker** have an active `RplComponent` on the shipped tree, before our edit?

**Answer:** **No. Confirmed, not suspected — and it is a shipped defect that predates this feature.** Bug-report candidate for the orchestrator (already listed in `tasks.md`).

**Evidence — the whole chain:**
1. `PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_bunker_plastic_foundation_camonet.et` at HEAD contains **zero** `RplComponent` lines.
2. Its parent `Prefabs/Props/Military/Sandbags/Sandbag_01_bunker_plastic_foundation_camonet.et` — none either.
3. Its grandparent `Sandbag_01_bunker_plastic_foundation.et:20-21` declares `RplComponent "{5E76C88E937D49B8}" : "{3B686EF5E3C610B9}Prefabs/Props/Core/DestructionMultiPhase_Rpl_Base.ct" { }` and overrides nothing.
4. That preset is two lines: `RplComponent { Enabled 0 }`.
5. Vanilla's own sibling that *does* want replication re-declares it: `Prefabs/Compositions/Misc/CustomEntities/DestructionEntities/Sandbag_01_bunker_plastic_CompositionDestruction.et:7-9` = `RplComponent "{5E76C88E937D49B8}" { Enabled 1 }`.
6. The bunker is a live buildable at cost 750 — `Configs/Resistance/buildables.conf:53-64`.

**Consequences beyond this feature (for whoever files it):** with no `RplComponent` a built bunker has no `RplId`, so `OVT_ResistanceRequestComponent.RpcAsk_RemovePlacedItem(RplId)` can never name one — dismantle is unreachable. Whether a built bunker replicates to clients **at all** in MP is the wider question the report should ask.

**Branch chosen:** the pre-recorded one — the retrofit authors `RplComponent "{5E76C88E937D49B8}" { Enabled 1 }` (`E_Sandbag_01…camonet.et:28-30`). The fix ships with this feature; the report is still owed because the defect is independent of it.

### S6 — ANSWERED (source evidence; live-fire confirmation owed)

> **⚠️ CORRECTED BY THE PHASE 2 GATE (2026-08-20):** the "no hit zone → OnPostInit early-returns, weapons path never arms" branch is **not available**. Spawning the Guard Tower probe in the Init suite logged `DEFAULT (E): HitZoneContainers require at least one hitzone` and the engine **dropped the component entirely** — `OVT_StructureDamage.IsDestructible()` returned false on the spawned prefab (`OVT_TEST_Init_StructureDamage_BPhaseRoundTripOnAProbe` red). Vanilla's own template `Prefabs/Props/Core/DestructionMultiPhase_Base.ct` always authors an `"Additional hit zones" { SCR_HitZone Default { HZDefault 1 … } }` block for exactly this reason. **Fix applied to `OVT_GuardTower_01.et`:** a `Default` hit zone with `HZDefault 1`, Melee 0.01 / Kinetic 0.1 / Explosive 1 / Incendiary 0.1 multipliers (sandbag-style, D18 lever (ii) + low multipliers). Bunkers inherits its hit zone from the vanilla base prefab. **Phase 4 MUST author the same block on every component it adds** (tents, garage, depot, helipad, ramp child) or the component silently never exists. The case went green on the re-run (332/333).


**Q:** With a damage manager present, can a player shoot a structure to phase 1, and does an entity without an authored hit zone still take the scripted phase change?

**Answer:** **The scripted path is independent of hit zones — confirmed from source.** Whether the *weapons* path arms differs per probe, so the retrofit authors both D18 levers and does not depend on the answer.

**Evidence:**
- Scripted independence: `GoToDamagePhase` (`:174-232`) touches no hit zone; `SetDamagePhase` (`:165-170`) writes the phase and a `SignalsManagerComponent` signal. `OnPostInit` (`:520-545`) early-returns at `:529-531` when `GetDefaultHitZone()` is null, skipping only `SetHitZoneHealth`, `SetTargetDamagePhase(1)` and `m_fNextPhaseHealth` — none of which our path reads.
- Weapons path: `OnDamage` is only reachable through a hit zone, and it early-returns at `:369-371` when `GetTargetDamagePhase() <= 0`, which is precisely the state a hit-zone-less component is left in. So **lever (i) is real**: no hit zone ⇒ weapons never arm, scripted path still works.
- **Guard Tower** → lever (i). Our component is a fresh instance with no `"Additional hit zones"` and no `.ct` behind it, so `GetDefaultHitZone()` (an engine proto on `HitZoneContainerComponent`, `scripts/Game/generated/Components/HitZoneContainerComponent.c:18`) has nothing to return.
- **Bunkers** → unknown, so lever (ii). The retype's hit-zone inheritance is the one thing the files cannot settle: the ancestor authors `"Additional hit zones" { SCR_HitZone Default { Melee 0.01, Kinetic 0.1 } }` over `Prefabs/Props/Core/DestructionMultiPhase_Base.ct`, which carries `SCR_HitZone Default { HZDefault 1 … "Explosive multiplier" 90 }`. If the retype preserves it, the weapons path arms with a ×90 explosive multiplier.
- **Lever (ii), the numbers, authored on BOTH probes:** `m_fBaseHealth 100000` plus a single phase at `m_fPhaseHealth 100000` (100000 is the attributes' authored slider maximum — `SCR_DestructionDamageManagerComponentClass:3-4`, `SCR_DamagePhaseData.c:5`), i.e. 200000 total; **and `m_fDamageThresholdMaximum 50000`**, up from the class default 3000. The third number is load-bearing: without it the `hitInfo.m_TotalDestruction` shortcut at `:302-325` bypasses the health ladder entirely and one satchel ruins the structure however high the health is. 50000 is vanilla's own figure for a fortification-sized destructible (`BrickGate_01_base.et:29`). This attribute is not named in the plan — recorded as build decision **BD2** below.

**Branch chosen:** **both levers**, deliberately — (i) where it falls out for free, (ii) authored everywhere so the answer does not depend on how the retype resolves the hit zones.

---

### Also recorded from Phase 1

**1.3 — the class retype was taken.** `OVT_StructureDestructionComponent "{5E76C88E922E8914}"` re-declares the inherited `SCR_DestructionMultiPhaseComponent` instance. Two independent pieces of evidence: the vanilla retype precedent in S4, and the fact that vanilla itself addresses *this very GUID* from a delta on *this very prefab family* (`Sandbag_01_bunker_plastic_CompositionDestruction.et:4-6` re-declares `{5E76C88E922E8914}` with `Enabled 1`). Only the class name differs in ours. **Only a Workbench load can prove it (1.U1).** If it fails, the fallback is mechanical and pre-planned: give our block a fresh GUID from the reserved series and add `SCR_DestructionMultiPhaseComponent "{5E76C88E922E8914}" { Enabled 0 }` beside it.

**1.5 — 🔴 THE BUNKERS ROOT CARRIES NO MODEL OF ITS OWN.** This is the headline Phase-1 finding and it changes Phase 4. `Prefabs/Props/Military/Sandbags/Sandbag_01_bunker_plastic_foundation.et` is a `StaticModelEntity` with **no `MeshObject`** — the visible geometry is three CHILD entities (`Sandbag_01_bunker_plastic`, `Sandbag_01_foundation_plastic`, and the camo net added by the `_camonet` delta), each with its own `MeshObject` and `RigidBody`. Three consequences:
1. A root-level mesh swap **adds** the ruin; it does not replace the bunker. The probe will show `Bunker_SPS_Ruin.xob` standing inside an intact sandbag bunker. Expect that in 1.U2 and do not read it as the mechanism failing.
2. `GetOwner().GetVObject()` on that root is null, so the cached intact model is legitimately **empty** — which is why `ChangeModel` is overridden to translate empty into `SetObject(null, …)`. Without that override a repaired bunker would keep the ruin mesh forever.
3. Vanilla's own answer to this composition is a destruction component **per child**: `Sandbag_01_bunker_plastic_CompositionDestruction.et:11-19` enables the child's `{5624A88D86EFE8BA}`, and the children already carry disabled ones (`Sandbag_01_bunker_base.et:44-51`, `Sandbag_01_foundation_base.et:10-12`).

→ **Phase 4 needs a composition strategy for the Bunkers**, and must check the Helipad and the two tents for the same shape *before* authoring them. This is the second composition special case after the ramp (plan §3.10).

**1.5 — the Guard Tower phase model is the bare `.xob`, not the ruin prefab.** `SCR_Global.GetModelAndRemapFromResource` (`scripts/Game/Global/Functions.c:1446-1509`) reads the prefab's **own** `components` array (`GetObjectArray("components")`) and returns false when there is none — it does **not** walk the prefab's inheritance. `Prefabs/Structures/Military/Houses/GuardTower_01/GuardTower_01_Ruin.et` has no `components` block of its own (the `MeshObject` is on its `_base`, `GuardTower_01_Ruin_base.et:4-6`) and there is no `Materials` remap anywhere in that chain, so the §3.10a preference for the prefab buys nothing and risks an empty model path. Authored `{F0FDB651ED7B2B76}…GuardTower_01_Ruin.xob` instead. **Phase 4 test to apply per structure:** use a `…_Ruin.et` prefab only when *that* prefab has its own inline `components { MeshObject { Object …; Materials … } }` — the `Prefabs/Weapons/Tripods/dst/Tripod_6T7_Damaged.et` shape.

Both ruin `.xob` GUIDs were verified to exist by reference (the reference tree ships no `Assets/`): `{F0FDB651ED7B2B76}` at `GuardTower_01_Ruin_base.et:5`, `{1C9E0D1CD5A0E4F9}` at `Prefabs/Structures/Military/Fortifications/Bunker_SPS/Dst/Bunker_SPS_Ruin_base.et:5`. Scale and ground alignment remain a play-test verdict; the `Bunker_SPS` ruin is a concrete MG bunker and may read far too large beside a sandbag emplacement.

---

### The eight prefabs, as retrofitted (Phase 4, 2026-08-20)

Every chain below was walked to its vanilla root before anything was authored. **Line 1 of every prefab is untouched** (`git diff -U0 -- Prefabs/ PrefabsEditable/ | grep -E "^[+-][A-Za-z_]+ *[:{]"` is empty).

| # | Buildable | Root class (unchanged) | Our component sits on | Damage manager disabled | `RplComponent` | Ruin model |
|---|---|---|---|---|---|---|
| 1 | Guard Tower | `SCR_DestructibleBuildingEntity` | the root | `{5D7E937DD0A125D0}` re-declared `Enabled 0` | `{50A4E7C9B5728062}` authored `Enabled 1` | `GuardTower_01_Ruin.xob` (real) |
| 2 | Recruitment Tent | `SCR_DestructibleBuildingEntity` | the root | inherited `{692CD84F45C9100B}` **already** `Enabled 0` (`TentUSSR_01_base.et:11-13`) — not re-declared | `{50A4E7C9B5728062}` authored `Enabled 1` | `DebrisPile_Wood_01.xob` (fallback) |
| 3 | Medical Tent | `SCR_DestructibleBuildingEntity` | the root | same inherited disabled one | `{50A4E7C9B5728062}` authored `Enabled 1` | `DebrisPile_Wood_01.xob` (fallback) |
| 4 | Vehicle Maintenance Ramp | `GenericEntity` (bare root) | **the `SCR_DestructibleBuildingEntity` child** | `{67148C0FAD779ACE}` re-declared `Enabled 0` **on the child** | root `{59CF196A053B1B4F}` + child `{50A4E7C9B5728062}`, both `Enabled 1` | `RampVehicle_01_metal_Ruin.xob` (real) |
| 5 | Bunkers | `StaticModelEntity` | the root (class **retype**, BD1) | n/a — the retype leaves exactly one | `{5E76C88E937D49B8}` authored `Enabled 1` | `Bunker_SPS_Ruin.xob` (fallback) |
| 6 | Garage | `SCR_DestructibleBuildingEntity` | the root | `{5D7AD092A3CB4B41}` re-declared `Enabled 0` | `{50A4E7C9B5728062}` authored `Enabled 1` | `Garage_E_02_Ruin.xob` (real) |
| 7 | Helipad | `StaticModelEntity` | the root | none anywhere in the chain | `{5D748E174E08BEBB}` (`Default_RplComponent.ct`) now says `Enabled 1` | `DebrisPile_Concrete_01_Medium.xob` (fallback) |
| 8 | Fuel Depot | `SCR_DestructibleBuildingEntity` | the root | `{5DA867F4F045AA84}` re-declared `Enabled 0` | `{50A4E7C9B5728062}` authored `Enabled 1` | `FuelTank_02_Ruin.xob` (real) |

Every one of the six new component blocks carries, without exception: `Enabled 1`, the `"Additional hit zones" { SCR_HitZone Default { HZDefault 1 … } }` block the S6 correction made mandatory, `m_fBaseHealth 100000`, `m_fDamageThresholdMaximum 50000` (BD2), `m_bDeleteAfterFinalPhase 0` (D4), one `SCR_DamagePhaseData` at `m_fPhaseHealth 100000`, and an `m_eMaterialSoundType`.

A grep for `"Additional hit zones"` across `Prefabs/`+`PrefabsEditable/` therefore returns **7, not 8** — the Bunkers is the exception the S6 correction already names, because it inherits one from `Sandbag_01_bunker_plastic_foundation.et:5-10` over `DestructionMultiPhase_Base.ct`. That inheritance was re-verified in Phase 4 and the prefab was deliberately left alone.

⚠ **The new Init case is the first automated test of the Bunkers class retype (BD1).** It spawns every buildable in the shipped config, so a retype that failed to carry the inherited hit zone across the class change shows up as `IsDestructible()` false on the Bunkers — a red naming that prefab, and the pre-planned fallback is in BD1 (fresh GUID + `SCR_DestructionMultiPhaseComponent "{5E76C88E922E8914}" { Enabled 0 }` beside it, plus a hit-zone block of our own). Until then 1.U1's Workbench load remains the authoritative check.

#### Composition: the Bunkers finding did NOT generalise

The Phase-1 open question was whether the Helipad and the two tents share the Bunkers' shape — a root with no mesh of its own, where a phase model is **added beside** intact children rather than replacing anything. **They do not.** Checked chain by chain:

- **Recruitment Tent / Medical Tent** — `TentUSSR_01_base.et:4-6` puts `MeshObject "{506EDA9D46FC6C06}"` = `TentUSSR_01.xob` on the root, and both Overthrow files inherit it (the recruitment tent re-declares the same instance only to remap the fabric to FIA colours). The root IS the canvas, so the swap replaces it.
- **Helipad** — `HelipadImprovised_US_01_base.et:4-6` puts `MeshObject "{56ACD1C4DC78C87B}"` = `HelipadImprovised_US_01.xob` on the root. Same story.
- **Garage / Fuel Depot** — root meshes at `Garage_E_02_base.et:4-6` and `FuelTank_02_Base.et:4-6`.
- **Vehicle Maintenance Ramp** — the root is genuinely bare, which is why the component went on the child. That child (`RampVehicle_01_metal_base.et:4-9`) carries the mesh, the `RigidBody`, the destructible component and both support stations, so putting ours there is the *mesh-carrying-child* strategy, not a workaround. `OVT_StructureDamage.Resolve()` finds it from the root by its child walk, and the new Init case spawns the real prefab to prove it.

So **Bunkers remains the only composition root in the eight**, and nothing about it was changed in Phase 4. Its known result — `Bunker_SPS_Ruin.xob` standing inside intact sandbags — is still 1.U2 / 4.U2's to judge, and the pre-planned answer if it reads badly is vanilla's own: a destruction component per child (`Sandbag_01_bunker_plastic_CompositionDestruction.et:11-19`).

**What a root swap does NOT do, on any of them: remove the children.** A ruined recruitment tent keeps its floor, table, cot, radio and lamp standing in the debris; a ruined medical tent keeps its floor and red-cross cloth; a ruined ramp keeps the tyre pile, welder, jerrycan, bucket, extinguisher and the two ground decals. That is cosmetic — the *actions* on those children are gated (4.6) — and hiding them would need per-child visibility state with its own replication and its own save field. Recorded, not built. Play-test verdict.

#### How each ruin model was chosen, and how it should read

- **Ramp, Garage, Fuel Depot — their real vanilla ruins, authored as the bare `.xob`, not the `…_Ruin.et` prefab.** BD3 is the reason and it held for all three: `RampVehicle_01/Ruin/RampVehicle_01_metal.et` has no `components` block at all, and `Garage_E_02/Dst/Garage_E_02_Ruin.et` has only a children list of debris props — for both, `GetModelAndRemapFromResource` would find no `MeshObject` and answer false. `FuelTank_02_green_Ruin.et` is the trap case: it **does** have an inline `components { MeshObject { Materials … } }` but **no `Object` line** (that is on `FuelTank_02_Ruin_base.et:4-6`), so under BD3's rule it would resolve to an EMPTY model path with a valid remap — a silent no-op. The bare `.xob` was authored instead. ⚠ **The cost is the green remap**: the depot's ruin will wear `FuelTank_02_Ruin`'s default materials rather than the green ones. If that reads wrong in play-test, the fix is not the prefab — it is one `Materials` block copied onto a new Overthrow-owned `_Ruin.et` with its own `Object` line.
- **Guard Tower, Bunkers** — unchanged from Phase 1.
- **Recruitment Tent, Medical Tent — `{0F7DDAB5C669F262}…DebrisPiles/DebrisPile_Wood_01.xob`.** §3.10a offered `Rubble_Ruin_01_V2` or `DebrisPile_Concrete_01_Medium` and flagged both as the honest problem: brick or concrete rubble where a canvas tent stood does not read. A collapsed timber frame does, and the wood pile is from the same authored-filler family the plan already pointed at, is a single self-contained mesh with no debris children, and needs no new art. **Honest risk: SCALE.** A tent is roughly 5 × 8 m and a `DebrisPile_*` is authored as filler, so this may read as "the tent collapsed and most of it was cleared away" rather than as a wreck filling the footprint. If it is too small, the pre-approved swap is `{D88A17A1346EA3C5}…Rubble_Ruin_01/Rubble_Ruin_01_V2.xob` (building-sized, wrong material) — a one-line change in each prefab.
- **Helipad — `{0335D6B08DBE867E}…DebrisPiles/DebrisPile_Concrete_01_Medium.xob`.** The intact model is a FLAT ground pad, so the ruin has to be low-profile or it will read as a pile that appeared from nowhere; this is exactly the "lower-profile alternative" §3.10a names. **Honest risk:** a helipad is wide (its own parking box is 16 m) and a medium debris pile is not, so expect a small mound in the middle of a bare patch rather than a wrecked pad.
- **`m_eMaterialSoundType`** — `BREAK_TENT` (tents), `BREAK_METAL` (ramp, depot), `BREAK_ROCK` (garage, matching its vanilla `Brick_Large` preset), `BREAK_WOOD_SOLID` (helipad — a judgement call on an improvised planked pad; the sound is a code-side fallback (BD8) and an unknown event is silence, not an error, so a wrong guess costs nothing but a missing noise).

### Vanilla-update checklist (D2)

Everything `OVT_StructureDestructionComponent` depends on that lives in the base game. Re-check every row against `/mnt/n/Projects/Arma 4/ArmaReforger` after a Reforger update; line numbers are 1.8.0.10.

| Symbol | Where | Why we depend on it | What breaks if it changes |
|---|---|---|---|
| `SCR_DestructionMultiPhaseComponent` / `…ComponentClass` | `Destruction/SCR_DestructionMultiPhaseComponent.c:2,20` | base class of both our class and our class-data class | compile error (loud) |
| `ENABLE_BASE_DESTRUCTION` defined | `#define` at the top of ~15 vanilla files, e.g. `SCR_DestructionDamageManagerComponent.c:1` | the whole base-class body is inside `#ifdef` | every inherited member vanishes — compile error (loud) |
| `GoToDamagePhase(int, bool)` | `:174-232` | we override it and call `super` for both branches | compile error (loud) |
| …its phase-0 early return at `:227-229` | `:227-231` | `RepairToIntact()` exists *because* super stops here. If a future version schedules the phase-0 mesh itself, our extra `CallLater` becomes a duplicate | silent double mesh set |
| …its `callQueue.Remove(ChangeModel)` at `:218-221` | `:218-221` | cancels a pending repair when a later phase change arrives | silent race between ruin and repair |
| …its "past the final phase = delete" at `:176-183` | `:176-183` | `RuinIt()` guards `GetNumDamagePhases() <= 1` because of it | a structure deletes itself (the exact bug this feature removes) |
| …its `RegenerateNavmeshDelayed` at `:189` | `:189` | D7: we add no navmesh call of our own | AI walks through ruins / into gaps |
| `ChangeModel(ResourceName, bool)` (protected) | `:237-266` | we override it and call `super`; we also schedule it by name | compile error (loud) |
| `GetDamagePhase()` / `GetNumDamagePhases()` | `:91-97`, `:101-109` | `IsRuined()`, the phase guards | compile error (loud) |
| `GetComponentData(owner)` cast to `SCR_DestructionMultiPhaseComponentClass` | `:198`, used by us in `RepairToIntact` | reads `m_fMeshChangeDelay` | null delay (harmless) |
| `OnRplSave` / `OnRplLoad` | `:486-517` | the entire JIP / stream-in story; we add nothing | JIP clients see the wrong mesh |
| `SCR_DestructionUtility.SetModel` + `phys.UpdateGeometries()` | `SCR_DestructionUtility.c:141-153` | S1's collision behaviour | silently walk-through structures |
| `SCR_Global.GetModelAndRemapFromResource` | `Global/Functions.c:1446-1509` | decides whether an `.et` or only an `.xob` works as `m_PhaseModel` | ruin mesh silently not applied |
| `SCR_DamagePhaseData.m_PhaseModel` / `m_fPhaseHealth` | `SCR_DamagePhaseData.c:5-9` | prefab authoring | prefabs stop loading (Workbench catches) |
| `IEntity.SetObject(null, string.Empty)` | vanilla idiom, `SCR_DestructibleBuildingComponent.c:1450` | clearing a composition root's model on repair | ruin mesh sticks after repair |
| `SCR_AudioSourceConfiguration` | `[Attribute]` on our class | Phase 2's sound | compile error (loud) |
| `SCR_DestructionCommon.PlayParticleEffect_CompleteDestruction` | `SCR_DestructionCommon.c:80` | Phase 2's particles | compile error (loud) |
| `SCR_SoundManagerModule.GetInstance(World)` / `CreateAudioSource(owner, config, pos)` / `PlayAudioSource` | `Systems/Sound/SCR_SoundManagerModule.c:324,113,250` | Phase 2's sound, deliberately not `SCR_DestructionUtility.PlaySound` | compile error (loud) |
| `SCR_SoundEvent.SOUND_MPD_` + `Sounds/Destruction/Multiphase/Destruction_Multiphase.acp` | `Helpers/SCR_SoundEvent.c:346`, `Prefabs/MP/MPDestructionManager.et:18` | the BD8 fallback event name and bank | ruins go silent (quiet) |
| `SetDamagePhase(int)` (protected) | `:165-170` | **the funnel** (BD27) - our effects and the support-station switch hang off it | compile error (loud) if renamed; silent loss of both if a future version stops routing every phase change through it |
| `OnDamage()`'s `m_TotalDestruction` branch | `:297-316` | the path GM "Neutralize" and weapon kills take; it is *why* the funnel exists | ruins with no effects again |
| `ParticleEffectEntity.SpawnParticleEffect` + `ParticleEffectEntitySpawnParams.FollowParent` / `TransformMode` | `GameLib/generated/Particles/ParticleEffectEntitySpawnParams.c` | the retained fire/smoke handles (BD28) | compile error (loud) |
| `BaseWorld.GetSurfaceY` | `Core/generated/World/BaseWorld.c:24` | terrain snap for the ground fire pool (BD28 correction) | compile error (loud) |
| `SCR_ParticleHelper.StopParticleEmissionAndLights` | `Helpers/SCR_ParticleHelper.c:9-26` | the only way a retained emitter is stopped | fire/smoke never stop (loud in play) |
| `SCR_AudioSource.PHASES_TO_DESTROYED_PHASE_SIGNAL_NAME` / `ENTITY_SIZE_SIGNAL_NAME`, `SCR_DestructionUtility.GetDestructibleSize` | `SCR_AudioSource.c:37-38`, `SCR_DestructionUtility.c:177` | the destruction sound resolves through these (BD29) | ruins go silent (quiet) |
| `SCR_MPDestructionManager.GetInstance()` / `GetAudioSourceConfiguration()` | `SCR_MPDestructionManager.c` | preferred audio config when the manager is up | falls back to ours (quiet) |
| `Sounds/Particles/Logistics/Explosion/TNT/Particles_Explosions_TNT_Large.acp` + `SOUND_EXPLOSION` (Fuel_Large on the depot) | `Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Large.et:20`, `FuelTank_03_base.et:31-36` | the long-range blast layer (BD30) | ruins are only a close-range break again (quiet) |
| The default particle resources | vanilla `Particles/` | `m_ExplosionParticle` / `m_DebrisParticle` / `m_FireParticle` / `m_SmokeParticle` defaults | no FX (quiet) |

---

### The deployment framework, as surveyed at HEAD for Phase 7 (task 7.1, 2026-08-20)

*Read-only. This table is the gate on Phase 7 and was written before any Phase 7 code.*

#### The virtual surface a behaviour module inherits

| Member | Where | Contract |
|---|---|---|
| `OnUpdate(int deltaTime)` | `OVT_BaseBehaviorDeploymentModule.c:98` | **`super.OnUpdate(deltaTime)` FIRST, always.** The base's body re-applies the behaviour to any group that spawned since the last update (`GetManagedGroups()` + `HasBehaviorApplied()`); skipping super silently strips that from every group registered after activation. |
| `OnActivate` / `OnDeactivate` / `OnCleanup` | `:72` / `:85` / `:288` | Also chained. `OnCleanup` cancels the deferred collection (below) — an override that forgets `super.OnCleanup()` leaves a queued call pointing at a dead parent. |
| `BuildVirtualPlan(vector)` | `:15` | "What waypoint plan should a group be REGISTERED with." Null = no opinion. ⚠ A repair module must answer **null**: the insertion module needs its own movable march plan to walk the team onto the base, and `OVT_TEST_Init_ObjectiveSabotage` case A refuses a patrol module in the sabotage config for exactly this reason. Not overriding it is how you say null. |
| `CountAliveRegisteredMembersWithin(centre, radius)` | `:152` | Counts through the **virtualization record**, never through agents — a dormant group reports zero agents and is still alive. |
| `NearestPlayerDistance(position)` | `:205` | `float.MAX` when nobody is connected, so an empty dedicated server is never "contested". Players only, deliberately. |
| `RequestDeploymentCollection(reason)` | `:248` | Idempotent; logs; `CallLater(CollectParentDeployment, 0, false)` — **one frame deferred**. `CollectParentDeployment` (`:269`) calls `manager.CollectDeployment()` (not `Delete`), so a force still at full strength is refunded. |
| `m_ParentDeployment`, `GetControllingFaction()`, `GetDeploymentPosition()` | `OVT_BaseDeploymentModule.c:4,96,87` | Protected. `m_ParentDeployment` is null on a bare `new` module, which is what makes the pure statics reachable from the test tiers. |

**The one-frame deferral is not padding.** `DestroyDeployment()` clears `m_aActiveModules` and then `delete GetOwner()` while `UpdateDeployment()` is mid-`foreach` over a local list of *weak* references into those very objects. `OVT_ReinforcementBehaviorDeploymentModule` gets away with deleting inline only because every config authors it **last**. A behaviour that ends its own mission is never last — it must run before the reinforcement module or the reinforcement module rebuys the force in the same pass that decided the mission was over.

#### `CloneModule()` — hand-copy, unchained, silent on omission

`OVT_BaseDeploymentModule.CloneModule()` (`:116`) spawns by `Type()` and calls `CopyTo()`, whose base body **does nothing**. Every shipped module overrides `CloneModule()` and assigns each attribute by hand (`OVT_BaseSabotageBehaviorDeploymentModule.c:637`, `OVT_ReinforcementBehaviorDeploymentModule.c:77`, `OVT_BaseControlConditionDeploymentModule.c:72`, `OVT_InsertionSpawningDeploymentModule.c:2516`). A dropped line does not warn, does not log and does not fail to parse — it ships the **class default** on every deployment forever. Runtime state (latches, counters, armed timers, target lists) must NOT be copied: a clone belongs to a different deployment and has done nothing yet.

#### How a `m_bDirectorOnly 0` config is picked

```
OVT_DeploymentManagerComponent.EvaluateFactionDeployments(faction)      OVT_DeploymentManager.c:1140   (every 30 s)
  pool        = m_mFactionResources[faction]
  available   = OVT_DeploymentSelection.SpendableResources(pool, objective reserve)
  candidates  = FindDeploymentCandidates(faction)                       :1297
                  → union of every EVALUATOR-SELECTABLE config's location mask
                  → GetBasePositions(faction) :1383 returns ONLY bases where baseData.faction == faction
  per candidate, sorted by jittered threat:
      config = FindBestDeploymentConfig(position, faction, threat, available)   :1721
      if (!config.m_bFreeAtGameStart && candidate.threatLevel < MIN_LOCAL_THREAT_TO_DEPLOY /*5*/) skip   :1227
      if (HasExistingDeploymentOfType(position, faction, name))                skip                       :1266
      if (available >= config.GetTotalResourceCost())  CreateDeployment(...)
  stops at MAX_DEPLOYMENTS_PER_EVALUATION (10) per pass
```

`FindBestDeploymentConfig` filters the registry by `IsValidConfig()` → `IsSelectableByEvaluator()` (`!m_bDirectorOnly`) → `CanFactionUse` → `CanUseLocationType` → cost → `CheckDeploymentConditions` (the config's own condition modules **plus** `m_iMinimumThreatLevel`) → `m_iMaxInstances` → an `m_fChance` roll, then hands the survivors to `OVT_DeploymentSelection.SelectNextConfigIndex(names, priorities, alreadyHere)`.

`SelectNextConfigIndex` (`OVT_DeploymentSelection.c:55`): **lowest `m_iPriority` wins**, strictly-less-than so a tie keeps the FIRST entry, i.e. ties resolve to **registry order**; configs the position already holds (name-scoped 250 m dedup) are skipped; `NOTHING_TO_BUY` (-1) when nothing is missing. This makes `m_iPriority` the **order of acquisition at one place** — a base buys one concern per pass, cheapest priority first.

#### The base-defense numbers a new BASE config has to sort behind

All nine are `OCCUPYING_FACTION` / `BASE` / `m_iBaseCost 20` / `m_bDirectorOnly 0`.

| Config | `m_iPriority` | `m_fChance` | `m_iMaxInstances` | `m_bFreeAtGameStart` | `m_iMinimumThreatLevel` |
|---|---|---|---|---|---|
| Base Garrison Patrol | **1** | 100 | -1 | 1 | 0 |
| Base Defense Positions | **2** | 100 | -1 | 0 | 0 |
| Base Tower Guards | **2** | 100 | -1 | 1 | 0 |
| Base Sniper Positions | **2** | 100 | -1 | 0 | 0 |
| Base Checkpoints | **3** | 100 | -1 | 0 | 0 |
| Base Fortifications | **4** | 100 | -1 | 1 | 0 |
| Base Heavy Patrol | **5** | 100 | -1 | 0 | 10 |
| Base AT Section | **6** | 100 | -1 | 0 | 20 |
| Base Parked Vehicles | **10** | 75 | -1 | 1 | 0 |

*(For reference, the director-only objective configs sit at 3 — Objective Sabotage, `m_bDirectorOnly 1` — and never reach the evaluator at all.)*

**→ 10 is the number to beat.** Anything above it is acquired after every base-defense concern, which is D16's "priced to sort behind the base-defense configs so it never starves a defense".

#### Two registry-wide test interactions, checked before authoring

- `OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions` walks **every** registry config but `continue`s when the config authors no `OVT_CompositionSpawningDeploymentModule` (`:79`). A repair config that builds no composition is skipped — so it must **not** author one.
- `OVT_TEST_Init_ObjectiveOperations_DirectorConfigsAreNotEvaluatorCandidates.CheckNothingElseIs()` fails any registry config marked `m_bDirectorOnly 1` that the director does not send. `m_bDirectorOnly 0` skips that walk entirely.
- `OVT_TEST_InitSuite`'s escalation-ladder case probes a position `FindClearPosition()` chose to be clear of every town, base and tower, so a `BASE`-only config is never suitable there.

---

## Important Decisions

The plan's **D1–D18** (`implementation.md` §5) are the decision record and are not duplicated here. This file records decisions **made during the build**.

### BD1 — The Bunkers component is a RETYPE of the inherited instance, not a second component (2026-08-20, task 1.3)
Chosen because vanilla proves a delta can re-declare an inherited component's instance GUID under a subclass (S4 evidence), and because it leaves exactly one damage manager on the entity, which removes the S4 coexistence question from that prefab entirely. Pre-planned fallback if the Workbench load rejects it: fresh GUID + `SCR_DestructionMultiPhaseComponent "{5E76C88E922E8914}" { Enabled 0 }` beside it.

### BD2 — `m_fDamageThresholdMaximum 50000` is authored on both probes; it is a THIRD D18 lever (2026-08-20, task 1.5)
D18 names two levers (no hit zone; high `m_fBaseHealth`/`m_fPhaseHealth`). Neither is sufficient on its own, because `SCR_DestructionMultiPhaseComponent.OnDamage:302-325` short-circuits the whole health ladder when a single hit exceeds `m_fDamageThresholdMaximum` (class default **3000**) — one satchel would ruin a 200000-health structure. 50000 is vanilla's own number for a fortification-sized destructible (`BrickGate_01_base.et:29`). **Phase 4 must author it on the other six.**

### BD3 — Ruin models are authored as bare `.xob`, not as `…_Ruin.et` prefabs, unless the prefab carries its own inline `MeshObject` (2026-08-20, task 1.5)
`GetModelAndRemapFromResource` does not walk prefab inheritance (see the 1.5 note above), so a `_Ruin.et` whose mesh lives on its `_base` resolves to an empty model path and the mesh change silently does nothing. This narrows plan §3.10a's "prefer the prefab" preference to "prefer the prefab only when it has its own `components { MeshObject { … Materials … } }`".

**AMENDED 2026-08-20 (task 4.5), and the amendment is the load-bearing half:** the test is the `MeshObject`'s own **`Object` line**, not the presence of a `MeshObject`. `FuelTank_02_green_Ruin.et` has an inline `MeshObject` carrying only a `Materials` override — `GetModelAndRemapFromResource` reads an empty `Object`, builds a valid remap and **returns true**, so the caller proceeds with an empty model path and the swap silently does nothing. All four real ruins in this feature failed the amended test and are authored as bare `.xob`.

### BD4 — An empty cached intact model means "clear the object", not "do nothing" (2026-08-20, task 1.1)
`ChangeModel` is overridden so `ResourceName.Empty` becomes `owner.SetObject(null, string.Empty)`. Required by composition roots that carry no model of their own (the Bunkers case); without it those structures would keep the ruin mesh through a repair.

### BD5 — `RuinIt()` refuses when no damage phase is authored (2026-08-20, task 1.1)
`GoToDamagePhase(1)` on a prefab with no `m_aDamagePhases` is "past the final phase", which **deletes the entity** (`:176-183`). The guard turns a mis-authored prefab into a logged refusal instead of the exact bug this feature exists to remove.

### BD6 — The component's `[Attribute]`s live on the component, not on `…ComponentClass` (2026-08-20, task 1.1)
`SCR_DestructionTireComponent` does the same inside this family, and it makes the values readable as plain members. Authoring in a `.et` is identical either way, and the defaults live in script so no prefab has to author FX (plan §D5).

### BD7 — The temporary admin commands duplicate the admin gate inline rather than extracting a helper (2026-08-20, task 1.6)
`OVT_AdminCommandsComponent` inlines `SCR_Global.IsAdmin` + log + `AdminCommandRefused` in every handler, and task 1.6 asks for the `RpcAsk_GiveMoney` shape exactly. Phase 6 rewrites these two handlers anyway; extracting a shared gate is that phase's call, not this one's.

### BD8 — When a prefab authors no sound config, the component falls back to vanilla's own destruction bank (2026-08-20, task 2.3)
The plan asked for one `[Attribute] ref SCR_AudioSourceConfiguration`, but a `ref` class attribute cannot carry script-side defaults, so an unauthored prefab would be silent — and D5's whole point is that no prefab has to author FX. `ResolveAudioConfiguration()` therefore builds one on first use from `{5B79C73C52E6A74A}Sounds/Destruction/Multiphase/Destruction_Multiphase.acp` (the bank `Prefabs/MP/MPDestructionManager.et:18` uses) with event name `SOUND_MPD_` + the prefab's inherited `m_eMaterialSoundType`, which is exactly how `SCR_DestructionUtility.PlaySound` names its events. An authored config still wins. Nothing about this touches `SCR_MPDestructionManager` — only its sound bank's resource name is borrowed, so D6 holds.

### BD9 — The admin commands drive the facade, not the component (2026-08-20, task 2.1)
`FindNearestStructure()` now returns the structure `IEntity` and the handlers call `OVT_StructureDamage.IsRuined/Ruin/Repair`; the component-walking helper written in Phase 1 was deleted rather than left as a second copy of `Resolve()`. Phase 6 rewrites these handlers anyway, but leaving two resolvers would have let them drift in the meantime.

---

### BD10 — The round-trip suite gained a THIRD persistence seam: re-apply ONE entity's record (2026-08-20, task 3.3)
`ReapplyLatestSaveData()` asks the persistence system for exactly one instance, the game mode entity, so it can never restore a buildable — which is why the `FuelDepot_LevelSurvivesSave` case is a documented save-only degradation. A buildable is its own tracked instance, so `OVT_PersistenceManagerComponent.ReapplyEntitySaveData(IEntity)` asks for it by name (vanilla's own precedent for a non-game-mode instance is `SCR_SpawnLogic.c:309-314`, re-applying a live PlayerController's data). Both re-applications now share one body and one diagnostic; the gate class exposes the new one as `RequestInstanceReload()` on the same terms as the other two seams. Without it these two cases would prove only that a ruin survives in memory, which the Init tier already proves. **The FuelDepot case can be promoted to a full round trip with the same seam whenever `economy/fuel` next comes up.**

### BD11 — `RestorePhase()` defers exactly one frame when the owner has no model yet, then takes what is there (2026-08-20, task 3.2)
The intact model can only be learned from the owner's own `VObject`, and it must be learned BEFORE the ruin mesh replaces it — a structure restored as a ruin has no other record of what to repair to (`GetOriginalResourceName()` is empty for it, plan §3.2). A save can be applied while the entity is still assembling, so a restore that finds no model gets one deferral through the call queue. Exactly one: an empty model is legitimate for a composition root (BD4), so waiting for one to appear would hang the Bunkers case forever. Rejected: caching in the constructor (no model yet), and an authored `[Attribute] ResourceName` for the intact model (the same "one fact, two homes" objection recorded in the Phase 1 gotchas — it stays the fallback if 1.U2 shows the cache losing a race).

### BD12 — A phase change also switches the structure's vanilla SUPPORT STATIONS (2026-08-20, task 4.6)
Hiding a structure's own user actions is not enough to make a ruin inert, because a support station is reached from the **consumer's** side too: a vehicle parked at a wrecked fuel depot finds it through the station registry and refuels, whatever the depot's own action list says. `RpcDo_ApplyPhase` therefore calls `ApplySupportStationState()` on the authority for all three entry points (ruin, repair and a save restore), which walks the owner and one level of children and calls `SCR_BaseSupportStationComponent.SetEnabled()`.

That is vanilla's own server-side switch, it broadcasts itself, it is a no-op when the state already matches, and `IsValid()` refuses a disabled station from either direction. It covers the depot's `SCR_FuelSupportStationComponent`, the ramp's repair and salvage bays and the medical tent's aid station in one place. **`OVT_FuelUtils.FindBestFillSource` and `HasFillSourceInRange` already skip `!station.IsEnabled()`**, so both the client's offer and the server's re-derivation in `OVT_FuelRequestComponent` fall out for free — which is what makes 4.2's "a ruined depot refuses refuelling" true rather than merely hidden.

Two honest limits, neither this feature's to fix:
- **A repair re-enables every station it finds.** None of the eight buildables authors one disabled (`m_bIsEnabled` defaults to 1), so this round-trips exactly today; remembering which were disabled would mean holding component references across a phase change.
- **`SetEnabled` has no JIP replication of its own** — it is an RPC, not a property. A client that streams in an already-ruined depot has `m_bIsEnabled` true locally and can be *offered* a support-station action; the server-side `IsValid()` on execution still refuses it. That is a vanilla limitation on a vanilla switch.

Rejected: subclassing each vanilla station to add a `CanBeShownScript`-style gate (four subclasses and four prefab retypes for one boolean), and gating `OVT_FuelUtils` alone (it would fix fuel and leave repair, salvage and heal working on a wreck).

### BD13 — The tents and the Helipad take DEBRIS-PILE meshes, not either of §3.10a's two named fallbacks (2026-08-20, task 4.5)
§3.10a named `Rubble_Ruin_01_V2` and, as "a lower-profile alternative", `DebrisPile_Concrete_01_Medium` — and flagged the tents and helipad as "the honest problem" because brick rubble where canvas stood will not read. The tents took `DebrisPile_Wood_01.xob` from that same authored-filler family instead: a collapsed timber frame is the right *material* for a tent, it is a single self-contained mesh, and it needs no new art. The Helipad took the plan's own low-profile suggestion, because its intact model is a flat ground pad.

The remaining risk is **scale, not material**, and it is a play-test verdict rather than a source question — a `DebrisPile_*` is authored as filler and a tent footprint is ~5 × 8 m. Both swaps are one line if 4.U2 says no; the escalation path §3.10a set (a Resource Browser session with the user, never new art) is unchanged.

### BD14 — The shared phase-0 gate is `OVT_StructureDamage.IsUsable()`, and it resolves from the ROOT PARENT (2026-08-20, task 4.6)
One helper on the existing facade rather than a new `OVT_RuinGate` class: there is no state, and every caller already has the facade in scope.

The root-parent walk is the load-bearing part. A surface routinely sits somewhere other than the entity that carries the phase — the recruitment tent's actions are on a **table child**, and the maintenance ramp's shop/menu surfaces are on a bare root whose phase lives on a **child**. `IsUsable()` goes up to the root and lets `Resolve()` come back down one level, which covers both directions with one call and no per-caller knowledge. It answers **true** for null and for everything that is not a retrofitted structure, so a gate never has to ask "is my owner even a buildable".

Where it is applied, and why each surface got the treatment it did:

| Surface | Gate | Why this one |
|---|---|---|
| `OVT_FillFuelAction` | `CanBeShownScript` | Its other two contexts are vehicles, where `IsUsable()` is always true |
| `OVT_RecruitFromTentAction` | `CanBeShownScript` (was a bare `return true`) | Owner is the tent's table child — the root walk is what makes it work |
| `OVT_BuyEquippedRecruitAction` | `CanBeShownScript` | Same table, same tent |
| `OVT_HealAction` | new `CanBeShownScript` | **Beyond the plan's 4.6 list**, but it is an action on a retrofitted prefab and D15 says a ruin is inert |
| `OVT_OpenStorageAction` / `OVT_LoadStorageAction` / `OVT_UnloadStorageAction` | `CanBeShownScript`, then `super` | No buildable carries storage today (every box is a placeable), so this changes nothing until `core/storage` gives one — which is exactly the seam I8 asks to be left intact |
| `OVT_MainMenuContextOverrideComponent` | new `IsOwnerUsable()`, read by `CanShow()` **and** by both find-filters (`OVT_MainMenuContext.FindOverride`, `OVT_EconomyInfo.FindOverride`) | The menu is opened from a sphere query, not from an action — there is no `CanBeShownScript` to override, so the gate went where the query already decides |
| `OVT_ParkingComponent.GetParkingSpot()` | refuses | The one choke point every consumer shares (vehicle purchase, fast travel, parked-vehicle deployments); gating each caller would have been five copies |
| `OVT_ShopTransactionComponent.ResolveShop()` | returns null + `WARNING` | **Server handler**, per the "no per-frame show check ⇒ gate the handler" rule. It is the only hook the three shop RPCs share, and all three already null-guard it |
| `OVT_VehicleRequestComponent.RpcAsk_BuyVehicle()` | refuses + `WARNING` | **Server handler**, inline because that one resolves its shop itself. Necessary as well as belt-and-braces: its parking failure path falls back to `SpawnVehicleNearestParking`, so gating parking alone would still have sold the vehicle |
| every vanilla support station | `SetEnabled(false)` | BD12 — no script of ours runs on those actions at all |

**Deliberately NOT gated:** `SCR_CheckFuelAction` on the depot. It reads a fuel level and changes nothing; the fuel is genuinely still in the tank and comes back on repair, so "check fuel on a wreck" is arguably correct and certainly harmless. Recorded so it is a decision rather than an oversight.

### BD15 — All eight prefabs author `RplComponent { Enabled 1 }` explicitly, even where it was already inherited (2026-08-20, task 4.8)
Only the Bunkers was actually broken (S5). The other seven either inherited an active component (`Building_Base.et:25-27`) or declared one already. Authoring it anyway on the tents, the depot, the ramp root and the Helipad makes the 4.8 sweep a **grep** instead of an eight-chain inheritance walk, matches what the Guard Tower and Garage already did, and re-declares the same instance GUID with the same value — the mechanism vanilla itself uses.

### BD16 — `GetRepairCost()` answers -1 for "cannot be priced", and only BUILDABLES are repairable (2026-08-20, task 5.4)
`GetStructureCost()` answers `UNKNOWN_STRUCTURE_COST` (a deliberately huge sentinel) for anything no config claims, which is right for sabotage's cheapest-first ordering and catastrophic as a price - it would quote a million dollars. `GetRepairCost()` therefore returns **-1**, and both consumers treat a negative as "no repair here": the action does not draw itself, and the manager refuses. The placeables half of the cost join is deliberately NOT reachable from it - a placeable has no ruined phase to come back from, so `FindBuildableForEntity()` (buildables only) is the join the price uses.

### BD17 — The success notification needed a new BROADCAST MESSAGE PRESET, not just a loc key (2026-08-20, task 5.4)
`OVT_NotificationManagerComponent.SendTextNotification()` takes a TAG and looks it up in `Configs/overthrowBroadcastMessages.conf`; a raw `#OVT-…` key sent as a tag matches nothing and the notification silently never appears. So the preset `RepairedStructure` → `#OVT-RepairStructure-Repaired` was added there (icon `repair` from the wrapper imageset, the same one the maintenance ramp's map marker uses). That file is a hand-authored config, not a localization export, so editing it does not touch the "never edit the `.conf` exports" rule.

### BD18 — The repair action hides itself when the price is unknown, rather than showing an unpriced label (2026-08-20, task 5.6)
The plan's 5.6 says `CanBeShownScript` → ruined. It is ruined **AND priced**. Two states have no price: a ruin the buildables config does not claim (unrepairable at all - the server would refuse the request), and a client that has not yet read the config stream and therefore has no `repairCostMultiplier`. Showing the action in either state means drawing a label with no number in it and a hold that ends in a server refusal. Affordability is emphatically NOT part of this gate - a player who cannot pay still sees the action greyed out with `#OVT-CannotAfford`, which is the whole point of the local gate. The second state clears itself within one 1 s cache TTL.

### BD19 — The action is mounted on the ROOT of every prefab, including the ramp whose phase is on a child (2026-08-20, task 5.7)
`ResolveRoot()` + `OVT_StructureDamage.Resolve()` is the same up-then-down walk `IsUsable()` uses (BD14), so the action does not care where the component lives. Mounting on the root is what makes the RplId sent to the server the root's - which is the entity the server prices by prefab name and the entity `FindBuildableForEntity()` can join. A ramp action mounted on the destructible child would send the CHILD's RplId, and the child's prefab (`RampVehicle_01_metal.et`) is in no buildables entry, so every repair would be refused as unpriceable.

### BD20 — The Medical Tent got a SECOND action context rather than sharing the heal context (2026-08-20, task 5.7)
Its existing `UserActionContext "default"` is `Offset 0 1.8 0` / `Radius 0.65` - a point inside the standing tent, sized for leaning over a casualty. A ruin is a debris pile on the ground and that context would be almost unreachable at it, so a `"repair"` context (`Offset 0 1 0`, `Radius 5`) sits beside it in the same `ActionsManagerComponent`. The Fuel Depot's existing `"default"` context (`Offset 1.3 1.3 0`, `Radius 3`) was reused as-is: it is already sized for standing beside the tank.

### BD21 — A ruin's storage is closed by the SAME gate as everything else, and no shipped buildable is a container (2026-08-20, task 6.3)
§3.8/D15 as built. There is no storage-specific mechanism: `OVT_StructureDamage.IsUsable(GetOwner())` in `CanBeShownScript`, exactly as the recruit, heal and fuel actions do it, so a container on a ruin stops offering its actions and starts again on repair. **Contents are untouched** — a phase change swaps a mesh and nothing else, which is why the whole gear answer is "verify and don't break it" rather than a save field.

The sweep behind it, re-run in Phase 6: every user action in the tree that touches a storage or an inventory (`OpenInventory` / `SetStorageToOpen` / `SetLootStorage` / `InventoryStorageManagerComponent` — 13 files), plus a walk of all eight buildable prefab chains to their vanilla roots looking for `SCR_UniversalInventoryStorageComponent`, `InventoryItemComponent` or a storage manager. Results:

| Action | State | Why |
|---|---|---|
| `OVT_OpenStorageAction` / `OVT_LoadStorageAction` / `OVT_UnloadStorageAction` | gated in 4.6 | The three Overthrow container actions |
| `SCR_OpenStorageAction` (modded) | **gated in 6.3** | The GENERIC container action — it appears on anything carrying a storage, so it is the first one a buildable container would show. The only code change this task needed |
| `SCR_LootAction`, `SCR_OpenVehicleStorageAction`, `OVT_LootIntoVehicleAction`, `OVT_SellVehicleCargoAction` | not gated | Bodies, wrecks and vehicles. None is ever a buildable, and `IsUsable()` would answer true for all of them anyway — the gate would cost a per-frame root walk to change nothing |
| `OVT_LoadLoadoutAction` / `OVT_SaveLoadoutAction` / `OVT_SaveOfficerLoadoutAction`, `OVT_GunDealerAction`, `OVT_ShopAction`, `OVT_SellDrugsAction`, `OVT_DeliverMedicalSuppliesAction`, `OVT_DialogUserAction` | not gated | Authored only on characters, vehicles and PLACEABLE boxes/cabinets (grep of `Prefabs/`+`PrefabsEditable/`). A placeable has no ruined phase. The shop half is gated server-side anyway (BD14) |

**Zero of the eight buildables carries a storage component anywhere in its chain**, so today this decision changes nothing a player can see. That is the point: the gate is in place before the container arrives, and case D proves it works on a container that does not ship.

### BD22 — The two seams §3.8 leaves are recorded and deliberately NOT built (2026-08-20, task 6.5)
- **`logistics/storage`** (the feature moved out of `core` on 2026-08-20; docs now at `docs/features/logistics/storage/`) is building an item LEDGER component — type + quantity as data, not spawned entities — for the warehouse, placed boxes and vehicles. When a buildable gains one, **nothing here needs changing**: the ledger is component state on an entity that a phase change never deletes (case B and case D both assert that), and its "Open Storage" action gates on `IsUsable(GetOwner())` like every other surface. The one thing that feature must not do is put the ledger on a CHILD of a buildable and then gate on the child's own phase — `IsUsable()` asks the root parent for exactly this reason (BD14), and case D is built in that shape on purpose.
- **`occupying/counter-attacks`' `IsGearContainer()` exclusion** (`OVT_BaseSabotageBehaviorDeploymentModule.c`) keeps gear containers off the sabotage target list. It is **unreachable on shipped data** — every box is a placeable and placeables were dropped from sabotage candidacy — and this feature does not change that. Whether a buildable container should be sabotage-exempt, sabotage-able but repairable, or lootable by the sabotage team is the occupying epic's call **once a buildable container exists**; the honest answer today is that the question has no subject.

### BD23 — Repair reads the SABOTAGE difficulty fields; no new difficulty field was added (2026-08-20, task 7.6)
Plan §3.7's own table says a repair detail "holds the same interval" as a sabotage team, and an existing field already says it: `objectiveSabotageHoldSeconds` runs **180 s on Easy → 60 s on Insane**, and `objectiveSabotageStructuresPerMission` runs **1 → 3**. Both ladders move the right way for repair (harder = the occupying faction recovers faster and takes more back per detail), so a second pair of fields would be two more numbers to keep in step across six presets with nothing ever able to disagree with them. `ResolveIntervalTicks()` / `ResolveStructuresPerMission()` therefore read the sabotage fields, and `OVT_TEST_Init_ObjectiveRepair` case D pins that join so a future split has a second place named for it.

**If the `occupying` epic ever does want them decoupled:** add `objectiveRepairHoldSeconds` / `objectiveRepairStructuresPerMission` to `OVT_DifficultySettings`, author them in all six `Configs/Difficulty/*.conf`, and point the two resolvers at them. ⚠ **They would NOT need the JIP stream.** Every `objective*` field is server-only — `OVT_OverthrowConfigComponent.RplSave/RplLoad` (`:714-770`) writes the wanted timers, the prices, the QRF knobs and the item limits and **none** of the objective-director fields — so `CONFIG_STREAM_VERSION` would not move. Nothing on a client reads a repair interval; only the server's deployment tick does. (Contrast D12's `repairCostMultiplier`, which *did* have to move the version, because the client draws the price in the action label.)

### BD24 — The balance numbers are provisional, and they are chosen to bound the EMPTY-BASE case rather than the repairing one (2026-08-20, task 7.4)
Plan §3.7 hands `m_iBaseCost`, `m_iCostPerGroup`, priority, `m_fChance` and `m_iMaxInstances` to the `occupying` epic and asks this plan for conservative values. These are them, with the arithmetic behind each:

| Field | Value | Why |
|---|---|---|
| `m_iPriority` | **15** | Strictly behind Base Parked Vehicles (10), the last base-defense config — D16's "priced to sort behind the base-defense configs so it never starves a defense". `m_iPriority` is the ORDER OF ACQUISITION at one place, so a base finishes defending itself before it repairs anything. 15 rather than 11 leaves room for the base-defense ladder to grow. |
| `m_iMaxInstances` | **1** | **The real throttle.** One repair detail alive per faction at a time, map-wide. This is what bounds the empty-base case below, and it also matches what a repair detail is: maintenance, not a campaign effort. |
| `m_fChance` | **100** | ⚠ Deliberately NOT used as the throttle, and the arithmetic is the reason. The roll is made per candidate per pass, so with N held bases the probability that *some* base rolls it is `1-(1-c)^N`; getting a useful map-wide rate out of it would need a per-base chance so low that a base with a real ruin would wait tens of minutes. `m_iMaxInstances` throttles the map without punishing the one base that actually needs the detail. |
| `m_iBaseCost` | **20** | The figure every one of the nine base configs uses. Not refunded on collection (it is the admin cost). |
| `m_iCostPerGroup` | **30** | One `light_fireteam`, which both shipped factions field at registry cost 15. Cheaper than a sabotage team's 40 `specops_team`, because putting a tent back up is not a special-forces job. **Refunded in full** when the detail is collected at full strength. |
| `m_iTruckCostOverride` | **0** | A repair deployment is created AT a base the faction holds, so `OVT_NearestControlledBaseSourceProvider` resolves that same base, the separation is zero, it is under `m_fWalkThresholdDistance 400` and the detail always walks. Budgeting for a transport that is structurally never spawned would be a receipt for nothing. |

**What the empty-base case actually costs**, since on shipped data it is the COMMON case (see the trigger-surface note below): a base with every cheaper-priority config already standing buys a detail, the detail holds the base for one interval (120 s on Normal), finds nothing ruined, reports "there was nothing left to repair", and is collected with its group at full strength. Net cost **20 resources per ~130 s, map-wide** — the group's 30 comes back. That is the price of not having a ruin-presence check.

**The proper fix, named and not built:** a condition module that asks whether the location has any ruined buildable, so the config is never created where there is no work. It would be a new `OVT_BaseDeploymentModule` class and a new sphere query on the creation path, which is framework surface owned by `occupying/counter-attacks` rather than by this feature; plan §3.7 explicitly assigns these numbers to that epic. Until it exists, `m_iMaxInstances 1` is the bound.

### BD25 — The config deliberately authors NO `OVT_NoPlayersNearbyConditionDeploymentModule` (2026-08-20, task 7.4)
Base Garrison Patrol authors one ("Never fortify a base a player is standing in") and copying it here would have looked like good hygiene. It would also have made the feature untestable: **7.U1's whole procedure is a player standing at a base, ruining two structures with an admin command, and waiting for the detail** — a no-players condition refuses that outright, at creation, with no log line aimed at the tester. The behaviour that condition is really protecting (a squad spawning on top of a player) is already covered from the other side: `m_fClearRadius 150` pauses the work while a player is inside it, so a player who stays put sees the detail arrive and *wait*, which is legible, rather than never seeing one at all. The plan's task 7.4 lists the four modules to author and this is not one of them.

**⚠ The same 150 m circle is the top trap in the play-test.** Ruin the structures, then withdraw past 150 m. Standing there watching is indistinguishable from the feature not working.

### BD26 — The deployment is named "Base Repair Detail" while its file keeps the plan's `Deployment_ObjectiveRepair.conf` path (2026-08-20, task 7.4)
The plan mandates the path; the *name* is what the registry, the Game Master panel, the dedup and every log line use, and calling it "Objective Repair" would have said the one thing D16 exists to deny — that it is a director objective. It is not: it is `m_bDirectorOnly 0` maintenance in the same family as the nine `Base *` configs, so it is named like them. ⚠ **Consequence: this is the only config in `Configs/Deployment/` whose file name does not match its deployment name.** Somebody grepping for `Deployment_BaseRepairDetail.conf` will not find it.

### BD27 — `SetDamagePhase()` is the single funnel for the effects and the support stations (2026-08-20, play-test fix)
Play-test found that GM **"Neutralize"** ruined a structure with no explosion, no sound and its support stations still working. The path never touches our code: `SCR_EditableEntityComponent.Destroy()` → `SCR_DamageManagerComponent.Kill()` (`:536-550`, TRUE damage = MaxHealth) → `SCR_DestructionMultiPhaseComponent.OnDamage()` (`:284`, which does **not** call super) → the `m_TotalDestruction` branch (`:297`, `:306-316`) = `GoToDamagePhase(lastPhase, false)` + `ReplicateDestructibleState(lastPhase, silent: true)`. `RuinIt()` and `RpcDo_ApplyPhase()` are bypassed entirely, so the same held for any weapon kill.

The fix moves both side effects off our RPC and onto an override of the protected **`SetDamagePhase(int)`**, which every phase change on every machine reaches: it is the last thing `GoToDamagePhase` does (`:211`) after the same-phase guard (`:186-187`), and `GoToDamagePhase` is the only writer of the phase in the base class (`SCR_DestructionTireComponent:225` is a different class). So the funnel is reached by our `RuinIt`/`RepairIt`/`RestorePhase`, by vanilla's weapons path, by vanilla's own broadcast receiver on the client (`RPC_ReplicateMultiPhaseDestructionState` calls `GoToDamagePhase(phase, true)` even when `silent`) and by the JIP `OnRplLoad`.

`previous == 0 && new > 0` → `OnBecameRuin()`: stations off (server) + `RaiseEffects()` (unless suppressed). `previous > 0 && new == 0` → `OnBecameIntact()`: stations on (server) + `StopRuinEffects()` (everywhere).

**Suppression** (`m_bSuppressEffects`, set around the drive and cleared after) is what keeps a load silent: `RestorePhase()`, an overridden `OnRplLoad()` wrapping super, `RuinIt(withEffects: false)`, and the client side of `RpcDo_ApplyPhase` when `withEffects` is false. `RpcDo_ApplyPhase` no longer raises effects or touches stations at all — its whole body is now the client's phase drive — so nothing double-fires. Its arity is unchanged (two primitives).

### BD28 — The ruin effect is an explosion + debris one-shot plus a RETAINED fire and smoke (2026-08-20, play-test fix)
`Building_Explosion_Smoke.ptc` was the wrong asset: it is vanilla's collapse **dust**, authored to be played with `m_fParticlesMultiplier 0.2–0.5` and `m_bSnapToTerrain 1` (`Configs/Destruction/Building_FX_Particle/*.conf`). Played unscaled at the bounding-box centre it read as a smoke grenade. The effect set is now:

| Attribute | Default | How it plays |
|---|---|---|
| `m_ExplosionParticle` | `{EEAC86461B982EE4}Particles/Props/Explosion_Generic.ptc` | one-shot, `PlayParticleEffect_CompleteDestruction` at the bbox centre |
| `m_DebrisParticle` | `{6D89EA548ABDDF25}…Building_Explosion_Debris_Brick.ptc` | same |
| `m_FireParticle` | `{4D5CD8B2B5DE8916}Particles/Vehicle/Vehicle_fire_engine_medium.ptc` | retained handle, follows the owner at the **bbox centre** (corrected — see below) |
| `m_GroundFireParticle` | `{A9259561960FD620}Particles/Vehicle/Vehicle_fire_ground_medium.ptc` | retained handle, **unparented, world position, terrain-snapped** (corrected — see below) |
| `m_SmokeParticle` | `{3F7B398D4D154CC9}Particles/Vehicle/Vehicle_smoke_damaged_medium_01.ptc` | retained handle, parented at the **bbox centre** |
| `m_fFireSeconds` / `m_fSmokeSeconds` | 120 / 600 | `CallLater(StopRuinFire/StopRuinSmoke, s * 1000)` |

The retained pair follows `SCR_FlammableHitZone` exactly — `ParticleEffectEntitySpawnParams { FollowParent = owner; PlayOnSpawn; UseFrameEvent; DeleteWhenStopped; Transform[3] = local offset }`, and the timer + `SCR_ParticleHelper.StopParticleEmissionAndLights(handle)` **is** the lifetime mechanism (a `ParticleEffectEntity` has no lifetime field). `StopRuinEffects()` stops both and drops the pending `CallLater`s; it runs from `OnBecameIntact()` (so a repair puts the fire out) and from `OnDelete`. All of it is behind `if (System.IsConsoleApp()) return;`.

Per BD6 the attributes stay on the component, so a prefab overrides them: **`OVT_FuelDepot.et` authors `m_ExplosionParticle "{69766E33781FC23F}Particles/Logistics/Explosion/Fuel/Explosion_Fuel_Large.ptc"`** and takes the defaults for everything else.

#### Correction, 2026-08-20 (play-test fix #3): no fire was visible

Explosion and smoke read correctly; the fire did not exist on screen. Cause, from the vanilla source: `Vehicle_fire_ground_medium.ptc` is vanilla's **`m_sBurningGroundParticle`**, and `SCR_FlammableHitZone.StartDestructionGroundFire()` (`:604-660`) spawns it **unparented, in world space, with `Transform[3][1] = GetSurfaceY(x, z)`** and `UseFrameEvent` only. The wreck flame players actually see is a different asset — **`m_sBurningParticle` = `Vehicle_fire_engine_{medium,big}.ptc`** — and that one is the `FollowParent` + local-offset spawn (`UpdateFireEffects()` `:678-737`, `Vehicle_Base.et:118-120`, `Wheeled_Truck_Base.et:20`).

We had spawned the flat ground pool as a `FollowParent` emitter at local `vector.Zero`. Most Overthrow buildables carry their origin at the **foundation**, i.e. at or under the visible floor, so the pool rendered inside the ruin mesh/terrain. Smoke was visible because it alone sat at the bbox centre — the diagnosis is therefore **position, not the emitter params** (diagnosis (a)/(d)/(f); (b), (c) and (e) were checked and cleared: vanilla sets no `MultParam`/`SetParam` to gate visibility, `m_fFireSeconds * 1000` = 120000 is a fine `CallLater` delay, and nothing reaches `StopRuinEffects()` on the ruin path).

Now:

* `m_FireParticle` defaults to `Vehicle_fire_engine_medium.ptc` and spawns through `SpawnAttachedEffect()` at the **bounding-box centre** (same params as vanilla `UpdateFireEffects`) — same helper as the smoke, differing only in resource.
* `m_GroundFireParticle` is a second, cheap handle spawned through `SpawnGroundEffect()`: `TransformMode = ETransformMode.WORLD`, no parent, position = world bbox centre with `y = GetSurfaceY(x, z) + 0.2`, skipped entirely when the structure's world bbox min is more than `GROUND_FIRE_HEIGHT_TOLERANCE` (1 m) above the surface. Vanilla's repeating `CallLater(StartDestructionGroundFire, 1000, true)` retry loop is **not** ported: it exists to wait for a *physics wreck* to come to rest, and a buildable never moves.
* One `StopRuinFire` timer covers both handles; `StopRuinEffects()`, the repair/delete paths, the 120 s lifetime, the headless guard and the `[Attribute]` overridability are unchanged.

All four GUIDs re-verified against the reference tree (`Prefabs/Vehicles/Core/Vehicle_Base.et:118,120`, `Wheeled_Truck_Base.et:20`, `Helicopter_Base.et:65`).

**Human verification owed:** ruin a Guard Tower and a low buildable (tent/ramp) and confirm a flame is visible from ~50 m and from directly alongside, that the ground pool is not clipping through the ruin mesh, that both stop at 120 s while the smoke keeps going to 600 s, and that a repair extinguishes them instantly. If `Vehicle_fire_engine_medium` reads too small on the larger structures, `{93CAB5E5559B5C32}…_big.ptc` is the drop-in swap.

### BD29 — The destruction sound needs its two signals, and the MP destruction manager DOES exist (2026-08-20, play-test fix)
`RaiseSound()` mirrored `SCR_DestructionUtility.PlaySound()` (`:54-88`) but stopped one step short: the `Destruction_Multiphase.acp` bank resolves its events through two signals, and without them nothing audible came out. Between `CreateAudioSource` and `PlayAudioSource` it now sets `SCR_AudioSource.PHASES_TO_DESTROYED_PHASE_SIGNAL_NAME` = `GetNumDamagePhases() - GetDamagePhase() - 1` and `SCR_AudioSource.ENTITY_SIZE_SIGNAL_NAME` = `SCR_DestructionUtility.GetDestructibleSize(owner)` (names at `SCR_AudioSource.c:37-38`).

`ResolveAudioConfiguration()` now also **prefers `SCR_MPDestructionManager.GetInstance().GetAudioSourceConfiguration()`** when the manager is up, rewriting its event name per play the way vanilla does, and keeps our own config as the fallback. D6's rule is unchanged: a null manager is silent-fallback, never a fault — which is the whole reason we do not simply call `PlaySound()`, since the manager is 500 ms (10 s dedicated) late.

### BD30 — The ruin sound is a vanilla big-explosion bank layered over the material break (2026-08-20, play-test fix)

BD29 made the ruin audible but it was still only the `Destruction_Multiphase.acp` per-material **BREAK** event: a close-range structural crack, not a blast. The demolitions this feature exists for happen with **no player inside 150 m** and are meant to be heard hundreds of metres to a couple of km out, so the requirement is a loud distant thud with a bearing.

Candidates found in the vanilla tree (there is no `Sounds/` directory in the extract, so every one of these is evidenced by a prefab reference):

| bank | event | how vanilla plays it |
|---|---|---|
| `{E4EF3755472EC669}Sounds/Particles/Logistics/Explosion/TNT/Particles_Explosions_TNT_Large.acp` | `SOUND_EXPLOSION` | `Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Large.et:20` — `SpawnParticleEffect.SoundEvent`, 45 kg charge |
| `…/TNT/Particles_Explosions_TNT_Medium.acp` / `_Small` | `SOUND_EXPLOSION` | same, smaller warheads (`Cinematic_Explosion.et` uses Small) |
| `{9D94BEBA8AA8A997}…/Fuel/Particles_Explosions_Fuel_Large.acp` | `SOUND_EXPLOSION` | `Explosion_Fuel_Large.et:20` (200 kg) **and** `FuelTank_03_base.et:31-36` as an `SCR_AudioSourceConfiguration` on `SCR_TimedSound`, flags 7 |
| `…/Ammo_Rack/Particles_Explosions_Ammo_Rack_{Large,Medium,Small}.acp` | `SOUND_EXPLOSION` | the vehicle ammo-rack secondaries |
| `…/Battery/Particles_Explosions_Battery.acp` | `SOUND_EXPLOSION` | small vehicle secondary |
| `Sounds/Destruction/Buildings/Destruction_Buildings_{Brick,Metal,Wood}_Generic.acp` | `SOUND_BUILDING_DESTRUCTION` | `SCR_DestructibleBuildingComponent`'s `SCR_TimedSound` — a **collapse rumble**, not a blast |
| `Sounds/Weapons/Explosives/DemoBlocks/_SharedData/Weapons_Explosives_DemoBlock_Generic.acp` | (unknown — plays through `SoundComponent`/`HitEffectComponent`, no event name in the prefab) | `Warhead_ExplosiveCharge_M112.et` |

**Chosen: `Particles_Explosions_TNT_Large.acp` / `SOUND_EXPLOSION`**, with the **Fuel_Large** bank overridden on `OVT_FuelDepot.et` next to its fireball particle. Reasons: it is the *satchel-charge* blast, which is the fiction; the vanilla precedent for playing it exactly the way we do — `SCR_AudioSourceConfiguration` → `SCR_SoundManagerModule.CreateAudioSource(owner, config, worldPos)` → `PlayAudioSource` — is `FuelTank_03_base.et`'s own destruction, so no projectile, warhead or `SoundComponent` is needed; and the demo-block bank's event name is not discoverable from the extract. The building-collapse banks were rejected as the wrong character (and they are what the break layer already approximates).

Code shape: two attributes, **`ResourceName m_ExplosionSoundProject` + `string m_sExplosionSoundEvent`**, not a `ref SCR_AudioSourceConfiguration` — BD8, a `ref` class attribute cannot carry a script-side default, and this one must work with no prefab authoring. The config is built once at first ruin with **flags 7** (`Static | EnvironmentSignals | FinishWhenEntityDestroyed`), matching `FuelTank_03_base.et`. `RaiseSound()` keeps the funnel and the headless guard, resolves the bbox centre once and plays **two layers**: `PlayExplosionSound()` first, `PlayMaterialBreakSound()` (the BD29 code, signals and all) underneath it. The break layer is kept rather than dropped — it is quiet enough not to muddy the blast and it is the only thing that says "a structure", and it costs nothing when a bank is missing (both bails are silent).

**Range and volume are the bank's own authoring — there is no script-side override.** `SCR_AudioSourceConfiguration` exposes only project/event/offset/flags, and `AudioSystem.PlayEvent` takes no gain. The one thing script contributes is the **`Distance` signal**, which `SCR_SoundManagerModule.CreateAudioSource` sets from `AudioSystem.IsAudible()` on every path — that is what selects the bank's far layers/tails — plus the four `EnvironmentSignals` (sea/forest/houses/meadows) set in `PlayAudioSource`. So "pick the loudest vanilla big-explosion bank" *is* the volume control, and TNT_Large is the largest generic one that is not fuel-specific.

---

## Gotchas & Learnings

### Found in Phase 1 (2026-08-20)

- 🔴 **A composition root has no model, and a mesh swap on it ADDS geometry instead of replacing it.** The Bunkers prefab is the case (full evidence in the 1.5 note above). Check `GetVObject()`/the `MeshObject` chain before assuming any prefab can carry a phase model.
- 🔴 **`GetModelAndRemapFromResource` does not walk prefab inheritance** (`Global/Functions.c:1446-1509`), so an `.et` in `m_PhaseModel` whose `MeshObject` sits on a parent resolves to an empty path and the mesh change silently does nothing.
- 🔴 **`GoToDamagePhase(N)` with `N >= GetNumDamagePhases()` deletes the entity** (`:176-183`) — not just when `m_bDeleteAfterFinalPhase` is 1. A prefab with the component but no authored phase is one call away from deleting itself.
- ⚠ **The plan's Q4 gate greps the literal string `NavmeshRebuild` in `Scripts/Game/Components/Damage/`, so even a COMMENT mentioning `OVT_NavmeshRebuild.Queue()` fails it.** The D7 rationale in the component header is worded to avoid the token deliberately — do not "fix" it back.
- ⚠ **The intact-model cache is learned at runtime, so it depends on `OnPostInit` running before `OnRplLoad`.** `CacheIntactModel()` refuses to learn while the phase is non-zero, so the worst case is a client that streamed in a ruin never learning the intact model and clearing the object on a repair rather than restoring the wrong one. If 1.U2 shows that, the fix is `SCR_DestructionTireComponent`'s: an authored `[Attribute] ResourceName` for the intact model. Deliberately NOT added yet — an unused authored attribute would give the same fact two homes.
- ℹ **`super.GoToDamagePhase(0, …)` does everything phase 0 needs except the mesh** — including cancelling a pending `ChangeModel`. Calling super and then adding only the `CallLater` is both shorter and safer than `SCR_DestructionTireComponent`'s hand-rolled `ReturnToInitialDamagePhase`.
- ℹ **A delta can retype an inherited component to a subclass, keeping the instance GUID** (S4 evidence). Useful well beyond this feature.
- ℹ **`SCR_DestructibleBuildingEntity` is an empty class at runtime** — its whole body is behind `#ifdef ENABLE_BUILDING_DESTRUCTION`, which nothing defines. It is a `Building` subclass and a `ClassName()` string, nothing more.

### Found in Phase 2 (2026-08-20)

- ⚠ **The Q4 gate greps `Scripts/Game/Utilities/OVT_StructureDamage.c` for the literal `NavmeshRebuild` too**, and the first draft of its header failed it by *naming* the facade it is modelled on. Same trap as the component header — describe it, do not spell it.
- ℹ **A `ref` class `[Attribute]` cannot carry a script-side default**, unlike a `ResourceName` or a primitive. Anything that must work without prefab authoring needs a code-built fallback (BD8).
- ℹ **`SCR_SoundManagerModule.CreateAudioSource(owner, config, worldPosition)` answers null for an inaudible or unknown event** rather than logging, so "no sound" is free of error spam — which is what lets the config be optional at all.
- ℹ **The Init tier can spawn prefabs** (`OVT_Global.SpawnEntityPrefab` is used a dozen times in `OVT_TEST_InitSuite.c`), so entity-level mechanisms do not have to wait for the Persistence tier. What it cannot assert is anything deferred through the call queue — the ruin/repair MESH change is scheduled, so only the phase is a safe subject.

### Found in Phase 3 (2026-08-20)

- 🔴 **The persistence round-trip suite's reload seam covers the GAME MODE ENTITY ONLY.** Any case whose subject is a world entity with its own record — a buildable, a placeable, a vehicle — reloads nothing through `RequestSessionReload()`, and will pass vacuously if it asserts a value that was never dirtied. Use the new `RequestInstanceReload()` (BD10).
- ⚠ **Persistence registration is asynchronous, so a freshly built structure has no stored record for a few frames.** Both new cases wait on a tracked-instance check before they save; without it the save can legitimately contain nothing and the failure names the wrong half.
- ℹ **A version guard cannot be exercised in-process here.** The suite resets the save state before every run, so no version 1 payload exists in CI. The source-level exercise (flip the writer to a v1 payload, compile, confirm the reader stops, revert) was done; the real one is a **Continue on a campaign saved before this phase**, and it is folded into 3.U1.
- ℹ **The phase is written for every buildable, retrofitted or not** — binary contexts are positional, so a version 2 payload always carries the int and a non-destructible structure simply writes 0.

### Found in Phase 4 (2026-08-20)

- 🔴 **BD3's rule was not strong enough, and the Fuel Depot is the case that proves it.** `FuelTank_02_green_Ruin.et` carries its own inline `components { MeshObject { Materials … } }` — which is what BD3 said to look for — but **no `Object` line**; the mesh is on `FuelTank_02_Ruin_base.et:4-6`. `GetModelAndRemapFromResource` (`Global/Functions.c:1478-1505`) finds the `MeshObject`, reads an empty `Object`, builds a perfectly valid remap string and **returns true**, so `ChangeModel` would proceed with an empty model path. The rule is: *a `…_Ruin.et` is only usable as `m_PhaseModel` when that prefab carries its own `Object` line inside its own `MeshObject`* — a `Materials`-only override is worse than useless, because it looks like a hit.
- ⚠ **Entities nested in a prefab's children block are real hierarchy children even with NO `Hierarchy` component.** The maintenance ramp's destructible child has no `Hierarchy` and neither does its tyre pile — but the tyre pile's `coords -2.044 0.877 -0.706` are plainly local, so the parent link exists and `GetChildren()` returns them. `Hierarchy` controls pivot attachment and auto-transform, not parentage. This is what `OVT_StructureDamage.Resolve()`'s child walk depends on, and the new Init case spawns the real ramp prefab to keep it honest.
- ⚠ **The `.et` files in this tree have no trailing newline after the final `}`.** A patch anchored on `…}\n` silently matches nothing at end-of-file.
- ⚠ **`OVT_VehicleRequestComponent.RpcAsk_BuyVehicle` falls back to `SpawnVehicleNearestParking` when the shop's own parking refuses a spot** (`:576-585`). Gating `OVT_ParkingComponent` alone would therefore have sold the vehicle anyway and parked it somewhere else — the handler needed its own refusal.
- ℹ **`IEntity.FindComponents(typename, notnull array<Managed>)` exists** (`Core/generated/Entities/IEntity.c:539`). `FindComponent` answers the first only, and the ramp child carries two support stations, so the plural is mandatory there.
- ℹ **`SCR_BaseSupportStationComponent.SetEnabled()` is public, server-side and self-broadcasting**, `IsEnabled()` defaults to true from an attribute, and `IsValid()` refuses a disabled station from either direction. It is the only switch in the support-station system reachable from outside, and Overthrow's own `OVT_FuelUtils` already honours it (`:267`, `:307`).
- ℹ **`SCR_EMaterialSoundTypeBreak` has a `BREAK_TENT` member** (`SCR_DestructionUtility.c:375`), used by 32 vanilla prefabs.
- ℹ **`Prefabs/Editor/Components/Default_RplComponent.ct` is an empty body**, so the Helipad's `RplComponent` was already active — unlike the Bunkers' `DestructionMultiPhase_Rpl_Base.ct`, which explicitly ships `Enabled 0`. A `.ct` preset name tells you nothing; open it.
- ℹ **`OVT_FuelUtils.FindFuelSourcesCovering` / `FindFuelSourcesNear` do NOT filter on `IsEnabled()`** — deliberately, they answer "what fuel is near here". They have no live consumer today (only the Logic suite), but a future high-command tick using them would see a ruined depot as a fuel source.

### Found in Phase 5 (2026-08-20)

- 🔴 **`SCR_AutotestCaseBase.SetFailure(string, …)` takes at most THREE parameters** (`SCR_AutotestCaseBase.c:88`), unlike `string.Format`'s nine. A four-parameter call compiles clean under a `string.Format` habit and then drops the fourth silently - wrap the message in `string.Format` when a failure needs four.
- ⚠ **A notification TAG is not a localization key** - see BD17. `SendTextNotification("#OVT-Something")` looks up a preset named `#OVT-Something`, finds nothing, and returns without a word.
- ⚠ **`Math.Round`'s tie behaviour is undocumented** (`Core/generated/Math/Math.c:30-40` shows only 5.3 and 5.8), and the shipped ladder contains a real tie: 1500 × 1.5 × 0.75 = 1687.5, i.e. the Vehicle Maintenance Ramp and the Helipad on Hard. The Logic suite asserts that rung as a two-value range and every other rung exactly; do not "tighten" it without evidence.
- ℹ **The buildables/placeables configs are loaded on EVERY machine** (`OVT_ResistanceFactionManager.OnPostInit` → `LoadConfigs()`), which is what lets the client-side action price a repair from the real config instead of a replicated copy. Only the difficulty multipliers have to ride the JIP stream.
- ℹ **`OVT_DifficultySettings` fields that are not authored in a preset fall back to the `[Attribute]` defvalue**, so `Difficulty_Normal.conf` authoring no `buildableCostMultiplier` still means 1. The new multiplier is authored in all six confs anyway, so nothing in the ladder depends on that.

### Found in Phase 6 (2026-08-20)

- ⚠ **`SCR_InventoryAction.CanBeShownScript` returns false whenever the owner has no `InventoryItemComponent`** (`SCR_InventoryAction.c:30-33`, `m_Item` is set from the owner at `:96`). Every storage action in the tree inherits it, so on a static container the vanilla half of the answer is already false and a `super`-calling gate cannot be tested by "the action is visible before, invisible after". Case D asserts the ruined answer is false AND that the post-repair answer returns to whatever the pre-ruin answer was, which is true either way.
- ⚠ **An item inside a storage is a hierarchy child of the container**, so a test that deletes the container and the item separately can delete the item twice or leave the storage holding a dead entity. Case D cleans up with `SCR_EntityHelper.DeleteEntityAndChildren()` on the container and never touches the item.
- ℹ **`ActionsManagerComponent.GetActionsList()` + a direct `CanBeShownScript()` call is a legitimate way to test a user action's visibility rule** — vanilla does it itself (`SCR_AIShootStaticArtillery.c:35`). It is the only way to exercise the real action instance rather than a copy of its rule.
- ℹ **The reference tree ships no `.meta` files, so a prefab GUID can only be taken from an existing reference in the mod's own tree** — and some of those references carry a STALE path beside a live GUID (`CivilianClothes.conf:18` names `Prefabs/Items/Equipment/Maps/PaperMap_01_folded.et`, which no longer exists at that path). Case D uses `Radio_R148.et`, whose GUID and path pair is confirmed live in all six difficulty presets.

### Found in Phase 7 (2026-08-20)

- 🔴 **The evaluator only ever offers a faction bases that faction HOLDS.** `OVT_DeploymentManagerComponent.GetBasePositions()` (`:1383`) skips `baseData.faction != factionIndex` outright, added 2026-08-20 the same day the threat rescale made a captured base systematically top-of-sort. So an evaluator-selectable `BASE` config is *structurally* confined to friendly ground before its `OVT_BaseControlConditionDeploymentModule` is ever asked — the condition module is the belt to that braces, not the mechanism. (The director's own operations are unaffected: `ForceCreateDeployment()` never asks for a candidate list, which is how sabotage still reaches enemy bases.)
- 🔴 **`m_fChance` is a per-candidate roll, not a per-pass one**, so it does not throttle a map. With N eligible locations the chance that *something* is bought in a pass is `1-(1-c)^N`; the only knobs that bound a map-wide rate are `m_iMaxInstances` and how long a deployment lives. See BD24 — this is why the repair config ships at chance 100.
- ⚠ **A deployment that stands itself down FASTER can cost MORE.** With `m_iMaxInstances 1` the instance slot is the throttle, so a mission that discovers it has nothing to do and completes on its first update frees the slot for the next evaluator pass 30 s later, whereas one that discovers it at the end of its hold interval blocks re-purchase for two minutes. An "obvious" early stand-down was prototyped and dropped for exactly this: it would have roughly quadrupled the empty-base spend.
- ⚠ **A `.conf` fault is invisible to `tools/compile-check.sh`.** It compiles EnforceScript; a misspelled `m_sDeploymentName`, an inverted `m_bRequireControl` or a deleted registry entry all leave it at exit 0 with the same file count. All seven config-side fault injections recorded in `OVT_TEST_Init_ObjectiveRepair` case A are therefore proofs that the fault is SILENT, not proofs that the tool caught anything — the Init case is the only thing that can catch them.
- ⚠ **`OVT_InsertionSpawningDeploymentModule` needs a source provider that does not depend on the objective director.** Every shipped insertion config uses `OVT_ObjectiveAnchorSourceProvider`, because every one of them is director-only. An evaluator-selectable config using it would resolve nothing whenever no objective was running and register nobody, silently. `OVT_NearestControlledBaseSourceProvider` (`OVT_DeploymentSourceProvider.c:131`) is the director-free answer, and case A asserts the anchor provider is *not* used.
- ℹ **A zero-separation insertion is a supported outcome, not a degenerate one.** `OVT_NearestControlledBaseSourceProvider`'s own header says so: a deployment created at a base its faction holds resolves that base as its source, and the walk threshold then sends the force in on foot from where it already is. That is why the repair config can budget `m_iTruckCostOverride 0` honestly.
- ℹ **`OVT_ObjectiveSelection.NextTargetIndex(costs, null)` is a general cheapest-first picker**, not a sabotage-specific one — same ragged-input refusal and first-wins tie rule as `SelectBestIndex`, downward. Reusing it is what keeps "cheapest first" a single rule with one home.
- ℹ **The Logic tier's reviewer grep bans the manager accessor's NAME anywhere under `TestSuites/Logic/`, comments included.** A Logic case about difficulty precedence therefore cannot even *mention* how the live value is fetched; splitting the precedence into a pure `(fallback, campaign)` static is what makes the claim expressible there at all, with the live wiring asserted one tier up.

Pre-loaded from the plan — traps every implementing agent must already know:
- `m_bDeleteAfterFinalPhase` defaults to **1** — must be authored `0` in all eight prefabs (D4, Q5 greps for eight).
- `GoToDamagePhase(0, …)` never restores the mesh; `GetOriginalResourceName()` is read by nothing and is empty for a structure loaded as a ruin — cache the intact model in `OnPostInit` (plan §3.2, hole 1).
- Vanilla's `ReplicateDestructibleState` cannot send a repair; its receiver takes the FX-only branch for phase 0 — use our own broadcast RPC (hole 2, D3).
- The engine never loops a broadcast back to the sender — the server invokes `RpcDo_ApplyPhase` locally **and** `Rpc()`s it (BUG-090 class). `Rpc()` arity is a compile-check blind spot.
- **Do not add `OVT_NavmeshRebuild.Queue()`** to the ruin/repair path — `GoToDamagePhase` already regenerates the navmesh (D7). The existing `Queue(owner)` line in the serializer stays first.
- **Never change line 1 (root class) of any prefab** — eight script files filter on `ClassName() == "SCR_DestructibleBuildingEntity"` (plan §3.10).
- `SCR_DestructionUtility.PlaySound` hard-returns without `SCR_MPDestructionManager` — sound goes through `SCR_SoundManagerModule` (D6). ⚠ **Corrected 2026-08-20 (BD29):** the manager *does* exist in an Overthrow session (`OVT_OverthrowGameMode.et:279` spawns it 500 ms / 10 s in), so the reason we bypass `PlaySound()` is only the startup window — and the two bank signals it sets are mandatory either way.
- Loc keys go in the `.st` master only — never the `.conf` exports.

---

## Needs human verification

*User-gated items the automated spine cannot reach. Each phase's acceptance lists them; tick here with the date and result.*

- [x] ✅ **Phase 1** — Workbench loads the Bunkers and Guard Tower probe prefabs clean. ⚠ The Bunkers one is the real test: it is the **class retype** (BD1). A rejection shows up here and nowhere else.
- [x] ✅ **Phase 1** — Listen host + one client: `/ruin-structure` swaps the mesh on both machines; `/repair-structure` puts it back on both. Also, while there: **(a)** walk into the repaired Guard Tower — collision can be destroyed at ruin time and never rebuilt (S1); **(b)** empty a magazine and throw a grenade at each probe — neither should be ruinable by fire (S6); **(c)** expect the ruined Bunker to show rubble *inside* intact sandbags — that is the known composition finding (1.5), not a mechanism failure.
- [x] ✅ **Phase 2** — Listen host + one client: a sabotage team demolishing a retrofitted structure produces explosion, smoke, sound and a ruin on both machines; the per-mission notification fires exactly once.
- [x] ✅ **Phase 3** — Listen host save → quit → **Continue** brings a ruin back as a ruin with no explosion and no sound. ⚠ Do it **first on a campaign saved before this phase**: that save carries version 1 buildable payloads, and loading it cleanly (structures intact, ownership and base association unchanged) is the only real exercise of the serializer's version guard.
- [x] ✅ **Play-test fix (2026-08-20, BD30)** — **the blast carries.** Ruin a structure with a spotter posted **≥ 500 m away** (and again at ~1 km): they should hear a deep explosion thud and be able to point at it, not a faint crack. Confirm the Fuel Depot's blast reads as a fuel detonation, and that the material break underneath does not muddy it (if it does, the fix is to drop the `PlayMaterialBreakSound()` call — BD30).
- [x] ✅ **Play-test fix (2026-08-20, BD27-29)** — Game Master **"Neutralize"** on each of the eight structure types produces the **full** effect: an explosion (a fuel fireball on the Fuel Depot), a debris shower, an audible destruction one-shot, a ground fire that burns ~2 minutes and a smoke column that lasts ~10 minutes; the structure's support stations stop working (park a vehicle at a neutralized depot/ramp and confirm refuel/repair is refused from the vehicle's side). Then `/repair-structure`: the fire and smoke stop immediately and the stations come back. Also confirm a **loaded** save and a JIP client streaming in a ruin are still silent and smokeless.
- [x] ✅ **Phase 4 (4.U1)** — **Workbench loads all eight retrofitted prefabs clean.** This is the only gate that can catch a mis-typed attribute, a dropped component or a GUID collision, all of which are silent.
- [x] ✅ **Phase 4 (4.U2)** — Listen host + one client, `/ruin-structure` and `/repair-structure` on each of the eight, correct mesh both ways. While there, five judgements only a human can make:
      **(a) scale and ground alignment of each of the six new ruins** — nothing floating, sunk or wildly the wrong size;
      **(b) do the two tents read as collapsed tents?** They use a WOOD debris pile (BD13) and the risk is that it is too small for a 5 × 8 m footprint. Pre-approved swap: `Rubble_Ruin_01_V2.xob`;
      **(c) the Helipad's ruin is a low mound on a bare patch** by design — is that acceptable, or does a flat pad want something wider?
      **(d) the Fuel Depot's ruin will NOT be green** — the green remap lives only in `FuelTank_02_green_Ruin.et`, which BD3 forbids us from using. Judge whether the default-material wreck is fine;
      **(e) THE RAMP IS THE REPLICATION QUESTION.** Its component and its `RplComponent` are on a CHILD entity, so it is the one structure whose broadcast has to cross the wire from a child of a composition. If any of the eight shows the mesh on the host and not on the client, expect it to be this one.
- [x] ✅ **Phase 4 (4.U2, cont.)** — With a **vehicle parked beside a ruined Fuel Depot**, its own refuel/fill actions must be gone too, not just the depot's (BD12). Same check for the ramp's repair action on a wrecked ramp and the medical tent's heal on a wrecked tent. All three must come back on repair.
- [x] ✅ **Phase 5 (5.U1)** — Dedicated server: a client repairs a ruin, is charged exactly the server-side price (label == charge), and the structure returns intact for every connected player. Also: repair with insufficient funds (greyed out, no hold possible), and a repair attempted from 100 m away (refused with a server log line).
- [x] ✅ **Phase 5 (5.U1, cont.)** — **The hold ring on each of the eight ruins.** Six prefabs gained a brand-new `ActionsManagerComponent` whose `Offset`/`Radius` were sized to the RUIN mesh on paper (0 1 0 with radius 4-8 depending on footprint; the Garage takes 8, the Bunkers and Guard Tower 4). Nothing automated can stand next to a ruin, so "can I actually reach the action, and does it run for the authored 20 s" is a human check per structure. The two that reuse an existing manager are the Fuel Depot (the fill context, radius 3) and the Medical Tent (a second `"repair"` context, radius 5).
- [x] ✅ **Phase 6 (6.U1)** — Dedicated server: an admin ruins/repairs any of the eight from chat (`/ruin-structure`, `/ruinstructure`, `/repair-structure`, `/repairstructure` — all four aliases), and a **non-admin** is refused with the `AdminCommandRefused` notification on their screen and a `WARNING` line in the server log. The refusal half is the one nothing automated can reach: `SCR_Global.IsAdmin()` answers true for the local player of an offline session, so a listen host cannot produce a non-admin caller either. While there: a placed ammo box standing beside a ruin (not parented to it) must KEEP its storage action — the gate asks the root parent, and an unparented box is its own root.
- [x] ✅ **Phase 7 (7.U1)** — Listen host: at a base the OCCUPYING faction holds, `/ruin-structure` two structures of DIFFERENT price (e.g. a bunker at 750 and a garage at 8000 — the cheap one must come back first). Then:
      **🔴 (a) WALK AWAY. More than 150 m from the base centre, and stay there.** `m_fClearRadius 150` is both "the detail counts as holding the base" and "a player pauses the work"; a tester standing at the base sees the detail arrive and then do nothing, which is indistinguishable from the feature being broken (BD25).
      **(b) Expect a wait before anything is bought at all.** The evaluator runs every 30 s, the config sits at priority **15**, and `FindBestDeploymentConfig` only offers it once the base already holds every cheaper-priority base-defense config it is eligible for; the base also has to score at least `MIN_LOCAL_THREAT_TO_DEPLOY` (5). Watch the log for `Deployment created` naming **"Base Repair Detail"**.
      **(c) Then one structure per hold interval** — 120 s on Normal, `objectiveSabotageHoldSeconds` (BD23) — cheapest first, up to `objectiveSabotageStructuresPerMission` (2 on Normal), then `[Overthrow] Base repair detail finished after N structure(s)`.
      **(d) The common outcome at a base with NOTHING ruined is a detail that arrives, waits one interval and reports "there was nothing left to repair".** That is correct behaviour, not a bug — see the trigger-surface note in §3.7 and BD24. It is worth watching once deliberately, because it is what will happen most of the time in a real campaign.
      **(e) `m_iMaxInstances 1`** — only one repair detail exists map-wide at a time. Ruining structures at two bases will fix them one base after the other, not at once.
- [x] ✅ **Phase 8 (8.U1)** — **Workbench re-export of the localization `.conf` files after the `.st` edits.** Eleven strings are affected: the ten new `OVT-FieldManual_Ruins_*` keys and the rewritten value of the existing `OVT-FieldManual_CounterAttacks_Text3`. Until the export runs, the new Field Manual page draws raw keys and the Counter Attacks page still reads the old "gone for good, no rubble, no repair" paragraph in game.
- [x] ✅ **Phase 8 (8.U1, cont.)** — **The three surfaces agree.** In game, the Field Manual's *Ruins and Repair* page and the *Counter Attacks* page must say the same thing about a demolished structure, and the price wording (half on Easy/Normal, three quarters on Hard, full at Extreme/Insane) must match what the repair action actually charges on the preset being played. The wiki is the third surface and is **not published yet** — see below.
- [x] ✅ **Phase 8 (8.3)** — **The wiki sync is owed and could not be attempted.** No `mcp__wikijs__*` tool was exposed to the Phase 8 session, so nothing was written and nothing was invented. Paste-ready text: `docs/features/core/damage/wiki-draft.md`. ⚠ `occupying/counter-attacks` T10.3 is owed on the same page set; do both in one pass.
- [x] ✅ **Final** — the 11-item dedicated-server MP play-test in `implementation.md` §7.

---

_All items above confirmed green by the user's play-test on 2026-08-20 (closed)._

## Testing Approach

Per `implementation.md` §7. Every phase leaves `tools/compile-check.sh` at exit 0; **the orchestrator alone runs `tools/run-tests.sh`** once per completed phase (`.claude/test-policy.md`).

| Tier | Suite | Phase |
|---|---|---|
| Init | `OVT_TEST_Init_StructureDamage.c` | 2, 4 |
| Persistence | `OVT_TEST_PersistenceRoundTripSuite.c` (two new cases, + gear/identity case) | 3, 6 |
| Logic | `OVT_TEST_Logic_RepairPricing.c` | 5 |
| Init | `OVT_TEST_Init_RepairSeam.c` | 5 |
| Init | `OVT_TEST_Init_ObjectiveRepair.c` | 7 |
| Logic | `OVT_TEST_Logic_ObjectiveRepair.c` | 7 |

The Init file now holds four cases: **A** (null and a plain prop are refused cleanly), **B** (one probe goes intact → ruined → intact with an unchanged identity, and is reachable through a parent), and **C** — `OVT_TEST_Init_StructureDamage_CEveryBuildableIsRetrofitted`, which reads the LIVE buildables config and round-trips **every** prefab it lists. C is the cheap catch for a silently dropped component and the first automated test of both the Bunkers retype (BD1) and the ramp's child-mounted component. **D** — `…_DGearSurvivesAPhaseRoundTrip` (Phase 6) — assembles the container-carrying buildable nothing ships (a Guard Tower with an `OVT_AmmoBox_Placed` parented to it, one item inside) and asserts the contents survive both halves of the round trip while the storage ACCESS closes for the duration.

**Phase 7 adds six cases across two tiers.** `OVT_TEST_Init_ObjectiveRepair.c`: **A** the config (registered, valid, BASE, evaluator-selectable, priority behind the base-defense ladder, module order, `m_bRequireControl 1`, no objective/patrol/composition module, a non-anchor source provider, a real group prefab for both US and USSR), **B** the target filter (an INTACT structure, another base's, a resistance-held base's, a camp's, a forward base's and an unassociated one are all refused), **C** the hold interval pauses and never resets, **D** clone fidelity plus the LIVE difficulty precedence. `OVT_TEST_Logic_ObjectiveRepair.c`: **A** the full decision ladder, **B** the interval/quota precedence and their floors.
⚠ Init C and Logic A overlap on one claim (pause-not-reset) deliberately — the plan's Phase 7 acceptance names it as an initialisation-tier claim, and it is the one a player would notice breaking.

House rules: recorded proof-it-can-fail preamble per case; **no `maxAttempts`**; no case reads another's state; no float comparisons.

**Baseline before Phase 1:** not yet taken — the orchestrator records the current Fast/All counts here before the first gate run.

---

## Open Questions

- [x] **Q:** Can the inherited `SCR_DestructionMultiPhaseComponent` on Bunkers be re-declared as the `OVT_` subclass in a delta, or must ours be a second component? (task 1.3)
      **A:** **It can, and it was.** Vanilla does exactly this in the damage-manager family — `Turret_Base.et:66` (`SCR_DamageManagerComponent "{51ACD09C524A7924}"`) → `Tripod_Base.et:70` / `Mortar_Base.et:122` (`SCR_DestructionMultiPhaseComponent`, same GUID). Confirmation that the retype loads is user-gated (1.U1); the fallback is written down in BD1.
- [ ] **Q:** Do the fallback rubble meshes read acceptably for the tents and helipad? (task 4.5; play-test item 4). Escalation is a Resource Browser session with the user, not new art.
      **A:** **Still open — it is a play-test verdict and nothing in the source can settle it.** What Phase 4 did do is pick better candidates than the two §3.10a named and write down the risk: the tents take `DebrisPile_Wood_01.xob` (right material, possibly too small) and the Helipad `DebrisPile_Concrete_01_Medium.xob` (right profile for a flat pad, narrower than the pad). Both swaps are one line. See BD13 and 4.U2 (b) and (c).
- [x] **Q:** How should a composition buildable whose ROOT carries no model be ruined — a per-child destruction component (vanilla's answer), a single root mesh that visually subsumes the children, or hiding the children? Bunkers is the known case; Helipad and the two tents are unchecked. (blocks task 4.5)
      **A:** **The question turned out to be much narrower than feared: only TWO of the eight are not plain root-mesh prefabs, and they take different answers.** The Helipad and both tents were checked chain by chain and all three carry their own root `MeshObject`, so a root swap replaces the model exactly as intended (evidence in the per-prefab section above). The **maintenance ramp** is a bare root whose geometry, physics, damage manager and support stations all live on one child, so the component went **on that mesh-carrying child** — the strategy is "put it where the mesh is", and `Resolve()`'s child walk makes it reachable from the root. **Bunkers** is the only true composition root and is deliberately unchanged: its three children each carry their own mesh, so vanilla's per-child answer is available (`Sandbag_01_bunker_plastic_CompositionDestruction.et:11-19`) but costs three components, three phase blocks and three GUIDs for a 750-cost buildable, and the current single-root result may well be acceptable. **Judge it in 1.U2/4.U2 first; only then spend the three components.** Hiding children was rejected outright for every case — it needs per-child visibility state with its own replication and its own save field.

---

## Session Notes

### 2026-08-20 — CLOSED (user play-test green)

- **All 10 user-gated items confirmed by the user** (1.U1, 1.U2, 2.U1, 3.U1, 4.U1, 4.U2, 5.U1, 6.U1, 7.U1, 8.U1): Workbench loads all eight prefabs clean; listen-host + dedicated MP ruin/repair/sabotage checks on both machines; save → Continue brings a ruin back silently; GM Neutralize raises FX/sound/fire; occupying 'Base Repair Detail' repairs at a held base; localization re-exported and the Field Manual reads right.
- **Four post-build play-test fixes landed the same evening** (BD27–BD30): `SetDamagePhase()` funnel so GM/weapon-driven transitions raise effects and disable support stations; vehicle-style smoke plume + explosion one-shot; `TNT_Large` / `Fuel_Large` explosion sound bank layered over the material break; visible wreck flame at the bbox centre + terrain-snapped ground pool.
- **Final gates:** compile 0 (6202 files); Fast **346/347**; All **405/406** — the only red is the pre-existing `CompositionSlotGate` case from HEAD.
- **BUG-193 filed** (built Bunkers have no active `RplComponent` on `main`; fixed forward here).
- **Sequencing:** `occupying/objectives` will move `OVT_BaseSabotageBehaviorDeploymentModule.c`, which this feature edits — commit/merge this feature first, or rebase that edit.
- Non-blocking debt carried: wiki publication (`wiki-draft.md`, wikijs MCP not connected), 'at a ruin' tutorial trigger (framework), provisional occupying repair balance numbers, tents/helipad generic rubble, FM tile image.

### 2026-08-20 — Autorun close-out (orchestrator)

**Gate history:** P1 Fast 330/331 · P2 Fast 332/333 (after the hit-zone fix) · P3 All 391/392 · P4 Fast 333/334 · P5 All 398/399 (after the inherited-ActionsManagerComponent fix) · P6 Fast 340/341 · P7 Fast 346/347 (after a test-arithmetic fix) · P8 skipped (conf/`.st`/docs only). The single persistent red, `OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions` ('Base Fortifications' builds compositions but authors no `OVT_CompositionSlotConditionDeploymentModule`), is **pre-existing on v1.5 HEAD** — neither file is on `main`; both last touched by f9b08a38/a62d7b6f, a `virtualization/base-defense-migration` / counter-attacks leftover, not this feature's. It belongs in that feature's docs, not a bug (in-dev).

**Three defects the gates caught, fixed by the orchestrator in the main thread:**
1. A damage-manager component with zero hit zones is DROPPED by the engine (`HitZoneContainers require at least one hitzone`) — the Guard Tower hit-zone block was authored and S6 corrected.
2. An entity may carry exactly one `ActionsManagerComponent` — the Guard Tower's inherited `{5A30CE5EDD9DBDB5}` is re-declared as a delta instead of adding a second.
3. An off-by-one in `OVT_TEST_Init_ObjectiveRepair_CHoldIntervalPausesRatherThanResetting`'s own arithmetic (module correct).

**BUG-193 filed (2026-08-20):** built Bunkers have no active `RplComponent` on `main` (S5) — dismantle unreachable; fixed forward on v1.5 by this feature's retrofit.

**Sequencing note:** `occupying/objectives` (requirements only, planned 2026-08-20) will later move `OVT_BaseSabotageBehaviorDeploymentModule.c`, which this feature edits (2.4/2.5) — merge/commit this feature before that work starts, or rebase its edit.

**Tech debt / owed:** wiki publication; tutorial popup needs a new trigger (framework, `new-player-experience/tutorial-content`); Field Manual entry uses the shared `default_ui.edds` tile; occupying repair balance numbers provisional; tents/helipad use generic rubble; GUID ledger next free `6B70D00000000053` (and `…0018` free).

**Git:** nothing committed; the user owns git.

### 2026-08-20 — Phase 8 built (tasks 8.1–8.4; 8.2 skipped by design)

**Gate (orchestrator, 2026-08-20):** compile-check 0 (6202 files). Suite run **skipped** — conf/`.st`/docs-only phase, the suites cover none of it (test-policy §2).


**Gate:** `tools/compile-check.sh` exit 0. `tools/run-tests.sh` deliberately not run (orchestrator only). No `.c` file was touched; the phase is conf, `.st` and docs only.

**8.1 — Field Manual.** A new `SCR_FieldManualConfigEntry_Standard "{6B70D0000000003F}"` titled **Ruins and Repair** in `Configs/FieldManual/Categories/FM_Overthrow.conf`, placed between *FOBs and Building* and *Capturing a Base* so it reads in the order a player meets the mechanic. Ten pieces (`…0040`–`…0048`), four sections: what a ruin is and that it survives a save; that a ruin is inert (with the vehicle-side support-station half from BD12, which the plan's §3.6 wording does not cover); that the gear inside survives; the 20 s repair action and its difficulty share; the occupying faction's repair detail and its 150 m pause. Tile image is the shared `default_ui.edds` — **no artwork was commissioned and none is needed for the entry to render**, but a dedicated tile would match the neighbouring entries.

**The stale surface found by the audit was `OVT-FieldManual_CounterAttacks_Text3`**, added 2026-08-19 by `occupying/counter-attacks` Phase 10, which said in as many words: *"There is no rubble, no repair, no salvage and no refund, and it does not come back when the campaign is loaded again."* Every clause of that is now false. It was rewritten in place rather than removed: the cheapest-first ordering, the per-base scope, the player pause and the once-per-mission notification in the same paragraph are all still accurate, and its `Comment` keeps the original fact-check and appends this one. **This is the only stale in-game string the audit found** — a sweep of every `Target_en_us` containing sabotage/demolish/destroyed/wreck/rubble/ruin turned up nothing else about structures. The sabotage broadcast (`"Enemy saboteurs are demolishing what you built at %1"`) is still literally true and was left alone.

**8.2 — SKIPPED, and this is the honest reason.** `OVT_TutorialEvent` (`Scripts/Game/Configuration/OVT_TutorialTrigger.c:12-45`) has fourteen values and not one of them can fire at a ruin: the closest are `PLAYER_BUILD` (a buildable *finished*), `PLAYER_ENTER_BASE` (crossing into an occupying-held base's close range) and `MENU_OPENED`. Nothing in the destruction path raises a tutorial event, and there is no proximity-to-entity trigger of any kind. Inventing a mechanism was explicitly out of scope, so no `Configs/Tutorials/*.conf` was touched and the `ActionContexts` grep-count guard did not apply. **Gap reported:** a "you are standing at a ruin" popup needs a new trigger, which is tutorial-system framework and belongs to `new-player-experience/tutorial-content`.

**8.3 — Drafted, not published.** Identical wall to `occupying/counter-attacks` T10.3: **no `mcp__wikijs__*` tool was exposed to the session at all**, not even `wikijs_connection_status`, so this is the MCP server not being connected rather than an auth failure. `docs/features/core/damage/wiki-draft.md` carries four page edits ready to paste: the sabotage-page correction, a new player-facing **Ruins and Repair** page, the `repairCostMultiplier` row on `difficulty`, and one sentence for the FOB/building page.

**8.4 — Loc.** Ten keys added to `Language/localization_Overthrow.st` only, GUIDs `{6B70D00000000049}`–`{6B70D00000000052}`, filed in Id order between `OVT-FieldManual_Resistance_Text` and `OVT-FieldManual_Shops_Head`. Braces counted before (1776/1776) and after (1796/1796) — an unbalanced `.st` is data loss on the next Workbench save. **No `Language/*.conf` export was touched.** Every body string's `Comment` carries its own claim-by-claim fact-check with file:line, which is the standing rule after two shipped tips described mechanics that did not exist.

**GUIDs taken this phase: `6B70D0000000003F`–`6B70D00000000052` (20 values). Next free: `6B70D00000000053`.** (`6B70D00000000018`, reserved in Phase 5 and never taken, is still free.)


### 2026-08-20 — Phase 7 built (tasks 7.1–7.7)

**Gate (orchestrator, 2026-08-20):** compile-check 0 (6202 files). First Fast run 345/347 — `OVT_TEST_Init_ObjectiveRepair_CHoldIntervalPausesRatherThanResetting` red because the case's own tick arithmetic was off by one (3 → 2 → pause keeps 2 → resumed tick leaves **1** → next completes; the case expected completion one tick early). `EvaluateRepair` is correct; the case was extended with the 2→1 step. Re-run **Fast 346/347**, only the pre-existing `CompositionSlotGate` red.


**Gate (orchestrator):** owed. `tools/compile-check.sh` exit 0 (6202 files). `tools/run-tests.sh` deliberately not run.

The survey (7.1) was written into this file before any code and it changed two decisions on its own:

- **`GetBasePositions()` already confines an evaluator-selectable `BASE` config to friendly ground**, so `OVT_BaseControlConditionDeploymentModule { m_bRequireControl 1 }` is a second lock rather than the only one. It is still authored, because a condition module also collects a live deployment when the base changes hands, which the candidate filter cannot do.
- **The base-defense ladder tops out at priority 10** (Parked Vehicles), so the repair config takes **15** — behind every defensive concern, with room for the ladder to grow. D16's "never starves a defense" is now a number rather than an intention.

The module is sabotage's file with three inversions and one subtraction. The subtraction is the interesting one: **there is no director in it at all** — no `OVT_ObjectiveDirectorComponent`, no success call, no objective name lookup, so the concurrent `occupying/objectives` rewrite cannot reach it. `CompleteMission()` logs and asks for collection, and that is the whole of it.

Two things were deliberately NOT built, both recorded rather than hidden:

- **A ruin-presence condition.** Nothing in the framework can ask "does this base have a ruin" at creation time, so a repair detail is bought, walks in, holds for one interval and reports "there was nothing left to repair". On shipped data that is the COMMON outcome (§3.7's honest note: a ruin only stands on occupying-held ground after the resistance built at a base, the occupying faction sabotaged it and then recaptured it — or after an admin ruins one). `m_iMaxInstances 1` bounds the cost to ~20 resources per ~130 s map-wide; the proper fix is a new condition module and it belongs to the `occupying` epic. BD24.
- **An early stand-down** on the first update when the base has no ruins. Prototyped, then dropped on the arithmetic: with the instance cap doing the throttling, standing down sooner frees the slot for the next 30 s evaluator pass and roughly quadruples the spend. Recorded in the Phase 7 gotchas because it is a genuinely counter-intuitive result.

Also of note: **no difficulty field was added.** Repair reads `objectiveSabotageHoldSeconds` / `objectiveSabotageStructuresPerMission`, which is what plan §3.7's table asks for and what the shipped ladders already say. Had a field been needed it would **not** have ridden the JIP stream — every `objective*` field is server-only and `CONFIG_STREAM_VERSION` would not have moved. BD23 spells out both halves.

All 26 recorded fault injections were made one at a time, compiled, and restored; every one exited `tools/compile-check.sh` 0, which for the seven config-side ones is a proof that a `.conf` fault is **silent to the tooling** rather than a proof the tooling caught it.

### 2026-08-20 — Phase 6 built (tasks 6.1–6.5)

**Gate (orchestrator, 2026-08-20):** compile-check 0 (6199 files). **Fast 340/341** — the container gear-survival case is green on its first run; the only red is the pre-existing `CompositionSlotGate` case from HEAD.


The smallest code phase in the feature, and most of it was verification.

- **The admin commands needed no behaviour change.** Both aliases were already registered in `RegisterChatCommands()` in the double-alias style, both handlers already had the `RpcAsk_GiveMoney` gate shape (server guard → `ResolveOwningPlayerId()` → `SCR_Global.IsAdmin` → `WARNING` + `AdminCommandRefused` → work → audit line), both already drove `OVT_StructureDamage` (BD9) and neither takes a parameter of any kind. 6.1/6.2 were therefore a **documentation** promotion: the "TEMPORARY DEBUG DRIVER — Phase 6 promotes them" banner is now a production header. Diff is comment-only; grep confirms no command in the class takes an entity.
- **6.3 found nothing broken and one thing missing.** No shipped buildable is a container (all eight chains walked to their vanilla roots), and no ungated storage action is authored on one. The generic modded `SCR_OpenStorageAction` took the same three-line `IsUsable()` gate the three `OVT_*StorageAction`s took in 4.6, because it is the action a future buildable container would show first. Everything else that touches an inventory lives on a character, a vehicle, a body or a placeable — table in BD21.
- **6.4 got the REAL container case, not the fallback.** The plan allowed a weaker invariant if nothing ships as a container; instead case D assembles one at runtime out of two real prefabs (Guard Tower + `OVT_AmmoBox_Placed` as its child, one `Radio_R148` inserted), which is the exact shape `logistics/storage` will produce. It asserts contents survive the ruin AND the repair, the container is never deleted, `IsUsable()` closes and reopens, and the real `OVT_OpenStorageAction` instance stops offering itself while ruined. Case B's identity invariant is not repeated — D asserts it about the CONTAINER instead.
- **Compile-check exit 0 (6199 files).** No `.st`, no `.conf`, no prefab touched — the phase adds no localization key and no GUID (next free is still `6B70D00000000038`).
- **Owed:** 6.U1 (the dedicated-server admin/non-admin check) and the orchestrator's suite run.

### 2026-08-20 — Phase 5 built (tasks 5.1–5.10)

**Gate (orchestrator, 2026-08-20):** compile-check 0 (6199 files). First All run **394/399** — the Guard Tower entity failed to spawn (`SCR_DestructibleBuildingEntity component ActionsManagerComponent cannot be combined with component ActionsManagerComponent`): `GuardTower_01_base.et:146` already carries `ActionsManagerComponent "{5A30CE5EDD9DBDB5}"` (ladder + door contexts) and Phase 5 added a second one. **Gotcha: an entity may carry exactly ONE `ActionsManagerComponent`; always walk the vanilla chain for an inherited one and re-declare its GUID as a delta** (Overthrow's `Prefabs/Vehicles/Core/Vehicle_Base.et:4` is the precedent — inherited arrays merge by GUID, so new contexts/actions append). Fixed by re-declaring `{5A30CE5EDD9DBDB5}` with the `repair` context + `OVT_RepairStructureAction`; re-run **All 398/399** (only the pre-existing `CompositionSlotGate` red). The other five new `ActionsManagerComponent`s are legitimate — no root-level one exists anywhere in their chains (verified by chain walk). `6B70D00000000018` is free again.


Repair exists end to end: a price, a difficulty lever that replicates, a server verb, and a held action on all eight ruins.

- **One rounding, one expression** (D11). `OVT_RepairPricing.RepairCost(rawCost, buildMultiplier, repairMultiplier)` is the only place the number is computed, and both machines call it through `OVT_ResistanceFactionManager.GetRepairCost()`. `IsRepairable()` guards the `UNKNOWN_STRUCTURE_COST` sentinel (BD16).
- **The ladder is authored, not defaulted.** All five presets plus `Difficulty_TestWorld.conf` carry `repairCostMultiplier` explicitly (0.5 / 0.5 / 0.75 / 1 / 1 / 0.5). `CONFIG_STREAM_VERSION` is 5 and the writer, the reader and the constant moved together.
- **The cost join was extracted, not copied.** `FindBuildableForEntity()` holds the prefab-name lookup and its header; `GetStructureCost()` calls it and keeps its placeables fallback, so its behaviour - including the sentinel - is unchanged.
- **One new RPC on an existing seam** (D13), no new component, no `OVT_Global` accessor. Ladder: server guard → `ResolveOwningPlayerId()` → manager → `ResolveEntity` → not-ruined → within 10 m → manager. Every refusal logs. No identity parameter; arity hand-audited (one `RplId`).
- **The action is mounted on the root everywhere** (BD19) and hides itself when it has no price (BD18). Six prefabs gained an `ActionsManagerComponent`; the Fuel Depot and the Medical Tent gained an entry (BD20).
- **Tests:** four Logic cases (the headline half/full rule, the full 8×5 shipped ladder written out by hand, rounding in both directions plus the `.5` boundary, and repairability + the never-dearer-than-a-rebuild invariant) and two Init cases (the verb resolves off the local controller and the replicated multiplier is sane; every buildables entry spawns and prices through the REAL prefab join).
- **Compile-check exit 0 (6199 files).** Static gates: identity gate clean, root-class gate clean, `grep -c repairCostMultiplier Configs/Difficulty/*.conf` = 1 in each of 6.
- **Owed:** 5.U1 (dedicated-server charge check + the hold ring on each of the eight ruins) and a Workbench re-export of the localization `.conf` files once Phase 8 adds its keys.

### 2026-08-20 — Play-test fixes: engine-driven ruins, the effect set, the sound signals (BD27–BD29)

- **Defect 1** — GM "Neutralize" (and any weapon kill) bypassed `RuinIt`/`RpcDo_ApplyPhase`, so no effects ran and the support stations stayed live. Fixed by making the protected `SetDamagePhase(int)` the funnel (BD27); `RpcDo_ApplyPhase` is now only the client's phase drive, and `m_bSuppressEffects` keeps loads/stream-ins silent.
- **Defect 2** — the smoke asset was vanilla's collapse dust and read as a smoke grenade. Replaced by an explosion + debris one-shot and a retained fire (120 s) and smoke (600 s) column (BD28). `OVT_FuelDepot.et` overrides the explosion with the fuel fireball.
- **Defect 3** — the destruction sound never set the `PhasesToDestroyed` / `EntitySize` signals the multiphase bank needs (BD29); the stale "the MP destruction manager does not exist here" claim in S3/D6 is corrected in place with a dated note.
- `tools/compile-check.sh` exit 0 (6202 files). Not run, by policy: `tools/run-tests.sh` (orchestrator only). The `OVT_TEST_Init_StructureDamage` cases all ruin with `withEffects: false`, so they stay in the suppressed path — no FX, no retained handles, no sound — and their assertions are unchanged.
- One new human-verification item added above.

### 2026-08-20 — Phase 4 built (tasks 4.1–4.8)

**Gate (orchestrator, 2026-08-20):** compile-check 0 (6195 files); `m_bDeleteAfterFinalPhase 0` = 8/8; `git diff` on Prefabs/PrefabsEditable is insertion-only. **Fast 333/334** — the new `…_CEveryBuildableIsRetrofitted` case is green on its first run (every config-listed buildable spawns destructible, not born ruined, usable→ruined/unusable→repaired/usable); the only red is the pre-existing `CompositionSlotGate` case from HEAD.


- **All six remaining prefabs retrofitted**, 12 new GUIDs (`6B70D0000000_0004`–`_000F`, each re-verified unused across the repo *and* the vanilla tree first). Every new component block carries the mandatory hit zone from the S6 correction, `m_fBaseHealth 100000`, `m_fDamageThresholdMaximum 50000` (BD2), `m_bDeleteAfterFinalPhase 0` and one authored phase. The full per-prefab table — root class, where the component sits, which manager was disabled, `RplComponent` state, ruin model and how each should read — is above.
- **The Phase-1 composition worry did not generalise.** The Helipad and both tents carry their own root `MeshObject`; only the Bunkers is a true composition root, and it was left exactly as Phase 1 authored it. The ramp is the other special case and took the mesh-carrying-child strategy, which is also what makes the new Init case worth having.
- **Ruin models:** Ramp/Garage/Depot use their real vanilla ruins as bare `.xob` (BD3, and the depot is the case that showed BD3's rule needed tightening — see the Phase 4 gotchas). Tents and Helipad take debris piles (BD13), with the scale risk written down rather than hidden.
- **One shared phase-0 gate**, `OVT_StructureDamage.IsUsable()`, resolving from the ROOT parent so a child-mounted action works (BD14). Applied to seven user actions, the menu-override component and both of its find-filters, the parking component, and two server handlers. Vanilla support stations are switched by the phase change itself (BD12), which is what actually makes a ruined depot refuse to refuel a vehicle parked next to it — `OVT_FuelUtils` already skips disabled stations on both the client's offer and the server's re-derivation.
- **New Init case `OVT_TEST_Init_StructureDamage_CEveryBuildableIsRetrofitted`** — reads the LIVE buildables config, spawns every prefab it lists, and asserts each is destructible, is not born ruined, ruins, does not delete itself, reports unusable while ruined, repairs, and reports usable again. It is the cheapest possible catch for the silent failure that already bit once (a damage manager with no hit zone is dropped whole by the engine), and a ninth buildable added without a retrofit turns it red on the first run.
- **Static gates:** `m_bDeleteAfterFinalPhase 0` = **8**; active `RplComponent` = **8/8** (now authored explicitly on all eight, BD15); root-class gate clean; `DeleteEntityAndChildren`/`NavmeshRebuild` still absent from `Scripts/Game/Components/Damage/` and `OVT_StructureDamage.c`; brace counts balanced in all eight prefabs.
- No localization change: Phase 4 adds no user-facing string. Every new refusal is a `LogLevel.WARNING` in English.
- `tools/compile-check.sh` exit 0 (6195 files). Not run, by policy: `tools/run-tests.sh` (orchestrator only). **4.U1 and 4.U2 are owed**, and 4.U2 carries five specific judgements listed under "Needs human verification".

### 2026-08-20 — Phase 3 built (tasks 3.1–3.4)

**Gate (orchestrator, 2026-08-20):** compile-check 0 (6195 files). **All group 391/392** — `…_StructureDamage_RuinSurvivesSave` and `…_RepairSurvivesSave` both green on the first run; the only red is the pre-existing `CompositionSlotGate` case from HEAD.


- `OVT_BuildableComponentSerializer` is version 2: the damage phase is appended after the existing three fields, read live off the destruction component through `OVT_StructureDamage.Resolve(owner)`, and restored silently through `RestorePhase()`. A version 1 payload — every save taken before today — reads its three fields and stops. The `OVT_NavmeshRebuild.Queue(owner)` line stays first and the header now says why.
- `RestorePhase()` orders the intact-model cache before the mesh change and tolerates a restore that arrives mid-spawn (BD11). It still broadcasts, silently, because a listen host doing a Continue can already have clients connected.
- Two round-trip cases, and they are **real storage round trips** rather than the FuelDepot degradation, which took a third seam on the gate class (BD10) plus `ReapplyEntitySaveData()` on the persistence manager. Each case builds its own Guard Tower beside a base (on opposite sides, so their re-find queries cannot see each other's), waits for the record to exist, saves, dirties the phase to the opposite state and re-reads the entity's own record. Both towers are left standing, for the FuelDepot case's reason: deleting a tracked entity mid-suite drives the transient-untrack retry queue (BUG-118).
- No `maxAttempts`, no `DeleteEntityAndChildren`, no new localization. `Rpc()` arity re-audited at the `RestorePhase()` call site: two primitives, two passed.
- `tools/compile-check.sh` exit 0 (6195 files). Not run, by policy: `tools/run-tests.sh` (orchestrator only). 3.U1 is owed, and doing it on a pre-Phase-3 save is also the only real exercise of the version 1 branch.

### 2026-08-20 — Phase 2 built (tasks 2.1–2.7)

**Gate (orchestrator, 2026-08-20):** compile-check 0 (6195 files). Fast group first run **331/333** — the new `…_BPhaseRoundTripOnAProbe` case was red because the Guard Tower's component had no hit zone (engine drops a HitZoneContainer with zero hit zones — see the S6 correction). Orchestrator authored the hit-zone block on `OVT_GuardTower_01.et`; re-run **332/333**, the only red being the pre-existing `CompositionSlotGate` case from HEAD. The round-trip case passing means ruin→repair on a real spawned structure works in-process with RplId/owner unchanged.


- `OVT_StructureDamage` written (`Scripts/Game/Utilities/`), `RaiseEffects()` filled in (two particles at the bounding-box centre + a positional one-shot), sabotage now ruins with the existing delete as its fallback and skips a structure that is already a ruin, and Phase 1's admin commands were moved onto the facade.
- `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_StructureDamage.c`: case A (null + a plain prop are refused cleanly) and case B (a spawned Guard Tower goes intact → ruined → intact with an unchanged `RplId`, `OVT_BuildableComponent` instance and owner id, and `Resolve()` finds it through a parent arranged with `AddChild()`). Registration is the `[Test(suite: OVT_TEST_InitSuite)]` attribute — the suite has no case list to edit. No `maxAttempts`; each case carries its proof-it-can-fail preamble.
- No localization change: Phase 2 adds no user-facing string.
- `tools/compile-check.sh` exit 0 (6195 files). Static gates: `DeleteEntityAndChildren|NavmeshRebuild` absent from the sabotage module, and from `Scripts/Game/Components/Damage/` and `OVT_StructureDamage.c`.
- Not run, by policy: `tools/run-tests.sh` (orchestrator only). 2.U1 (listen host + one client, sabotage FX on both machines, one notification) is owed.

### 2026-08-20 — Phase 1 built (tasks 1.1–1.7)

**Gate (orchestrator, 2026-08-20):** compile-check 0 (6193 files). Fast group **330/331** — the single red, `OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions` ('Base Fortifications' builds compositions but authors no `OVT_CompositionSlotConditionDeploymentModule`), is **pre-existing on HEAD**: neither `Configs/Deployment/Deployment_BaseFortifications.conf` nor the test file is touched by this feature (last changed in f9b08a38 / a62d7b6f). Carried as a known pre-existing red; not this feature's fault.

- `OVT_StructureDestructionComponent` written (`Scripts/Game/Components/Damage/`), both probe prefabs retrofitted, temporary `/ruin-structure` and `/repair-structure` added to `OVT_AdminCommandsComponent`, S1–S6 answered above from source evidence.
- `tools/compile-check.sh` exit 0 (6193 files). Static gates run: root-class gate clean (`git diff` shows no changed line 1 of any prefab); `m_bDeleteAfterFinalPhase 0` count is **2 of the eventual 8**; `DeleteEntityAndChildren`/`NavmeshRebuild` absent from `Scripts/Game/Components/Damage/`.
- Two findings that change later phases: **the Bunkers root carries no model of its own** (Phase 4 needs a composition strategy, and the Helipad/tents must be checked for the same shape), and **`GetModelAndRemapFromResource` does not walk prefab inheritance** (narrows §3.10a's "prefer the ruin prefab" to prefabs with their own inline `MeshObject`).
- One shipped defect confirmed for the orchestrator to file: **the Bunkers prefab has no active `RplComponent`** in the shipped chain (S5), which breaks dismantle independently of this feature.
- Not run, by policy: `tools/run-tests.sh` (orchestrator only).

### 2026-08-20 16:47
- Scaffolded `context.md` and `tasks.md` from the templates; `implementation.md` header moved to In Progress / Started 2026-08-20.
- No feature code written. Next session: Phase 1 task 1.1 (`component-developer-advanced`).

---

*Update this file at the end of each work session. Run `/update-feature core/damage` before compacting conversations.*

**Gate (orchestrator, 2026-08-20, post-build fix):** compile-check 0 (6202 files); **All 405/406** — only the pre-existing `CompositionSlotGate` red. The funnel/FX/sound rework broke none of the damage, persistence or repair cases.

**Gate (orchestrator, 2026-08-20, post-build fixes 2+3 — explosion sound bank + visible fire):** compile-check 0 (6202 files); **Fast 346/347** — only the pre-existing `CompositionSlotGate` red.
