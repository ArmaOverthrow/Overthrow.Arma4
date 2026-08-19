# Counter Attacks — Requirements

**Epic:** occupying
**Created:** 2026-08-17 (formalized 2026-08-17 after design discussion)

## Overview

Our counter-attack systems are haphazard: if the OF has enough spare resources a QRF fires at a
uniformly random base. This is unpredictable, unrealistic, and leaves the occupying faction more
reactive than proactive. This feature retires all existing OF-initiated counter-attack behavior and
rebuilds it on the deployment/virtualization systems as a single, legible "current objective" state
machine with visible phases — a ramping build-up that experienced resistance players can read,
predict, and prepare against.

## What is retired

All **OF-initiated** attack triggers go:

- **The hourly random base counter-attack** — `OVT_OccupyingFactionManager.CheckUpdate()`
  (`:1216-1229`): 10 %/game-hour roll, uniformly random base, `StartBaseQRF`. Retire outright,
  including `m_bCounterAttackTimeout` and the `counterAttackTimeout` difficulty field.
- **The town suppression QRF** — `CheckUpdate()` (`:1232-1267`): resistance town, support < 25 %,
  player within 300 m → instant `StartTownQRF` with zero build-up. Retire; in practice it almost
  never fires anyway (the OF can rarely push support that low). A support-collapsed town becomes a
  high-weight objective instead and gets its battle through the phase system.
- **Specops missions** (`UpdateSpecops()` + `OVT_BaseUpgradeSpecops`, including its 600 s
  tower-recapture timer and its `StartBaseQRF` entry point) — being **dropped by the
  base-defense-migration work currently in progress**, not by this feature. This feature owns the
  *replacements*: tower recapture and base sabotage are rebuilt below as objective-phase
  deployments. Coordination note: with specops gone, the OF has no tower-recapture path at all
  until this feature lands.

**Kept untouched:** player-initiated battles — the uprising flag action and the player base-capture
assault. Those are resistance-initiated, not counter-attacks.

## Current Objective

- The deployment system gains a single **current objective**: bases, cities and towns only. FOBs,
  villages and radio towers are never objectives themselves (they don't cause a QRF); towers are
  handled *within* an objective's phases, and villages fall naturally as towers and bases are
  recaptured.
- Selection is intelligent and **predictable**: weighted by what territory the OF currently holds —
  population, short distance to held bases, radio-tower coverage, etc. It should feel like a natural
  choice; an experienced resistance officer should be able to guess the target and bolster defenses
  there in advance. The objective brain is deliberately omniscient about territory ownership (it is
  the strategic AI — the existing "target discovery not by magic" TODO does not apply here;
  resistance-side knowledge is the Intel epic's problem).
- All deployment systems **prioritize the current objective** over other work (a scoring input +
  position anchor in the deployment manager's evaluate→score→create loop).
- The objective is **re-evaluated only when situations change**: the resistance captures a new
  town/base, or the current target is recaptured by the OF. It idles when the resistance holds no
  towns or bases (early game).
- The current objective and its phase are shown on the **GM Overthrow panel** (the empty
  `DetailSection` in `OVT_GMPanelUIComponent` is the insertion point; objective + phase fields ride
  the existing `OVT_GMSnapshotBuilder` → `OVT_GMCampaignState` pipeline).

## Objective phases

The objective progresses through phases the resistance can see and feel — a build-up that, left
alone, escalates to a counter-QRF.

### Phase 1: Harassment

**Towns/cities:** groups of increasing size are sent to harass the town. If a group reaches the
center alive and holds it for some time, it applies a hefty support debuff that **stacks** per
successful mission (new deployment-keyed support modifier, same pattern as
`OVT_PatrolHarassmentStabilityModifier`). If a radio tower affects the objective, separate
specops-type groups are also sent to retake that tower (reach-and-hold-to-flip behavior module —
the inverse of `OVT_RadioTowerCaptureBehaviorDeploymentModule`, inheriting the retired specops
hold-timer mechanic). When support drops below 50 %, Phase 2 begins.

**Bases:** sabotage teams are sent to infiltrate and be a general nuisance. If they hold the base
for a time with no enemy around, they start destroying buildables — smallest first (recruitment
tents, vehicle ramps) escalating to larger ones (garages etc.). Destruction is scripted/timed while
the hold condition is met, not literal AI fire. **N successful sabotage missions** (difficulty-
scaled, see below) is part of the Phase 3 gate for bases.

**Insertion (new reusable module):** all Phase 1+ deployments come **from a controlled base**, never
spawned out of thin air. Insertion is a general-purpose deployment module any deployment config can
use:

- Close enough to walk → walk (rare).
- Otherwise a **live transport truck** (per the vehicle-patrol precedent: truck + registered crew,
  always-materialized, live AI drives real roads — `OVT_VehicleSpawningDeploymentModule` patterns)
  drives from the source base and drops the group at a **road LZ** a little short of the target,
  roughly on the line between source base and objective. Road points come from the proven
  `OVT_WorldUtils.FindNearestRoadSpawn` / `GetClosestRoad` helpers (`OVT_WorldUtils.c:259`, `:628`).
  The truck then returns to base and despawns.
- **Stuck fallback:** if the truck stops moving nowhere near its destination, flips, or takes enough
  damage, the group dismounts and walks the rest of the way.
- Concurrent live insertions are capped (config/difficulty knob) to bound cost.

### Phase 2: FOB

An OF FOB is established between the closest controlled base and the objective.

- **Position:** prefer an **authored position** — a new slot-like entity placed by map authors
  (precedent: `OVT_VehiclePatrolSpawn`, `OVT_SniperPosition`) within a range band from the
  objective. Fallback: attempt to **generate** a position — clear, flat, elevated, not too far from
  a road where possible (bounded-attempt sampling, QRF-LZ TraceBox pattern). Either way the
  position must be well clear of resistance FOBs, bases, and resistance-controlled villages/towns.
  If no position is found at all: reset, **blacklist that objective for one selection round**, and
  restart objective selection.
- **Raise:** a supply truck (live — it **can be ambushed or stolen**) is sent with supplies; on
  arrival it raises the FOB (a new prefab we author) plus one free garrison, then returns to base.
  The truck is not itself the FOB (unlike resistance mobile FOBs).
- **Funding:** the FOB is a deployment **anchor with its own budget inside the deployment pool
  accounting** — not a third funding system. Its resources are spent on garrisons and on further
  Phase 1-style operations into the objective (support harassment, tower capture, base sabotage).
  FOB garrison size maxes out at a difficulty-scaled level.
- **No notification:** the resistance is not told about new enemy FOBs (later, the Intel epic may
  surface them).
- **Removal:** the area must be cleared of enemies, then a held action on the FOB's flag deletes it
  and **resets the objective** (selection restarts and may or may not pick the same target). The OF
  loses resources when an FOB is removed, to set them back.
- **Starvation:** resupply is abstract for now (no physical resupply trucks). The FOB is starved
  when its **source base is cleared of garrison and/or has a strong resistance presence**. Starved
  long enough → the FOB is pulled down, the objective is abandoned, and selection restarts.
  ("Strong resistance presence" is intended to be measured by `resistance/high-command` group
  presence once that feature lands — see its requirements; until then, player presence suffices.)

### Phase 3: Counter-QRF

If an FOB is up, the gate is met — town/city support < 25 %, or for bases enough successful
sabotage operations — and the OF can afford a QRF, one is triggered to recapture the objective. The
existing QRF controller remains the battle-resolution layer; changes to it are limited to:

- The **FOB is added as a wave source** alongside controlled bases (the `m_Bases` position list in
  `OVT_QRFControllerComponent`).
- Wave LZ selection is **biased toward the actual source's bearing** (compute the preferred-
  direction fields from source geometry instead of relying on authored cosmetic values), so the
  attack visibly comes from where the OF actually is.

No matter the battle's outcome, the objective is reset, the FOB pulled down, and objective
selection restarts.

### Phase 3 addendum — the counter-attack QRF mode (added by the user, 2026-08-19)

A counter-attack QRF is **not** the battle a player starts. The QRF controller gains a **mode**, and
in counter-attack mode the battle runs in three stages. Player-initiated battles are unchanged.

1. **Silent deployment.** The battle begins with **no notification and no scoring UI**. The OF spends
   the **full cost of the QRF in one pass — no waves** — and moves those groups into position around
   the objective centre on a **perimeter of roughly 100–150 m**. Throughout this stage the resistance
   **cannot start a QRF of its own** (there is technically one active already). That restriction may
   alert an attentive player, and that is **accepted**.
2. **Muster.** The moment the **last group is spawned**, the resistance is notified and a
   **30-minute timer** begins. **Nothing is scored** during this window — it is time for the
   resistance to mount up and do something.
3. **Battle.** When the timer expires, normal zone scoring resumes and the battle is decided exactly
   as it is today. **If every spawned attacker is neutralised during the 30-minute window the timer
   ends early** and scoring begins immediately.

**The OF does not start a counter-attack at night** — only in the window from roughly **05:00 to
15:00**. A gate that ripens outside the window waits for morning; it is never a reason to abandon an
objective.

Design decisions taken with the user on 2026-08-19 while planning this addendum:

- The 30 minutes are **real minutes**, not in-game minutes — the window exists so players can
  physically travel to the fight, so `timeMul` must not shrink it.
- The world **keeps living** until the battle proper: civilians, garrisons, deployments and the OF
  economy are not suppressed during the silent and muster stages. Suppressing them would empty the
  target town half an hour before anyone was told, which is the loudest possible tell.
- Fast-travel and respawn restrictions apply **from the reveal**, not from the silent stage — a
  travel refusal before the notification is a free reveal, and blocking travel during a window meant
  for mustering is counterproductive.
- Groups spawn at the **usual landing zones** (250–750 m out) and **walk into** the perimeter, rather
  than appearing on it — the encirclement forms visibly, and nothing pops in beside a player standing
  in the objective.

The implementation plan's [§3.9](implementation.md) is the authority for how this is built; where it
and this document disagree in detail, §3.9 wins.

## Mid-phase objective changes

Objectives can only change during **Phase 1**. Once an FOB is deployed the OF is invested and sees
it through — but the FOB can be starved (above), which is the organic way a changed situation
unwinds Phase 2+. Starvation → teardown → abandon → reselect.

## Cross-cutting rules

- **One battle at a time:** while any QRF is live (including player-initiated ones anywhere on the
  map), **phase progression pauses, the FOB persists, and all objective timers freeze** — consistent
  with the existing `m_CurrentQRF` singleton freeze.
- **Persistence:** current objective, phase, timers, blacklist, FOB record (position, budget,
  garrison state) and harassment/sabotage success counters all persist (occupying-faction /
  deployment manager serializer extensions). A live QRF still deliberately rolls back on load.
- **Faction group registries** need new entries: the harassment size ramp, specops-type teams, and
  truck crews (registries currently ship only `light_patrol` and `light_fireteam`).
- **Difficulty scaling** (`OVT_DifficultySettings`, replacing the retired `counterAttackTimeout`):
  phase durations/cadence, harassment group ramp, FOB garrison cap, insertion cap, resource gates,
  and the Phase 3 sabotage-success count — which scales **inverted**: easier difficulties require
  *more* successful sabotage missions (more warning), the hardest only a few.
- **Villages:** no special handling. Map-authoring follow-up (not code): ensure each radio tower
  has at least one town/city in range so villages fall as natural collateral of the objective
  system.

## Goal

A ramping build-up of harassment and forces that makes sense and gives prior warning of a coming
QRF to recapture a liberated town, city or base — giving the resistance a reason to invest in
defense as well as offense, and to redirect those resources where they are needed (garrisons, high
command, entrenched positions, etc.).

## Dependencies

- `virtualization` epic: core (frozen `api.md`), movement, integration — all complete on v1.5.
  Consumption is via the deployment module seams (`RegisterGroup`, `BuildVirtualPlan`, owner-key
  reclaim); vehicle-borne groups stay live per the §3.8 seam contract.
- The deployments framework (`OVT_DeploymentManager` + module hierarchy) — the build target.
- **Coordination with `virtualization/base-defense-migration`** (in progress): it drops the specops
  upgrade class; this feature rebuilds the offensive behaviors specops used to own. Neither
  feature blocks the other, but the specops removal should be recorded there as
  "dropped — replaced by occupying/counter-attacks".
- QRF controller (`OVT_QRFControllerComponent`) reused for Phase 3, plus the two changes listed
  above **and the counter-attack mode of the Phase 3 addendum** (2026-08-19). It remains the only
  thing that resolves a battle, and the player-initiated path through it is unchanged.

## Out of Scope

- **OF counter-operations against resistance FOBs** — needed eventually, but a separate feature.
- **Physical/interdictable resupply trucks** for OF FOBs — later addition; starvation is abstract
  for now.
- **QRF waves using the insertion system** (no more spawn-from-thin-air QRFs) — a later QRF-systems
  upgrade; that feature drives any further insertion-module changes it needs.
- Resistance-side intel/notification surfaces for enemy FOBs (Intel epic).
- New client-facing UI beyond the GM panel objective/phase fields.

## Testing expectations

- Logic-tier coverage for objective selection scoring, phase-gate math, and the inverted
  difficulty scaling.
- Persistence-tier round trip for objective/phase/FOB state.
- Live-truck insertion (drive, drop-off, stuck fallback) and FOB raise/removal are play-test gated.
