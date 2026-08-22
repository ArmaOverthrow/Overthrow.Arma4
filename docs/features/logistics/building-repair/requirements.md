# Building Repair — DESCOPED, BUILT AS A SMALL CHANGE 2026-08-22

> **This was never built as a feature.** By the time `core/damage` and `logistics/resources` were both
> done, everything this document specced except two small pieces already existed — the destructible
> ruin, the held repair action, the money price, the difficulty ladder, the persistence, and the three
> position-based resource helpers. User call 2026-08-22: *"now that all the systems are built we don't
> need a whole feature… it only needs to add the Requirements action and wire the repair action up to
> use nearby crates."*
>
> **What was built (2026-08-22), directly on the two existing seams:**
> - `OVT_ResourceRules.RepairRequirement(scaledQty, repairMultiplier)` — the repair share of a build
>   requirement, floored at 1 and capped at the build. Pure; Logic case
>   `OVT_TEST_Logic_ResourceRules_RepairRequirementIsAShareOfTheBuild`.
> - `OVT_RepairRequirementsAction` + `OVT_RepairRequirementsReader`
>   (`Scripts/Game/UserActions/OVT_RepairRequirementsAction.c`) — the ruin's "Repair Requirements"
>   readout, the counterpart of `OVT_SiteRequirementsAction`, shown only on a ruin that costs materials.
> - `OVT_RepairStructureAction` — advisory resource gate with a reason naming the short resource.
> - `OVT_ResistanceFactionManager.RepairStructure()` — validates money **and** materials, repairs, then
>   consumes from the nearby piles and charges. Server-initiated repairs (`playerId == -1`) stay free.
> - Three `.st` keys + a `RepairNeedsMaterials` notification; the Requirements action authored on all
>   ten destructible buildables.
> - ⚠ **Found on the way:** `OVT_Barracks.et` and `OVT_Warehouse.et` carried no `ActionsManagerComponent`
>   at all, so both were ruinable and **impossible to repair**. Both now carry the repair pair.
>
> **Which buildables cost materials to repair (checked 2026-08-22):** Garage, Helipad, Warehouse,
> Barracks. The other six — Guard Tower (its requirements were removed by the user), Recruitment Tent,
> Medical Tent, Vehicle Maintenance Ramp, Bunkers, Fuel Depot — repair for money alone and the
> Requirements action hides itself on them.
>
> **Deliberately not built** from the spec below: a separate Init case per buildable for repair pricing
> (case F already spawns every config-listed buildable), and the `buildableRepairCostMultiplier` split
> (one ladder, as specced). The original requirements are kept below for the record.

---

**Epic:** logistics
**Created:** 2026-08-11 · **Rewritten:** 2026-08-20 against `core/damage` (In Progress, 16/70 — `docs/features/core/damage/implementation.md`), which now owns everything this feature originally planned except the resources.

## Overview

`core/damage` makes Overthrow structures destructible **in place**: a buildable driven to `EDamageState`/phase 1 by `OVT_StructureDamage.Ruin()` stays in the world as the **same entity** with a ruin mesh (`OVT_StructureDestructionComponent : SCR_DestructionMultiPhaseComponent`, `m_bDeleteAfterFinalPhase 0`), its functions are gated off while ruined (D15), the phase persists in `OVT_BuildableComponentSerializer` **v2**, and a held `OVT_RepairStructureAction` restores it through `OVT_ResistanceFactionManager.RepairStructure(entity, playerId)` for `round(m_iCost × buildableCostMultiplier × repairCostMultiplier)` money (`repairCostMultiplier`: Easy/Normal 0.5 → Hard 0.75 → Extreme/Insane 1). The occupying faction repairs its own ground through a deployment module (server-initiated, `playerId == -1`, free).

`building-repair` adds the **resources**: repairing a ruin also requires a difficulty-scaled fraction of the buildable's resource requirement (from `logistics/resources`) to be sitting in crate piles nearby, and consumes them. There is no separate repair mechanic, no site prefab spawned at the ruin, no second detection path and no new persistence — the feature is a resource gate on the seam `core/damage` left open (§3.12: *"the change is confined to `OVT_ResistanceFactionManager.RepairStructure()`"*) plus the readout that makes the requirement legible on the ruin.

## Requirements

- **Resource-gated repair on the existing seam.** `OVT_ResistanceFactionManager.RepairStructure()` gains, before the money check, a **resource requirement check and consumption** using `resources`' construction machinery: requirement = the buildable's (difficulty-scaled) resource requirement × the repair fraction; satisfied when the summed contents of crate piles within the configured radius of the ruin cover it; consumed server-side in deterministic order; then the existing money charge and `OVT_StructureDamage.Repair()` proceed unchanged. Ordering: validate resources **and** money, consume resources, repair, charge — a repair must never consume resources and then fail on money, or charge and then fail on resources.
- **A buildable with no resource requirement repairs exactly as `core/damage` shipped it** (money only). Additive, like construction.
- **One repair fraction for money and resources.** Reuse `OVT_DifficultySettings.repairCostMultiplier` (shipped by `core/damage`) for the resource fraction rather than adding a parallel `buildableRepairCostMultiplier` — one knob, same ladder, same accessor shape. (If play balance later wants them apart, split then.) Resource repair = `round(scaledRequirement × repairCostMultiplier)` per resource, rounding **never** turning a non-zero requirement into zero and never exceeding the construction requirement; composition with `resources`' `buildableResourceCostMultiplier` is stated and asserted.
- **The ruin shows what it needs.** `OVT_RepairStructureAction`'s name/reason and `resources`' requirements readout (per resource: needed vs available nearby, what is short) are available on the ruin exactly as on a construction site; the action is performable only when both resources and money are satisfied, and the reason says which is missing. The client's copy is advisory — the server re-derives requirement, availability and price.
- **`resources`' construction machinery must therefore be callable against an arbitrary entity position/buildable**, not hard-wired to a spawned site entity: "nearby availability for (position, requirements)" and "consume (position, requirements)" are the two entry points this feature calls. Recorded here so `/plan-feature logistics/resources` builds it that way.
- **Server-initiated repairs stay free** (`playerId == -1`: the occupying faction's module, `/repair-structure`) — no resources, no money, per `core/damage`'s convention.
- **Persistence: nothing new.** The ruin phase is already saved; resources stay in the piles until the single repair action consumes them, so a half-supplied repair has no state to persist.
- **Multiplayer:** validation and consumption are server-side through the existing `OVT_ResistanceRequestComponent.RpcAsk_RepairStructure` ladder; the piles' replicated contents and `core/damage`'s broadcast phase message cover clients and JIP.
- **The buildable warehouse** (`resources` §E) must be part of `core/damage`'s retrofit and ruin-gate (destruction component authored, storage actions hidden while ruined) — owned by `resources` when it creates the prefab; this feature only checks it repairs like the rest.
- New strings (reasons naming the missing resource) in `Language/localization_Overthrow.st`.
- Automated coverage: **Logic** — resource repair fraction maths (half on Normal, full on Insane, never zero for a non-zero requirement, never above the construction requirement, composition order with the construction multiplier), the satisfied/short predicate through the shared helper; **Init** — the seam still resolves and every buildable with requirements prices to a positive, finite resource repair cost; play-test — a ruined Garage cannot be repaired without the piles, can with them, the piles are debited and the structure returns intact for every client.

## Dependencies

- **`core/damage`** — must be complete: `OVT_StructureDamage`, `OVT_StructureDestructionComponent`, `OVT_RepairStructureAction`, `OVT_ResistanceFactionManager.RepairStructure/GetRepairCost/FindBuildableForEntity`, `OVT_RepairPricing`, `repairCostMultiplier`, serializer v2, the ruin gate, the occupying repair module.
- **`logistics/resources`** — resource definitions, the ledger, crate piles, the construction requirement list + `buildableResourceCostMultiplier`, and the position-based availability/consume helpers.
- `resistance` epic — `OVT_BuildableComponent`, `buildables.conf`.
- `core/persistence` — nothing new (recorded to make the point).

## Dropped from the 2026-08-11 version (now `core/damage`'s, or moot)

- Destruction audit per buildable, server-side detection, the self-deleting-prefab case (`m_bDeleteAfterFinalPhase` is authored `0` everywhere), per-building disabled-state gating, the destroyed flag + `OVT_BuildableComponentSerializer` v2, the "spawn a bare site at the recorded transform" path, the separate `buildableRepairCostMultiplier`, and "XP for repair" (decide in `core/damage` or planning; not a resource question).
- The epic's earlier rationale *"a ruin is a construction site because engine destruction cannot be reversed"* — `core/damage` reversed it (the subclass restores the intact mesh and broadcasts the phase), so the ruin stays a ruin entity that **borrows** the site's requirements/consume machinery rather than becoming a site.

## Out of Scope

- Reversing destruction differently, partial damage states, repair timers beyond the held action's authored duration, repairing vanilla town buildings (dropped permanently by `core/damage` §3.9), vehicles, AI/recruit repairs, occupying-faction repairs costing resources.
