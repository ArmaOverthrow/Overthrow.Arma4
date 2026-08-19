# Damage & Destruction — Implementation Plan

**Status:** Planning
**Started:** 2026-08-19
**Target Completion:** TBD
**Last Updated:** 2026-08-19

**Epic:** `core` (feature #8 — see `docs/features/core/epic-overview.md:31`)
**Requirements:** `docs/features/core/damage/requirements.md` (authoritative for scope)
**Approach:** multi-phase retrofit of the eight buildable prefabs, binary intact↔ruined, spike-first; 8 ordered phases (mechanism decided by the user 2026-08-19)
**Branch:** `v1.5` (concurrent sessions exist on this tree — re-baseline before every phase)

---

## 1. Executive Summary

Overthrow has exactly one way to remove a structure from the world: **delete it**. `OVT_ResistanceFactionManager.DestroyPlacedItem()` (`Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c:924`) queues a navmesh rebuild and calls `SCR_EntityHelper.DeleteEntityAndChildren()`. That is fine for a player dismantling their own tent and poor as *destruction*: a sabotaged guard tower vanishes silently, and because there is no damaged state there is nothing to repair — sabotage is pure subtraction with no counterplay.

This feature adds the missing state. A destroyed structure **stays in the world as a ruin**, driven there by the engine's own multi-phase destruction system, with an explosion and smoke at the site. A held action on the ruin restores it for half the build cost, difficulty-scaled to 1× at the harder presets. The occupying faction gets the same ability over its own ground, mirroring sabotage.

Four things make it more than "swap a mesh":

1. **The entity is never deleted, and that is the whole persistence answer.** The requirements name persistence as the single biggest design risk — a *deleted* entity is never saved (which is why destroyed things currently stay destroyed), whereas a *replacement* ruin would be new world content that must be tracked or it re-opens the BUG-030 class. Multi-phase destruction sidesteps the problem entirely: the same entity stays alive with a different mesh, so its persistence record, its `RplId`, its `OVT_BuildableComponent` ownership and its inventory contents all survive untouched. The persistence work reduces to **one integer appended to an existing serializer** (`OVT_BuildableComponentSerializer`, version 1 → 2).

2. **Two vanilla holes have to be closed from inside the class, and both are confirmed, not suspected.** `SCR_DestructionMultiPhaseComponent.GoToDamagePhase(0, false)` sets the phase back to intact and then **never schedules a mesh change**, because `GetDamagePhaseData(0)` returns null by design (`:69`) and the guard at `:227-229` returns before `CallLater(ChangeModel, …)`. The intact model's resource name *is* recorded (`SetOriginalResourceName`, `:191-196`) and is **read nowhere in the entire vanilla tree** — the only two references are its own definition and the data-class getter. Separately, `ReplicateDestructibleState()` (`:462`) sends nothing at all unless `GetDestructionHitInfo()` is non-null, and a scripted phase change has no hit info. An `OVT_` subclass closes both: it reaches the protected `ChangeModel()` to restore the intact mesh, and it carries **one broadcast RPC** that is the whole live-replication story.

3. **The effect is raised by us, on every machine, and depends on nothing optional.** Overthrow has no world-space FX code today. Vanilla's authored-FX path (`SCR_DestructionUtility.SpawnDestroyObjects`) is `notnull` on hit info and early-returns when the owner has no `Physics`; its sound path hard-depends on an `SCR_MPDestructionManager` entity that **is not present in the Eden campaign world**. So the same broadcast RPC that applies the phase also raises the particles (`SCR_DestructionCommon.PlayParticleEffect_CompleteDestruction`, which needs neither hit info nor physics) and a positional one-shot through `SCR_SoundManagerModule`.

4. **The retrofit is eight edits to files Overthrow already owns.** All eight buildable prefabs named in `Configs/Resistance/buildables.conf` are Overthrow-authored `.et` files — no new override prefabs, no `buildables.conf` repointing. One of them (**Bunkers**) already inherits `SCR_DestructionMultiPhaseComponent` and needs only phases authored and the component enabled; five carry the older one-way `SCR_DestructibleBuildingComponent` which is disabled in the delta (a mechanism **vanilla itself already uses** — `TentUSSR_01_base.et:11-13` ships that component `Enabled 0`).

**Explicitly not in scope:** repairable vanilla town buildings (requirements §5 — **dropped**, see §3.9), vehicles, characters, placeables, resource-dependent repair (the `logistics` epic — money only, seam left), and any change to what sabotage targets.

---

## 2. Goals

### Primary

- **G1** A destruction API — `OVT_StructureDamage.Ruin(IEntity)` / `.Repair(IEntity)` / `.IsRuined(IEntity)` — exists and is the only way a structure becomes a ruin. `OVT_ResistanceFactionManager.DestroyPlacedItem()` remains the **only** way a structure leaves the world; the feature adds a destruction path, not a second removal path.
- **G2** Sabotage (`OVT_BaseSabotageBehaviorDeploymentModule.DemolishNextStructure()`, `:322`) ruins instead of deleting, falling back to the existing delete only for a structure that carries no destruction component.
- **G3** The moment of destruction is **audible and visible on every client** — an explosion particle, rising smoke, and a one-shot positional sound at the structure — raised by the API, not by anyone shooting the building.
- **G4** All eight buildables switch to a ruin mesh instead of disappearing, in exactly two states: **intact (phase 0)** and **ruined (phase 1)**. No intermediates.
- **G5** A ruin survives save → quit → **Continue** as a ruin, and a repaired structure comes back repaired. Neither reverts, and neither duplicates.
- **G6** A held action on a ruin restores it for `round(m_iCost × buildableCostMultiplier × repairCostMultiplier)`, where `repairCostMultiplier` is **0.5 at Easy/Normal rising to 1.0 at Extreme/Insane**. The price is re-derived and re-charged **server-side**; the client's copy is for the label and the local grey-out only.
- **G7** The occupying faction repairs ruined structures at bases it controls, through a deployment behaviour module that mirrors sabotage.
- **G8** A structure's inventory contents survive destruction; a ruin's storage actions are **hidden**, and the gear returns when the structure is repaired.
- **G9** An admin chat command ruins or repairs the structure the admin is standing next to, gated server-side by `SCR_Global.IsAdmin(playerId)`.

### Secondary

- **G10** No new manager, no new `OVT_Global` accessor, no new controller component. Repair is one verb on the existing `OVT_ResistanceRequestComponent`; the API is a static facade in the shape of `OVT_NavmeshRebuild`.
- **G11** Every buildable prefab that is retrofitted also gains an **active `RplComponent`** — the Bunkers prefab appears not to have one today (§3.11), which would make it invisible to `RpcAsk_RemovePlacedItem` as well as to this feature.
- **G12** The feasibility finding that killed requirements §5 is **written down** (§3.9) so it is never re-litigated.

### Explicitly out of scope

- Repairing vanilla town buildings (§3.9).
- Vehicles, characters, placeables (`OVT_PlaceableComponent`), and ammo boxes as *targets* — sabotage's target filter is `occupying/counter-attacks`' business and it has just settled on buildables only.
- Resource-dependent repair. Money only; the seam is a single pricing function (§3.6).
- Rebalancing sabotage cadence, structure costs, or the occupying faction's resource pool.
- Making structures meaningfully destructible **by weapons fire**. Adding a damage manager makes that theoretically possible; the plan deliberately authors it out (§3.10, D18).

---

## 3. Architecture Overview

### 3.1 Where each piece lives

```
ENTITY-SIDE MECHANISM (one modded subclass; the only place vanilla internals are touched)
  Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c
     : SCR_DestructionMultiPhaseComponent
     RuinIt(bool withEffects)      authority-only; phase 0 -> 1 + broadcast
     RepairIt()                    authority-only; phase 1 -> 0 + mesh restore + broadcast
     RestorePhase(int phase)       authority-only, SILENT; used by the save loader
     IsRuined()                    GetDamagePhase() != 0, safe on clients
     RpcDo_ApplyPhase(int, bool)   [RplRpc(Reliable, Broadcast)] - THE one wire message
     RepairToIntact()              protected; the phase-0 mesh-restore hole fix
     RaiseEffects()                protected; particles + sound, runs on every machine

THE API OTHER SYSTEMS CALL (pure statics, no state, modelled on OVT_NavmeshRebuild)
  Scripts/Game/Utilities/OVT_StructureDamage.c
     static bool Ruin(IEntity, bool withEffects = true)
     static bool Repair(IEntity)
     static bool IsRuined(IEntity)
     static bool IsDestructible(IEntity)
     static OVT_StructureDestructionComponent Resolve(IEntity)   entity, then its children

PURE LOGIC (world-free, Logic-tier testable)
  Scripts/Game/Data/OVT_RepairPricing.c
     static int RepairCost(int baseCost, float buildMultiplier, float repairMultiplier)
     static bool IsRepairable(int baseCost)      // guards UNKNOWN_STRUCTURE_COST

MANAGER (the fat end; owns the cost join, the charge and the call into the API)
  OVT_ResistanceFactionManager
     + OVT_Buildable FindBuildableForEntity(IEntity)   extracted from GetStructureCost
     + int  GetRepairCost(IEntity)
     + bool RepairStructure(IEntity, int playerId)     -1 = server-initiated, free

CLIENT -> SERVER SEAM (one new verb on an existing component)
  OVT_ResistanceRequestComponent
     + void RepairStructure(RplId)                     client wrapper
     + RpcAsk_RepairStructure(RplId)                   server handler

USER ACTION
  Scripts/Game/UserActions/OVT_RepairStructureAction.c : ScriptedUserAction
     (hold duration authored on each prefab, never in script)

OCCUPYING FACTION
  Scripts/Game/GameMode/Objectives/Modules/OVT_BaseRepairBehaviorDeploymentModule.c
  Configs/Deployment/Deployment_ObjectiveRepair.conf  (+ registry entry)

ADMIN
  OVT_AdminCommandsComponent  + /ruin-structure and /repair-structure

CONFIG
  OVT_DifficultySettings   + float repairCostMultiplier
  OVT_OverthrowConfigComponent   CONFIG_STREAM_VERSION 4 -> 5

PERSISTENCE
  OVT_BuildableComponentSerializer   version 1 -> 2 (one int appended)

PREFABS (all eight already Overthrow-owned; edits only)
  Prefabs/Structures/Military/Houses/GuardTower_01/OVT_GuardTower_01.et
  Prefabs/Structures/Military/FOB/OVT_RecruitmentTent.et
  Prefabs/Structures/Military/FOB/OVT_MedicalTent.et
  Prefabs/Structures/Military/FOB/OVT_VehicleMaintenanceRamp.et
  Prefabs/Structures/Military/FOB/OVT_FuelDepot.et
  Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et
  PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_bunker_plastic_foundation_camonet.et
  PrefabsEditable/Auto/Structures/Military/Camps/HelipadImprovised_01/Helipad.et
```

**No new manager, and no `OVT_Global` accessor.** The decision framework asks whether there is system-wide state to coordinate. There is not: **there is no registry of placed structures** — every consumer in the tree finds them with `QueryEntitiesBySphere` + `FindComponent`. All per-structure state lives on the per-structure component, so the only thing a manager could hold is a lookup nobody needs. The API is therefore a static facade exactly like `OVT_NavmeshRebuild`, and the pricing/charging half lives on `OVT_ResistanceFactionManager`, which already owns buildables and their costs. There is also **no game-mode invoker** (the `SCR_CharacterDamageManagerComponent` → `GetOnCharacterKilled()` precedent) because nothing subscribes: the sabotage module already knows it destroyed something, and the occupying repair module polls. Adding one later is three lines.

### 3.2 The two engine holes, and the subclass that closes them

Both are verified against `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Game/Destruction/SCR_DestructionMultiPhaseComponent.c` (the whole class is inside `#ifdef ENABLE_BASE_DESTRUCTION`, which **is** defined — this is live code, unlike the building-destruction API in §3.9).

**Hole 1 — `GoToDamagePhase(0, …)` restores the state but not the mesh.** The method runs its navmesh regen, records the original resource name on the *first departure* from phase 0 (`:191-196`), sets the phase and the target phase, cancels any pending `ChangeModel`, and then:

```
SCR_DamagePhaseData data = GetDamagePhaseData(damagePhase);
if (!data)
    return;
callQueue.CallLater(ChangeModel, delay, param1: data.m_PhaseModel, param2: data.m_bUseMaterialsFromParent);
```

`GetDamagePhaseData` (`:67-73`) starts `if (damagePhase <= 0 || damagePhase >= GetNumDamagePhases()) return null;`. So for phase 0 it **always** returns null and the mesh change is **never scheduled**. `GetOriginalResourceName()` (`:141`, protected) is the obvious intended consumer and is read by nothing — `grep -rn "GetOriginalResourceName()" scripts/Game/Destruction/` returns its own definition and the data-class getter, nothing else.

> This is a **certainty, not a spike question**, and **vanilla itself has a worked solution to copy.** `SCR_DestructionTireComponent` hit exactly this gap and fixed it the way we must: it **overrides `GoToDamagePhase`**, special-cases `damagePhase == 0` into its own `ReturnToInitialDamagePhase()` (`:219-235`) which sets the phase and then calls `ChangeModel` with a resource name **it tracked itself** — from a companion component data class carrying an `[Attribute] ref SCR_DamagePhaseData InitialData`, resolved in `OnPostInit` — pointedly **not** from `GetOriginalResourceName()`. Our subclass does the same, sourcing the intact model from the owner's own `VObject` at `OnPostInit`. (`SCR_DestructionUtility.SetModel(owner, modelPath, remap)` is public static and is the alternative to the protected `ChangeModel`, if that proves easier.)

**Hole 2 — a scripted phase change reaches no client, and vanilla's replication path structurally cannot repair one.** Two separate defects:

- `ReplicateDestructibleState(int damagePhase = 0, bool silent = false)` (`:462-474`) reads `GetDestructionHitInfo()` — which does **not** create one by default — and when it is null the whole body is skipped and nothing is sent. A phase change we drive from script has never produced a damage event, so there is no hit info. This one is *fixable* from outside: `CreateDestructionHitInfo(...)` is **public** (`SCR_DestructionDamageManagerComponent.c:310`), so a synthetic hit info could be manufactured first.
- **But the receiver cannot repair.** `RPC_ReplicateMultiPhaseDestructionState` (`:421-459`) does `SCR_DamagePhaseData phaseData = GetDamagePhaseData(phase); if (phaseData) { …FX…; GoToDamagePhase(phase, true); } else { …FX only… }`. For **phase 0** `GetDamagePhaseData` returns null (§3.2 hole 1), so the client takes the `else` branch: it plays destruction effects and **never changes phase**. Vanilla's own broadcast is incapable of telling a client to become intact again.

> So the fix is **our own broadcast RPC**, which handles both directions uniformly — not a workaround layered on vanilla's. See §3.3 and D3.
>
> *(Vanilla's repair path, `SCR_CampaignServiceEntityComponent.RepairEntity():71-80`, calls `ReplicateDestructibleState(true)` — a `bool` landing in the `int damagePhase` parameter, i.e. "replicate phase **1**, not silently", immediately after repairing to phase 0. Do not copy it; listed for the orchestrator in §10.)*

**What vanilla already gets right, and must not be duplicated:**

- **JIP and streaming.** `OnRplSave` (`:486`) writes a 1-bit "is damaged" flag plus an 8-bit phase, and `OnRplLoad` (`:500`) replays `GoToDamagePhase(phase, false)`. A client that streams the structure in after it was ruined sees the ruin. This works only if the entity has an **active `RplComponent`** — see G11/§3.11.
- **Navmesh.** `GoToDamagePhase` calls `SCR_DestructionUtility.RegenerateNavmeshDelayed(GetOwner())` at `:189`, which is server-only, captures the bounds **at call time** (while the intact model still stands) and issues the rebuild 1000 ms later. That is `OVT_NavmeshRebuild.Queue()`'s exact contract, including the delay. **Do not add `OVT_NavmeshRebuild.Queue()` to the ruin or repair path** — see D7.

### 3.3 One wire message

```
SERVER                                        EVERY MACHINE (incl. the server, self-invoked)
OVT_StructureDamage.Ruin(entity)
  -> comp.RuinIt(withEffects = true)
       GoToDamagePhase(1, false)              RpcDo_ApplyPhase(1, true)
       Rpc(RpcDo_ApplyPhase, 1, true)   --->    if (!IsServer) GoToDamagePhase(1, false)
       RpcDo_ApplyPhase(1, true)  [local]       if (withEffects) RaiseEffects()

OVT_StructureDamage.Repair(entity)
  -> comp.RepairIt()
       RepairToIntact()                       RpcDo_ApplyPhase(0, false)
       Rpc(RpcDo_ApplyPhase, 0, false)  --->    if (!IsServer) RepairToIntact()
       RpcDo_ApplyPhase(0, false) [local]       (no effects on a repair)
```

- **`[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]`.** Reliable, because a missed message leaves a client looking at the wrong building forever.
- **The server invokes the handler locally as well as broadcasting**, because the engine never loops a broadcast back to the sender — the same trap `ShouldRespondLocally` exists for on the owner-response path (BUG-090). The handler's own `if (!Replication.IsServer())` guard prevents the phase being driven twice on the server; the effects branch runs on both.
- **Arity is hand-audited.** `Rpc()` is an untyped variadic proto: a wrong argument count compiles clean and dies silently at the wire (BUG-090). Two arguments, both primitives.
- **`RestorePhase()` broadcasts with `withEffects = false`.** A save being re-applied must be silent, but a listen host doing a **Continue** can have clients already connected (the BUG-104 class), so the message still goes out.

### 3.4 Binary damage, and what the retrofit writes into each prefab

Exactly **one** `SCR_DamagePhaseData` per structure, so `GetNumDamagePhases()` returns 2: phase 0 intact, phase 1 ruined. The authoring shape, verified against `Prefabs/Props/Agriculture/CultivatorWreck_01.et:13-77` and `Prefabs/Props/Agriculture/Beehive_01/Beehive_01_stack.et:13-43`:

```
OVT_StructureDestructionComponent "{6B70D0…}" {
 Enabled 1
 m_bDeleteAfterFinalPhase 0          // MANDATORY - the class default is 1
 m_fBaseHealth <high>                // see D18
 m_eMaterialSoundType <material>     // vanilla's own break-sound classifier; harmless, we do not use its sound path
 m_aDamagePhases {
  SCR_DamagePhaseData "{6B70D0…}" {
   m_fPhaseHealth <high>
   m_PhaseModel "{GUID}Assets/…/<ruin>.xob"
  }
 }
}
```

`m_bDeleteAfterFinalPhase` defaults to **`1`** (`SCR_DestructionMultiPhaseComponentClass:4`). Left at the default, the structure is *deleted* when driven past the final phase — which is exactly the behaviour this feature exists to remove. **It is authored `0` in every one of the eight, and the Phase 4 acceptance criteria greps for it.**

`m_PhaseDestroySpawnObjects` is deliberately left **empty** — see D5.

### 3.5 Persistence: one integer, and no new record

Because the entity is never deleted there is no new world content, no new persistence rule, and no `SelfSpawn` question. `Configs/Systems/Persistence/Overthrow.conf:175-192` already binds `OVT_BuildableComponentSerializer` to every entity carrying `OVT_BuildableComponent`, with `SavePrefabChildren 1` and `Priority 35000`. The serializer bumps 1 → 2:

```
Serialize:    context.WriteValue("version", 2)
              …existing three fields, unchanged order…
              context.Write(phase)                     // 0 or 1, read from the destruction component

Deserialize:  OVT_NavmeshRebuild.Queue(owner)          // EXISTING line, stays first - see below
              read version; if (version < 1) return true
              …existing three fields…
              if (version >= 2) { context.Read(phase); if (phase != 0) restore it }
```

Two things a reader will want to "fix" and must not:

- **The existing `OVT_NavmeshRebuild.Queue(owner)` stays where it is, before the phase is restored.** It captures the *intact* bounds; vanilla's own `RegenerateNavmeshDelayed` inside `GoToDamagePhase` then captures them again; both rebuild ~1 s later with the ruin model in place. Measuring the larger model and rebuilding after the smaller one appears is the conservative, correct order — the same order the live destruction path uses.
- **Nothing is written when the phase is 0.** A `version >= 2` payload always carries the integer (binary contexts are positional), but the restore is a no-op for an intact structure, so an ordinary save costs one int per buildable and does nothing on load.

**No second copy of the phase on `OVT_BuildableComponent`.** See D9.

### 3.6 Repair: price, seam, action

```
PRICE (pure, Logic-tier)
  OVT_RepairPricing.RepairCost(baseCost, buildMultiplier, repairMultiplier)
      = Math.Round(baseCost * buildMultiplier * repairMultiplier)

  baseCost         OVT_Buildable.m_iCost, joined by PREFAB RESOURCE NAME (never the type string)
  buildMultiplier  OVT_DifficultySettings.buildableCostMultiplier   (Easy .8 / Normal 1 / Hard 1.5 / Extreme 3 / Insane 4)
  repairMultiplier OVT_DifficultySettings.repairCostMultiplier      (NEW: Easy .5 / Normal .5 / Hard .75 / Extreme 1 / Insane 1)

SERVER                                   CLIENT
OVT_ResistanceFactionManager             OVT_RepairStructureAction
  GetRepairCost(entity)                    CanBeShownScript   -> OVT_StructureDamage.IsRuined(owner)
  RepairStructure(entity, playerId)        CanBePerformedScript -> LocalPlayerHasMoney(price) (advisory)
     resolve buildable by prefab           GetActionNameScript -> "#OVT-RepairStructure ($" + price + ")"
     price it                              PerformAction -> OVT_ControllerComponent<
     PlayerHasMoney(persId, price)?            OVT_ResistanceRequestComponent>.Get().RepairStructure(rplId)
     OVT_StructureDamage.Repair(entity)     HasLocalEffectOnlyScript -> true
     TakePlayerMoney(playerId, price)      hold duration: authored on the prefab (Duration N)
```

**One rounding, not two.** `OVT_OverthrowConfigComponent.GetBuildableCost()` already rounds; multiplying its output by 0.5 and rounding again drifts. The pure function takes the raw `m_iCost` and both multipliers.

**Charge after performing, never before** — the shape `OVT_ShopTransactionComponent.RpcAsk_RearmVehicle` uses (`:506-542`): check `PlayerHasMoney`, perform, then `TakePlayerMoney`. `DoTakePlayerMoney` clamps at zero, so an explicit funds check is mandatory rather than defensive.

**`playerId == -1` means server-initiated and free**, the convention `BuildItem` (`:800-822`) and `ChargeForGarrison` already use. That is how the occupying-faction module and the admin command repair without a wallet.

**The hold duration is authored in the prefab, not in script.** `GetActionDuration` is not overridden anywhere in Overthrow *or* vanilla; every held action in the mod (`OVT_CaptureBaseAction` `Duration 15`, `OVT_SabotageTowerAction` `Duration 20`, `OVT_DismantleEnemyFOBAction` `Duration 15`, `OVT_FillFuelAction` `Duration 5`) gets its number from the `additionalActions` block. `OVT_DismantleEnemyFOBAction`'s own header states the rule: *"There is no script-side duration to set and adding one would give the same number two homes."*

**Where the action is attached.** `OVT_FuelDepot.et:41-76` is the in-repo template for adding a whole `ActionsManagerComponent` (with its own `UserActionContext`, `Position`/`Offset` and `Radius`) to a static structure from an Overthrow delta. Six of the eight prefabs need one added; the Fuel Depot already has one and takes only a new `additionalActions` entry.

### 3.7 The occupying faction repairs its own ground

`OVT_BaseRepairBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule`, structurally a mirror of `OVT_BaseSabotageBehaviorDeploymentModule`:

| Sabotage | Repair |
|---|---|
| `OVT_BaseControlConditionDeploymentModule { m_bRequireControl 0 }` — deploy while the base is **not** ours | `{ m_bRequireControl 1 }` — deploy while the base **is** ours |
| `CollectTargets` → buildables at the base, **cheapest first** | `CollectTargets` → **ruined** buildables at the base, cheapest first |
| holds `objectiveSabotageHoldSeconds`, then `DestroyPlacedItem` | holds the same interval, then `OVT_StructureDamage.Repair` |
| `m_bDirectorOnly 1` — an offensive operation against the current objective | `m_bDirectorOnly 0` — maintenance, evaluator-selectable at any held BASE |
| one broadcast notification per mission | **no notification** — this is the occupying faction tidying up, not an event aimed at the player |

Reusing the sabotage module's own shape is deliberate: `CountAliveRegisteredMembersWithin` / `NearestPlayerDistance` / the pause-don't-reset interval / `CompleteMission("nothing left")` / `CloneModule()` copying every attribute by hand are all machinery this framework already requires, and the sabotage suite (`OVT_TEST_Init_ObjectiveSabotage.c`) proves the decision half is testable as pure functions.

**Cost: the deployment's insertion cost only, nothing per structure.** A repair team is bought like a sabotage team and then works for free. Balance numbers (`m_iBaseCost`, `m_iCostPerGroup`, priority relative to the base-defense configs, `m_fChance`, `m_iMaxInstances`) belong to the `occupying` epic; this plan authors conservative values and says so.

⚠ **Be honest about the trigger surface.** On shipped data a ruin only stands on occupying-held ground after the resistance built at a base, the occupying faction sabotaged it, and then recaptured the base — or after an admin ruins one. That is narrow, and it is the reason the admin command (G9) is scoped into the same feature: `/ruin-structure` is how this module gets play-tested at all. The module is built now because it is the natural symmetric consumer of the API and because `occupying/base-upgrades` will widen its surface, not because it will fire often in v1.

### 3.8 Gear survival

With multi-phase the entity, its `BaseInventoryStorageComponent` and its contents are never touched by a phase change — the mesh swaps, nothing else. So the work is **verify and don't break it**, plus one decision:

- **A ruin's storage is closed.** `CanBeShownScript` on the storage actions returns false while `OVT_StructureDamage.IsRuined(owner)`. Reaching into a pile of rubble for a rifle reads wrong, and hiding the action makes repair the way to get the gear back — which is the point of the mechanic. Contents are untouched, so repairing returns them exactly.
- **The seam this leaves for two other features is recorded, not built.** `core/storage` (in planning) is adding an `OVT_StorageComponent` ledger; when a buildable can hold one, the same phase gate applies to its actions and the ledger needs no special handling because it is component state on a surviving entity. `occupying/counter-attacks`' `IsGearContainer()` exclusion (`OVT_BaseSabotageBehaviorDeploymentModule.c:271-279`) is currently unreachable on shipped data — every ammo box is a *placeable* and placeables were dropped from candidacy — and this feature does not change that. Revisiting it is the occupying epic's call once a buildable container exists.

### 3.9 Requirements §5 is dropped — the feasibility finding, recorded once

**Repairing arbitrary destructible vanilla buildings is not achievable in 1.8.0.10, and this is why.**

1. The bidirectional region-repair API lives on `SCR_DestructibleBuildingEntity` (`scripts/Game/Destruction/Building/SCR_DestructibleBuildingEntity.c:52`) and `SCR_BuildingRegionEntity.c:9`, both behind **`#ifdef ENABLE_BUILDING_DESTRUCTION`**. That symbol is **defined nowhere in the shipping script tree** — those two `#ifdef` sites are its only occurrences. The code does not exist at runtime.
2. Every destructible vanilla building instead uses `SCR_DestructibleBuildingComponent` (`: SCR_DamageManagerComponent`, `:525`), which is **one-way**: it sinks and rotates the building and then discards the model with `SetObject(null, "")`. There is no un-destroy.
3. Converting them to multi-phase would mean a per-prefab delta on **every destructible building in the game**, each needing an authored ruin mesh that in most cases does not exist (vanilla buildings have no whole-object ruin models precisely because the building component never swaps to one).
4. Contrast with `ENABLE_BASE_DESTRUCTION`, which **is** defined and is why `SCR_DestructionMultiPhaseComponent` is live code. The two systems are siblings, not a hierarchy.

The requirements marked §5 "if possible" deliberately and set the test: *"If it turns out to need hundreds of prefab deltas, that is a 'no'."* It does. **Do not re-open this without a Reforger release that defines `ENABLE_BUILDING_DESTRUCTION`.**

### 3.10 What the eight prefabs actually look like today

Verified file-by-file. **All eight are Overthrow-owned files** — there is no vanilla prefab to override and no `buildables.conf` entry to repoint.

| # | Buildable | Cost | Root class in the OVT file | Destruction in the chain today | Retrofit |
|---|---|---|---|---|---|
| 1 | Guard Tower | 1200 | `SCR_DestructibleBuildingEntity` | `SCR_DestructibleBuildingComponent "{5D7E937DD0A125D0}"` (Metal_Tiny), `GuardTower_01_base.et:124` | disable it, add ours |
| 2 | Recruitment Tent | 1000 | `SCR_DestructibleBuildingEntity` | same component, **already `Enabled 0`** at `TentUSSR_01_base.et:11-13` | add ours only |
| 3 | Medical Tent | 1000 | `SCR_DestructibleBuildingEntity` | inherits the same **disabled** component | add ours only |
| 4 | Vehicle Maintenance Ramp | 1500 | bare `GenericEntity`, **no parent** | none on the root; the destructible is a **child** at `OVT_VehicleMaintenanceRamp.et:71` | see below |
| 5 | Bunkers | 750 | `StaticModelEntity` | **`SCR_DestructionMultiPhaseComponent "{5E76C88E922E8914}"`** already inherited (`Sandbag_01_bunker_plastic_foundation.et:4`, `m_fBaseHealth 4500`) but the `.ct` preset ships it `Enabled 0` | enable + author phases |
| 6 | Garage | 8000 | `SCR_DestructibleBuildingEntity` | `SCR_DestructibleBuildingComponent "{5D7AD092A3CB4B41}"` (Brick_Large, `MaxHealth 30000`) | disable it, add ours |
| 7 | Helipad | 1500 | `StaticModelEntity` | **none anywhere in the chain** | add ours |
| 8 | Fuel Depot | 2000 | `SCR_DestructibleBuildingEntity` | `SCR_DestructibleBuildingComponent "{5DA867F4F045AA84}"` (Metal_Small), `FuelTank_02_Base.et:15` | disable it, add ours |

Three consequences:

- **Disabling the inherited building component is a proven mechanism, not a hope.** Vanilla itself ships `SCR_DestructibleBuildingComponent "{692CD84F45C9100B}" … { Enabled 0 }` on the tent base. A delta re-declares the inherited component **by its own instance GUID** and sets `Enabled 0`. That is what tasks 4.x do for #1, #6 and #8. (Two `SCR_DamageManagerComponent` descendants coexisting on one entity is the thing being avoided; whether the engine tolerates it at all is spike question **S4**, and the answer does not change the plan — we disable it either way.)
- **🔴 Do not change any root entity class.** Eight script files filter world queries on the literal string `entity.ClassName() == "SCR_DestructibleBuildingEntity"` — `OVT_RealEstateManagerComponent.c:88,784,807`, `OVT_TownManagerComponent.c:1340,1353`, `OVT_OwnerManagerComponent.c:178,207,212`, `OVT_EconomyManagerComponent.c:2027`, `OVT_TownCivilianSourceConfig.c:861`, `OVT_TowerCoverPostPlacementProvider.c:103`. Five of the eight buildables match that string today. Retyping a root would silently change which entities real-estate, ownership, economy, town-scan and AI-cover-post logic can see, with no compile error. **The retrofit adds and disables components; it never touches line 1 of a prefab.**
- **The ramp is the awkward one.** Its root is a bare `GenericEntity` carrying `OVT_BuildableComponent` and its own `RplComponent`; the visible, destructible object is a child (`SCR_DestructibleBuildingEntity : RampVehicle_01_metal.et`). The destruction component therefore goes on the **child**, and `OVT_StructureDamage.Resolve()` walks `GetChildren()`/`GetSibling()` after failing on the entity itself. This is the same root-vs-child walk `OVT_LootIntoVehicleAction.c:35-49` does for truck beds.

### 3.10a The ruin meshes — half of them already exist

Surveyed against the reference tree. **Vanilla building ruins are not named `_dst_NN.xob`** — they are `<Name>_Ruin.xob`, reached through `SCR_DestructibleBuildingComponent.m_aEffects` → `SCR_TimedPrefab.m_sRuinsPrefab` → a `…_Ruin.et` deriving from `Prefabs/Structures/Core/BuildingRuin_base.et`, whose `MeshObject.Object` is the ruin. The `_dst_NN.xob` convention belongs to the multi-phase *prop* system; `_dbr_NN.xob` files are loose thrown fragments and are **never** whole-object replacements.

| Buildable | Ruin available? | Resource |
|---|---|---|
| Guard Tower | ✅ | `{F0FDB651ED7B2B76}Assets/Structures/Military/Houses/GuardTower_01/GuardTower_01_Ruin.xob` (prefab `GuardTower_01_Ruin.et`) |
| Vehicle Maintenance Ramp | ✅ | `{F93C1C026EABAEF5}Assets/Structures/Industrial/Repair/RampVehicle_01/dst/RampVehicle_01_metal_Ruin.xob` |
| Garage | ✅ | `{B405E1CD1596AA90}Assets/Structures/Industrial/Garages/Garage_E_02/Garage_E_02_Ruin.xob` |
| Fuel Depot | ✅ | `{9012D46112E2D3D5}Assets/Structures/Industrial/Containers/FuelTanks/FuelTank_02/Dst/FuelTank_02_Ruin.xob` |
| Recruitment Tent | ❌ none in the whole `TentUSSR_01/` family | fallback |
| Medical Tent | ❌ (same family) | fallback |
| Bunkers | ❌ — the `Sandbag_01_bunker`/`_foundation` pieces have destruction **disabled** and no phase data anywhere; the `_dst_` meshes that do exist belong to *other* sandbag pieces (wall/long/single) | fallback |
| Helipad | ❌ not destructible at all in vanilla | fallback |

**Fallbacks, in preference order:** `{1C9E0D1CD5A0E4F9}Assets/Structures/Military/Fortifications/Bunker_SPS/Dst/Bunker_SPS_Ruin.xob` for the military/concrete pieces (Bunkers), and `{D88A17A1346EA3C5}Assets/Structures/Ruins/Rubble_Ruin_01/Rubble_Ruin_01_V2.xob` as the generic — authored as identity-less filler rubble, self-contained, no dependent debris children, and sized for a small-to-medium demolished structure. ⚠ **The tents and the helipad are the honest problem**: brick rubble where a canvas tent stood will not read well. `{0335D6B08DBE867E}Assets/Structures/Debris/DebrisPiles/DebrisPile_Concrete_01_Medium.xob` is a lower-profile alternative. Play-test item 4 judges all four; if none reads acceptably, the escalation is a Workbench Resource Browser session with the user, **not** new art.

**`m_PhaseModel` accepts `"et xob"`** (`SCR_DamagePhaseData.c:7`), so for the four that have a ruin *prefab* the plan authors the **prefab**, not the bare mesh — `ChangeModel` runs it through `SCR_Global.GetModelAndRemapFromResource`, which brings the ruin's material remap along. The four fallbacks reference the mesh directly.

**Authoring template with our exact shape** — `Prefabs/Structures/Walls/Brick/BrickGate_01/BrickGate_01_base.et` is the vanilla prefab closest to what we need: fortification-sized, **one** `SCR_DamagePhaseData`, and `m_bDeleteAfterFinalPhase 0` explicitly authored. Copy its structure. `Configs/Destruction/MultiphaseDestruction/` (41 `.conf` files under `Phases/`, `FX_Debris/`, `HitZone/`) holds reusable authored fragments if debris is ever wanted.

### 3.11 `SCR_MPDestructionManager`, and a replication hole worth checking

- **The manager is spawned at runtime, not placed in the world.** `Prefabs/GameMode/OVT_OverthrowGameMode.et` carries `SCR_DestructionManagerComponent "{5DFEE7C5F4445327}"`, whose `EOnInit` (`SCR_DestructionManagerComponent.c:17-29`) calls `SCR_MPDestructionManager.InitializeDestructionManager()` after 500 ms (10 s on dedicated) on the **authority only** — and that static method *spawns* `{9BB369F2803C6F71}Prefabs/MP/MPDestructionManager.et`, which carries an `RplComponent` and therefore replicates down to every client. So the manager exists on the Eden campaign map even though the prefab is placed in **no** Eden layer (the single repo reference, `Worlds/MP/OVT_Campaign_Test_Layers/default.layer:328`, is redundant with the runtime spawn).
- **⚠ There is a startup window in which `GetInstance()` is null** — up to 10 s on a dedicated server. Nothing in this feature may assume the manager exists at init.
- **This plan does not depend on it either way**, which is why D5 and D6 avoid `SCR_DestructionUtility.PlaySound` (which hard-returns without it, `:60-63`) and the `SCR_DestructionSynchronizationComponent` path (whose `RegisterDynamicallySpawnedDestructible` has **no callers anywhere in vanilla** — nothing is ever registered, which is one of the three reasons the campaign repair flow is dead, §10). Every other `GetInstance()` call site in vanilla is defensive and early-returns; none null-derefs. Spike question **S3** confirms the auto-spawn happens in an Overthrow session and that nothing misbehaves before it does. **Do not add the prefab to any Eden layer** — it is unnecessary, and placing it would also drag in `SCR_BuildingDestructionManagerComponent` and switch on 1.8's building-destruction persistence map-wide.
- **🔴 The Bunkers prefab may have no active `RplComponent`.** Its root inherits `RplComponent "{5E76C88E937D49B8}" : "…DestructionMultiPhase_Rpl_Base.ct"`, and that `.ct` ships `Enabled 0`; the vanilla sibling that *does* want replication (`Sandbag_01_bunker_plastic_CompositionDestruction.et`) re-declares both components with `Enabled 1` explicitly. Overthrow's override adds neither. If the component really is inactive then a built bunker has no `RplId`, which would break `RpcAsk_RemovePlacedItem` (dismantle) as well as this feature's JIP path. The Helipad's author evidently hit the same wall and added `RplComponent` by hand (`Helipad.et:62`). Spike question **S5** settles it; either way the retrofit sets `Enabled 1`, and a confirmed defect goes to the orchestrator (§10).

### 3.12 Boundary: the `logistics` epic

Repair costs **money**. When logistics can deliver materials, the change is confined to `OVT_ResistanceFactionManager.RepairStructure()` — swap or supplement the `PlayerHasMoney`/`TakePlayerMoney` pair. `OVT_RepairPricing` stays a money function; nothing in this feature may be widened for a hypothetical resource consumer, and the class comment must say so.

---

## 4. Implementation Phases

Every phase must leave `tools/compile-check.sh` at exit 0. **Do not run `tools/run-tests.sh` from an implementation agent** — the orchestrator runs it once per completed phase (`.claude/test-policy.md`).

**Reserved GUID series for this feature: `6B70D0000000xxxx`** (verified unused repo-wide, 2026-08-19). Every new `.conf`/`.et`/`.meta` object takes the next value. A GUID collision fails **silently**.

---

### Phase 1 — Engine-behaviour spike ⚠️ ADVANCED AGENT

*Agent: **`component-developer-advanced`.** It writes production code against undocumented vanilla internals, edits two prefabs by hand, and its output is a **written decision record every later phase depends on**. Nothing downstream may start until S1–S6 are answered in `docs/features/core/damage/context.md`.*

**Estimate:** 8–12 h (plus one user-gated Workbench/play-test session)

**Probe subjects, chosen deliberately:** **Bunkers** (already has the multi-phase component; the cleanest possible test of the engine mechanics) and **Guard Tower** (carries an active `SCR_DestructibleBuildingComponent`; the only way to test coexistence).

| # | Task | Acceptance |
|---|---|---|
| 1.1 | `Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c : SCR_DestructionMultiPhaseComponent` + its `…ComponentClass : SCR_DestructionMultiPhaseComponentClass`. Members per §3.1. `RuinIt`/`RepairIt`/`RestorePhase` all start `if(!Replication.IsServer()) return;`. **Follow `SCR_DestructionTireComponent` (`:219-251`) exactly**: cache the intact model in `OnPostInit` (`GetOwner().GetVObject().GetResourceName()`), `override GoToDamagePhase` to route `damagePhase == 0` into `RepairToIntact()` and everything else to `super`, and have `RepairToIntact()` set the phase and then `CallLater(ChangeModel, …)` with the cached name. **Do not rely on `GetOriginalResourceName()`** — nothing consumes it and it is empty for a structure restored from a save as a ruin. `IsRuined()` is safe on a client and before init. | Compile 0. |
| 1.2 | `RpcDo_ApplyPhase(int phase, bool withEffects)` — `[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]`. Server path invokes it locally **and** `Rpc()`s it. Arity hand-audited against the handler signature, argument by argument. | Two args, both primitive; a comment records the audit. |
| 1.3 | Retrofit **Bunkers** (`PrefabsEditable/…/E_Sandbag_01_bunker_plastic_foundation_camonet.et`): re-declare `SCR_DestructionMultiPhaseComponent "{5E76C88E922E8914}"` as `OVT_StructureDestructionComponent` if the class can be swapped in a delta — **if it cannot, add ours as a second component and disable the inherited one** (record which). `Enabled 1`, `m_bDeleteAfterFinalPhase 0`, one phase with a ruin mesh. Also set the inherited `RplComponent "{5E76C88E937D49B8}" { Enabled 1 }`. | Workbench loads it clean (**user-gated**). |
| 1.4 | Retrofit **Guard Tower** (`OVT_GuardTower_01.et`): add `OVT_StructureDestructionComponent` **and** re-declare `SCR_DestructibleBuildingComponent "{5D7E937DD0A125D0}" { Enabled 0 }`, mirroring `TentUSSR_01_base.et:11-13`. Do **not** touch line 1. | Workbench loads it clean (**user-gated**). |
| 1.5 | Ruin models for the two probes, from the §3.10a table: **Guard Tower** uses its real ruin (prefer the prefab `GuardTower_01_Ruin.et` over the bare `{F0FDB651ED7B2B76}…GuardTower_01_Ruin.xob`, so the material remap comes along); **Bunkers** has none and exercises the fallback (`{1C9E0D1CD5A0E4F9}…Bunker_SPS_Ruin.xob`). Deliberately one of each so both routes are proven before Phase 4. Judge scale and ground alignment in-game, not in the editor. | Both read as wreckage of roughly the right size, sitting on the ground rather than floating or sunk. |
| 1.6 | A **temporary** debug driver so the probes can be triggered without the rest of the feature: two `SCR_Global.IsAdmin`-gated chat commands on `OVT_AdminCommandsComponent` (`/ruin-structure`, `/repair-structure`, nearest buildable within 15 m of the caller). These are the real Phase 6 deliverable arriving early because the spike cannot run without them. | Both commands work on a listen host. |
| 1.7 | **Answer S1–S6 in `docs/features/core/damage/context.md`**, each with the observed evidence and the chosen branch. | The record exists and is unambiguous. |

**The six spike questions, with their pre-recorded fallbacks:**

| ID | Question | Expectation | Fallback if wrong |
|---|---|---|---|
| **S1** | Does the `SCR_DestructionTireComponent`-style `RepairToIntact()` put the intact model back — geometry, materials and collision? | Yes; it is vanilla's own working pattern. `ChangeModel` → `SCR_DestructionUtility.SetModel` → `owner.SetObject` + `phys.UpdateGeometries()` (which **destroys** physics if the new model has no geometry — check collision after a repair, not just the visual). | If the protected `ChangeModel` proves awkward, call the public static `SCR_DestructionUtility.SetModel(owner, modelPath, remap)` directly after resolving through `SCR_Global.GetModelAndRemapFromResource`. |
| **S2** | Does the broadcast RPC reach a second machine and drive the phase there, **in both directions**? | Yes, given an active `RplComponent`. Repair is the direction to watch: vanilla's own broadcast provably cannot do it (§3.2), so there is no fallback that reuses vanilla's receiver. | If a component-hosted broadcast RPC misbehaves, move the message to `OVT_ResistanceRequestComponent` as an `RplRcver.Broadcast` verb carrying `(RplId, int, bool)`. |
| **S3** | Does `SCR_MPDestructionManager.GetInstance()` become non-null in an Overthrow session, and does anything misbehave during the startup window before it does? | It should auto-spawn ~500 ms in (10 s dedicated) via the game mode's existing `SCR_DestructionManagerComponent` — §3.11. | Nothing we call needs it, so a null instance is acceptable. **Do not place the prefab in an Eden layer.** If something unexpected does need it, gate on a short delay rather than on world content. |
| **S4** | Can `OVT_StructureDestructionComponent` and `SCR_DestructibleBuildingComponent` coexist on one entity, and does `Enabled 0` in a delta really deactivate the latter? | `Enabled 0` works (vanilla uses it); coexistence is unknown and irrelevant if the disable works. | If `Enabled 0` does not deactivate it, the buildable's damage-manager surface is ambiguous: fall back to **hosting our component on a child entity** for the five `SCR_DestructibleBuildingEntity` prefabs, exactly as the ramp already requires. |
| **S5** | Does a built **Bunker** have an active `RplComponent` on the shipped tree, before our edit? | Suspected **no** (§3.11). | Confirmed-no is a shipped defect: report it (§10). Either way the retrofit sets `Enabled 1`. |
| **S6** | With a damage manager present, can a player shoot a structure to phase 1, and does an entity without an authored hit zone still take the scripted phase change? | `OnPostInit` early-returns when `GetDefaultHitZone()` is null (`:527-531`), so the weapons path never arms while the scripted path still works — the ideal outcome. | If a hit zone *is* present and inherited, author `m_fBaseHealth` and `m_fPhaseHealth` high enough that no infantry weapon reaches them (D18), and record the numbers. |

**Phase acceptance:** compile 0; both probe prefabs load in Workbench; on a **listen host with one connected client**, `/ruin-structure` swaps the mesh **on both machines**, `/repair-structure` puts it back **on both machines**, and `context.md` records an answer plus evidence for all six questions.

---

### Phase 2 — The destruction API, effects, sound, and sabotage adoption

*Agent: `component-developer`.*

**Estimate:** 8–12 h

| # | Task | Acceptance |
|---|---|---|
| 2.1 | `Scripts/Game/Utilities/OVT_StructureDamage.c` — statics per §3.1. `Resolve()` checks the entity, then walks `GetChildren()`/`GetSibling()` one level (the ramp). `Ruin()`/`Repair()` return **false** when there is no component, and are server-only no-ops elsewhere. Header comment states the D8 contract: *this is a destruction path, not a removal path; `DestroyPlacedItem` remains the only way a structure leaves the world*. | Compile 0; `Ruin(null)` and `Ruin(<a plain prop>)` both return false without an error. |
| 2.2 | `RaiseEffects()` on the component: `SCR_DestructionCommon.PlayParticleEffect_CompleteDestruction(GetOwner(), m_ExplosionParticle, EDamageType.EXPLOSIVE, true)` then the same for `m_SmokeParticle`. Both are `[Attribute]` `ResourceName`s on the component **class** with sensible defaults, so no per-prefab FX authoring is needed. Runs on every machine from `RpcDo_ApplyPhase`. | An explosion and a lingering smoke column appear at the structure's bounding-box centre. |
| 2.3 | Sound in the same handler: `SCR_SoundManagerModule.GetInstance(GetOwner().GetWorld())` → `CreateAudioSource(…)` positional at the bounding-box centre → `PlayAudioSource`. An `[Attribute]` `ref SCR_AudioSourceConfiguration` on the component class. ⚠ **Do not** use `SCR_DestructionUtility.PlaySound` — it hard-returns without `SCR_MPDestructionManager` (§3.11). | Audible from ~100 m on both machines; nothing logs an error when the sound config is unset. |
| 2.4 | Sabotage adoption — `OVT_BaseSabotageBehaviorDeploymentModule.DemolishNextStructure()` (`:322`): replace `resistance.DestroyPlacedItem(target);` with `if(!OVT_StructureDamage.Ruin(target)) resistance.DestroyPlacedItem(target);`. Everything else in the method is untouched: `NotifyOnce`, the `Print`, `m_iDestroyed`, the quota check and the interval reset all stay. | `grep -n "DeleteEntityAndChildren\|NavmeshRebuild" Scripts/Game/GameMode/Objectives/Modules/OVT_BaseSabotageBehaviorDeploymentModule.c` → still empty. |
| 2.5 | Sabotage re-targeting guard: `CollectTargets`' callback must **skip a structure that is already ruined**, or a base with one ruin and one intact tower can "demolish" the ruin again and burn the mission quota on nothing. One `OVT_StructureDamage.IsRuined(entity)` test in `CollectTargetCallback`. | A base whose only buildable is already a ruin completes the mission with "nothing left to demolish" rather than re-ruining it. |
| 2.6 | Init case `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_StructureDamage.c` — `OVT_StructureDamage.Ruin/Repair/IsRuined` on null and on a component-less entity return false without erroring; `Resolve()` finds a component on a child. | Case passes with a recorded proof-it-can-fail preamble. **No `maxAttempts`.** |
| 2.7 | Loc keys in `Language/localization_Overthrow.st` **only** (never the `.conf` exports — Workbench build output). | `.st` braces balanced — an unbalanced brace loses data on the next Workbench save. |

**Phase acceptance:** compile 0; the Init case passes; on a listen host with one client, a sabotage team demolishing a retrofitted structure produces an explosion, smoke, a sound and a ruin **on both machines**, and the existing per-mission notification still fires exactly once.

---

### Phase 3 — Persistence

*Agent: `component-developer`.*

**Estimate:** 4–6 h

| # | Task | Acceptance |
|---|---|---|
| 3.1 | `OVT_BuildableComponentSerializer` → version 2 per §3.5. Write the phase after the existing three fields; read it behind `if (version >= 2)`. On restore call `comp.RestorePhase(phase)` — silent, no effects. Extend the file header with the new field and the reason the `OVT_NavmeshRebuild.Queue(owner)` line stays first. | A v1 save still loads (version guard exercised deliberately once). |
| 3.2 | `RestorePhase(int)` on the component: authority-only, drives `GoToDamagePhase(1,false)` or `RepairToIntact()`, broadcasts with `withEffects = false`. Must tolerate being called before the entity has finished spawning its children. | No effects, no sound, no notification on a load. |
| 3.3 | Persistence round-trip case in `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`, modelled on `OVT_TEST_PersistenceRoundTrip_FuelDepot_LevelSurvivesSave` (`:7959`) — build a real buildable, ruin it via `OVT_StructureDamage`, save, **dirty the live phase back to 0**, reload, re-find the entity by prefab + radius, assert it is ruined. The dirty step is what proves the reload path rather than proving nothing touched it. | Suite still exits 0. |
| 3.4 | A second round-trip case for the inverse: ruin, save, reload, **repair**, save, reload, assert intact. This is the assertion the requirements ask for by name ("a repaired structure not reverting"). | Passes. |

**Phase acceptance:** compile 0; both new round-trip cases pass; a manual listen-host save → quit → **Continue** brings a ruin back as a ruin with no explosion and no sound.

---

### Phase 4 — Retrofit the remaining six prefabs ⚠️ ADVANCED AGENT

*Agent: **`component-developer-advanced`.** Six hand-edited prefabs where a GUID collision fails silently, three inherited damage managers to disable, one root-vs-child special case, and a ruin-mesh choice per structure. The Workbench load is the only gate that can catch a mistake.*

**Estimate:** 10–14 h (plus a user-gated Workbench session)

| # | Task | Acceptance |
|---|---|---|
| 4.1 | **Recruitment Tent** + **Medical Tent** — add `OVT_StructureDestructionComponent` only. Their inherited `SCR_DestructibleBuildingComponent` is **already** `Enabled 0` (`TentUSSR_01_base.et:11-13`); do not re-declare it. | Both load in Workbench. |
| 4.2 | **Garage** (`Garage_E_02.et`) and **Fuel Depot** (`OVT_FuelDepot.et`) — add ours; re-declare `SCR_DestructibleBuildingComponent "{5D7AD092A3CB4B41}"` / `"{5DA867F4F045AA84}"` with `Enabled 0`. ⚠ The Fuel Depot's fuel plumbing (`SCR_FuelManagerComponent`, `SCR_FuelSupportStationComponent`, the fuel serializer binding) must be untouched, and a ruined depot must not keep dispensing fuel — gate `OVT_FillFuelAction`/the refuel actions on phase 0 in 4.6. | Both load; a ruined depot refuses refuelling. |
| 4.3 | **Helipad** (`Helipad.et`) — add ours. It already carries its own `RplComponent` (`:62`). | Loads. |
| 4.4 | **Vehicle Maintenance Ramp** — component on the **child** at `OVT_VehicleMaintenanceRamp.et:71`, plus `SCR_DestructibleBuildingComponent "{67148C0FAD779ACE}" { Enabled 0 }` on that same child. Verify `OVT_StructureDamage.Resolve()` finds it from the root. | Ruining the ramp from the admin command changes the visible mesh. |
| 4.5 | Ruin models for all six, from the §3.10a table. **Ramp, Garage and Fuel Depot have real ruins** — author their `…_Ruin.et` prefabs. **Recruitment Tent, Medical Tent and Helipad have none** — author the generic fallback and be honest about how it reads. Record each choice in `context.md`. | Every structure visibly becomes wreckage; none is invisible, floating, sunk into terrain, or wildly the wrong size. |
| 4.6 | Phase-0 gates on the actions that must not work on a ruin: the Fuel Depot's refuel actions, the Recruitment Tent's `OVT_RecruitFromTentAction`, the Garage/Helipad shop and parking surfaces (`OVT_MainMenuContextOverrideComponent`, `OVT_ShopComponent`, `OVT_ParkingComponent`), and any storage action on a buildable. One shared helper; `CanBeShownScript` returns false while ruined. | Every one of them disappears on a ruin and returns on repair. |
| 4.7 | Sweep: `grep -n "m_bDeleteAfterFinalPhase 0" Prefabs/ PrefabsEditable/` returns **eight** matches, one per buildable. | Exactly eight. Anything less is a structure that will delete itself instead of ruining. |
| 4.8 | Sweep: every retrofitted prefab has an **active** `RplComponent`. | Eight for eight. |

**Phase acceptance:** compile 0; Workbench loads all eight prefabs clean (**user-gated**); the admin command ruins and repairs each of the eight on a listen host with a client attached, with the correct mesh both ways.

---

### Phase 5 — Repair: pricing, difficulty, seam and the held action

*Agent: `component-developer`.*

**Estimate:** 10–14 h

| # | Task | Acceptance |
|---|---|---|
| 5.1 | `Scripts/Game/Data/OVT_RepairPricing.c` — `RepairCost(int baseCost, float buildMultiplier, float repairMultiplier)` and `IsRepairable(int baseCost)` (false for `OVT_ResistanceFactionManager.UNKNOWN_STRUCTURE_COST`). Pure, no world, no config. | Compile 0. |
| 5.2 | `OVT_DifficultySettings` — `[Attribute(defvalue: "0.5", desc: "Repair cost as a fraction of the build cost", category: "Economy")] float repairCostMultiplier;` placed next to `buildableCostMultiplier`. Author it explicitly in **all five** ladder presets — Easy `0.5`, Normal `0.5`, Hard `0.75`, Extreme `1`, Insane `1` — plus `Difficulty_TestWorld.conf`, so nothing depends on the default. | Six `.conf` files edited; `grep -c repairCostMultiplier Configs/Difficulty/` = 6. |
| 5.3 | `OVT_OverthrowConfigComponent` — append `repairCostMultiplier` to the difficulty block of `RplSave`/`RplLoad` **after `fuelPricePerLitre`**, bump `CONFIG_STREAM_VERSION` 4 → 5, and add the "Version 5 appended…" paragraph to the header in the established style. ⚠ Positional stream: write order must equal read order. | Version constant, writer and reader all move together; a mismatch hard-errors rather than shifting (existing behaviour). |
| 5.4 | `OVT_ResistanceFactionManager` — extract the prefab-name join out of `GetStructureCost` into `OVT_Buildable FindBuildableForEntity(IEntity)`; `GetStructureCost` calls it (behaviour unchanged, including `UNKNOWN_STRUCTURE_COST`). Add `int GetRepairCost(IEntity)` and `bool RepairStructure(IEntity, int playerId)` per §3.6. ⚠ Keep the existing header comment about joining on the prefab, not the type string — move it with the code. | `GetStructureCost` still returns the **raw** authored cost; only `GetRepairCost` applies multipliers. |
| 5.5 | `OVT_ResistanceRequestComponent` — `void RepairStructure(RplId)` wrapper + `RpcAsk_RepairStructure(RplId)`. Ladder, in this order: `if(!Replication.IsServer()) return;` → `ResolveOwningPlayerId()` → manager null-bail → `ResolveEntity(entityId)` → **not ruined → reject** → `CallerIsWithin(playerId, entity.GetOrigin(), REPAIR_MAX_DISTANCE)` → `resistance.RepairStructure(entity, playerId)`. `REPAIR_MAX_DISTANCE` is the interaction radius, not the build radius. Every refusal goes through `RejectResistanceRequest(playerId, "repair structure", reason)`. | No identity parameter: `grep -n "RpcAsk_RepairStructure.*playerId\|persId" …` returns nothing. |
| 5.6 | `Scripts/Game/UserActions/OVT_RepairStructureAction.c` — modelled on `OVT_RearmVehicleAction` (the in-repo held + priced precedent): `CanBeShownScript` → ruined; `CanBePerformedScript` → `LocalPlayerHasMoney(price)` with `SetCannotPerformReason("#OVT-CannotAfford")`; `GetActionNameScript` → `"#OVT-RepairStructure ($" + price + ")"`; `PerformAction` → the controller verb; `HasLocalEffectOnlyScript()` → true. Cache the price behind a ~1 s TTL exactly as `OVT_RearmVehicleAction.RefreshCache()` does — this is polled every frame. `GetDifficulty()` can be null on a client before `RplLoad`; guard it. | The label shows a live price and the action greys out when the player cannot pay. |
| 5.7 | Wire the action into all eight prefabs' `additionalActions` with a `Duration` (start at **20 s**) and a sensible `ParentContextList`. Six prefabs need a whole `ActionsManagerComponent` added — copy the shape from `OVT_FuelDepot.et:41-76`, sizing `Offset`/`Radius` to the ruin, not the intact model. The Fuel Depot only needs a new entry. | The action is reachable standing next to every ruin, and the hold ring runs for the authored time. |
| 5.8 | Logic suite `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_RepairPricing.c` — half cost at 1.0/0.5; full cost at 1.0/1.0; the full authored ladder (Easy `.8×.5`, Normal `1×.5`, Hard `1.5×.75`, Extreme `3×1`, Insane `4×1`) over each of the eight real costs; rounding at a `.5` boundary; a zero cost; `IsRepairable(UNKNOWN_STRUCTURE_COST)` is false; repair is never more than the build cost at any authored preset. | Each case carries a recorded proof-it-can-fail preamble. **No `maxAttempts`.** |
| 5.9 | Init case `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_RepairSeam.c` — the request component resolves off `OVT_OverthrowController`; every buildable config entry prices to a positive, finite repair cost via the real join. | Passes with a recorded proof-it-can-fail. |
| 5.10 | Loc keys in the `.st` master: `#OVT-RepairStructure`, `#OVT-RepairStructure-Repaired`. English code fallbacks present. | Braces balanced. |

**Phase acceptance:** compile 0; new Logic and Init cases pass; on a dedicated server a client repairs a ruin, is charged exactly the server-side price, and the structure returns intact **for every connected player**.

---

### Phase 6 — Admin commands and the gear-survival gate

*Agent: `component-developer`.*

**Estimate:** 4–6 h

| # | Task | Acceptance |
|---|---|---|
| 6.1 | Promote Phase 1's debug commands to production on `OVT_AdminCommandsComponent`: `/ruin-structure` (+ `/ruinstructure`) and `/repair-structure` (+ `/repairstructure`), registered in `RegisterChatCommands()` in the existing double-alias style, each with an `RplRcver.Server` handler gated by `SCR_Global.IsAdmin(playerId)`, a `LogLevel.WARNING` line on refusal, and an `AdminCommandRefused` notification — the exact shape of `RpcAsk_GiveMoney` (`:269-306`). | A non-admin is refused and logged; an admin's command works. |
| 6.2 | Target resolution is **server-side from the caller's own character origin**, nearest buildable within a fixed radius. ⚠ Never from client-supplied aim data — no command in `OVT_AdminCommandsComponent` takes an entity today and none should start. | The handler takes no entity parameter. |
| 6.3 | The shared "ruined structures are inert" helper from 4.6 gets its storage case: any storage/inventory user action on a buildable is hidden while ruined. Verify with a container-carrying buildable created for the test if none ships. | The action disappears on a ruin and returns on repair. |
| 6.4 | Gear-survival assertion — an Init or Persistence case that puts an item into a buildable's storage, ruins it, repairs it, and asserts the item is still there. If no shipped buildable is a container, assert the weaker but still meaningful invariant: **a phase change does not delete or re-create the entity** (same `RplId` and same `OVT_BuildableComponent` owner before and after). | Passes with a recorded proof-it-can-fail. |
| 6.5 | `context.md`: record the §3.8 decision (ruin storage closed), and the seams left for `core/storage` and the occupying epic's ammobox question. | Written. |

**Phase acceptance:** compile 0; new cases pass; an admin can ruin and repair any of the eight from chat on a dedicated server, and a non-admin cannot.

---

### Phase 7 — The occupying faction repairs its own ground ⚠️ ADVANCED AGENT

*Agent: **`component-developer-advanced`.** A new deployment behaviour module plus a new deployment config plus a registry entry, inside a framework (`occupying/counter-attacks`) that shipped days ago and whose selection/pooling/persistence rules are exacting.*

**Estimate:** 10–14 h

| # | Task | Acceptance |
|---|---|---|
| 7.1 | **Read-only survey first, and it gates the phase.** Re-verify against HEAD: `OVT_BaseBehaviorDeploymentModule`'s virtual surface and the `super.OnUpdate()` requirement; `OVT_BaseDeploymentModule.CloneModule()`'s hand-copy convention; how `m_bDirectorOnly 0` configs are picked by `EvaluateFactionDeployments()` and `FindBestDeploymentConfig()`, and how `OVT_DeploymentSelection.SelectNextConfigIndex()` orders them (**lowest `m_iPriority` wins**); `RequestDeploymentCollection()`'s one-frame-deferred self-teardown. Record the numbers the existing base-defense configs use so the new config can be priced to sort **after** them. | The table exists in `context.md` before any code. |
| 7.2 | `Scripts/Game/GameMode/Objectives/Modules/OVT_BaseRepairBehaviorDeploymentModule.c` per §3.7. Attributes mirror sabotage's (`m_sModuleName`, `m_fClearRadius`, `m_fSearchRadius`, `m_fMaxBaseDistance`, `m_iHoldSeconds`, `m_iStructuresPerMission`). `OnUpdate` calls `super.OnUpdate(deltaTime)` first. Targets: buildables associated with this base **that are ruined**, cheapest first, via `OVT_StructureDamage.IsRuined` + the existing `GetStructureCost` ordering. Repair with `playerId = -1` (free). `CloneModule()` copies every attribute by hand and copies **no** runtime state. | `grep -n "DestroyPlacedItem\|DeleteEntityAndChildren" <the new module>` → empty. |
| 7.3 | Split the decision half into pure statics so the Init tier can reach it, exactly as sabotage did: `EvaluateRepair(ticksLeft, aliveInside, enemyPresent, …)` and `IsRepairTarget(...)`. | Both are callable on a bare `new` module with no live deployment. |
| 7.4 | `Configs/Deployment/Deployment_ObjectiveRepair.conf` — insertion module (a small team), the repair behaviour module **authored before** any reinforcement module, `OVT_ReinforcementBehaviorDeploymentModule { m_bDeleteOnConditionFail 1 }`, `OVT_BaseControlConditionDeploymentModule { m_bRequireControl 1 }`, **no** `OVT_ObjectiveConditionDeploymentModule`, `m_iAllowedFactionTypes OCCUPYING_FACTION`, `m_iAllowedLocationTypes BASE`, `m_bDirectorOnly 0`, and a `m_iPriority`/`m_fChance`/`m_iMaxInstances` set that keeps it behind the base-defense configs. Register it in `Configs/Deployment/overthrowDeployments.conf`. Fresh GUIDs from the reserved series. | Config resolves and validates. |
| 7.5 | Init case `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ObjectiveRepair.c` — the config resolves, is BASE-typed, has `m_bRequireControl 1`, orders its modules correctly, and resolves a real group prefab for both `US` and `USSR`; the target filter excludes an **intact** structure, a structure at a different base, and a structure at a base the resistance holds; the hold interval pauses (never resets) when an enemy is near. Mirror `OVT_TEST_Init_ObjectiveSabotage.c`'s case shapes. | Passes with recorded proof-it-can-fail preambles. |
| 7.6 | Logic case for `EvaluateRepair`'s arithmetic (interval ticks, difficulty precedence over the fallback attributes) in the shape of sabotage's case D. | Passes. |
| 7.7 | `context.md`: the balance numbers chosen, why they are provisional, and an explicit note that the trigger surface is narrow on shipped data (§3.7). | Written. |

**Phase acceptance:** compile 0; new cases pass; on a listen host — admin-ruin two structures at an occupying-held base, confirm a repair deployment is selected, a team inserts, and the structures come back one per interval, cheapest first.

---

### Phase 8 — Help & documentation sync

*Agent: `help-docs-sync`.*

**Estimate:** 3–4 h

This feature changes what players see and do — structures leave ruins, ruins can be repaired for money, and the price moves with difficulty — so the closing documentation phase is **in scope**.

| # | Task | Acceptance |
|---|---|---|
| 8.1 | Field Manual (`Configs/FieldManual/`): a repair entry covering what a ruin is, that the gear inside survives, the half-cost rule and that it rises at harder difficulties. ⚠ **Every sentence must be traceable to code** — two shipped tips have already described mechanics that did not exist. | Each claim cites a file:line in the agent's report. |
| 8.2 | Tutorial popup (`Configs/Tutorials/`) the first time a player stands at a ruin, if it fits the existing trigger vocabulary. Otherwise skip rather than invent a mechanism. | Registered in an `ActionContexts` block if one is needed — a conf merge silently drops `ActionContexts` while keeping `Actions`, so grep-count the block after editing. |
| 8.3 | Public wiki: the sabotage page stops saying structures are destroyed permanently; a repair section is added. | Pages updated. |
| 8.4 | Loc keys added to `Language/localization_Overthrow.st` **only**; ask the user for a Workbench re-export of the `.conf` exports. | Never edit the `.conf` exports directly — they are build output. |

**Phase acceptance:** the three surfaces agree with the shipped behaviour and with each other.

---

## 5. Key Technical Decisions

**D1 — `SCR_DestructionMultiPhaseComponent`, not `SCR_DestructibleEntity` and not `SCR_DestructibleBuildingComponent`.**
User-decided 2026-08-19, and the code agrees. `SCR_DestructibleBuildingComponent` is one-way — it sinks the building and discards the model with `SetObject(null, "")`; there is no un-destroy. `SCR_DestructibleEntity` authors its phases on the *prefab class* and its `GoToDamagePhase` is `protected` with the comment *"Only call from OnStateChanged, otherwise you have HUGE desync"* (`SCR_DestructibleEntity.c:115-116`) — it is driven by the engine's native destructible-state stream and has no repair path at all. Only the multi-phase component exposes a **public, bidirectional** `GoToDamagePhase(int, bool)`, which is why vanilla's own (campaign) repair uses it.

**D2 — An `OVT_` subclass, because the phase-0 restore can only be done from inside the class, and vanilla shows how.**
`ChangeModel`, `SetDamagePhase`, `SetOriginalResourceName` and `GetOriginalResourceName` are all `protected`. `SCR_DestructionTireComponent` is the proof this is the intended shape: faced with the same hole, vanilla subclassed, overrode `GoToDamagePhase`, special-cased phase 0 and called `ChangeModel` with a model it tracked itself. We copy that. The subclass also gives us one obvious home for the broadcast RPC and the effects, and one file to re-check on a Reforger update. Cost: a dependency on vanilla internals, which is why Phase 1 is a spike and why the subclass gets a vanilla-update checklist in `context.md`.

**D3 — One broadcast RPC is the whole live-replication story; `ReplicateDestructibleState` is not used.**
Two independent reasons, both verified. (i) Vanilla's method sends nothing without hit info (`:462-474`) — surmountable, since `CreateDestructionHitInfo` is public and a synthetic hit info could be manufactured. (ii) **Its receiver cannot repair.** `RPC_ReplicateMultiPhaseDestructionState` (`:421-459`) only calls `GoToDamagePhase` inside `if (phaseData)`, and `GetDamagePhaseData(0)` is null by design — so a phase-0 broadcast makes every client play destruction effects and stay ruined. There is no way to make vanilla's path do a repair. JIP and stream-in are already correct via `OnRplSave`/`OnRplLoad` (`:486`/`:500`), so the only gap is *already-connected clients at the moment of the change* — exactly one reliable broadcast, handling both directions identically. Rejected: (a) an `RplProp` on the component (the phase already round-trips through `OnRplSave`; a second replicated copy could disagree with the mesh actually displayed), (b) routing through `OVT_ResistanceRequestComponent` as a broadcast verb (works, kept as the S2 fallback, but puts entity-mechanism state on the player seam).

**D4 — Binary: exactly one authored damage phase, and `m_bDeleteAfterFinalPhase 0`.**
User-decided. Intermediate states would need an intermediate mesh per structure (art we do not have), a rule for what a half-damaged tower *does*, and a repair price ladder. The default `m_bDeleteAfterFinalPhase 1` is the single most dangerous attribute in the feature — left unset, a structure driven past phase 1 **deletes itself**, which is the exact behaviour being removed. Phase 4 greps for eight zeroes.

**D5 — Effects are scripted in the RPC handler, not authored per damage phase.**
Three reasons, all verified. (i) `SCR_DestructionUtility.SpawnDestroyObjects` early-returns when `owner.GetPhysics()` is null, and several of our structures are static geometry. (ii) It takes a `notnull SCR_DestructionHitInfo`; ours would be a synthetic all-zero object, and `SCR_ParticleSpawnable` with the default `m_bDirectional 1` builds its orientation basis from `-hitInfo.m_HitDirection` — a zero vector, i.e. a degenerate matrix. (iii) Authored FX would mean the same two particle references copy-pasted into eight prefabs. `SCR_DestructionCommon.PlayParticleEffect_CompleteDestruction(entity, particle, damageType, atBoundBoxCenter)` needs neither hit info nor physics and spawns at the bounding-box centre in world space — one call, two `[Attribute]` defaults on the component class, no per-prefab authoring. *(Escape hatch if authored FX are ever wanted: `SCR_DestructionDamageManagerComponent.RPC_DoSpawnAllDestroyEffects()` is a public `[RplRpc(…, Broadcast)]` that passes `new SCR_DestructionHitInfo()` — vanilla itself proves a synthetic hit info is acceptable to that path.)*

**D6 — Sound goes through `SCR_SoundManagerModule` directly.**
`SCR_DestructionUtility.PlaySound` returns early without `SCR_MPDestructionManager.GetInstance()` (`:60-63`), and that entity is **not in the Eden campaign world** (§3.11). Depending on it would make the destruction moment silent on the shipping map and force a world edit whose side effects (building-destruction persistence for the entire map) are far outside this feature.

**D7 — Navmesh comes from vanilla. Do NOT add `OVT_NavmeshRebuild.Queue()` to the ruin or repair path.**
`GoToDamagePhase` already calls `SCR_DestructionUtility.RegenerateNavmeshDelayed(GetOwner())` (`:189`), which is server-only, captures bounds at call time and rebuilds after 1000 ms — `OVT_NavmeshRebuild.Queue()`'s contract, delay included. Adding ours would do the same work twice. This decision is called out loudly because *"queue before delete, never after"* is a strongly-held house rule with a 20-line comment attached (`OVT_ResistanceFactionManager.c:908-923`), and a reader will reasonably want to apply it here. The rule still governs `DestroyPlacedItem`, which is untouched.

**D8 — `Ruin()` returns false for a non-destructible entity, and sabotage falls back to the existing delete.**
The requirements insist there must remain **one** removal path. There does: `DestroyPlacedItem`. This feature adds one *destruction* path beside it, and the fallback is what keeps them from becoming two removal paths — a structure that cannot be ruined is removed exactly the way it is removed today. It also lets the retrofit land prefab-by-prefab without a half-broken intermediate state.

**D9 — The phase lives on the destruction component; `OVT_BuildableComponent` gains nothing.**
One source of truth. A mirrored `[RplProp]` would need its own replication, its own persistence and its own invariant against the mesh actually displayed, and would drift the first time someone drove the component directly. The serializer and the user action both read `GetDamagePhase()` live. This also matches persistence decision rule v2-5 — *does an Overthrow system already rebuild it? then don't persist it* — inverted: the engine already **holds** it, so don't copy it.

**D10 — Serializer version 1 → 2 on the existing binding, with no config change.**
`Configs/Systems/Persistence/Overthrow.conf:175-192` already matches every buildable. The entity is never deleted, so there is no new record, no `SelfSpawn` question, no `Priority` question and no BUG-030 exposure. One int, appended, behind a `version >= 2` guard. Rejected: a new `EntityPersistenceConfig` (an entity gets exactly one, and stealing the buildables' one would drop ownership) and a scripted `PersistenceConfigRule` (the engine never calls scripted `IsMatch` — BUG-018, dead code).

**D11 — One pure pricing function, one rounding.**
`GetBuildableCost()` already rounds; halving its output and rounding again drifts by up to half a currency unit and makes the Logic tier test the wrong thing. `OVT_RepairPricing.RepairCost(baseCost, buildMultiplier, repairMultiplier)` takes the raw `m_iCost` and both multipliers, and is the one place a resource-based cost will later plug in.

**D12 — `repairCostMultiplier` joins the difficulty JIP bitstream; `CONFIG_STREAM_VERSION` 4 → 5.**
The action label shows the price and the local grey-out reads it, which is *precisely* the case `fuelPricePerLitre` was added for at v4 (`OVT_OverthrowConfigComponent.c:703-707`: *"without it a client would draw its own preset's price and refuse (or offer) a refuel the server prices differently, which is the gate disagreeing with the authority"*). The stream is positional and hand-rolled, but it is **versioned and it hard-errors on mismatch** rather than reading shifted garbage (BUG-078's fix), so the hazard is a clean refusal on a version-mismatched client. The server still re-derives and re-charges; the client's number is advisory. Rejected: computing server-side only and showing no price — every other priced action in the mod shows one, and a repair action that silently fails on the hold is worse than one that greys out.

**D13 — Repair rides `OVT_ResistanceRequestComponent`; the manager owns the money and the phase change.**
Thin seams, fat managers. `OVT_ResistanceFactionManager` already owns buildables, the prefab-name cost join and `DestroyPlacedItem`. Repair is one verb; a new controller component for one verb would be the seam sprawl `core/controller-migration` just finished removing.

**D14 — No manager, no `OVT_Global` accessor, no invoker.**
There is no registry of placed structures — every consumer sphere-queries — so there is no global state a manager could hold. The static facade matches `OVT_NavmeshRebuild`, the per-entity state matches every other per-entity component, and `OVT_StructureDamage` is reached by its class name like `OVT_PrefabUtils`. No game-mode invoker because nothing subscribes; adding one when something does is three lines.

**D15 — A ruin's storage is closed, and everything else on a ruin is inert.**
Contents survive untouched (that is free), but the *actions* are gated on phase 0: no refuelling from a wrecked depot, no recruiting from a collapsed tent, no shop from a flattened garage, no reaching into rubble. Repair is the way to get any of it back, which is what makes repair worth its price.

**D16 — The occupying repair module is evaluator-selectable maintenance, not a director objective.**
Sabotage is `m_bDirectorOnly 1` because it is an offensive operation against the resistance's *current* objective. Repairing your own ground is not tied to any objective and should be able to happen at any base the occupying faction holds, so `m_bDirectorOnly 0` + `m_bRequireControl 1` + `BASE`. It is priced to sort behind the base-defense configs so it never starves a defense.

**D17 — Requirements §5 is dropped, permanently, for the reasons in §3.9.**

**D18 — Structures are authored so weapons fire does not realistically ruin them.**
Adding a damage manager makes a structure damageable in principle. Two levers, in preference order: (i) if no hit zone is inherited, `OnPostInit` early-returns at `:527-531` and the weapons path never arms while the scripted path keeps working — free and exact; (ii) otherwise author `m_fBaseHealth` and `m_fPhaseHealth` above any plausible infantry damage total (the Garage's existing `MaxHealth 30000` is the in-tree precedent for how high vanilla goes). S6 decides which applies per prefab. **This is deliberate, not an oversight:** destruction in Overthrow is scripted and timed, and a player who can flatten their own base with a grenade launcher is a different feature with different balance.

---

## 6. Definition of Done

An independent evaluator with no implementation context can verify every item below.

### Functional criteria

- **F1** Standing at an intact structure, there is **no** repair action. Standing at a ruin of the same structure, `Repair (— $N —)` is present, is held for the authored duration, and completes.
- **F2** When a sabotage team demolishes a structure: an explosion and a rising smoke column appear at the structure, an explosion sound is audible from roughly 100 m, and what remains is visible wreckage in the same place — not a gap in the ground. All three happen for a player standing nearby **who did not trigger it**, on a dedicated server.
- **F3** All eight buildables — Guard Tower, Recruitment Tent, Medical Tent, Vehicle Maintenance Ramp, Bunkers, Garage, Helipad, Fuel Depot — leave a ruin. None disappears. None becomes invisible, floating, or a duplicate of another structure's wreckage.
- **F4** Repairing a ruin restores the original model exactly (geometry, materials and collision) and costs `round(m_iCost × buildableCostMultiplier × repairCostMultiplier)`. On **Normal** the player is charged exactly **half** the price they paid to build it. On **Insane** they are charged **the full build price**.
- **F5** A player who cannot afford the repair sees the action greyed out with "Cannot afford"; a player who *can* afford it is charged exactly once, and their balance drops by exactly the displayed amount.
- **F6** A ruined Fuel Depot cannot be refuelled from; a ruined Recruitment Tent cannot be recruited from; a ruined Garage/Helipad offers no shop or parking; no storage action is offered on any ruin. All of them return on repair.
- **F7** Items left inside a structure are still inside it after it is ruined and repaired — same items, same counts.
- **F8** Save → quit → **Continue**: a ruin comes back a ruin, silently (no explosion, no sound), and a structure repaired before the save comes back intact. Neither reverts; neither duplicates.
- **F9** A JIP client joining after a structure was ruined sees the ruin, and can repair it.
- **F10** An admin typing `/ruin-structure` next to a structure ruins it, with effects, for everyone; `/repair-structure` restores it. A non-admin typing either is refused with a message and a server log line.
- **F11** At a base the occupying faction controls with two ruined structures, a repair deployment is selected, a team arrives, and the structures are restored one per hold interval, cheapest first.
- **F12** A structure that carries no destruction component is still **deleted** by sabotage, exactly as today. `DestroyPlacedItem` is still the only code path that removes a structure from the world.

### Quality criteria

- **Q1 — Server-authoritative.** `RpcAsk_RepairStructure` starts `if(!Replication.IsServer()) return;`, resolves the caller from its own controller entity, re-checks proximity and affordability server-side, and takes no identity parameter. The admin handlers take no entity parameter and gate on `SCR_Global.IsAdmin(playerId)` server-side.
- **Q2 — MP correctness.** Every phase change is visible on every machine: the actor's, a bystander client's, a listen host's, and a client that joins afterwards. No client is ever looking at a mesh the server disagrees with.
- **Q3 — Persistence integrity.** No entity is deleted or re-created by a phase change. The `RplId` and the `OVT_BuildableComponent` owner/association are identical before and after ruin and after repair. A pre-feature (v1) save still loads.
- **Q4 — No second removal path.** `grep -rn "DeleteEntityAndChildren" Scripts/` shows no new call site. `grep -rn "NavmeshRebuild" Scripts/Game/Components/Damage/ Scripts/Game/Utilities/OVT_StructureDamage.c` is empty (D7).
- **Q5 — No self-deleting structures.** `grep -rn "m_bDeleteAfterFinalPhase 0" Prefabs/ PrefabsEditable/` returns exactly **8**.
- **Q6 — No silent rejection.** Every refusal on the repair and admin paths emits a `LogLevel.WARNING` naming the player, the request and the reason.
- **Q7 — No regressions.** Building, dismantling, camp/FOB cleanup, sabotage's per-mission notification and quota, refuelling an *intact* depot, recruiting from an *intact* tent, and the shop all behave exactly as before.
- **Q8 — Root classes untouched.** `git diff` shows no change to line 1 of any prefab (§3.10).
- **Q9 — Localization hygiene.** New keys exist in `Language/localization_Overthrow.st` and nowhere else; every consumer has an English code fallback; the `.st` braces are balanced.

### Integration criteria

- **I1 — Sabotage.** `OVT_BaseSabotageBehaviorDeploymentModule` calls `OVT_StructureDamage.Ruin()` and falls back to `DestroyPlacedItem()`. Its per-mission notification, `m_iDestroyed` quota, cheapest-first ordering and "nothing left to demolish" completion all behave exactly as before, and it no longer counts an existing ruin as a target.
- **I2 — The other five `DestroyPlacedItem` callers are untouched.** Player dismantle (`RemovePlacedItem`), camp removal, camp-object cleanup, FOB-area cleanup and `OVT_ObjectiveDirectorComponent.RemoveFOBStructure` still **delete**. Nothing in this feature turns a cleanup into a field of ruins.
- **I3 — Persistence.** The buildables `EntityPersistenceConfig` in `Configs/Systems/Persistence/Overthrow.conf` is unchanged; only `OVT_BuildableComponentSerializer`'s payload grew. A save written before this feature loads without error, and the fuel-depot fuel-level round-trip still passes.
- **I4 — The controller seam.** `OVT_ResistanceRequestComponent` gains exactly one verb; no new component is registered on `OVT_OverthrowController`; `OVT_Global` gains no accessor. `grep -rn "OVT_StructureDamageManager\|GetStructureDamage()" Scripts/` returns nothing.
- **I5 — Difficulty.** `CONFIG_STREAM_VERSION` is 5, writer and reader agree field-for-field, and every other client-read difficulty value still arrives correctly (spot-check a build price and a fuel price on a client after the bump).
- **I6 — Deployments.** The new repair config is registered in `overthrowDeployments.conf`, validates, and does not displace or starve any existing base-defense or sabotage deployment; the sabotage Init suite still passes unchanged.
- **I7 — Economy.** Repair debits through `OVT_EconomyManagerComponent` exactly like every other priced action; no new money path exists, and a `playerId == -1` repair charges nobody.
- **I8 — `core/storage` seam intact.** Nothing in this feature writes to, assumes, or pre-empts an `OVT_StorageComponent`; the only interaction is the phase gate on storage actions, applied through the shared helper.

### Verification method

```bash
# 1. Static gate (free, headless, run after every edit)
cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4" && tools/compile-check.sh      # expect exit 0

# 2. Regression gate — ORCHESTRATOR ONLY, once per completed phase
tools/run-tests.sh --group All                                            # expect exit 0

# 3. Self-deletion gate (Q5)
grep -rn "m_bDeleteAfterFinalPhase 0" Prefabs/ PrefabsEditable/ | wc -l    # expect 8

# 4. Removal-path gate (Q4)
grep -rn "DeleteEntityAndChildren" Scripts/Game/Components/Damage/ Scripts/Game/Utilities/OVT_StructureDamage.c
grep -rn "NavmeshRebuild"          Scripts/Game/Components/Damage/ Scripts/Game/Utilities/OVT_StructureDamage.c
# expect: no matches for either

# 5. Identity gate (Q1)
grep -rn "RpcAsk_RepairStructure.*playerId\|RpcAsk_RepairStructure.*persId" Scripts/Game/Components/Controller/
# expect: no matches

# 6. Root-class gate (Q8)
git diff -U0 -- Prefabs/ PrefabsEditable/ | grep -E "^[+-][A-Za-z_]+ *[:{]"
# expect: no matches (no changed line 1 of any prefab)

# 7. Difficulty ladder gate
grep -c repairCostMultiplier Configs/Difficulty/*.conf                     # expect 1 in each of 6 files
```

**Manual (listen host):** F1, F3–F7, F10, F12, Q7.
**Manual (dedicated server, 2 clients):** F2, F8, F9, F11, Q2, Q6, plus the §7 MP list.

---

## 7. Testing Strategy

### Automated coverage — where each behaviour lands

| Tier | Suite | What it pins |
|---|---|---|
| **Logic** | `OVT_TEST_Logic_RepairPricing.c` (P5) | `RepairCost` across the full authored difficulty ladder × all eight real costs; rounding at a `.5` boundary; zero cost; `IsRepairable(UNKNOWN_STRUCTURE_COST)` false; the invariant that repair never exceeds build at any preset. |
| **Logic** | `OVT_TEST_Logic_ObjectiveRepair.c` (P7) | The repair module's interval arithmetic and difficulty-over-fallback precedence, in the shape of sabotage's case D. |
| **Init** | `OVT_TEST_Init_StructureDamage.c` (P2) | `OVT_StructureDamage` returns false rather than erroring on null / component-less entities; `Resolve()` finds a component on a child (the ramp). |
| **Init** | `OVT_TEST_Init_RepairSeam.c` (P5) | The request component resolves off `OVT_OverthrowController`; every buildables-config entry prices to a positive finite repair cost through the real prefab join. |
| **Init** | `OVT_TEST_Init_ObjectiveRepair.c` (P7) | Config resolves/validates/BASE-typed/`m_bRequireControl 1`/module order/group prefabs for both factions; target filter excludes intact, other-base and resistance-held structures; the hold pauses rather than resets. |
| **Persistence** | `OVT_TEST_PersistenceRoundTripSuite.c` (P3, P6) | A ruin survives save → dirty → reload; a repaired structure does not revert; the entity is not re-created (same `RplId`, same owner). |

House rules: every new case carries a **recorded proof-it-can-fail** preamble (the deliberate fault, the observed failure, the revert); **no `maxAttempts` anywhere**; no case reads state another wrote; no float comparisons.

**What the existing sabotage suite does and does not cover.** `OVT_TEST_Init_ObjectiveSabotage.c`'s six cases are all pure functions or config inspection — none of them reaches the destroy call — so swapping `DestroyPlacedItem` for `OVT_StructureDamage.Ruin` should break none of them. That is also the point: **the swap is invisible to the automated spine and must be play-tested.**

### What the automated spine cannot reach — manual MP play-test

Run on a **dedicated server with two clients** (`tools/launch-server.sh` runs this working tree; a listen host cannot prove the broadcast branches). Nothing rendered — mesh swaps, particles, sound, the hold ring — is reachable by any tier.

1. **The mesh swap replicates.** Client A ruins a structure via admin command; **Client B, standing next to it, sees it change**. Then repair it; B sees it change back. Repeat with the host as the actor (the engine never loops a broadcast back to the sender — BUG-090).
2. **`Rpc()` arity on the wire.** The phase RPC and the repair request both arrive. A wrong argument count compiles clean and dies silently.
3. **The destruction moment reads.** Stand 50 m from a base while a sabotage team works. Without looking at the notification, is it obvious something was destroyed and where? This is the requirement's actual bar and there is no other way to measure it.
4. **All eight structures, both directions.** Ruin and repair every one. Look for: the wrong mesh, a mesh at the wrong height or rotation, children left floating after the root changed, collision that no longer matches what is drawn, a ruin you can walk through.
5. **JIP.** Client C joins after two structures were ruined; sees both as ruins; repairs one.
6. **Continue.** Save with one ruin and one repaired structure, restart the server, reconnect: both come back correctly, **silently**, and the navmesh around them is walkable in the right places.
7. **Money.** Repair on Normal and on Insane; confirm the charge matches the label and the label matches the server. Try with insufficient funds; confirm the grey-out and that no hold is possible.
8. **Rejection paths.** From a client: repair a structure 100 m away; repair an intact structure; a non-admin `/ruin-structure`. Each refused with a log line.
9. **Gear.** Put items in a buildable that has storage, ruin it, confirm the storage action is gone, repair it, confirm the items are all there.
10. **The occupying module.** Admin-ruin two structures at an occupying-held base; watch a repair team insert and restore them cheapest-first; confirm it does **not** run at a base the resistance holds.
11. **Sabotage end-to-end, unassisted.** Let a real sabotage mission run to completion against a base with three structures. Confirm ruins (not gaps), one notification, correct quota, and that the mission completes rather than re-ruining a ruin.
12. **Performance.** Ruin six structures in quick succession with two clients connected. No frame spike, no particle leak, no lingering audio sources.
13. **Fuel Depot specifically.** Its fuel plumbing is the most component-dense of the eight — refuel from it, ruin it, confirm refuelling is refused, repair it, confirm refuelling works and the fuel level survived.

---

## 8. Quality Bar

This is a backend/gameplay feature with a **rendered payoff**. In priority order:

1. **MP correctness above everything.** Every phase change must be visible on every machine — the actor's, a bystander's, a listen host's, and a client that joins later. A structure that is a ruin on the server and intact on a client is worse than no feature at all: it breaks cover, shooting lines and navigation for that player. One reliable broadcast, self-invoked on the server, arity hand-audited, and every branch play-tested on a dedicated server with two clients.
2. **Persistence integrity.** The entity is never deleted, and that invariant is the feature's foundation — the moment something deletes and re-creates a structure, BUG-030's "destroyed thing comes back" class re-opens and the inventory goes with it. The round-trip cases assert the `RplId` and owner are unchanged, not merely that the phase looks right.
3. **The destruction moment must read in-game.** The requirement is *"a player nearby knows something just happened here without reading a notification."* That is a judgement only a play-test can make (§7 item 3). If the explosion is invisible at 50 m, or the ruin is unrecognisable as the thing that was there, the feature has not met its bar even with every gate green. Tune the particles and the ruin mesh until it reads, and treat "good enough, it's only cosmetic" as a failure — legibility *is* the requirement.
4. **One removal path, one destruction path.** `DestroyPlacedItem` stays the only way a structure leaves the world. Nothing in this feature may call `DeleteEntityAndChildren`, and nothing may add a second navmesh queue (D7). The fallback in `Ruin()` exists precisely so the two never blur.
5. **Prefab edits are the highest-risk, lowest-feedback work in the feature.** A GUID collision fails silently; a missing `m_bDeleteAfterFinalPhase 0` turns a ruin into a deletion; a changed root class silently changes what eight manager files can see. Fresh GUIDs from the reserved series, a `grep` before each insertion, the §6 gates, and a **user-gated Workbench load** at the end of Phases 1 and 4.
6. **Server-authoritative validation.** Repair moves money. Every request resolves the caller from its own controller entity, re-derives the price server-side, re-checks proximity and funds, and logs every rejection. The client's price is advisory and is documented as such at the call site.
7. **Documented reasoning at every non-obvious decision.** Several load-bearing comments in this codebase are the only record of a bug's root cause. Three in particular must be written and must be right: why `OVT_NavmeshRebuild` is *not* called on the ruin path (D7), why the phase is not mirrored onto `OVT_BuildableComponent` (D9), and why `m_bDeleteAfterFinalPhase` is `0` (D4).

---

## 9. Dependencies

**Internal (all present, none blocking):**

- `OVT_ResistanceFactionManager` — `DestroyPlacedItem` (`:924`), `GetStructureCost` (`:950`, the prefab-name join), `BuildItem` (`:790`), `UNKNOWN_STRUCTURE_COST`.
- `OVT_ControllerRequestComponent` (`ResolveOwningPlayerId`, `ResolveEntity`, `ShouldRespondLocally`) and `OVT_ControllerComponent<T>.Get()`; `OVT_ResistanceRequestComponent`'s `CallerIsWithin` / `RejectResistanceRequest` ladder.
- `OVT_EconomyManagerComponent` — `PlayerHasMoney(persId, amount)`, `TakePlayerMoney(playerId, amount)`, `LocalPlayerHasMoney(amount)`.
- `OVT_AdminCommandsComponent` — the chat-command registration and `SCR_Global.IsAdmin` gate.
- `OVT_BuildableComponentSerializer` + the buildables `EntityPersistenceConfig` (`Overthrow.conf:175-192`).
- `OVT_RearmVehicleAction` + `OVT_ShopTransactionComponent.RpcAsk_RearmVehicle` — the held + priced + charged precedent, copied rather than re-derived.
- The deployments framework (`OVT_BaseDeploymentModule`, `OVT_BaseBehaviorDeploymentModule`, `OVT_DeploymentConfig`, `OVT_DeploymentManager`, `overthrowDeployments.conf`) and `OVT_BaseSabotageBehaviorDeploymentModule` as the mirror.
- `OVT_FuelDepot.et:41-76` — the in-repo template for adding a whole `ActionsManagerComponent` to a static structure from a delta.

**External:** Arma Reforger **1.8.0.10** — `SCR_DestructionMultiPhaseComponent`, `SCR_DamagePhaseData`, `SCR_DestructionUtility`, `SCR_DestructionCommon`, `SCR_SoundManagerModule`. Reference tree at `/mnt/n/Projects/Arma 4/ArmaReforger/`. **A Reforger update is the feature's main external risk** — the subclass reaches two `protected` members and depends on the shape of `GoToDamagePhase`; `context.md` carries the re-check list.

**Scheduling:** the tree is `v1.5` with concurrent sessions. Re-check `git status` and the highest BUG id before each phase; expect the tree to have moved. The `occupying/counter-attacks` work that owns the sabotage module shipped days ago — re-read `OVT_BaseSabotageBehaviorDeploymentModule.c` at HEAD before Phase 2 and Phase 7 rather than trusting the line numbers here.

**Independent of:** `core/storage` (in planning — seams recorded, nothing shared), `core/options`, the virtualization epic, and the `logistics` epic.

---

## 10. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **The phase-0 model restore does not work.** Vanilla's `GoToDamagePhase(0,…)` provably never schedules a mesh change, and `GetOriginalResourceName()` is empty for a structure restored from a save as a ruin (it is recorded only on the *first* departure from phase 0, which never happened). | Low **now that the pattern is known** — it was the feature's biggest unknown until `SCR_DestructionTireComponent` was found | **Critical** — repair would leave a rubble-looking "repaired" structure | Copy `SCR_DestructionTireComponent.ReturnToInitialDamagePhase` (`:219-235`), vanilla's own working solution: override `GoToDamagePhase`, special-case 0, `ChangeModel` with a **self-cached** intact model. Never rely on `GetOriginalResourceName()`. S1 confirms collision follows the mesh, not just the visual. |
| **R2** | **The broadcast RPC does not reach clients**, or reaches them before the entity exists. | Medium | **Critical** | S2, tested on a listen host with a real client in Phase 1 before anything else is built; documented fallback is to move the message onto `OVT_ResistanceRequestComponent`. ⚠ There is **no** fallback that reuses vanilla's own broadcast — its receiver cannot drive a client to phase 0 (§3.2) — so this RPC has to work. Play-test item 1 re-proves it on a dedicated server. |
| **R3** | **Four of the eight have no ruin model** — Recruitment Tent, Medical Tent, Bunkers and Helipad. Brick rubble where a canvas tent stood will not read as "the wreckage of that tent". | **Certain** (surveyed, §3.10a) | Medium — half the feature's visual payoff is already solved; this is the other half | Four structures use their **real** vanilla ruins. The other four take a fallback (`Bunker_SPS_Ruin` for the bunker, `Rubble_Ruin_01_V2` / `DebrisPile_Concrete_01_Medium` for the rest). Task 1.5 proves **both** routes on the two probes before Phase 4 commits. Play-test item 4 judges legibility per structure; a fallback that reads badly is escalated to a Workbench Resource Browser session with the user — **not** new art, and **not** a reason to ship a tent that leaves brick rubble without saying so. |
| **R4** | **`Enabled 0` does not actually deactivate the inherited `SCR_DestructibleBuildingComponent`**, leaving two damage managers on one entity with undefined hit-zone behaviour. | Low — vanilla ships this exact pattern at `TentUSSR_01_base.et:11-13` | High | S4. Fallback is hosting our component on a child entity for the five affected prefabs, which the ramp already requires anyway — so the pattern is being built regardless. |
| **R5** | **A retrofitted prefab silently breaks.** A GUID collision, a missing `m_bDeleteAfterFinalPhase 0`, or a changed root class breaking the eight `ClassName() == "SCR_DestructibleBuildingEntity"` query sites. | Medium | **Critical** — a structure that deletes itself is the exact bug this feature removes | Reserved GUID series `6B70D0000000xxxx` verified unused; a `grep` before each insertion; the §6 gates 3 and 6 are mechanical; a **user-gated Workbench load** ends Phases 1 and 4; Phase 4 is on an advanced agent. |
| **R6** | **The Bunkers buildable has no active `RplComponent`** — its `.ct` ships `Enabled 0` and neither the vanilla leaf nor Overthrow's override turns it on. If true, built bunkers do not replicate at all today. | Medium-high | High — and it is a **pre-existing** defect, not one this feature creates | S5 confirms it on the shipped tree *before* the retrofit changes anything, so the finding is attributable. The retrofit sets `Enabled 1` either way. A confirmed defect goes to the orchestrator as a bug report. |
| **R7** | **Adding a damage manager makes structures shootable**, and a player flattens their own base with a grenade launcher. | Medium | Medium | D18: prefer inheriting no hit zone (the damage path never arms); otherwise author health above any plausible infantry total. S6 decides per prefab; play-test explicitly tries to destroy one with weapons. |
| **R8** | **Sabotage regresses.** The one changed line sits inside a module that shipped days ago and whose suite deliberately does not reach the destroy call. | Medium | High | The change is one line with an explicit fallback (D8); task 2.5 adds the already-ruined guard that the new state makes necessary; play-test item 11 runs a full unassisted sabotage mission. Re-read the module at HEAD before editing — the tree moves. |
| **R9** | **The occupying repair module has almost nothing to do on shipped data** and ships untested in practice. | **High** | Low | Acknowledged in §3.7 rather than hidden. The admin command is scoped into the same feature specifically so the module can be exercised (play-test item 10). If Phase 7 runs long it is the **first** phase to defer — the API, the ruins, the effects and player repair are the feature; this is its symmetric completion. |
| **R10** | **A Reforger update changes `SCR_DestructionMultiPhaseComponent`.** The subclass reaches two `protected` members and depends on `GoToDamagePhase`'s internal shape. | Medium (per release) | High | One file to re-check, with a vanilla-update checklist in `context.md` naming the exact members and line numbers relied on. The `/update-reforger` flow already diffs the reference tree. |
| **R11** | **The difficulty stream bump strands mismatched clients.** `CONFIG_STREAM_VERSION` 4 → 5 means a 1.4-era client cannot read a 1.5 server's config block. | Low | Medium | Existing, intended behaviour: the reader hard-errors with a version message instead of reading shifted garbage (BUG-078's fix). Version-skewed clients are already unsupported. D12 records the trade. |
| **R12** | **Something depends on `SCR_MPDestructionManager` during the startup window** (up to 10 s on a dedicated server) before the game mode's auto-spawn completes. | Low | Low | D5/D6 avoid every path that needs it; every vanilla call site is defensive and early-returns. S3 confirms the auto-spawn. Never place the prefab in a world layer (§3.11). |
| **R13** | **Concurrent sessions on `v1.5`.** Other agents are editing `Configs/`, `Language/` and `Scripts/` on this same tree. | High | Medium | Re-baseline (`git status`, highest BUG id) before each phase; never run the suites while another session is writing `Configs/`/`Language/`; never touch the localization `.conf` exports (Workbench build output — `.st` master only). |

### Bug-report candidates for the orchestrator — do not file from this plan

1. **Upstream (BI):** `SCR_DestructionMultiPhaseComponent.GoToDamagePhase(0, …)` sets the phase to intact but never restores the intact mesh — `GetDamagePhaseData(0)` returns null by design (`:69`) and the guard at `:227-229` returns before `CallLater(ChangeModel, …)`. The recorded `GetOriginalResourceName()` (`:141`, set at `:191-196`) is read nowhere in the shipping tree. `SCR_DestructionTireComponent` works around it privately; nothing else can.
2. **Upstream (BI):** `SCR_DestructionMultiPhaseComponent.ReplicateDestructibleState()` (`:462-474`) silently sends nothing when `GetDestructionHitInfo()` is null, so any script-driven phase change is invisible to already-connected clients. Compounding it, the receiver `RPC_ReplicateMultiPhaseDestructionState` (`:421-459`) gates the phase change on `GetDamagePhaseData(phase)` being non-null, so **phase 0 can never be replicated at all** — clients get destruction FX and no repair.
3. **Upstream (BI):** the whole campaign repair flow is non-functional in 1.8.0.10, in **four** independent ways: (a) `SCR_CampaignRepairEntityUserAction.CompositionInit()` has its entire body commented out (`:15-50`), so `m_DestructionMultiphaseComp` is never assigned and `CanBeShownScript` can never return true; (b) `SCR_MPDestructionManager.RegisterDynamicallySpawnedDestructible` (`:298`) has **zero callers**, so `FindDynamicallySpawnedDestructibleByIndex` always returns null and `SCR_CampaignNetworkComponent.RpcAsk_RepairComposition` early-returns before reaching `RepairEntity()`; (c) `SCR_CampaignServiceEntityComponent.RepairEntity()` (`:71-80`) calls `ReplicateDestructibleState(true)` — a `bool` implicitly converted into the `int damagePhase` parameter, i.e. "replicate damage phase 1, not silently" immediately after repairing to phase 0; (d) even without (c) it could not work, per item 2. **Consequence for this plan: `SCR_CampaignRepairEntityUserAction` is not a working reference implementation and must not be treated as one** — `OVT_RearmVehicleAction` is the action precedent and `SCR_DestructionTireComponent` is the mechanism precedent.
3a. **Upstream (BI), minor:** `SCR_TireReplacementManagerComponent.InitReplace()` calls `GoToDamagePhase()` with one argument against a two-argument signature and references an undeclared local `myDamage`, under the author's own `//TODO … This makes no sense` comment.
4. **Overthrow (shipped):** **15 prefabs still carry dead `EPF_PersistenceComponent` blocks** after the EPF → vanilla persistence migration, including two of the eight buildables (`E_Sandbag_01_bunker_plastic_foundation_camonet.et`, `Helipad.et`). Harmless but dead; `grep -rl EPF_ --include=*.et` for the list.
5. **Overthrow (shipped), pending S5:** the Bunkers buildable may have **no active `RplComponent`**, which would break dismantling it (`RpcAsk_RemovePlacedItem` needs an `RplId`) as well as any replicated state. Same family as BUG-088/BUG-132.

**Not a bug, corrected here:** the Helipad prefab reference in `buildables.conf` is **not** dangling. `PrefabsEditable/Auto/Structures/Military/Camps/HelipadImprovised_01/Helipad.et` exists in the Overthrow repo (it is an Overthrow-authored file, which is why it is absent from the vanilla tree) and its parent `{CF1FFB7BAA1A0769}Prefabs/Structures/Military/Camps/HelipadImprovised_01/HelipadImprovised_US_01.et` exists in 1.8.0.10.

---

## Agent Routing Summary

| Phase | Agent | Why |
|---|---|---|
| 1 — Engine spike | **`component-developer-advanced`** | Production code against undocumented vanilla internals; two hand-edited prefabs; produces the decision record every later phase depends on. |
| 2 — API, FX, sound, sabotage | `component-developer` | Straightforward once S1–S6 are answered. |
| 3 — Persistence | `component-developer` | One serializer version bump on an existing binding. |
| 4 — Retrofit six prefabs | **`component-developer-advanced`** | Six silent-failure-prone prefab edits, three inherited damage managers to disable, a root-vs-child special case, six ruin-mesh choices. |
| 5 — Repair | `component-developer` | New RPC + user action + difficulty stream bump, all with close in-repo precedents. |
| 6 — Admin + gear gate | `component-developer` | Small, pattern-following. |
| 7 — Occupying repair module | **`component-developer-advanced`** | New module + config + registry entry inside a just-shipped framework with exacting selection/pooling/persistence rules. |
| 8 — Help & docs | `help-docs-sync` | Player-facing behaviour changed; tutorial, Field Manual and wiki must agree. |
