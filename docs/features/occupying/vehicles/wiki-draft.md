# Wiki draft: Enemy Armour (player-facing)

**Status:** UNPUBLISHED. The `wikijs` MCP server was **not attached** to the Phase 7 session, so nothing was
searched for, created or updated on https://wiki.armaoverthrow.com. This file is the draft to publish.

**Proposed path:** `enemy-armour`
**Proposed title:** Enemy Armour

**Before publishing, do these four things:**

1. `wikijs_connection_status`, then `wikijs_search_pages` for `occupying`, `patrols`, `counter attack`,
   `armour`, `vehicles`. If a page already describes the occupying faction's forces, **update it in place**
   rather than creating this one beside it. A section on an existing "occupying faction" page is the
   preferred outcome.
2. `wikijs_get_page_children` on the Documentation root to see where flat player pages sit
   (`recruits`, `difficulty`, ...) and match that level.
3. Re-check every number below against the shipped `.conf` at publication time. Every figure here was
   verified on 2026-08-23 and each is cited to a file in `context.md` → *Phase 7*.
4. This page is **player-facing**: no class names, no GUIDs, no attribute names. The modder-facing view of
   the same machine belongs under `development-documentation/`; it is not written yet.

---

## Enemy Armour

The occupying faction does not only walk. As a campaign turns against it, it starts putting armed vehicles
on the road: at first a jeep with a machine gun on the back, later a gun truck or a scout car, and late in a
campaign a wheeled armoured carrier with a cannon or a heavy machine gun. Which one turns up is decided by
the faction's threat level and by what its budget covers at the moment it is fielded.

### The ladder

Each faction fields three rungs of armed vehicle, unlocked by threat:

| Rung | USSR | US | Unlocks at threat | Costs |
|---|---|---|---:|---:|
| Light | UAZ-469 with a PKM | M151A2 with an M2HB | 0 | 25 |
| Medium | BRDM-2 | M1025 with an M2HB | 400 | 70 |
| Heavy | BTR-70 | LAV-25 | 900 | 120 |

Two things narrow that down in play:

- **Money.** Each mounted force carries its own vehicle budget, and any rung dearer than that budget is
  refused. A force whose budget covers a scout car fields a scout car even when the heavy rung is unlocked.
- **Difficulty.** Campaign difficulty stretches or compresses the whole threat scale. On Easy every
  threshold is doubled, on Normal it is unchanged, and on Hard it is halved, on Extreme cut to 0.35 and on Insane to a
  quarter. So the heavy rung wants threat 1800 on Easy and 225 on Insane.

If nothing on the ladder can be bought, the force still exists and simply walks in.

### Where you meet them

**A checkpoint on a road into a town.** While the faction is working against a particular town or base, one
of the operations it can run is a mounted one: an armed vehicle drives out and parks on an approach road
150 to 300 m from the place, the infantry that rode in spreads out around it, and the gun stays manned. It
moves to a different approach every few minutes rather than holding one road all campaign. It is a
deliberate roadblock, not a wandering patrol, and it can be destroyed like anything else.

**Armour parked at a base.** From mid-campaign some occupying bases have a single armed vehicle standing in
the compound, unmanned, that you can walk up to and look at. It belongs to that base. If fighting starts
*there*, a crew mounts it and takes it out to a firing position short of the shooting. If fighting starts
somewhere else, it does not move.

**Driving to a battle.** When a base or town is being fought over, the faction can send up to two armed
vehicles by road from a holding of its own. They are not teleported in: they take the time the drive takes,
they arrive behind the infantry, and the road they come down is a road somebody could be waiting on. They
stop short of the fighting rather than driving into the middle of it, and they are gone when the battle
ends.

**A sweep after a loss.** A vehicle destroyed in the open marks that ground. The faction may send another
armed vehicle to work the area over, loitering around the spot for about twelve minutes before it leaves.
Only one sweep runs at a time, it does not run while a battle is being fought, and the faction has to be
able to afford it.

### What this means for you

- **Anti-tank starts paying for its weight around the middle of a campaign**, once the medium rung is
  unlocked. Before that, armed vehicles are jeeps and small arms are enough.
- **Killing the vehicle does not kill the force.** The men it carried get out and carry on on foot. Killing
  the vehicle removes the gun, not the operation.
- **Armour is a signal.** A parked vehicle in a compound and a roadblock on an approach are both telling you
  something about what the faction can afford and what it is currently working against.

### Known limits

- The faction refuses to drive an echelon from a source it knows is on an island, and refuses when there is
  no road near either end of the trip. It has **no way to ask whether a route actually exists**, so a source
  with roads at both ends but water in between will still be tried. When that happens the vehicle stalls,
  the stuck check fires, and the force gets out and walks. That is a degradation and not a failure, but it
  is silent.
- Live vehicles are **not saved**. Loading a campaign brings the force back on foot.

---

## Also owed on the wiki when this is published

- The in-game Field Manual's "Patrols and Garrisons" page gained an **Armour and Checkpoints** section on
  2026-08-23 covering the same ground in shorter form. If a wiki page mirrors the Field Manual, mirror that
  section too rather than letting the two drift.
