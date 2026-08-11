# Building Repair — Requirements

**Epic:** logistics
**Created:** 2026-08-11

## Overview

`building-repair` makes Overthrow's built structures answerable to the war around them: when a building is destroyed it stops working, and getting it back means hauling resources to the wreck exactly as building it did the first time. The design deliberately reuses `resource-construction` rather than inventing a parallel repair mechanic — **the ruin *is* a construction site**, carrying the same requirements action, the same nearby-crate-pile check and the same Build action, at a difficulty-driven discount.

## Requirements

- **Audit first.** Establish which of the buildables in `Configs/Resistance/buildables.conf` are actually destructible and how. Overthrow's prefabs (`OVT_GuardTower_01.et`, the FOB tents) declare no destruction components of their own — they are same-GUID deltas, so behaviour is inherited from each vanilla base prefab and will differ per buildable. Record the finding per buildable in `context.md`; it drives every decision below.
- **Detect destruction server-side.** An Overthrow building entering `EDamageState.DESTROYED` (or its destruction component reaching its final phase) is recognised by its `OVT_BuildableComponent` and flagged destroyed. Detection must handle **both** cases:
  - the entity survives as a ruin/rubble mesh, and
  - the entity **deletes itself** (`m_bDeleteAfterFinalPhase` on `SCR_DestructionMultiPhaseComponent`), leaving nothing to attach to.
- **A destroyed building is disabled.** Its function stops working while destroyed. This is **per-building integration work**, not one flag — each functional buildable must have its own capability gated at the point that capability is offered:
  - Recruitment Tent → recruitment refused (`OVT_RecruitManagerComponent`)
  - Medical Tent → healing/medical function refused
  - Garage / Vehicle Maintenance Ramp → vehicle services refused
  - Warehouse (from `logistics/resource-storage`) → storage access refused
  - Guard Tower / Bunkers / Helipad → whatever they currently confer (spawn point, cover, aircraft landing) withdrawn
  Enumerate the full list during planning; anything conferring a benefit must be covered or explicitly documented as unaffected.
- **The ruin is the construction site.** A destroyed building presents the same interaction surface as an unbuilt one: a requirements action listing what is needed versus what is available in nearby crate piles, and a Build (repair) action that unlocks only when satisfied. Reuse `resource-construction`'s machinery — do not build a second requirements/consumption path.
  - Where the entity survives, the ruin itself carries the site (visually the wreck stays put).
  - Where the entity self-deleted, a bare construction site is spawned at the destroyed building's recorded transform, so the flow is identical from the player's side.
- **Repair cost is difficulty-driven.** The fraction of the buildable's original resource requirement charged to repair it is a **difficulty setting**, not a per-buildable value and not a constant. Follow the shipped multiplier pattern exactly:
  - Add a field to `OVT_DifficultySettings` (`Scripts/Game/Configuration/OVT_DifficultySettings.c`) in the `Economy` category, alongside `placeableCostMultiplier` / `buildableCostMultiplier` / `realEstateCostMultiplier` — e.g. `buildableRepairCostMultiplier` with an `[Attribute(defvalue: "0.5", ...)]`.
  - Add the matching accessor to `OVT_OverthrowConfigComponent` in the same shape as `GetBuildableCost()` (`m_Difficulty.buildableCostMultiplier * buildable.m_iCost`, rounded), so callers never touch `m_Difficulty` directly.
  - Difficulty `.conf` files only override what they change — `Difficulty_Normal.conf` and `Difficulty_TestWorld.conf` list no multipliers at all and inherit the attribute default. Only the tiers that want a non-default repair fraction (`Difficulty_Easy.conf` sets 0.8 for the others; Hard / Extreme / Insane likewise) need editing. Choose per-tier values during planning; do not add a redundant line to every file.
  - Applies to the **resource** requirement. Whether the money side of a repair is also multiplied depends on what the money question below settles.
- **Repair scaling stacks on construction scaling.** `resource-construction` scales the base resource requirement by its own difficulty multiplier; repair's multiplier applies **on top of that already-scaled figure**, not on the raw config number. Confirm the composition order and the rounding rule when planning, and assert it — a repair on Easy must cost less than the same repair on Insane, and both less than their own initial build.
- **Completing a repair** removes the required resources from nearby piles, deletes the ruin/site, and spawns a fresh building through the same path `resource-construction` uses, restoring ownership, base association and function. XP reward for a repair is a planning decision — decide and state it.
- **Money:** decide during planning whether repair costs money in addition to resources, and stay consistent with whatever `resource-construction` settled for the placement-vs-completion charge.
- **Destroyed state persists.** `OVT_BuildableComponentSerializer` is currently **version 1** and stores only owner persistent id and base association. It needs a **version 2** payload carrying the destroyed flag (and, for self-deleting prefabs, whatever is needed to recreate the site at the right transform). Version 1 saves must still load — a missing field means "not destroyed", following the existing `version < 1` precedent in that serializer.
- A building destroyed in one session must still be destroyed, disabled and repairable after a save/load.
- **Multiplayer:** destroyed state replicates to all clients including join-in-progress, so a late joiner sees the ruin as a repairable site and cannot use the disabled function. Detection and repair are validated server-side; the client never asserts a building is destroyed or repaired.
- New strings (repair action, disabled reasons) go in `Language/localization_Overthrow.st`.
- Automated coverage: Logic-tier assertions for the repair-requirement fraction maths and the destroyed→disabled predicate; Persistence-tier assertion for a destroyed building round-tripping through the version-2 serializer, including a version-1 save still loading as not-destroyed.

## Dependencies

- **`logistics/resource-core`** — resource definitions and the ledger.
- **`logistics/resource-transport`** — crate piles supply the repair.
- **`logistics/resource-construction`** — this feature is largely a second entry point into that feature's requirements/site/Build machinery. It must be complete first.
- **`logistics/resource-storage`** — not a code dependency, but going last means the buildable warehouse is included in the disabled-state gating pass rather than retrofitted. Can be implemented in parallel with storage if that ordering changes.
- `resistance` epic — `OVT_BuildableComponent`, the build/spawn path, and the removal flow.
- `core/persistence` — `OVT_BuildableComponentSerializer` version bump.
- Other epics' code for the disabled gating: `OVT_RecruitManagerComponent`, vehicle services, and any system that reads a built structure's presence as a benefit.

## Out of Scope

- **Reversing vanilla destruction in place.** No `GoToDamagePhase(0)` / `SetHealthScaled(1)` healing of a damaged entity. The engine's destruction is one-directional in practice, and the ruin-is-a-site design avoids depending on it. If a future feature wants smooth in-place repair, this is where to revisit.
- **Partial or gradual damage states.** Only destroyed-versus-not matters. A building at 40% health is fully functional; there is no degraded mode, no repair of undestroyed damage.
- **Repair time, timers or progress bars** — requirements satisfied → action → repaired, matching `resource-construction`.
- **Making non-destructible buildables destructible.** If a vanilla base prefab has no destruction, that buildable is simply never destroyed; do not add destruction components to make it so.
- **Destruction of world/civilian buildings, houses or purchased real estate.** Only Overthrow-built structures carrying `OVT_BuildableComponent` are in scope.
- **Repairing vehicles** — unrelated system, already partly served elsewhere.
- **Recruits or AI performing repairs.**
- **Occupying-faction AI deliberately targeting resistance structures.** Buildings get destroyed by whatever already damages them; no new attack behaviour.
