# Deployments - Context & Decisions

**Last Updated:** 2026-08-21
**Current Phase:** Retrospective documentation + incremental enhancements (dated entries below)
**Status:** ✅ Documented (Existing Feature) — the epic's only force-placement system; 20 shipped configs

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code; framework complete, migration from BaseUpgrades stalled at one upgrade)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- ⏸️ Play-test the **town sweep** (2026-08-21 entry at the bottom): house interiors, posture, route feel; then the civilian-reaction follow-up
- 📋 `implementation.md` is stale (2026-08-02): BUG-028 is CLOSED (the faction list is pruned at `OVT_DeploymentManager.c:2114,2140`), the manager is ~3,000 L, 20 configs ship. The remaining retrospective items (`m_iResourcesInvested`/`m_fThreatLevel` at creation, world-time units, runtime conditions) need re-verifying against current code before anyone works them

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c` — server-only manager: pools, registry, evaluate→score→create loop (30 s)
- `.../OVT_DeploymentComponent.c` — per-instance component on `Prefabs/GameMode/OVT_Deployment.et`; proximity activation; `ApplyPersistedDeployment`
- `.../OVT_DeploymentConfig.c` / `OVT_DeploymentRegistry.c` — authored configs; `FindConfigByName` is the runtime + persistence key
- `.../Modules/` — condition/spawning/behavior module hierarchy (prototype-clone pattern)
- `Configs/Deployment/overthrowDeployments.conf` — the 3 shipped configs (Town Patrol, Light/Heavy Vehicle Patrol)
- `Scripts/Game/Persistence/Serializers/Components/OVT_DeploymentManagerSerializer.c` + `OVT_DeploymentComponentSerializer.c`; `Overthrow.conf` entity rule (`SelfSpawn 1`)
- `docs/archive/ModularDeploymentSystem.md` — the original design (including the stalled migration strategy)

---

## Important Decisions

- **Marker entity = durable record:** forces virtualize by proximity; the marker persists with self-spawn and spawns nothing on load — the modules re-spawn near players. Module-internal state (routes, cooldowns) is deliberately not persisted.
- **Prototype-clone modules:** config holds one instance per module; `CloneModule` + hand-written `CopyTo` per class (new attributes must be added to the clone list manually).
- **Config-name strings** are both the persistence key and the integration key (`OVT_PatrolHarassmentStabilityModifier` looks up "Town Patrol" by literal).
- **Eliminated flag applied before module init** in `ApplyPersistedDeployment` — deliberate fix for the EPF-era resurrect-on-load bug (documented in-code).
- **Designed successor to BaseUpgrades**, coexisting instead: one migration (town patrols) done; both systems share `m_iResources`.

---

## Gotchas & Learnings

- **`m_mFactionDeployments` never prunes dead IDs** — the per-faction list grows to the 100 cap and the faction silently stops deploying. The system's long-campaign kill switch.
- **`m_iResourcesInvested` and `m_fThreatLevel` are never set at creation** — refunds always 0, persisted threat always 0.
- **Two time conventions in one folder:** patrol-scan interval and town-cache timeout authored in seconds but compared to millisecond world time (scan every tick; cache never caches); the reinforcement module uses ms correctly.
- **Runtime conditions are only evaluated by the reinforcement module** — a base flipping to the resistance leaves its patrols running forever; the design's `AreAllConditionsMet` gate was never built.
- **The marker teleports to the last-spawned vehicle** (`SetOrigin` in the per-vehicle loop), mutating the persisted position; waypoints are double-inserted and double-deleted.
- **Zero replication:** clients see nothing; several manager getters null-deref if called client-side (collections unallocated).
- **Renaming a deployment config** silently orphans persisted instances *and* disables the town stability modifier.
- Four of seven location flags (PORT/AIRFIELD/CHECKPOINT/OPEN_TERRAIN) can never produce candidates — stub getters.
- `OVT_DeployFOBAction`/`OVT_UndeployFOBAction` are unrelated resistance FOB actions — naming overlap only (verified: no imports either way).

---

*This context file was created retrospectively by analyzing existing code.*

---

## 2026-08-20 — 🔴 The evaluator was choosing positions at RANDOM, and the authored threat gates had been dead for the whole campaign

**The report** (author, Normal-difficulty play-test): *"there is a lot of spending going on where it doesnt
make sense. building up bases far from the frontline or where there is any kind of threat. we might need to
tweak that as id rather the manager saved resources than just spend where it isnt needed."*

### The arithmetic, which is the whole diagnosis

`CalculateThreatLevel(position)` was:

```
threat  = ofManager.GetThreatLevel()          // GLOBAL - the SAME number at every position on the map
threat += ofManager.GetThreatByLocation(pos)  // the only term that knows where you are
```

and the evaluator then jittered the total by ±20%.

| Term | Magnitude (from the campaign that exposed it) |
|---|---|
| Global threat, identical everywhere | **~420** |
| Spatial score, realistic spread across a whole map | **0–60** |
| ±20% jitter, applied to the **total** | **±84** |

🔴 **The jitter alone was larger than the entire difference between the safest and the hottest position on the
map.** Ordering was a constant, plus noise, plus a signal a quarter the size of the noise. Sorting that is
sorting the noise — which is exactly the symptom reported.

The spatial term is small *by construction*, and that is fine as long as nothing swamps it: a known enemy base
within 1 km is worth **10**, a tower or FOB **5**, a warehouse **1**; a town contributes up to `5 × size` for
each of its support and stability components.

### It had also silently killed every authored `m_iMinimumThreatLevel`

Those thresholds are compared against this number. With ~420 added to every position, **every gate passed
everywhere, permanently, from the moment global threat first exceeded the threshold.** Three shipped configs
authored one, and all three had been inert for most of a campaign's life.

⚠ **The epic's own tech-debt list named this and was half right**: *"two threat concepts — the global
escalation scalar and the spatial `GetThreatByLocation` score — share a name but never interact."* They did
interact, in exactly one place, by addition, and that was the defect.

### The fix, in two halves (author chose both)

**1. The scores are separated.** `CalculateThreatLevel()` returns the spatial score alone. The global scalar
is not lost — it already governs *how much* the faction has to spend, through `PredictResourceGain()`, which
is what an escalation scalar should do. Deciding *where* to spend is a different question it was never
qualified to answer.

**2. A spend floor.** `MIN_LOCAL_THREAT_TO_DEPLOY = 5`: below it the candidate is skipped and the money stays
in the pool. Deliberately low — it excludes only the reported case (ground with no known target within a
kilometre and no town within three times its range, scoring 0–4) while leaving anywhere with a town or a
target able to develop. A higher floor would refuse to garrison real places early in a campaign, when support
is high and stability intact and every score is naturally small, and an undefended map is not recoverable by
waiting.

⚠ **The floor reads `candidate.threatLevel`, the UNBIASED score.** The objective anchor writes only to
`sortBy` (D5: it biases ordering, never eligibility), so an objective cannot make a dead corner worth
garrisoning — only change which *worthwhile* place is bought first.

⚠ **Free-at-game-start seeding does not pass through the floor** (`SeedFreeDeployments` → `PassesSeedConditions`
is a separate path) and — checked, not assumed — **none of the four free-at-start configs authors a threat
gate**, so a fresh campaign still seeds its garrisons, tower guards and town patrols exactly as before.

### Re-tuned to the new scale

| Config | Was | Now | Note |
|---|---|---|---|
| `Deployment_BaseHeavyPatrol` | 25 | **10** | same intent, scale that is actually compared against it |
| `Deployment_BaseATSection` | 50 | **20** | a notably hot area rather than a number nothing could fail |
| `Deployment_VehiclePatrol_Heavy` | 1200 | **30** | ⚠ **its meaning changed** — see below |
| `OVT_TEST_InitSuite.AT_MINIMUM_THREAT` | 50 | **20** | the one test pinning an authored threshold |

⚠ **`VehiclePatrol_Heavy` is a genuine semantic change, not a rescale.** At 1200 it was the one gate global
threat could not trivially clear, so it had become a *late-campaign* gate — "things are serious everywhere".
On the spatial scale it is now a *locality* gate — "this area is very hot". That is more consistent with every
other gate and with what the field is called, but it is a different rule and should be play-tested as one.

### A side effect worth knowing: the objective anchor works now

`m_fObjectiveAnchorWeight` is 25. Against the old scores (~420 ± 84) it was almost invisible — the jitter was
three times the bias. Against a 0–60 spatial score it is a strong, deliberate pull. **The anchor was
effectively inert until this change**, which also means any earlier judgement about whether objective-adjacent
deployments were being favoured was made against a bias that was not really operating.

`tools/compile-check.sh` exit 0. ⚠ **Suites not run** — the author is play-testing. The rescale touches a test
constant, so the All group is genuinely owed before this is trusted.

## 2026-08-20 — Two follow-ups to the threat rescale: a resource leak found, and the scoring widened

### 🔴 A wiped deployment was being charged for reinforcements it could never buy

**The report** was about log spam — *"'All spawned units for deployment Town Patrol have been eliminated' shows
up a lot but im not doing anything"* — and the spam turned out to be the visible edge of a resource leak.

**From `logs_2026-08-20_01-22-02`:** 16 "have been eliminated" lines, **zero** "no longer eliminated" lines, on
a metronome ~66 s apart. That log is EDGE-TRIGGERED (`CheckAllSpawningModulesEliminated` prints only when the
flag changes), so a repeating line means something was clearing the flag between checks.

**`Reinforce()` was clearing it.** It sets both the module's and the deployment's eliminated flags false
*before* converging, so the convergence will register at all; on failure it restored **only the module's**.
The recompute then saw `false -> true` and re-announced. Hence one line per failed rebuy, forever.

🔴 **And each of those failed rebuys had already charged the faction.** `Reinforce()` debits up front and the
method's own header admitted it: *"resources are charged up front and not refunded on a failed registration,
which is what this path has always done"*. The play-test measured the cost: **13 failed rebuys x 100 = 1300
resources in twenty minutes, for zero groups, with not one successful reinforcement in the whole session.**

**Why it could never succeed, and why nothing stopped it:** the patrol's town had changed hands, so the
registration had nowhere valid to go — and `Deployment_TownPatrol` is authored **`m_bDeleteOnConditionFail 0`**
in the registry override, so the deployment is never collected. A permanent, self-repeating charge.

**Fixed:** the refund is pro-rata (`(groupsNeeded - successfulSpawns) x m_iReinforcementCost`, so a partial
success pays for what it got), through the manager's own `AddFactionResources` on the precedent the patrol
module's recovery already set, and the deployment's previous flag is restored on failure so the log goes back
to being a state change rather than a heartbeat.

⚠ **NOT fixed, and it is the actual root cause:** a deployment whose condition can never be met again keeps
retrying forever, because `m_bDeleteOnConditionFail 0` is deliberately authored on Town Patrol. The refund
stops it costing anything; it does not stop it happening. Whether a town patrol should be collected when its
town flips is a design call, not a bug fix.

### The spatial score was too narrow to see a frontline

**The report:** *"the OF isnt spending a dime now... I do own a radio tower that isnt being recaptured and
support is dropping in various places. local threat might need to take more into account such as support,
radio towers and captured bases."*

Fair, and partly a consequence of the rescale earlier the same day: once the ~420 constant was removed, the
range and weights of the spatial terms started mattering for the first time, and they were sized for a world
where they did not.

| | Was | Now |
|---|---|---|
| Known-target range | 1000 m | **2500 m** |
| Captured enemy BASE | 10 | **40** |
| Enemy BROADCAST_TOWER | 5 | **25** |
| Enemy FOB | 5 | **20** |
| Enemy CAMP | **0 — no branch at all** | **20** |
| Enemy WAREHOUSE | 1 | **5** |

⚠ **`CAMP` was not a missing weight, it was a missing branch.** `UpdateKnownTargets()` has always inserted a
target for every resistance camp; `GetThreatByLocation()` simply had no `if` for it, so camps contributed
exactly nothing. **Support is already covered** by the town loop (`supportScore` scales with resistance
support in an OF-held town), which is why it is not in the table.

⚠ **The map threat overlay is unaffected by the magnitudes** - it normalises against its own
`maxThreatRecorded` - checked rather than assumed.

### And a diagnostic, because the floor is a guess until it is measured

The pass that buys nothing now prints **the best candidate's score beside the threshold it failed**. On the day
the floor was added the very next report was "the OF isnt spending a dime", and no log could distinguish
*floor too high* from *scoring too narrow* - they look identical from outside. Now: a top score just under the
floor means lower the floor; a top score near zero on a map with a live frontline means the scoring is not
seeing it.

⚠ **The three re-tuned config thresholds were deliberately NOT touched a second time.** They are still in a
sane band on the new scale, and changing them again without the measurement the diagnostic is there to provide
would be a third guess stacked on two.

⚠ **Suite state:** All was **385/385 green** before these last changes; the author restarted Workbench, so the
re-run is owed. `tools/compile-check.sh` exit 0.

## 2026-08-20 — The occupying faction was deploying INTO bases the player had captured

**The report:** *"a town patrol was bought for Levie and has spawned at Levie base (I control Levie base).
they need to come from the closest controlled base."*

Four distinct defects behind that one sentence. All four fixed.

### 1. 🔴 Enemy-held bases were offered to the evaluator as places to build

`GetBasePositions()` walked **every** base in `m_Bases`, filtered only by `IsPositionRelevantToFaction()` -
which answers **true unconditionally** for the occupying faction. So a base the resistance had captured was a
perfectly good candidate for the occupying faction's own garrison, defences and parked vehicles.

⚠ **Latent for the whole project's life, and the threat rescale earlier the same day is what exposed it.**
While every candidate carried the global threat plus noise, an enemy base was no more attractive than anywhere
else and rarely won the sort. Now a captured base is worth `THREAT_WEIGHT_ENEMY_BASE` and is
**systematically** among the top candidates - the rescale turned a lottery into a certainty. Filtered to
`baseData.faction == factionIndex`.

⚠ **The director is untouched and must be.** Its operations AGAINST enemy bases go through
`ForceCreateDeployment()`, which never asks for a candidate list.

### 2. 🔴 `GetNearestControlledBasePosition()` asked the wrong question

It fetched the nearest base and *then* checked whether that one happened to be friendly, answering
`vector.Zero` when it was not. On a contested map that is exactly backwards: the moment the resistance takes
the base beside a town, every deployment there loses its source, however many friendly bases lie a kilometre
further on. **Levie is precisely that case.**

Now delegates to `OVT_NearestControlledBaseSourceProvider`, which walks the faction's OWN base list - **the
class whose header already describes this exact defect as the reason it exists.** The insertion module has had
the correct answer available all along while this method kept the broken copy.

### 3. Town patrols now come from a base

`m_bSpawnAtNearestBase` defaults **false**, so initial town-patrol groups materialised in the town. Authored
`1` on `Deployment_TownPatrol`.

✅ **Checked before authoring it, because it could have been much worse than the bug:** the patrol plan is
built by `OVT_PatrolBehaviorDeploymentModule.BuildVirtualPlan()`, which anchors on `GetPatrolCenter()` - the
**deployment marker**, i.e. the town - and only falls back to the group's position for a config template with
no deployment behind it. So men registered at a base still patrol the TOWN; they walk in. Had the plan
followed the spawn position instead, this flag would have set every town patrol circling a base.

⚠ **The campaign-start consequence was flagged, and the author immediately ruled on it** - *"they shouldnt
spawn at nearest base at game start (if free at game start = true they should spawn in the town)"*. A map the
occupying faction has held for years should not open with every town empty while its garrison walks in. So the
free-at-game-start pass now **ignores `m_bSpawnAtNearestBase`**: the founding garrison is simply already
there, and only patrols BOUGHT later travel from a base.

⚠ **THE FLAG IS ON THE DEPLOYMENT, NOT THE CONFIG, and it cannot be derived from anything else.**
`config.m_bFreeAtGameStart` is the wrong test - the very same config is bought again by the evaluator
mid-campaign, when the force SHOULD come from a base, so answering the config would ground those purchases
too. `resourcesInvested == 0` is wrong as well: Town Patrol authors `m_iCostPerGroup 0`, so an evaluator
purchase of it also costs zero. Only the caller knows, so `CreateDeployment()` takes it as a parameter and
`OVT_DeploymentComponent.WasSeededAtGameStart()` answers it. **Stamped BEFORE `InitializeDeployment()`**,
because initialisation activates the modules and a spawning module reads it on its first convergence.

⚠ **Not persisted, deliberately** - on load, spawning modules reclaim their groups by owner key rather than
registering new ones, so nothing re-reads it for a restored deployment.

### 4. The diagnostic I added an hour earlier was lying, and it caught itself

Its first real log read:

```
all 27 candidate position(s) are below the local-threat floor of 5 (best was 61.3831)
```

**61.38 is not below 5.** `%3` was the SUPPRESSED count, not the total - 27 of 28 fell below the floor and the
28th cleared it and then bought nothing for an unrelated reason. The line now prints **both** numbers and says
which conclusion each case supports. Recorded because it cost a wrong diagnosis before the numbers were
re-read, and because a diagnostic that can lie is worse than none.

⚠ **What that corrected reading actually says:** the floor was NOT why the faction stopped spending. The best
position on the map cleared it comfortably and had simply run out of things to buy, while everywhere else was
genuinely quiet. Whether 27-of-28-quiet is right is now a question the log can answer directly.

`tools/compile-check.sh` exit 0. ⚠ Suite re-run still owed (Workbench is running).

## 2026-08-20 — An armed patrol vehicle with nobody on the gun

**The report:** *"the gunner spawns in the gunner slot but then gets moved to co-driver"*, and on being told
the patrol still functions: *"well it doesnt function because the car has a gun with no gunner"*. Correct - an
armed vehicle nobody is shooting from is not a patrol, and that correction is what turned this from "park it"
into a fix.

**It was NOT the insertion module's seating rework, checked rather than assumed.** Two separate
implementations, and the patrol configs author only the second:

| | Class | Used by |
|---|---|---|
| Changed 2026-08-20 | `OVT_InsertionSpawningDeploymentModule.SeatRider()` | objective operations only |
| Vehicle patrols | `OVT_VehicleSpawningDeploymentModule.SeatAgent()` | `Deployment_VehiclePatrol_Light/Heavy` |

`SeatAgent()` already orders pilot -> turret -> cargo **and** early-outs on `access.IsInCompartment()`, so this
module physically cannot move a man who is already seated - which matches the report exactly: he is seated
correctly and moved afterwards **by something else**.

**The two candidates, both vanilla, both read rather than guessed at:**
- `SCR_AIGetEmptyCompartment` allocates a **non-leader** driver > turret > cargo and the **leader**
  turret > cargo, whenever the group boards a vehicle it owns. It only fills compartments that are EMPTY
  (`!comp.AttachedOccupant() && comp.IsCompartmentAccessible() && !comp.IsReserved()`) so it cannot evict on
  its own - but any member who leaves his seat for any reason is re-allocated by that priority rather than
  back to the seat he had.
- The crew despawns and respawns by proximity like everything else here, and members materialise through the
  AI spawn queue in whatever order it produces. **Nothing anywhere records that THIS man was the gunner**, so
  a fresh materialisation re-races the seats - structurally the same order-of-arrival race that put a
  passenger in the insertion truck's co-driver seat earlier the same day.

**The fix is a RE-ASSERT, not a change to the seating**, because the seating is already right. `OnUpdate` now
runs `EnsureTurretsManned()`: an unoccupied turret plus an occupied cargo seat means the passenger is moved to
the gun. It guarantees the property that actually matters - **if there is a gun and a spare crewman, the gun
is manned** - and self-heals within one tick of whatever moved him, without needing to know which of the two
causes it was.

**Decisions taken:**
- ⚠ **It promotes out of CARGO only, never out of the pilot seat.** Taking the driver would strand a patrol
  mid-route, a worse and far more visible failure than an unmanned gun. A vehicle with a driver and no spare
  passenger keeps its empty turret, which is correct - there is nobody to put in it.
- ⚠ **A player in the back is never moved.** Teleporting somebody who climbed into an occupying vehicle into
  its turret would be the game playing for them.
- ⚠ **It cannot fight itself**: the move only happens while the turret is genuinely empty, so it is a no-op on
  every tick after it succeeds. **If vanilla moves him straight back out, the two will alternate at the update
  interval and the log line will repeat** - that repetition is the agreed signal that the larger vanilla
  investigation (group roles, `SetReserved`) is worth doing after all.

⚠ **Related, and this one IS mine:** `Deployment_VehiclePatrol_Heavy`'s threat gate went 1200 -> 30 earlier the
same day. At 1200 it was effectively gated on global escalation and almost never fired, so armed patrols are
now reachable far more often. **The seating did not change; how often it is seen did.**

`tools/compile-check.sh` exit 0. ⚠ Suite re-run owed.

## 2026-08-20 — The threat floor made a missed baseline garrison PERMANENT

**The question:** *"if a deployment config has 'free at game start' and high chance does it definitely spawn at
game start?"* — asked because garrison patrols looked absent after the floor went in.

**Answering the question exactly, from `SeedFreeConfig()`'s own contract:**

**Not consulted at seed time:** the resource pool, **`m_fChance`** (*"A garrison that exists 70 % of the time
is not a baseline"*), `m_iMinimumThreatLevel` (*"Threat is a measure of what has already happened, and at t0
nothing has"*), `MAX_DEPLOYMENTS_PER_EVALUATION` — **and `MIN_LOCAL_THREAT_TO_DEPLOY`, which lives only in
`EvaluateFactionDeployments`.** So chance is irrelevant at game start, and the floor never touched seeding.

**Consulted:** the same-name 250 m dedup, `m_iMaxInstances`, the per-faction ceiling, `CanUseLocationType`,
and — the important one — **the config's own condition modules**, via `PassesSeedConditions()`.

**So "free at game start" does NOT mean "definitely spawns".** `Deployment_BaseGarrisonPatrol` authors
`OVT_NoPlayersNearbyConditionDeploymentModule` — *"Never fortify a base a player is standing in"*, 320 m — so
any base the player is near when seeding runs is **skipped by design**.

### 🔴 And that is where the floor actually bit — not at seeding, but at the REPAIR

Seeding is a **one-shot**. Before the floor, a base that missed its free garrison was quietly repaired by the
evaluator on a later pass. After it, a quiet base scores under 5, the evaluator refuses too, and **a garrison
missed once was missed for the rest of the campaign**. The author had been repeatedly loading saves while
standing at bases, which is precisely the condition that makes seeding skip them.

The play-test log is unambiguous: **zero** `Base Garrison Patrol` deployments, **26 of 39** candidates below
the floor every pass, **6 871 resources** sitting idle.

⚠ **This is the exact class of defect the diagnostic line was added to catch, and it did its job** - "best was
54.0172" against a floor of 5 said plainly that the floor was not why *that* position bought nothing, which is
what pointed at the quiet ones instead.

### The fix: the floor exempts baseline presence

`m_bFreeAtGameStart` marks what a place this faction holds is **supposed to look like**, not escalation.
`SeedFreeConfig()` already refuses to gate it on threat; the evaluator now agrees, so the two paths can no
longer disagree about whether a baseline garrison is allowed to exist.

⚠ **The floor therefore moved to AFTER the config is chosen.** It can no longer be a per-candidate
pre-filter, because "is this worth spending on" now depends on *what* would be bought. Cost: one
`FindBestDeploymentConfig` walk per quiet candidate per pass — a registry scan over a few dozen positions,
cheap against a permanent hole.

**What this does and does not restore:** escalation (AT sections, heavy patrols, fortifications) is still
floored and still stays unbought at quiet places, which is what was asked for. Baseline garrisons, tower
garrisons and town patrols are once again repaired wherever they are missing.

`tools/compile-check.sh` exit 0. ⚠ Suite re-run owed.

## 2026-08-20 — The opening defence budget was sized for a model that no longer exists

**The report:** *"where are all these resources coming from? normal difficulty starts at 850 total"* - a new
save opening with **9 940** in the deployment pool.

**It was never a leak, and the log named it one line below the one being read:**

```
03:54:28  Allocated 9940 resources to deployment manager
03:54:28  Opening defense budget: 9940 resources into the deployment pool across 10 base(s)
```

**`startingResources` is PER BASE, not per campaign** - that was the whole misunderstanding, and the attribute's
own description said "OF starting resources per base" while every reader of the number treats it that way:

```
seed = baseResourcesPerTick + Σ over ALL bases: floor(startingResources × base.m_fStartingResourcesMultiplier)
```

Normal's 850 across ten bases at ~1.14 apiece = **9 940**. Working exactly as authored.

### Why it was nonetheless wrong now

9 940 is almost exactly the cost of fortifying **all ten bases outright** - the five non-baseline base configs
total ~820 each (AT 140 + Checkpoints 80 + Defense Positions 320 + Heavy Patrol 140 + Snipers 140). That is
precisely what the deleted per-base distribution used to do with it on the first tick.

**Two changes since have made that figure meaningless**, and the second is from earlier the same day:
1. base defence is now bought concern-by-concern by the evaluator as threat allows, not spent at once;
2. `MIN_LOCAL_THREAT_TO_DEPLOY` refuses escalation at a quiet place - **and at t0 every place is quiet.**

So the opening credit could not be spent when it was granted. It banked, and turned into a burst the moment
the player made noise anywhere - the same burstiness the author had already objected to in the income cadence.

### Scaled down, ordering preserved

| Preset | Was | Now | Approx. opening pool (10 bases) |
|---|---|---|---|
| Easy | 350 | **60** | ~830 |
| Normal | 850 | **150** | ~1 960 |
| Hard | 1500 | **260** | ~3 280 |
| Extreme | 2000 | **350** | ~4 490 |
| Insane | *(inherited 3000)* | **500** authored | ~6 450 |
| TestWorld | 200 | **40** | ~500 |

Sized as "a few early deployments" rather than "fortify the map": Normal's ~1 960 is roughly two bases' worth
of escalation, against a per-base cost of ~820.

⚠ **`Difficulty_Insane` authored NOTHING and silently inherited the 3000 default**, opening on a ~35 000
credit - by a wide margin the largest number in the difficulty set, and reached by omission rather than by
decision. It now authors 500 explicitly, and the attribute default is lowered to 150 so the next preset that
forgets inherits something sane.

✅ **Safe to change in isolation, checked rather than assumed:** `grep -rn "startingResources" Scripts/` finds
exactly **one** production reader, `CalculateOpeningDeploymentSeed()`, and no test pins it.

### Suite: All 385/385 green

The first run after this change was **7 red in 132 s**. Exactly one had assertion text -
`OVT_TEST_Logic_QRFSiege_TimerCrossesFromMinutesToSecondsAtTwoMinutes`: *"a freshly armed muster window reads
exactly 30: got 15, expected 30"* - a **real** expectation break from halving `MUSTER_TIME_MS`, and a test
asserting the wrong thing: it hardcoded a balance number where the contract is the round-UP rule. The
expectation is now derived (`MUSTER_TIME_MS / MS_PER_MINUTE`), so the row pins the rounding and the knob stays
free to move. The neighbouring rows feed literal millisecond values to the pure function and were correct
unchanged.

The other six were `TestResultTimeout` with **no assertion text** - the documented host signature. The re-run
came back **385/385 in 81 s**, which is the clean baseline, retroactively confirming the 132 s run was
contended.

## 2026-08-20 — The baseline exemption duplicated what the seeding had just placed

**The report:** *"at game start levie base spawned 3 town patrols. 2 for levie and 1 for regina. both levie and
regina already have town patrols (from free at game start). these spawn at levie base and walk."*

**The log confirms it exactly** — two waves, one second apart:

```
04:13:06.86x   20 x 'Town Patrol', one per town        <- the free-at-game-start seeding
04:13:07.86x   'Tower Garrison' x2, 'Town Patrol' x4,  <- the evaluator's FIRST pass
               'Base Garrison Patrol' x2
04:13:06.914   Seeded 70 free-at-game-start deployment(s)
```

Lamentin ends the second wave with **three** patrols. Twenty-five Town Patrols on a twenty-town map.

### ⚠ This was MY regression to make reachable, and that is why it is answered here

The baseline exemption added earlier the same day - so a garrison the one-shot seeding skipped could still be
bought later - is what let these candidates through the floor. Before it they were floored at t0 and the
duplication was invisible. **The underlying weakness is older; the exemption is what surfaced it**, so the
exemption is where it is scoped.

### The mechanism for the town half, which IS established

`GetLocationTypeAtPosition()` **OR-s** the BASE bit into a position's classification. A base standing inside a
town's bounds therefore reads as `BASE|TOWN`, and `CanUseLocationType()` is a bitwise test - so
`(BASE|TOWN) & TOWN != 0` and **Town Patrol is eligible at a base position**. That position is far enough from
the town centre that the standard **250 m** dedup cannot see the patrol already standing there, so a second one
is bought. It is then created AT THE BASE, which is why the men appear at Levie base and walk: the patrol plan
anchors on `GetPatrolCenter()` - the nearest town centre - hundreds of metres away.

The OR is deliberate and documented ("so a tower inside a town's bounds still reads as a tower"); it was
designed for towers and has this side effect for towns.

### The fix: baseline presence is deduped at 1000 m, escalation still at 250 m

`BASELINE_DEDUP_RADIUS = 1000` - the framework's own `HasDeploymentNearPosition()` default rather than a new
invented number - applied **only** to `m_bFreeAtGameStart` configs on the exemption path.

⚠ **Deliberately asymmetric.** 250 m is *right* for escalation: a base fortifies by acquiring several
DIFFERENT configs within a few hundred metres, and widening that would break the per-concern model outright.
It is *wrong* for baseline presence, which is supposed to exist exactly once per place.

### ⚠ What is NOT proven, stated rather than glossed

The same wave also duplicated **Base Garrison Patrol** at base positions, and a BASE config offered a BASE
candidate should have deduped at 250 m with no classification subtlety involved. The wider radius covers it,
but **the mechanism for that half is unexplained** - the dedup machinery reads correctly (`RegisterDeployment`
inserts synchronously inside `InitializeDeployment`, `GetPosition()` returns the live entity origin, the
lookup walks `m_aActiveDeployments`), so something about it fails in a way this session did not isolate.
If duplicates reappear, that is the thread to pull, and a per-candidate log of the dedup's inputs is the way
to pull it.

**All 385/385 green** after the change.

## 2026-08-20 — A composition deployment is no longer bought where it cannot be built

**Why now:** the author confirmed the world's own base slots are absent and only the ones authored into
`slots.layer` exist - Bohemia most likely scoped them out so base-game slot placement would not dictate other
game modes' fortifications. So **"this base has no slot of that kind" is the NORMAL case** until every base
has been given slots by hand, and the pre-existing waste stops being an edge case.

**The waste:** a deployment's price is paid at CREATION; the slot is not looked up until CONVERGENCE. Nothing
connected the two, so the evaluator bought Base Checkpoints at bases with no `ROAD_LARGE` anywhere, charged in
full, discovered there was nowhere to build, and latched `m_bCompositionAttempted` so it never retried. Then
did it again on the next pass.

**New `OVT_CompositionSlotConditionDeploymentModule`** - a creation gate that refuses the deployment when the
base has a free slot of **none** of the types its compositions need.

**Decisions taken:**
- 🔴 **`EvaluateCondition()` is deliberately left inherited (always true).** This is the trap in the whole
  design: the moment the deployment builds, its composition **claims** the slot - so a runtime re-ask would
  answer "no free slot" and, on every config authoring `m_bDeleteOnConditionFail 1`, the reinforcement module
  would tear down the deployment it had just successfully built.
- ⚠ **ANY, not ALL.** Base Fortifications carries three composition modules; one free slot is enough to do
  something useful, and requiring a slot per module would refuse a partially-buildable deployment.
- ⚠ **A scan, not the existing roll.** `FindFreeSlot()` picks by random roll, which is right for CHOOSING and
  wrong for ASKING - a roll landing on a taken slot is indistinguishable from none being free, so a gate built
  on it would refuse buildable deployments at random. New pure `HasFreeSlot()` scans.
- ⚠ **The slot-type list is a SECOND COPY of what the composition modules already say**, and that cost is
  accepted rather than hidden: `EvaluateStaticCondition()` runs against the config TEMPLATE with no route to
  its sibling modules, so it cannot read their `m_eSlotType`. The drift is closed by an Init case asserting
  the two SETS match on every shipped config, not by hoping.
- ⚠ **An empty list allows everything** rather than refusing everything - a misauthored gate that silently
  stopped a base ever fortifying would be far harder to notice than one that does not gate.

**Scope is narrower than expected, checked rather than assumed:** only **two** shipped configs use
`OVT_CompositionSpawningDeploymentModule` - Base Checkpoints (ROAD_LARGE + ROAD_MEDIUM) and Base Fortifications
(SMALL). Base Defense Positions and Base Sniper Positions use `OVT_PlacedInfantrySpawningDeploymentModule`,
which places men on markers rather than claiming slots, and needs no gate.

**Also added:** `FindSlots()` logs a per-base slot inventory at init - every size including the zeroes, since a
missing size is the interesting case. It began as a diagnostic for this and is now an **authoring checklist**:
it shows which bases still need which sizes, and whether an authored slot is landing inside `baseRange` with
the right label (a slot just outside the radius, or missing `SLOT_ROAD_LARGE`, is invisible and looks identical
to never having been placed).

`tools/compile-check.sh` exit 0 (6191 files). ⚠ **Suite NOT verified** - the author asked for suite runs to
stop while play-testing, and the last attempt returned INDETERMINATE (no junit.xml) rather than a result.
**Two new Init cases are unrun and neither fault injection has been done**; both injections are written down at
their case headers.

**Two EnforceScript limits hit writing the inventory line, both worth remembering:** `string.Format` caps its
parameter count ("Too many parameters for 'Format' method"), and replacing it with one long `+` chain then
fails differently with "Formula too complex". Successive `+=` clears both.

---

## 2026-08-21 — Town patrols sweep houses; the road-snapped ring is gone from towns

**The report** (author, play-testing `objectives`): *"right now they just do a road-snapped perimeter but its kinda
boring, as well as means they do end up standing in the middle of a road waiting on each one, getting in the way of
the director's insertion missions or giving the player an easy target for their vehicle. Id like the town patrols to
be going from house to house 'searching' them and just being general douchebags to the civilians ... maybe we add a
new waypoint mode that picks a bunch of random houses to do that, but for variety they just sometimes do a perimeter
of random radius up to the authored town radius but not road-snapped."* Built in this session, in the deployments
feature, deliberately away from the files the concurrent `objectives` play-test session has uncommitted
(`OVT_InsertionSpawningDeploymentModule.c`, `OVT_BaseBehaviorDeploymentModule.c`, `OVT_MountedGroupActivation.c`,
`Modded/SCR_AIGroup.c` — none touched).

### What vanilla already had, and why it was the answer

`AIWaypoint_SearchAndDestroy.et` was wired into `OVT_OverthrowConfigComponent` (`m_pSearchAndDestroyWaypointPrefab`)
and used only by the QRF. `SCR_AISearchAndDestroyActivity` lays a grid over the waypoint's completion radius, snaps
every cell to **navmesh** (`GetClosestPositionOnNavmesh(tile, "15 15 15")` — house interiors included wherever a
building has interior navmesh), has each fireteam walk and *investigate* cell after cell, and `WP_SearchAndDestroy.bt`
carries `SCR_AITaskTimerGate` + `SCR_AICompleteWaypoint` like `WP_Wait.bt`, so the waypoint **completes after its
holding time** and the group moves on. It is a `SCR_TimedWaypoint`: hold and radius are both settable per waypoint.
Pinned to one building with a 15 m radius it is "search this house and its yard". Nothing custom had to be written
on the AI side.

### The mechanism

- **`OVT_EVirtualWaypointType.SEARCH`**, appended at the END of core's enum (additive; persisted plans carry types
  as ints, an old save reads back unchanged). Parameter = **hold seconds**, exactly like WAIT. Core
  (`CreatePlannedWaypoint`) spawns the S&D prefab, pins `SetCompletionRadius(SEARCH_WAYPOINT_RADIUS_M = 15)` and
  `SetHoldingTime(param)` **before** the waypoint is added — the activity reads the radius once, when it builds
  its grid. `ValidateWaypointPlan`'s upper bound moved to SEARCH. **Movement** treats SEARCH as WAIT (arrive, hold,
  advance), so a dormant sweep keeps touring houses and materialises AT one, not on a road corner.
  `docs/features/virtualization/core/api.md` and movement's §3.5 table updated.
- **`OVT_PatrolType.TOWN_SWEEP`** on `OVT_PatrolBehaviorDeploymentModule`, rolled **per group** in
  `BuildTownSweepPlan()`:
  - **House route** (`m_fSweepHouseChance`, 0.7): every ownable building (`OVT_RealEstateManagerComponent.
    BuildingIsOwnable` — the same filter the town manager counts population with; **player-owned houses
    included**, the author's call) within `GetTownRange(town)` → `PickRandomSites` (partial Fisher–Yates,
    `RandInt` max-exclusive) takes `m_iSweepHouseCount` (5) → `OrderNearestNeighbour` from the group's start →
    `BuildSearchPlan` with a 60–120 s hold per house, cycling. No WAIT follows a SEARCH: the hold *is* the pause.
  - **Loose ring** (otherwise, and always when the town has no searchable house): `BuildPerimeterPlan` at
    `range × [0.3, 1.0]`, ground-snapped, **not road-snapped** — except a corner in the sea, which is pulled onto
    the nearest road rather than left unreachable (`OVT_WorldUtils.IsOceanAtPosition`).
  - Both halves size from the **town controller's authored `m_iTownRange`** (Eden's placed controllers author
    100–213 m; the civilian crowd uses the same field), resolved through the manager's index-aligned
    `m_TownControllers`. ⚠ The first build used `OVT_TownManagerComponent.GetTownRange()` — the **deprecated
    per-size table** (250/400/600), 2–4× the drawn town — and the author caught it in review; that table is now
    only the fallback for a town with no controller, and `m_fPatrolRadius` for no town at all.
- `Deployment_TownPatrol.conf` authors `m_ePatrolType TOWN_SWEEP`. PERIMETER stays in the enum for any config
  that still wants a road ring (none shipped does).

### Decisions worth knowing

- **Radius is core's constant, not a module attribute.** A plan carries one parameter per point and SEARCH's is
  the hold; encoding two values was not worth a second array on a frozen contract. 15 m is "one house and its
  yard"; if interiors are not being entered in play, this and the hold band are the first knobs.
- **Per-group roll, not per-deployment.** Two patrols on one town should not walk the same ring or search the
  same houses; the roll happens where the plan is built, once per registration, and a rebought group rolls again.
- **The plan is fixed for the group's life** (core owns waypoints from registration). A 5-house loop of ~2 min
  each plus walking is a 15–20 min cycle — long enough not to read as a loop from inside the town.
- **`GivePatrolWaypoints` (legacy helper) is untouched**: it is driven by job-stage configs with their own patrol
  types and TOWN_SWEEP never reaches it.

### Verified / owed

- `tools/compile-check.sh` OK (6237 files). Logic `…_SearchPlan` and `…_NearestNeighbourRoute` (new) and Init
  `…_TownPatrolPlanCycles` (updated: TOWN_SWEEP, both knobs > 0, SEARCH counts as movable) each **green** as
  standalone runs; a full All run is owed before commit (another Workbench was open on the machine).
- ⚠ **Play-test owed**: whether live men enter interiors (navmesh-dependent per building), whether the S&D posture
  reads as a search rather than an assault, and whether the route feels like a patrol.
- ⚠ Three `PrintFormat` string params is the cap (the Init printout uses `Print(string.Format())` for four).
- ⚠ **`GetTownRange()` is a trap for anyone sizing anything to a town**: it is the deprecated size table, not the
  authored range. Other live callers still on it (not touched here): `OVT_UprisingRequestComponent.c:86`,
  `OVT_RaiseForwardBaseObjectiveOperation.c:742` (FOB clearance), the jobs stages/conditions, `OVT_BuildContext.c:314`,
  `OVT_EconomyManagerComponent.c:1144`, `OVT_OccupyingFactionManager.c:1557`.
- Civilian reaction (flee/cower at an arriving sweep) **deferred by the author** — "patrols first" — and belongs
  in `virtualization/civilians`' archetypes when it comes.

---

## 2026-08-21 — The house search is now RELAXED: Overthrow's first behaviour trees

**The report** (author, play-testing the sweep): *"its not bad, they are entering homes and searching them,
but they are acting like soldiers under threat. they crouch and sneak and run from waypoint to waypoint."*
Asked "what are the chances of creating our own behavior tree similar but more relaxed" — good, it turned out,
because vanilla's machinery is 90 % reusable and the posture comes from exactly two places, neither reachable
from a flag:

1. `SCR_AISearchAndDestroyActivity.AssignInvestigationPositions()` sends every soldier an Investigate with
   `dangerous = true`, hard-coded, and `SCR_AIMoveAndInvestigateBehavior` then pins `m_fThreat` above
   `VIGILANT_THRESHOLD` for the whole action.
2. `AI/BehaviorTrees/Chimera/Soldier/MoveAndInvestigate.bt` **unconditionally** sets stance CROUCH, speed RUN
   and weapon raised near its root (lines 181–201, 63), before any threat test.

### What was built — one small class per layer, everything else inherited (`Scripts/Game/AI/Behavior/OVT_HouseSearchAI.c`)

- **`OVT_AIHouseSearchBehavior : SCR_AIMoveAndInvestigateBehavior`** — same ports (the tree's
  Get/SetInvestigateBehaviorParameters nodes read them off a template instance by name), `isDangerous`
  defaults false, and `m_sBehaviorTree` = **`AI/BehaviorTrees/Overthrow/Soldier/HouseSearch.bt`**.
- **`OVT_AIHouseSearchActivity : SCR_AISearchAndDestroyActivity`** — vanilla's grid, tile loading, fireteam
  bookkeeping, `OnChildBehaviorFinished` and holding-time failure; only `AssignInvestigationPositions` is
  overridden to build our behaviour and `AddAction` it on each soldier's utility directly (exactly what the
  vanilla reaction does with the message). Per-spot dwell 15 s (vanilla 10).
- **`OVT_AIStartHouseSearch : SCR_AISendMessageGenerated`** (waypoint-tree node) — replaces the
  "Send Goal Message Seek And Destroy" node: fails running move activities and adds our activity to the group.
  ⚠ **Why a node and not a new goal message:** goal messages are indexed by an engine enum and their reactions
  are registered per prefab in `SCR_AIConfigComponent.m_aGoalReactions` on vanilla `Group_Base.et` — a new
  message type would mean editing a vanilla prefab or `modded`-ing a vanilla reaction class. The node sidesteps
  both.
- **Two hand-authored trees** (text copies, three nodes changed): `AI/BehaviorTrees/Overthrow/Waypoints/
  WP_HouseSearch.bt` (`WP_SearchAndDestroy.bt` with the goal node swapped) and `AI/BehaviorTrees/Overthrow/
  Soldier/HouseSearch.bt` (`MoveAndInvestigate.bt`: `m_eStance STAND`, `m_eSpeed WALK`, first
  `SCR_AISetWeaponRaised` → `m_bWeaponRaised 0`; the threat-GATED raises are untouched, so a patrol that meets
  something still reacts as vanilla). ⚠ `ECharacterStance` is `STAND/CROUCH/PRONE` — there is no ERECT.
- **`Prefabs/AI/Waypoints/OVT_AIWaypoint_HouseSearch.et`** — child of the S&D prefab with our tree; authored on
  the game mode as new **`m_pHouseSearchWaypointPrefab`**; `SpawnHouseSearchWaypoint()` falls back to S&D when
  unset; core's SEARCH case calls it. The plan/movement/persistence side is untouched.

### Facts worth keeping

- **`.bt.meta` resource class is `BehaviorTreeResourceClass`** — no example existed anywhere on disk (the
  extracted reference tree carries no .meta files); it was read out of `ArmaReforgerSteamDiag.exe` with
  `grep -a -o "[A-Za-z]*ResourceClass"`. Hand-minted GUIDs (`A086847134FE94FF` waypoint, `7ABD3B8D152B6DBA`
  soldier, `19BF9E9CE96176F0` prefab) registered fine — **proven**, not assumed: Init
  `…_HouseSearchWaypointResolves` does `Resource.Load()` on both GUID'd names and is green.
- Vanilla's own behaviours reference trees by bare path (`"AI/BehaviorTrees/Chimera/Soldier/Defend.bt"`), so a
  GUID-less ResourceName resolves too; ours carries the GUID anyway.
- EnforceScript calls the parent constructor implicitly with the **same-named** parameters — keep the vanilla
  parameter names verbatim in a behaviour/activity subclass or the base will be mis-constructed.
- If a tree ever fails to load the symptom is a group that reaches the house and **stands still until the hold
  expires**, with nothing in the log naming the tree — open both in Workbench's BT editor and resave.

### Verified / owed

- `compile-check.sh` OK; Init `…_HouseSearchWaypointResolves` (new) green; no tree-parse warnings in the run log.
- ⚠ **Play-test owed**: posture (stand/walk/weapon down), that they still go inside, and whether the 15 s dwell
  reads as "having a look". Author to open both `.bt` files in the BT editor once and resave (the files are
  text copies and have never been through the editor).
- Still owed from the previous entry: All-suite run before commit; civilian reaction later.
