# Damage & Destruction — Requirements

**Epic:** core
**Created:** 2026-08-19
**Status:** 📄 Requirements only — not planned, not started
**Requested by:** the author, during `occupying/counter-attacks` play-testing

---

## Why this exists

Overthrow currently has exactly one way to remove a structure from the world: **delete it**. `OVT_ResistanceFactionManager.DestroyPlacedItem()` queues a navmesh rebuild and then calls `SCR_EntityHelper.DeleteEntityAndChildren()`. The building stops existing.

That is adequate for a player dismantling their own tent. It is poor as *destruction*:

- A sabotaged structure **vanishes silently**. No sound, no effect, no debris — the player walks back to a gap in the ground and has to infer what used to be there. `occupying/counter-attacks` shipped a per-mission notification precisely because there was nothing else to tell them.
- **Nothing can be repaired**, because nothing is damaged — it is either whole or gone. A base that loses a guard tower has lost it permanently, which makes sabotage a pure subtraction with no counterplay beyond preventing it.
- **The engine already does better.** Reforger ships a multi-phase destruction system (`SCR_DestructionMultiPhaseComponent`, `SCR_DestructibleEntity`, `SCR_BuildingDestructionManagerComponent`) that swaps models through damage phases, and **1.8 began persisting building destruction states**. Overthrow uses none of it.

So this feature builds the damage and destruction layer the rest of the mod can call, and gives the player something to do about a ruin other than mourn it.

---

## Scope

### 1. A destruction API other systems call

A single seam that means "damage or destroy this thing, properly", which existing systems adopt in place of raw deletion. The first consumer is **`occupying/counter-attacks`' sabotage**, which today calls the shared delete path.

⚠ **It must remain one path, not two.** The removal helper it replaces was extracted specifically so that four copies of the navmesh-then-delete pair became one — do not reintroduce a second way to remove a structure.

### 2. The moment of destruction is audible and visible

At minimum an **explosion sound** and **rising smoke** at the site. A full explosion effect is wanted; the sound and smoke are the floor, because the point is that a player nearby knows *something just happened here* without reading a notification.

⚠ Destruction in Overthrow is **scripted and timed**, not the result of AI weapons fire — a sabotage team holds a position and structures come down on a timer. So the effect must be raised at the site by the destruction API, not produced as a side effect of someone shooting the building.

### 3. Destroyed structures switch model rather than disappear

Use the base game's destruction systems to move a structure to a **destroyed/ruin state** instead of deleting the entity. What is left should read as *wreckage of the thing that was there*, not absence.

### 4. A repair action on the ruin

A held action that restores the structure, costing **half** its build cost.

- **Difficulty-scaled**: half cost at easier settings, rising to **1× at harder settings**.
- **Later**: linked into the **logistics** epic so repair consumes actual resources that have to be delivered, rather than money alone.

### 5. Repairable vanilla buildings, if achievable

**If possible, override every destructible building in the game to offer the repair option** — not only player-built structures. This is newly worth doing because **Reforger 1.8 persists building destruction states**, so a town wrecked over a long campaign now *stays* wrecked, and there is currently no way for anyone to rebuild it.

⚠ Marked "if possible" deliberately: feasibility is a planning question, not a settled requirement. See Open Questions.

---

## Known constraints (established, do not re-derive)

- **Ordering is load-bearing.** `OVT_NavmeshRebuild.Queue(entity)` measures bounds at **call** time and issues a merged rebuild ~1 s later; it must run **while the structure still stands**, then the removal. Reversed, there is nothing to measure and the navmesh carve is permanent. Any new destruction path inherits this.
- 🔴 **A deleted entity is never saved; a replacement is new world content.** The current delete sidesteps persistence entirely — that is *why* destroyed structures stay destroyed. A ruin that replaces them is something that must be tracked, or it vanishes on the next load and re-opens **BUG-030**'s "destroyed thing comes back" class. **This is the single biggest design risk in the feature.**
- **`SCR_DestructionMultiPhaseComponent.SetDamagePhase()` is `protected`.** Driving a vanilla destructible to a chosen phase — or back, for repair — is not simply a public call. Expect a modded subclass or a damage-manager route.
- **Overthrow prefabs routinely lack vanilla components.** Several shipped prefabs are missing components their vanilla base carries; a destructible-looking building may not actually have the destruction component. Diff against the vanilla base before assuming.
- **Same-GUID prefab overrides are deltas, not replacements** — relevant if vanilla buildings are to be overridden en masse.
- **Cost data exists but its join is not obvious.** `OVT_ResistanceFactionManager.GetStructureCost()` joins a live entity to its config **by prefab ResourceName** — deliberately, because the type string and the config's display name disagree for **seven of the eight** shipped buildables. Repair pricing must use that join, not `m_sName`.

---

## Out of scope

- **Vehicle damage and repair.** Vehicles have their own systems and their own manager; this feature is about structures and buildings.
- **Character damage.** Entirely vanilla's.
- **Resource-dependent repair.** Explicitly a *later* addition, once the logistics epic can deliver materials. This feature ships money-cost repair and leaves the seam.
- **Rebalancing what sabotage targets.** `occupying/counter-attacks` owns that and has just settled it (buildables only).

---

## Open questions for planning

1. **Can vanilla buildings be given a repair action at all, and at what cost?** Every destructible building in the game is a large surface. Is there a component that can be added globally, a modded subclass of the destruction component, or does it need per-prefab overrides? **If it turns out to need hundreds of prefab deltas, that is a "no" and the feature should ship player-built repair only.**
2. **How is a ruin persisted?** Does 1.8's building-destruction persistence cover mod-authored structures, or only vanilla buildings? If it covers ours, does the repair need to write back through it? This decides whether §3 is cheap or the hardest part of the feature.
3. **Repair by whom, and can the occupying faction do it?** The requirement describes a player action. Should the occupying faction repair its own damaged structures — and if so, is that a deployment behaviour module, mirroring sabotage?
4. **What happens to a structure's contents?** A destroyed ammobox is the reason `occupying/counter-attacks` excludes ammoboxes from sabotage entirely, pending gear recovery. If destruction becomes recoverable, does the gear survive in the ruin? **That would close the ammobox question**, which is currently deferred in the occupying epic.
5. **Partial damage, or binary?** Multi-phase destruction implies intermediate states. Is a half-damaged guard tower a thing Overthrow wants, or is whole/ruined enough?

---

## Dependencies

- **Consumer: `occupying/counter-attacks`** — sabotage is the first caller, and two of its deferred ideas are **superseded by this feature**: *"sabotage should be audible and visible"* and *"a sabotaged structure should leave a wreck or ruin variant"* (both recorded in `docs/features/occupying/epic-overview.md`).
- **`resistance/building`** — owns buildables, their costs and the player's own dismantle path, which shares the removal helper.
- **`logistics` epic** — the later resource-dependent repair.
- **`core/persistence`** — whatever answer question 2 gets.

---

## Testing expectations

- Logic-tier for the repair-cost maths, including the difficulty scaling to 1×.
- Persistence-tier for the ruin surviving a save → **Continue**, and for a repaired structure not reverting.
- Effects, model switching and anything involving a rendered ruin are **play-test gated** — no tier renders anything.
