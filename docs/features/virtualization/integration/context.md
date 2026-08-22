# Virtualization Integration - Context

**Last Updated:** 2026-08-17 (Phase 8 complete — feature 62/62 + amendment A1 built)
**Status:** 🟢 Ready for Review (all 8 phases done; **post-completion amendment A1 built, suite run owed**; play-test, MP pass and the wiki sync are owed)
**Current Phase:** — (Phase 8 done; nothing left in this feature but review + the owed human passes)

---

## Quick Status

**What's Done:**
- Planning complete (implementation.md, 8 phases, revised 2026-08-17 for the tower-garrisons-become-a-deployment-config amendment)
- ✅ Phase 1: five inherited defects fixed, dead `DestroyDeployment` wrapper deleted, T1.8 dropped with verdict, **T1.9 observer spike GATE answered** (below). All 237 green 2026-08-17.
- ✅ Phase 2: key/plan statics (`OVT_DeploymentVirtualKey`, `OVT_VirtualPlanFactory`) + 6 Logic cases; serializer **v2** (key appended last, written as-stands never via `EnsureVirtualKey`; v1 payloads never read it); manager `HookVirtualization()` + `NextKeyOrdinal` **probes live keys** instead of counting (counter would collide with restored persisted keys). All 243 green 2026-08-17.
- ✅ Phase 3 (code): T3.1–T3.10. **The first code in the campaign that registers virtualization groups.** Infantry module now holds `array<int>` handles and converges through `EnsureGroups()`; patrol module answers `BuildVirtualPlan()` instead of authoring waypoints; the proximity toggle, `IsPlayerInRange()` and `DeactivateDeployment()` are gone; the evaluator's kill-switch guard is removed (9 → 8). Compile **0**. **All 245 green 2026-08-17.**
- ✅ Phase 4 (code): T4.1–T4.9. **Tower garrisons are a deployment config.** Two new module classes, one new shipped `.conf` + registry entry, `GetLocationTypeAtPosition` ORs in `RADIO_TOWER`, and the garrison halves of `CheckRadioTowers()` are deleted (−73 lines raw / net −51). Kill-switch guards 8 → **7**. Three Init cases added. Compile **0**; **All 248 green 2026-08-17** (sabotage round-trip case included).
- ✅ Phase 5 (code): T5.1–T5.8. **Vehicle crews are registered groups and `OVT_EntitySpawningAPI.c` is gone.** The multi-town module answers `BuildVirtualPlan()` instead of authoring waypoints; the vehicle module registers its crews always-materialised, seats them per member, and deletes a truck at teardown only when nobody's player or recruit is in it and no player owns it; the 40 m rule and the whole 460-line spawning-API file are deleted. Kill-switch guards 7 → **6**. One Init case added. Compile **0**. **All suite owed.**

- ✅ Phase 6 (code): T6.1–T6.6. **The epic's frozen core is additively extended for the fourth time, and something that is not a player can now wake the map.** Four methods (`AddEntityObserver` / `RemoveEntityObserver` / `HasEntityObserver` / `GetEntityObserverCount`) + one operator attribute (`m_bRecruitGroupsAreObservers`, default on) + a stale-entity sweep on the existing 2 s tick + an `OnDelete` teardown; the consumer is `OVT_InactiveRecruitGroupComponent`, which parks an observer following its own group entity. Core diff: **+344 / −2** (the `+305` first recorded here predates the same-day fix for the `EntityID.INVALID` finding; re-measured with `git diff --numstat` in Phase 7, which is the number a later phase's "core unchanged" gate should compare against), and both removed lines are the reworded `OVT_EntitySpawningAPI` comment Phase 5 handed over — that grep is now **0 tree-wide**. Kill-switch guards unchanged at **6**. Two Init cases added. Compile **0**. **All suite owed.**

- ✅ Phase 7 (code): T7.1–T7.7. **The persistence gate now has a deployment tier, and the Campaign GM-registry case asserts for real again.** Four new All-group cases (`Deployment...` ×4), the `OVT_TEST_Campaign_GMGroupRegistry` kill-switch guard **removed** — the last `OVT-VIRT-PLAYTEST-ONLY` hit under `Scripts/Game/Tests/`, ledger **6 → 5** — a full `RegisterGroup(` re-sweep, and a real save point decoded by hand (T7.7 below, including a **pre-feature save with 23 version-1 deployment records**). 🔴 One structural finding recorded below: **the suite's reload seam cannot reach a deployment marker at all**, so three of the four cases assert the restore half only, loudly and on purpose. Compile **0**. **All suite owed.**

- ✅ Phase 8 (docs): T8.1–T8.5. **All three player-facing surfaces fact-checked and synced, except the wiki, which could not be reached.** One Field Manual sentence corrected, one new Field Manual page ("Patrols and Garrisons", 9 new localization keys), 6 stale `file:line` citations re-cited, epic + master rows updated, `api.md` §6 and §8 corrected, kill-switch ledger **balances at 5**. ⚠️ **Wiki sync is OWED** (the `wikijs` MCP tools were not available to the session) and ⚠️ **a Workbench localization re-export is owed** (the `.st` was edited). Suite skipped on purpose: docs-only. Full session note below.

- ✅ **Amendment A1 (post-completion, user 2026-08-17): free-at-game-start deployments.** `m_bFreeAtGameStart` on `OVT_DeploymentConfig` + `SeedFreeDeployments()` at +9 s + the flag authored on Town Patrol and Tower Garrison. Supersedes D1's freeze for that one attribute. 2 new Init cases. Compile **0**. **Suite run owed.** Full section below.

**What's Next:**
- **Suite run for amendment A1** — 2 new Init-tier cases, both in Fast and All.
- **Wiki sync (T8.3)** — not done, tools unavailable. Draft copy is in the Phase 8 session note below, ready to paste.
- **Workbench localization re-export** — 9 new keys + 1 changed key, listed in the session note.
- Final review of the feature, then the §6 play-test and the MP pass.

**Needs human verification (running list):**
- §6 Manual play-test steps 1–13 (incl. step 13 resource-pacing tuning feeding back into T4.5 cost numbers)
- Dedicated-server / MP pass (the automated spine covers no multiplayer)
- **New after Phase 3:** town patrols now walk their perimeter while dormant and materialise wherever they got to; a partially wiped patrol tops back up to its wanted count at the next load (not on every tick); the perimeter footprint changes shape (see the pitch-for-yaw gotcha)
- **New after Phase 4 (all in T4.5/T4.9 below):** the authored garrison cost/size numbers are a starting point, not a derivation; a garrison now registers on the **nearest road**, which the shared snap can take up to 500 m from its tower; and a fully wiped tower garrison is **collected, never rebought**, which contradicts the second sentence of DoD **F15**
- **New after Phase 5 (all in T5.9 below):** a vehicle patrol's crew now **rides real waypoints built by the core** and its route legs are `MOVE` points rather than `PATROL` points; route completion is now **measured by position**, not by counting waypoints; a **reloaded** vehicle patrol gets a brand-new truck for its surviving crew and re-drives its route from the start; and a deployment truck a player has taken over is **left standing at teardown** rather than deleted
- **New after Phase 6 (T6.6 below — the AI-observer cost):** a **parked recruit squad now holds content awake**. Every registered group inside its ring stays materialised with its AI running for as long as the squad stands there — a squad parked in a town keeps that town's patrols **and its radio-tower garrison** spawned, with no player anywhere near. That is the requirement being met and it is the single biggest AI-budget change in the feature; the play-test needs to look at (a) whether a parked squad in a town is affordable on a busy server, (b) whether a garrison materialising next to a parked squad reads as a bug to a player, and (c) whether the shipped default of the off-switch (`m_bRecruitGroupsAreObservers`, ON) is the right one
- **New after Phase 7 (T7.7 below):** one save-inspection claim is **owed** — no version 2 deployment payload exists in any save point on this machine yet, because the CI world saves ~1 s into the campaign and the retail Eden save carries no deployments at all. After the next play-test on this branch, decode the newest save point and confirm a `virtualKey` key appears beside `spawnedUnitsEliminated` (`tools/decode-savepoint.py strings <savepoint>`). Two minutes, and it is the only part of the deployment save format nothing automated can see
- **New after Phase 8 (docs):** ⚠️ **a Workbench localization re-export is required** or the new Field Manual page renders raw keys (9 new + 1 changed key, listed in the Phase 8 session note); ⚠️ **the wiki sync never ran** (the `wikijs` MCP tools were not available to the session) and the wiki's state after the earlier crashed session is **unverified** — fetch each page before editing; and the new "Patrols and Garrisons" Field Manual entry ships with the **placeholder** `default_ui.edds` tile, so it wants a bespoke tile before release
- **New after amendment A1 (below):** the §6 step 13 resource-pacing pass now has a different question to answer. It is no longer "how long does a tower take to get a garrison" — every eligible tower and town has one at **+9 s** — it is **"is the map too full at t0"**: on Eden that is ~2 garrisons plus a patrol in every town, all seeded before the first paid evaluation, none of them costing the occupying faction anything. Watch for (a) AI budget pressure once a player drives across the map, (b) whether the opening pool now goes entirely on vehicle patrols because nothing else needs buying, and (c) whether a *continued* campaign gains deployments it should not (the seed runs on every load; the dedup and the configs' control conditions are what stop it, and both are asserted but only in the small test world)

---

## Post-completion amendment (user, 2026-08-17): free-at-game-start deployments

**Status: BUILT.** Compile `0`. Suite run owed (it belongs to the orchestrator's phase run, not to this
amendment). Feature stays at **62/62**; this is task **A1** in `tasks.md`.

### The report

A play-test on **Easy** found **many radio towers ungarrisoned**. The cause is a chain of three things
that are each individually correct:

1. garrisons stopped being bespoke code and became deployments **bought out of the faction pool**
   ([D17](implementation.md#d17--tower-garrisons-cost-resources-like-any-other-deployment)) —
   `EvaluateFactionDeployments` creates nothing unless `availableResources >= deploymentCost`;
2. **Easy allocates 150 per tick** (`Configs/Difficulty/Difficulty_Easy.conf`, `baseResourcesPerTick`)
   while a Tower Garrison costs **50** (`m_iBaseCost 20` + 2–3 groups × `m_iCostPerGroup 10`);
3. `MAX_DEPLOYMENTS_PER_EVALUATION = 10` paces each 30 s pass, and the evaluator sorts candidates by
   **threat**, which at t0 is near-uniform — so towers compete with every town on the map for the same
   opening pool and lose the ones they lose silently.

There is also a fourth, quieter contributor worth writing down because it survives this amendment: at a
**tower inside a town's bounds**, `FindBestDeploymentConfig` picks ONE config per position by
`m_iPriority`, Town Patrol and Tower Garrison are **both priority 1**, and ties resolve to whichever the
registry lists first — Town Patrol. The evaluator would therefore never choose the garrison at such a
position. Seeding sidesteps this entirely (it asks each config for its own kind of location), but the
evaluator still behaves that way for anything not marked free.

**User decision:** some deployment configs are marked **free at game start**, and for now that set is
**Town Patrol + Tower Garrison**.

### The design, as built

**1. `OVT_DeploymentConfig.m_bFreeAtGameStart`** — bool, `defvalue "0"`. Config data, not runtime state:
no serializer touched, no `CloneModule` concern. Verified by grep that **nothing hand-clones an
`OVT_DeploymentConfig`** — only *modules* are cloned (`OVT_DeploymentComponent.c:56`), and the config
itself is held by reference off the registry, so there is no second copy that could drop the field.

**2. `OVT_DeploymentManagerComponent.SeedFreeDeployments()`** — public, scheduled **once** from
`PostGameStart()` via `CallLater(SeedFreeDeployments, 9000, false)`, i.e. one second **before** the
existing one-shot `EvaluateDeployments` at 10000. Ordering is deliberate and commented at the call site:
run the other way round, the evaluator spends the opening pool on the same places and the seed then
finds them deduped.

Semantics:

| Gate | Seeding | Why |
|---|---|---|
| server-only + `m_bInitialized` | **respected** | same as the evaluator |
| `GetPlayerCount() == 0` | **bypassed** | baseline world state, not a spending decision. Must be on the ground **before the first join on a dedicated server** — this is what legacy tower spawning did unconditionally |
| `m_CurrentQRF` | **bypassed** | nothing is charged, so a QRF's claim on the pool is untouched |
| faction resource pool | **bypassed**, and `m_mFactionResources` is **not touched at all** | |
| `resourcesInvested` | **0**, load-bearing | a free deployment must refund nothing on collection (`SetResourcesInvested`) and must not read as money spent in the GM panel |
| same-name 250 m dedup (`HasExistingDeploymentOfType`) | **respected** | this is both the idempotence *and* what makes a loaded save gain only what it is genuinely missing |
| `m_iMaxInstances` | **respected** | |
| `m_iMaxDeploymentsPerFaction` | **respected**, and **logs a WARNING** naming the config when it bites | **Checked: it does not bite today.** The attribute's authored value is its class default **100** — `Prefabs/GameMode/OVT_OverthrowGameMode.et` declares the component with only `m_DeploymentRegistry` overridden — and Eden has **20 towns** (`Worlds/MP/OVT_Campaign_Eden_Layers/towns.layer`) plus **2 radio towers**, so a full seed is **22** of 100 for the occupying faction. The tension is real but latent: seeded and bought deployments now share one ceiling, so a long campaign or a larger map can reach it, and when it does some location goes without. Respected rather than bypassed, with the warning as the only honest way to see it happen |
| the config's **condition modules** (`EvaluateStaticCondition`) | **respected** | ⚠️ *not* in the amendment's explicit ignore-list, and keeping them turned out to be load-bearing — see below |
| `m_fChance` | **ignored** | a garrison that exists 70 % of the time is not a baseline |
| `m_iMinimumThreatLevel` | **ignored** | threat measures what has already happened; at t0 nothing has. Implemented as a seeding-local copy of `CheckDeploymentConditions` **minus** its threat floor, rather than by calling that method |
| `MAX_DEPLOYMENTS_PER_EVALUATION` | **ignored** | it paces an ongoing spend; there is nothing to pace |

**Why the condition modules are kept** (a judgement call the spec left open, and the one place this
build reads more into "every *eligible* location" than the letter of the amendment): the seed fires from
`PostGameStart()`, which runs on a **continued campaign** as well as a new one. The Tower Garrison's
`OVT_RadioTowerControlConditionDeploymentModule.EvaluateStaticCondition` refuses a tower the occupying
faction does not hold — so **loading a save cannot re-garrison a tower the player has already taken**.
Without this gate the amendment would have shipped a progression-destroying regression. Noted honestly:
`OVT_TownConditionalDeploymentModule` does **not** override `EvaluateStaticCondition` (it inherits the
base's `return true`), so Town Patrol has no creation gate at all — seeding will put a patrol in a town
the player holds, exactly as today's evaluator already does. That is pre-existing behaviour, not
something this amendment introduced, and it is left alone.

**Candidate enumeration:** `CollectSeedCandidates()` unions the **per-location-type getters** for
exactly the bits the config authors, rather than reusing `FindDeploymentCandidates()`. The latter is
right for the opportunistic evaluator and wrong twice over here: it unions the kinds wanted by *every*
config, so a tower position (which classifies as `TOWN | RADIO_TOWER` when the tower stands inside a
town) would be offered to Town Patrol and seed a second patrol on top of the tower; and its
`MIN_DEPLOYMENT_DISTANCE` filter — which exists to stop the evaluator *stacking* deployments — would
make a garrison unseedable purely because a town patrol happens to stand 90 m away. The composite
`GetLocationTypeAtPosition` / `CanUseLocationType` match the evaluator makes is **still applied** on top,
so a config's own location rule is the final word.

A config that authors **no** location types seeds nothing. `CanUseLocationType()` reads `0` as "no
restrictions", but seeding enumerates *places* and "anywhere" is not a place — so marking an
unrestricted config free is a no-op rather than a map-wide flood. Written down because it is the
obvious way for a future config author to be surprised.

**Persistence ordering:** the save is deserialized **synchronously during load**, long before the +9 s
timer fires, so every restored deployment is already registered and visible to the dedup. Stated in the
method's doc comment rather than left to be rediscovered. **No save format changed** — the flag is config
data, and nothing about a seeded deployment's serialized payload differs from a bought one except the
`resourcesInvested` value it always carried.

**3. Authored** on `Configs/Deployment/Deployment_TownPatrol.conf` and
`Configs/Deployment/Deployment_TowerGarrison.conf` (`m_bFreeAtGameStart 1`). Both are inherited by their
`overthrowDeployments.conf` registry entries (Tower Garrison's entry is an empty delta; Town Patrol's
overrides only a module), so no registry edit was needed.

**4. One incidental refactor:** `EnsureFactionDeploymentList()` was extracted and is now used by **both**
the seeding pass and `EvaluateFactionDeployments`. This is not cosmetic — `RegisterDeployment()` only
inserts into the per-faction list *when it already exists*, so a deployment created for a faction that
had never been evaluated would land in `m_aActiveDeployments` and nowhere else: invisible to
`GetFactionDeployments()` and uncounted by the per-faction ceiling. Seeding runs **before** the first
evaluation, so it is the first caller that could ever hit that. The evaluator's own behaviour is
bit-identical (the extracted body is exactly what it inlined).

### ⚠️ D1 freeze supersession

[**D1**](implementation.md#d1--in-place-module-rewrite-the-config-surface-of-the-three-shipped-configs-is-frozen)
froze the config surface of `Deployment_TownPatrol.conf` and the two vehicle patrol configs — "empty git
diff" was a per-phase acceptance criterion throughout the build. **This user amendment, dated
2026-08-17, supersedes that freeze for this one attribute on this one file.** Scope of the
supersession, stated narrowly so nothing else leaks through it:

- `Deployment_TownPatrol.conf` gains **one line**, `m_bFreeAtGameStart 1`, and nothing else;
- the two **vehicle patrol** configs are untouched and remain frozen;
- the attribute is **additive with a false default**, so every other config in this and any downstream
  mod is unaffected by its existence.

D1's actual reasoning (don't perturb authored behaviour mid-migration) is not violated — the migration
is finished, and the user is the one changing the authored behaviour.

### Fixture-safety verdict for the new cases (extends the T7.1 table)

| Site | Case | Plan | Verdict |
|---|---|---|---|
| *(none)* | `Deployments_FreeAtGameStartIsAuthored` (A1) | — | **safe** — a pure read of the shipped registry. Nothing created, nothing registered, nothing mutated |
| *(no direct `RegisterGroup(`)* | `Deployments_FreeSeedingIsFreeAndIdempotent` (A1) | — | **safe on BOTH grounds, deliberately.** It creates **real deployments** (unusual for this suite), so: (a) every one is `SetSpawnedUnitsEliminated(true)` on the deployment **and** on every spawning module — `ConvergeGroups()` refuses at both gates; **and** (b) `SeedFreeDeployments()` is synchronous, so creation, both passes and teardown all happen inside **one `Execute()` frame** and no `UpdateDeployment` tick can run in between. No group is ever registered, so there is nothing for the movement tick to walk. Teardown runs on every path including the red ones |

`grep -rn "RegisterGroup(" Scripts/Game/Tests/` is **unchanged by A1** — neither new case adds a call
site. The second case reaches the registration machinery only in the sense that it deliberately prevents
it from being reached.

Two pieces of **shared world state are borrowed and handed back exactly as found** by the second case,
the same pattern `Deployments_TowerCaptureOnlyOnRealWipe` established: the first radio tower's
controlling faction (set to the occupying faction, because the garrison's control condition rightly
refuses a tower the resistance holds — without it the case would assert nothing) and the occupying
faction's resource pool (a known 5000 is planted so "nothing was charged" is a claim about a budget that
*could* have been spent, rather than about a pool that was 0 either way). The pool is restored by
difference, so it comes back to its original value even on a red path where something *did* charge it.

### Tension with R15 / D17, and with the Field Manual

[**R15**](implementation.md#9-risks--mitigation) and
[**D17**](implementation.md#d17--tower-garrisons-cost-resources-like-any-other-deployment) accept
"towers may be found ungarrisoned when the occupying faction is short of resources" as a deliberate
consequence, and Phase 8 put a matching sentence in the Field Manual. **A1 softens that to a rare
case.** After this change an *occupying-held* tower is ungarrisoned only:

- between a garrison wipe and the capture flip (a window of at most one ~10 s deployment tick), or
- when `m_iMaxDeploymentsPerFaction` or `m_iMaxInstances` bites, or
- if the occupying faction **re-takes** a tower mid-campaign — the seed has already run, so a retaken
  tower still waits for the paid evaluator exactly as R15 describes.

**The Field Manual text is deliberately left alone.** It is still true, just rarer, and rewriting
player-facing copy is not this amendment's business — but the tension is recorded here so the §6
play-test pass can decide whether the sentence still earns its place. R15's mitigation list should be
read as gaining a fourth entry ("marked free at game start") the next time that table is revised.

---

## Key Decisions Made

(see implementation.md §5 D1–D21 — this file records only what emerges during the build)

### The key scheme, as built (Phase 2, T2.1/T2.4/T2.6)

A deployment's key is `<sanitised config name>@<round(x)>_<round(z)>` off the marker's origin, derived once and persisted, and a spawning module's owner key is `<deployment key>#<sanitised module tag>` where the tag is the authored `m_sModuleName` or `"m"+index` — `@`, `#` and whitespace are stripped out of both free-text parts, which is what stops `("a#b","c")` and `("a","b#c")` composing to one string. Collisions on one rounded spot are separated by an ordinal the manager resolves by **probing live deployments' keys** (`NextKeyOrdinal`, first free of 0 → 2 → 3 …) rather than by a counter: a counter would have to be persisted, because a restored deployment carries its saved key without ever asking for an ordinal and a session-local counter would then hand a new deployment on that spot the key the restored one is already using.

### Ordering, so nobody re-derives it (implementation.md §3.5)

`ApplyPersistedRegistry()` runs **synchronously inside the deserialize pass**, and a deployment's own `InitializeDeployment` runs in that same pass, so the registry is already populated by the time the first `UpdateDeployment` tick fires 8–12 s later. Even if the two ever did race, the system converges at the cost of one wasted registration — which is why `EnsureGroups()` is written as **converge to `wanted`**, never as **spawn `wanted`**, and why it is safe to call from activation, from the records-restored fan-out and from the rebuy path in any order and any number of times.

---

## Kill-switch removal ledger

Baseline before Phase 3 (to be verified by grep at each removal): guards exist for deployments evaluator, tower garrison spawn, base upgrades ×2, QRF queue, `OVT_EntitySpawningAPI.c:49`, plus the switch file itself.

| Phase | Guard removed | How |
|---|---|---|
| 3 ✅ | `OVT_DeploymentManager.c:289` (evaluator — the plan said `:144`, Phases 1/2 moved it) | un-guarded; the evaluator's own `GetPlayerCount() == 0` and `m_CurrentQRF` early returns are **kept** and now carry a doc comment saying why (D9) |
| 4 ✅ | `OVT_OccupyingFactionManager.c:564` (tower spawn) | **deleted with its enclosing block** (D20) — the guard line does not appear anywhere in the Phase 4 diff as a surviving line; it is inside a 73-line deletion. It was **never un-commented**, which is R16's whole point: an un-guarded line would have revived legacy tower spawning alongside the new config and doubled every garrison |
| 5 ✅ | `OVT_EntitySpawningAPI.c:49` | **file deleted outright** — the T5.7 gate was re-run first and came back with exactly two live callers, both in the vehicle module this phase rewrote. The guard line does not appear anywhere in the Phase 5 diff as a surviving line; it went with the 460-line file |
| — | base upgrades ×2, QRF queue, switch file | stay until epic end |

| 6 — | *(none — Phase 6 migrates no legacy spawner; it adds the observer API)* | count re-verified **unchanged at 6** after the phase |
| 7 ✅ | `OVT_TEST_Campaign_GMGroupRegistry.c:74` (the test's trivial-pass guard) | **un-guarded** (T7.6). The only guard in the removal set that was never around production code — it made the case pass while asserting nothing, because the epic had silenced every producer it could see. Deployments (incl. tower garrisons) tag again, so it asserts for real. **This was the last `OVT-VIRT-PLAYTEST-ONLY` hit anywhere under `Scripts/Game/Tests/`** |

**Count after Phase 7: 5** (was 6, 7, 8, 9). The 5 remaining, verified by `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/`:
`OVT_BaseControllerComponent.c:84`, `:298`; `OVT_QRFControllerComponent.c:369`;
`OVT_VirtPlaytestKillSwitch.c:1`, `:5` (the switch file's own header).
All five belong to systems this feature does **not** migrate (base upgrades ×2, the QRF queue) plus the
switch file itself, which is exactly the set Phase 8's T8.5 ledger expects to find. The switch leaves at
epic end, not here.

### T8.5 verdict — 2026-08-17 — THE LEDGER BALANCES

`grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` re-run at the close of Phase 8 returns **exactly 5 hits**,
and exactly the 5 predicted ones:

```
Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c:84
Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c:298
Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c:369
Scripts/Game/GameMode/Virtualization/OVT_VirtPlaytestKillSwitch.c:1
Scripts/Game/GameMode/Virtualization/OVT_VirtPlaytestKillSwitch.c:5
```

Base upgrades ×2 + the QRF spawn queue + the switch file's own two header lines. All three production
guards belong to systems this feature does **not** migrate; they are `base-defense-migration`'s to
remove, and the switch file itself leaves at **epic end**.

**Every removed guard is accounted for by the table above (9 → 8 → 7 → 6 → 5), and the two removals
that mattered are re-confirmed by their absence rather than by a comment:**

- **Tower guard (Phase 4, `OVT_OccupyingFactionManager.c:564`) — GONE WITH ITS ENCLOSING BLOCK.**
  `grep -n "OVT-VIRT-PLAYTEST-ONLY\|DISABLE_LEGACY_AI_SPAWNS" Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  returns **nothing at all** — not a live line and not a commented one. This is R16's exact test: an
  un-guarded line would have revived legacy tower spawning beside the new config and doubled every
  garrison.
- **`OVT_EntitySpawningAPI.c:49` (Phase 5) — GONE WITH ITS FILE.** The file does not exist
  (`git status` shows it deleted) and the identifier is 0 tree-wide.

No guard was un-commented anywhere. **Verdict: balanced.**

---

## T7.1 — `RegisterGroup(` fixture sweep, FULL RE-SWEEP — 2026-08-17

`grep -rn "RegisterGroup(" Scripts/Game/Tests/`, re-run from scratch (not incrementally) before a
single Phase 7 case was written, because Phases 3, 4 and 5 each added sites since the last full pass.
Movement's D12 discipline: a fixture is safe only if **(a)** it registers a null / empty / DEFEND-only
plan, or **(b)** it registers and unregisters inside **one frame**. Anything else is walked by the
movement tick, and a walked fixture turns a position claim into a timing lottery.

**19 hits, 18 real call sites + 1 comment. Every one safe. Nothing had to change.**

| Site | Case | Plan | Verdict |
|---|---|---|---|
| `Init:3540`, `Init:3562` | `RegisterRefusesUnknownComposition` | — | **safe** — both registrations are *refused* (`-1`); no record is booked |
| `Init:3746` | `RegisterBuildsDormantGroup` | `null` | **safe** by (a) **and** (b) |
| `Init:3903`, `Init:3905` | `GetAllHandlesEnumeratesRegistry` | omitted → `null` | **safe** by (a) **and** (b) |
| `Init:4223` | `VirtualMovement_TickAdvancesDormantGroup` | PATROL, movable | **safe, and walked ON PURPOSE** — being walked is the case's subject; `spawnDistanceOverride = 0` |
| `Init:4418` | `VirtualMovement_StationaryPlanIsNeverAdvanced` | DEFEND | **safe** by (a); `spawnDistanceOverride = 0` |
| `Init:4621` | `VirtualMovement_ManagerResolvesAndDoesNotLeak` | DEFEND | **safe** by (a); `spawnDistanceOverride = 0` |
| `Init:4765` | `Virtualization_WaypointsAreOwnedAndDeleted` | PATROL + MOVE, cycling | **safe by (b) only** — genuinely movable, but unregistered in the same frame and asserts nothing about position |
| `Init:4910` | `DeathsFlipMaskAndWipeRecord` | omitted → `null` | **safe** by (a) + (b) |
| `Init:5098` | `MaskDrivesSlotSelection` | omitted → `null` | **safe** by (a) + (b); deliberately materialises a member, which the movement tick's `IsSpawned` gate skips |
| `Init:7928` | `Deployments_EnsureGroupsIsIdempotent` (Phase 3) | `null` | **safe** by (a) **and** (b); `spawnDistanceOverride = 0` |
| `Init:8418` | `Deployments_TowerCaptureOnlyOnRealWipe` (Phase 4) | `null` | **safe** by (a) **and** (b); `spawnDistanceOverride = 0`, importance HIGH; every handle is wiped or unregistered in the same `Execute()` frame |
| `Persistence:3918` | `VirtualGroupsWiped_DoNotComeBack` — the wiped group | `null` | **safe** by (a); asserts absence, never position |
| `Persistence:4017` | same case — the resurrection group | `null` | **safe** by (a) |
| `Persistence:4165` | *(not a call site — the sweep note in a case header)* | — | — |
| `Persistence:4420` | `VirtualGroups_SurviveSaveAndReload` — the BOGUS group | `null` | **safe** by (a) |
| `Persistence:4648` | same case — the save/reload fixture | DEFEND ×2, cycling | **safe** by (a) — the types were changed to DEFEND by movement's T3.1 for exactly this reason; **do not revert them to PATROL** |
| **`Persistence:6253`** | **`DeploymentOwnedGroups_ReclaimAfterReload` (NEW, T7.5)** | `null` | **safe** by (a); `spawnDistanceOverride = 0` (Manual policy — never materialise), 3 registrations, all released before the case reports on every path including the red ones |

**The three NEW deployment-marker fixtures (T7.2/T7.3/T7.4) register nothing at all**, and that is a
property of how they are built rather than of how fast they finish: each is marked
`SetSpawnedUnitsEliminated(true)` on the deployment **and** on every spawning module before anything
can tick, and `ConvergeGroups()` refuses on either gate. Without it a live "Town Patrol" fixture would
register up to four groups at the **global 1750 m** ring on its own 8–12 s activation tick — inside
which the autotest camera is an observer — and hand the movement tick a cycling perimeter plan to walk.
Being inert by construction is what makes them survive a host stall (this project has measured a 105 s
main-thread stall in this harness) rather than merely usually finishing first.

**Standing rule for the next phase that adds a fixture here:** a deployment fixture that is *not*
marked eliminated is unsafe no matter how short the case looks.

## T7.6 verdict — the Campaign GM-registry case is UN-GUARDED — 2026-08-17

**Guard removed. It does NOT assert on `RADIO_TOWER_GARRISON`, so nothing had to be re-pointed.**

What the case actually asserts, checked before touching it: (1) the registry is non-empty after a
sweep, (2) no entry carries `OVT_EGroupOrigin.UNKNOWN`, (3) `Sweep()` is idempotent, (4) every tagged
group carries an `RplComponent`. Not one of those names an origin member, so §3.7's re-point clause
never fired — `RADIO_TOWER_GARRISON` stays declared, unproduced and untouched (T4.9 note 4).

**The producer moved, and that is the substantive change.** The case's header described a
base-upgrade chain (`OVT_BasePatrolUpgrade.BuyPatrol`, ~9–11 s) that is **still kill-switched** and
will be until epic end. What feeds the registry now is the deployment wave, and its chain is longer:

```
NewGameStart()   AllocateDeploymentResources(baseResourcesPerTick = 250)   <- the budget exists from t0
+10 s            first EvaluateDeployments(); shipped "Town Patrol" costs 0 and the test world's one
                 town is occupying-held with 0 support, so it is created there
+8-12 s more     that deployment's own jittered UpdateDeployment tick activates it -> EnsureGroups()
                 -> RegisterGroup -> TagForGameMaster()
=> first tag at ~18-22 s after campaign start, or ~48-52 s if the first evaluation produces nothing
   and the 30 s one after it does.
```

The old budget was `MAX_WAIT_MS = 25000` measured from the case's own first tick, sized for a ~9–12 s
observable. That covers the first-evaluation path with little to spare and does **not** cover the
second cycle at all, so it was raised to **55 s** (case `timeoutS` 60 → 90). This is not a retry
budget and it is not a way to make a red case pass: a green run exits the frame the registry is
non-empty, so the longer bound costs nothing except on a run that was going to be red. It is the
"bound on the EXPLANATION" the case's own header already claimed to be.

**The failure text now prints a deployment ledger too** (`DescribeDeploymentState()`: how many
deployments exist, their names, virtual keys, active/wiped flags, the occupying faction's deployment
budget when there are none, and the registry's total group count). "Registry empty" now has three
interesting causes rather than two — no deployment was ever created (evaluator refused: 0 players, an
active QRF, or no budget); deployments exist but have not converged yet; deployments hold groups and
nothing tagged them — and only the ledger tells them apart. Whoever reads the first red run gets the
answer without a repro.

⚠ **If it does come back red, read the ledger before restoring the guard.** "No deployment exists"
is an environment/pacing fact and the honest fix is the budget or a Campaign-tier precondition, not
the guard; "deployments hold groups, registry empty" is a real broken tag and the case is doing its
job. Restoring the guard is only correct if base-upgrade producers turn out to be *required*, which
the chain above says they are not.

## T7.7 — a real save point, decoded and read by hand — 2026-08-17

Ran `tools/decode-savepoint.py` (offline, read-only) over **every** save point on this machine — 5
worlds, 8 playthroughs, 40 blobs. Findings, in order of usefulness:

**1. A pre-feature save with 23 VERSION 1 deployment records exists, and its records are exactly the
shape the migration path assumes.** `ArmaReforgerWorkbench/…/D73CC1435D948AA1-OVT-Campaign-Eden/
playthrough000/savepoint000` (2026-08-04, 1481 records). Decoded bytes around one of the 23:

```
components { OVT_DeploymentComponent:65B2DE4CFAD412BA
  version 1 | configName "Town Patrol" | controllingFaction 3 | threatLevel 0 |
  resourcesInvested 0 | spawnedUnitsEliminated <bool>
```

`grep -c virtualKey` over the whole blob: **0**. `configName`: 24. `spawnedUnitsEliminated`: 24. So
the version 1 field set is complete, terminates at the wipe-out flag, and carries no key — which is
precisely the payload T7.4's case hands `ApplyPersistedDeployment`. **A real pre-feature save exists
in the wild and every one of its deployments takes the empty-key path.**

**2. 🔴 The save context is NAME-KEYED, not blind-positional — and this is a real safety margin
nobody had measured.** Every `context.Write(x)` writes the local's *identifier* as a key beside the
value: the blob literally contains `version`, `configName`, `controllingFaction`, `threatLevel`,
`resourcesInvested`, `spawnedUnitsEliminated`. The serializer headers' "binary contexts are
POSITIONAL, write order must equal read order" rule is still the right discipline to code by (the
reader consumes in order and the version gate is what stops a v2 reader eating a v1 record's
neighbour), but the stored form is self-describing, which is why a decoded save is readable at all.
⚠ **Consequence worth knowing: renaming a local variable in a serializer changes the stored key.**
Nothing in this feature does, but it is a live hazard for anyone "tidying" a serializer.

**3. No version 2 deployment payload exists on this machine yet, and that is not a gap in the code.**
Raw scan for `virtualKey`, `TownPatrol@`, `TowerGarrison@`, `LightVehiclePatrol@`, `HeavyVehiclePatrol@`
across all 40 blobs: **zero hits.** Why, per save family:
- **CI test world** (`OverthrowCI`, 10 save points from the Phase 6 suite run, 09:17:05–06 UTC): the
  whole Persistence suite saves within **~1 second** of each other, i.e. long before the evaluator's
  first run at +10 s. 11–13 records each, no deployment markers. *This is also the timing T7.2's own
  save lands in, which is why that case creates its deployment itself instead of waiting for one.*
- **Live Eden campaign** (`ArmaReforger/…/3DAD390C31623F04-24-OVT-Eden`, newest 2026-08-17 09:32 UTC,
  282 records, 166 Overthrow): `decode-savepoint.py prefab … 53D8FEE526831693` → **0** deployment
  markers, and no `OVT_PersistedVirtualGroup` entries either. That profile is the retail client, not
  the Workbench one; it carries `OVT_PersistedRadioTower`, `OVT_PersistedBaseUpgrade`,
  `OVT_PersistedJobV2` and no deployment or virtualization state at all.
- **`OverthrowDS`**: 2026-08-06/09, pre-feature.

So the version 2 write path has **not** been observed in a stored blob. It is asserted live by T7.2
(which really does take a save point with a keyed deployment tracked) and it is the one T7.7 claim
that remains **inspection-owed**: decode a save point taken from a campaign that has run for >30 s on
this branch and confirm a `virtualKey` key appears. That is a two-minute check after the next
play-test and it belongs on the manual list.

**4. The virtualization registry payload round-trips owner keys verbatim — read off the blob.**
`OverthrowCI/…/savepoint013` contains, in one readable run:
`OVT_VirtualizationManagerComponent → version, nextHandle, records[ $type OVT_PersistedVirtualGroup,
handle, ownerSystem "test_virtualization", ownerKey "roundtrip_virtual_group", factionKey "US",
groupRegistryName "light_patrol", resolvedPrefab, spawnDistanceOverride, importance, position,
slotAlive[], waypointPositions[], waypointTypes[], waypointParams[], waypointCycle ]`. Owner system
and owner key are stored as plain strings, which is what T7.5's `@`/`#` claim rests on.

**5. 🔴 The plan's "there is no vanilla `AIGroup`/`AIUnit`/`AIWaypoint` record for anything core owns"
is WRONG as written, and the code is right.** `AIWaypoint` records are present and expected:
`savepoint016` has 27, the pre-feature Eden save has 342, and core's own serializer header says so —
*"the waypoint ENTITIES are not persisted (they are **tracked but `SelfSpawn 0`**, and must stay that
way — every legacy Overthrow spawner builds waypoints through the same helpers, so a self-spawn rule
would resurrect every garrison and patrol waypoint in the save)"*. `Overthrow.conf`'s AI group carries
four `EntityPersistenceConfig` overrides, all `SelfSpawn 0`. The correct claim is **"no core-owned AI
record is ever SELF-SPAWNED back"**, not "no record exists": records are written, and nothing rebuilds
an entity from them. `AIGroup` records exist only in pre-feature saves (136 in the 2026-08-04 one, 0
in the current Eden save), which is the untrack fix working. **Phase 8 should correct the sentence
rather than a reader filing it as a bug.**

**6. Tower records: unchanged in field order.** `OVT_PersistedRadioTower` is present in the current
Eden save, and `git diff` over `Scripts/Game/Persistence/` shows the only serializer this feature
touched is `OVT_DeploymentComponentSerializer` (+38/−…); `OVT_PersistedRadioTower`, its serializer,
`RplSave`/`RplLoad` and `ApplyPersistedOccupyingFaction` are all untouched — the same empty diff
Phase 4's T4.1 gate demanded, re-verified here from the other end.

## T6.6 — what an AI observer COSTS, and why the off-switch is the whole answer — 2026-08-17

**An observer does not "wake a group up" — it holds every registered group inside its ring
materialised, with full AI running, for exactly as long as it exists.** That is one call and an
unbounded, open-ended bill. A player who parks a squad of recruits in the middle of a town has, from
Phase 6, pinned that town's patrol deployments *and* — because Phase 4 made tower garrisons ordinary
deployments — any radio-tower garrison inside the ring, spawned and thinking, indefinitely, whether or
not a single human being is within kilometres. The ring is the registration's own
(`virtualizationSpawnDistance`, **1750 m** as shipped, ×1.15 to despawn), so "inside the ring" is a
3.5 km-wide circle around the parked squad, not a courtyard. Two or three parked squads in different
towns are two or three of those circles, permanently, for the rest of the campaign — and nothing about
it is visible to the player who caused it, or to an admin reading a player count.

This is not a regression and it is not an accident: it is [G7](#) and requirement "*a recruit group
left in a town alone causes the enemy patrol to materialise*", implemented exactly as asked. The cost
**is** the feature. What makes it acceptable rather than reckless is that it is (a) opt-in per consumer
— core never parks an observer for anybody who did not ask — (b) bounded by the same spawn ring
everything else in the epic is bounded by, (c) swept when its entity disappears and torn down at world
teardown, so it cannot accumulate across a session or a campaign restart, and above all (d) **switchable
off by an operator without a code change**: `m_bRecruitGroupsAreObservers` on the virtualization
manager, default **true**, read by the parked group's own marker component. An admin who finds parked
squads holding a server's AI budget hostage flips one attribute on the game-mode prefab and parked
recruits are inert again — with nothing else in the epic changing behaviour, because the gate is read
by the consumer and not inside `AddEntityObserver`. That single knob is why the default could safely be
"on".

## T5.5 — `SCR_AIGroup.HasHeldMember` VERDICT, read off the engine source before the check was written — 2026-08-17

**It protects AI CREWMEN mid-use. It says nothing whatever about players, and it is NOT the
stolen-vehicle guarantee on its own.** Read at `ArmaReforger/scripts/Game/Entities/SCR_AIGroup.c:2795-2815`
(1.8.0.10, build 24548213) — vanilla, not a modded override:

```c
bool HasHeldMember()          // "held in materialised state by an external system"
{
    foreach (AIAgent agent : <this group's agents>)
    {
        if (agent.IsAIActivated())        return true;   // driver in a vehicle, player-squad
        if (agent.GetPermanentLOD() != -1) return true;   // explicit LOD pin (GM tools etc.)
    }
    return false;
}
```

Four consequences, all of which shaped `OnCleanup`:

1. **It iterates the group's OWN agents.** A player sitting in a deployment's truck does not make that
   truck's crew group "held" — the crew is held only if one of *its* men is doing something. So the
   player-occupancy question has to be asked of the VEHICLE, separately. That is exactly what D11 says
   ("`UnregisterGroup` respects held members" **and** "a player-occupancy / player-ownership check"),
   and it is why the two are two calls and not one.
2. **A dormant group is never held**, because `GetAgents()` returns nothing for it. That is correct and
   not a hole: a dormant group has no characters in the world for anything to be using.
3. **Core already consults it, and only there.** `OVT_VirtualizationManagerComponent.c:1485` -
   `if (group.GetAgentsCount() > 0 && group.HasHeldMember())` → `RetireGroupEntity(group)` instead of
   deleting. So a crew whose driver is at the wheel is retired in place by `UnregisterGroup` and the
   engine despawns it when its observers leave. **Nothing was rebuilt here; the module just calls
   `UnregisterGroup` and lets core do it.**
4. ⚠ Vanilla itself distrusts the `IsAIActivated` half: the sibling `HasPermanentLodMember`
   (`:2817-2831`) exists precisely because "that state can persist after a player teleports away and no
   observer remains in range - in that case no player is using any agent so the activation state is
   stale". Worth knowing if a retired husk is ever seen lingering; it is not a reason to second-guess
   the call at teardown, where erring toward *not* deleting is the safe direction.

### The ownership check used, and why that one

`OVT_PlayerOwnerComponent.GetPlayerOwnerUid() != ""` on the vehicle — **the project's existing notion**,
not a new one. It is the same test used by:

- `Scripts/Game/Systems/Modded/SCR_GarbageSystem.c:12-18` — the modded engine garbage collector refuses
  to collect a vehicle carrying one. If it is the bar for the engine's own delete path, it is the bar
  for ours.
- `OVT_VehicleManagerComponent.FilterPlayerOwnedVehicles` (`:1560-1575`) — the manager's own definition
  of "a player-owned vehicle" when it re-indexes the map.
- `OVT_TownVehicleSourceConfig.IsVehicleClaimed` (`:521-537`) — civilians' despawn hatch, the closest
  analogue there is: a hatch deciding whether it is allowed to delete a car.

**The component is verified present on the shipped patrol vehicles**, which is what makes the check mean
anything: `light_armed` → `UAZ469_PKM.et` and `heavy_armed` → `BTR70.et`
(`Configs/Factions/USSR_OverthrowData.conf:24-32`) both inherit `Prefabs/Vehicles/Core/Wheeled_Base.et`,
which carries `OVT_PlayerOwnerComponent`. And the claim really is automatic: `SCR_GetInUserAction`
(modded, `:24-42`) fires `OVT_VehicleRequestComponent.ClaimUnownedVehicle` when a player enters the
**pilot** compartment of a vehicle whose uid is empty, and the server stamps it
(`RpcAsk_ClaimUnownedVehicle`, `:236-263`). So "player drove off in the patrol's truck" ⇒ owned ⇒ spared.
⚠ A player who only ever rode as a **passenger** and then got out leaves no uid and is not aboard, so
that truck IS deleted at teardown. D11's wording accepts that; it is the one case the old 40 m rule
covered and this one does not.

`OVT_VehicleManagerComponent.GetOwnerID(entity)` (the RplId-keyed ownership map) was considered and NOT
used: it is a second, replicated bookkeeping surface that a freshly claimed vehicle only joins through
the request component, whereas the component uid is stamped on the entity itself and survives a
save/restore through `OVT_PlayerOwnerComponentSerializer`. Its sibling `IsOwned(EntityID)` additionally
dereferences the entity's `RplComponent` unguarded (`OVT_RplOwnerManagerComponent.c:145-158`), which
would VME on any vehicle without one.

**One addition beyond the task's wording, deliberately:** an occupant carrying a non-empty
`OVT_PlayerOwnerComponent` uid also vetoes deletion — that is a player's **recruit** riding in the
truck. The same distinction civilians draws in its own header ("the despawn hatch must not delete a car
with a player's RECRUIT in it either"). It is the same notion applied to the occupant instead of the
vehicle, it costs three lines, and the asymmetry justifies it: failing to delete leaks one truck,
deleting wrongly destroys a player's squadmate.

## T4.1 — the tower survey, run read-only BEFORE any edit — 2026-08-17

Every claim Phase 4's deletion rested on, re-verified against the tree rather than trusted from the plan.
**No persisted or streamed field was touched**, and the acceptance greps below were re-run after the edit.

| Claim | Verdict |
|---|---|
| `OVT_RadioTowerData.garrison` has **no reader outside `CheckRadioTowers`** | ✅ **Confirmed.** Tree-wide `grep -rn "garrison"` over `Scripts/` returned the declaration (`:72-73`) plus 8 uses, all inside `CheckRadioTowers` (`:562, 583, 593, 608, 610, 617, 620, 625`). Every other `garrison` hit in the tree belongs to `OVT_BaseData.garrison` / `garrisonEntities`, the resistance manager's camp & FOB lists, or the FOB/resistance request components — different types, different data |
| It is `[NonSerialized()]` | ✅ **Confirmed** at `:72` |
| Absent from `OVT_PersistedRadioTower` | ✅ **Confirmed.** `Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c:59-67` declares exactly `location`, `faction`, `disabledRemaining` — field order unchanged, file **not edited** (empty `git diff`) |
| Absent from the JIP stream | ✅ **Confirmed.** `RplSave` writes `location`, `faction`, `disabledRemaining` per tower; `RplLoad` reads the same three in the same order. Neither was edited |
| `ApplyPersistedOccupyingFaction` untouched | ✅ **Confirmed** — see the deliberate non-edit note in Gotchas below; the gate forbids touching it even for a now-stale comment |

**Consequence:** deleting the field moves neither a saved nor a replicated byte. This is why the Phase 4 edit
to a 1678-line replication-adjacent manager was safe: it is a **deletion of `[NonSerialized]`, unread state**.

## T4.2 — composition verdict: `light_patrol` MATCHES for both shipped factions — 2026-08-17

**No `tower_garrison` registry entry was needed. The config uses `light_patrol`.**

| Faction | `m_aTowerDefensePatrolPrefab` | `light_patrol` registry entry | Verdict |
|---|---|---|---|
| USSR (`Configs/Factions/USSR_OverthrowData.conf`) | `:67` → `{CB58D90EA14430AD}Prefabs/Groups/OPFOR/Group_USSR_SentryTeam.et` | `:6-7` → `{CB58D90EA14430AD}Prefabs/Groups/OPFOR/Group_USSR_SentryTeam.et` | ✅ **identical GUID and path** |
| US (`Configs/Factions/US_OverthrowData.conf`) | `:62` → `{3BF36BDEEB33AEC9}Prefabs/Groups/BLUFOR/Group_US_SentryTeam.et` | `:6-7` → `{3BF36BDEEB33AEC9}Prefabs/Groups/BLUFOR/Group_US_SentryTeam.et` | ✅ **identical GUID and path** |

So a `light_patrol` group registered for either occupying faction is byte-for-byte the composition the old
`SpawnEntityPrefab(faction.m_aTowerDefensePatrolPrefab, pos)` produced, and the core resolves it by
`(factionKey, groupName)` rather than by a faction index or a raw prefab, as `api.md` §3 requires.

`m_aTowerDefensePatrolPrefab` now has **zero readers** (`OVT_Faction.c:362` declaration + two authored values
in the faction configs + two in `Prefabs/GameMode/OVT_FactionManager.et`). **Left declared** per T4.7 — it is
authored in shipped configs and prefabs, and removing the attribute would break their parse. Cleanup is
feature 5's (`base-defense-migration`) to do alongside its own faction-prefab retirements.

## T4.5 — the Tower Garrison config, as authored — 2026-08-17

`Configs/Deployment/Deployment_TowerGarrison.conf`, GUID **`{6B2E4A7C00000001}`** (whole `6B2E4A7C` prefix
grep-verified unused repo-wide before use; module/registry containers use `…0002`–`…0007`). One registry
entry appended to `overthrowDeployments.conf` in the shipped shape — an inheritance line with an empty
override body, which is the same shape `Configs/Factions/CIV.conf` and `FieldManualConfigRoot.conf` use.
The game-mode prefab already inherits the registry `.conf` with an empty override block, so **no prefab edit
was needed** and the entry is picked up automatically.

**⚠ MODULE ORDER IS LOAD-BEARING, and a `.conf` cannot carry a comment saying so.** Modules are cloned into
`m_aActiveModules` in authored order and `UpdateDeployment` walks them in that order, so:

1. `OVT_InfantrySpawningDeploymentModule` — the garrison
2. `OVT_PatrolBehaviorDeploymentModule` — **first behaviour module ⇒ its DEFEND plan is the one every group registers with** (`ResolveVirtualPlan` takes the first non-null answer)
3. `OVT_RadioTowerCaptureBehaviorDeploymentModule` — **must precede the reinforcement module**, or the pass that decides the deployment is over runs before the capture is ever considered
4. `OVT_ReinforcementBehaviorDeploymentModule`
5. `OVT_RadioTowerControlConditionDeploymentModule`

The requirement is repeated in `OVT_RadioTowerCaptureBehaviorDeploymentModule`'s class header, which is the
only place a reader will find it.

| Value | Authored | Why, and how firm |
|---|---|---|
| `m_iMinGroupCount` / `m_iMaxGroupCount` | **2 / 3** | 🎚️ **TUNING.** Matches today's Normal-difficulty spread exactly: `RandInt(patrolGroupsMin=2, patrolGroupsMax=4)` is max-exclusive, so today a Normal tower gets 2–3 groups. ⚠ The **difficulty scaling is compressed** relative to today at Hard and above, because the shared `CalculateGroupCount` halves the difficulty band and then clamps to these bounds: Hard/Extreme/Insane towers get 2–3 groups where today they get 4–5 / 6–8 / 8–11. Raising `m_iMaxGroupCount` is the dial |
| `m_iBaseCost` / `m_iCostPerGroup` | **20 / 10** → total **50** | 🎚️ **TUNING (D17).** Deliberately not presented as derived. Calibration: the Normal deployment pool starts at `baseResourcesPerTick` = **250** and is topped up toward 500; Town Patrol costs **0**; each vehicle patrol costs **50**. Eden has 2 towers, so a full set of garrisons costs 100 of a 250 pool. §6 step 13 is the pass that decides whether this is right |
| `m_iReinforcementCost` | **25** per group | 🎚️ **TUNING.** Half the Town Patrol's 50, matching the garrison's smaller size. See the F15 note below — on this config the rebuy is very hard to reach |
| `m_fSpawnRadius` | **15** | Near-parity with the old single point (`tower.location + "5 0 0"`). ⚠ **Not a 15 m guarantee** — see the road-snap gotcha |
| `m_eImportance` | **HIGH** | R5. Asserted on the template **and on the clone** by `…TowerGarrisonHoldsItsPost` |
| `m_bScaleByTownSize` / `m_bSpawnAtNearestBase` | **0 / 0** | §3.3 |
| `m_bReinforceFromNearestBase` | **0** | ⚠ **NOT in the plan; a correctness call.** The class default is `true`, which anchors a rebuy on the nearest controlled base — for a tower that would register the replacement garrison at a base possibly kilometres away, holding a DEFEND plan, with no way to ever reach its tower |
| `m_ePatrolType` | **DEFEND** | D4. One point, no cycle, nothing movable — asserted |
| `m_bUseNearestTownCenter` | **0** | §3.3. Authored explicitly rather than left to the default, so the garrison's post cannot drift to a town centre if the default ever changes |
| Reinforcement `m_fInitialDelay` / `m_fCheckInterval` | **60000 / 20000** ms (from 300000 / 60000) | §3.3 — a lost tower's deployment is collected within ~1 min instead of ~5 |
| `m_bEnableReinforcement` | **1** | ⚠ **Load-bearing for the TEARDOWN, not the rebuy.** `OnUpdate` returns early when this is false, and the `m_bDeleteOnConditionFail` branch lives inside `CheckReinforcement()` — so with reinforcement disabled a captured tower's deployment would never be deleted at all |
| `m_bDeleteOnConditionFail` | **1** | The reverse direction of the capture loop (D18) |
| `m_iAllowedFactionTypes` / `m_iAllowedLocationTypes` | `OCCUPYING_FACTION` / `RADIO_TOWER` | §3.3 |
| `m_iPriority` / `m_fChance` / `m_iMaxInstances` | **1 / 100 / -1** | R17 — `-1`, never 1. One-per-tower comes from the evaluator's 250 m same-name dedup |

**Eden tower spacing verdict (R17): ✅ PASSES, by a factor of 15.** `Worlds/MP/OVT_Campaign_Eden_Layers/misc.layer:19-26`
places exactly **two** radio towers, at `4301.021 70.942 8550.3` and `6156.306 128.801 5252.812` —
**3783.6 m apart**, against the 250 m `HasExistingDeploymentOfType` radius. One deployment per tower is
guaranteed with enormous margin. (The autotest world, `OVT_Campaign_Test_Layers/default.layer:140-142`, has
exactly one tower, at `12.911 1 172.712` — which is what the two new Init cases read.)

## T4.8 — `RegisterGroup(` fixture sweep, Phase 4 additions — 2026-08-17

Movement's D12 discipline. A fixture is safe only if **(a)** it registers a null / empty / DEFEND-only plan,
or **(b)** it registers and unregisters inside **one frame**.

| Site | Case | Plan | Verdict |
|---|---|---|---|
| `Init` ×2 (in `RunCapture`) | **`Deployments_TowerCaptureOnlyOnRealWipe` (NEW, T4.8)** | `null` | **safe by (a) AND (b)** — `spawnDistanceOverride = 0` (Manual policy) so the autotest camera cannot materialise them; both handles are wiped or unregistered inside the same `Execute()` frame |

The other two new cases (`Deployments_TowerGarrisonHoldsItsPost`, `Deployments_RadioTowerLocationTypeIsOredIn`)
**register nothing at all** — one asks the shipped config template for a plan and an importance, the other is a
pure query on the live tower list — so they have no fixture footprint.

## T4.9 — behaviour changes to hand to Phase 8 — 2026-08-17

Player- and operator-facing consequences of turning tower garrisons into deployments. All deliberate.

1. **Garrisons materialise for ANY engine observer, not for players.** A GM free camera counts (`core/context.md`
   gotcha 0a), and from Phase 6 a parked recruit squad will too. Flying a GM camera near a tower spawns its
   garrison, and holding the camera there holds them spawned. Operator-facing wiki note.
2. **A QRF no longer clears the map's garrisons.** The old range test ANDed `!m_CurrentQRF`, so *any* QRF
   anywhere routed *every* tower into the despawn branch and deleted all tower garrisons for its duration.
   That branch is deleted (D12). Verified by *nothing happening* — DoD F8.
3. **Garrison CREATION now costs resources and can pause.** A tower garrison is bought out of the faction
   deployment pool like any other deployment (D17), and `EvaluateDeployments` returns early with **0 players
   connected** and **during any QRF** (D9). So: a resource-starved occupying faction can leave towers
   ungarrisoned; a tower can acquire a garrison minutes after you first pass it; and "no garrison appeared"
   is not necessarily a bug. Existing garrisons are unaffected by both guards. This is R15 and it is the
   subject of §6 play-test step 13.
4. **GM icons for tower garrisons read `DEPLOYMENT` / "Tower Garrison"**, not `RADIO_TOWER_GARRISON` /
   "RadioTower" — they are tagged by the shared infantry module like every other deployment group (§3.7).
   ⚠ **`OVT_EGroupOrigin.RADIO_TOWER_GARRISON` (`OVT_GMGroupRegistry.c:19`) has lost its only producer and is
   LEFT DECLARED.** Its integer is sent in the GM snapshot stream and `OVT_GMIconFormat.c:143` still maps it;
   reordering or removing the member would re-number every origin after it and mislabel every group on a
   client running a mismatched build. Do not touch the enum. If `OVT_TEST_Campaign_GMGroupRegistry` asserts on
   this member specifically, T7.6 re-points it at `DEPLOYMENT` rather than reviving the old origin.
5. **Clearing a tower is now the only way proximity can affect it** — walking away and coming back no longer
   resets the fight, and no longer takes the tower either.

---

## T5.9 — Phase 5 retired symbols, with grep verdicts — 2026-08-17

| Retired symbol | Where it lived | Verdict |
|---|---|---|
| **`OVT_EntitySpawningAPI` (the whole 460-line file)** | `Scripts/Game/GameMode/Deployments/OVT_EntitySpawningAPI.c` | **DELETED.** No `.meta` existed. Compile file count 6133 → 6132. See the T5.7 gate note below for the single surviving comment reference. |
| `CreatePatrolWaypoints` / `CreateWaypointsForVehicles` / `CreateTownWaypoint` / `CreateReturnWaypoint` / `GetVehicleAIGroup` | `OVT_MultiTownPatrolBehaviorDeploymentModule.c` | **gone tree-wide.** `GetVehicleAIGroup` existed only to find a vehicle's driver so waypoints could be bolted on his group; nothing needs it now. |
| `m_aWaypoints` / `m_bAssignedWaypoints` / `m_iCurrentWaypointIndex` | same | **gone tree-wide** — and T1.5's double-insert disappeared with the array, as planned. |
| `OnCrewGroupInitialized` + the whole-group init subscription | `OVT_VehicleSpawningDeploymentModule.c` | **gone.** Replaced by `OnCrewAgentAdded`, per member. ⚠ The tombstone comment is deliberately worded **without** the identifier, so `grep -rn "GetOnInit()" .../Deployments/` stays a real gate rather than one that always trips on its own epitaph — the same discipline T3.10 used for the deactivate path. |
| `m_mPendingCrewAssignments` | same | **renamed and re-meant** to `m_mCrewVehicles`: it is no longer "pending", it is the standing crew↔vehicle pairing, re-read every time a member materialises. |
| `CheckIfVehiclesEliminated()` (the Phase 3 shim) | same | **gone.** Its vehicle half became `PruneDestroyedVehicles()` + `CheckVehicleStatus()`; its group half (`group.GetAgentsCount() == 0`) is deleted outright and crew death arrives through `OnVirtualGroupWiped`. `IsVehicleOperational` is **kept** — a vehicle is not a group. |
| `GetGroupPrefabFromFaction()` (vehicle module's copy) | same | **gone.** Core resolves `(factionKey, groupName)`; a module that looked a raw prefab up by faction INDEX was the exact thing `api.md` §3 forbids. A block comment stands where it was. |
| the 40 m rule (`if (distance > 40) continue;` ×2) | same, `OnDeactivate` | **gone tree-wide, comments included.** |
| `OVT_InfantrySpawningDeploymentModule.OWNER_SYSTEM` / `SPAWN_DISTANCE_GLOBAL` / `GetSpawningIndex` / `TagForGameMaster` / `ResolveFactionKey` | infantry module | **hoisted, not deleted** — see the note below. |

**Acceptance greps, run after the edits:**

- `grep -rn "GetOnInit()" Scripts/Game/GameMode/Deployments/` → **empty**.
- `grep -rn "distance > 40\|m_aWaypoints" Scripts/Game/GameMode/Deployments/` → **empty**, comments included.
- `grep -rn "AddWaypoint\|RemoveWaypoint\|GetWaypoints\|GivePatrolWaypoints" Scripts/Game/GameMode/Deployments/` → **empty**. This clears Phase 3's recorded deviation in full: all 9 residual hits in 2 files are gone, and no code path in this directory now creates, removes or even *reads* a waypoint.
- `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/ | wc -l` → **6** (was 7).
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**. `git diff` on the three shipped frozen configs → **empty**.

### ⚠ Deviation: ONE `OVT_EntitySpawningAPI` reference survives, and it is inside the frozen core

`grep -rn "OVT_EntitySpawningAPI" Scripts/` returns exactly one line:
`Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c:707` — a **comment** in core's
waypoint-ownership header, citing the deleted helper as the example of the leak core avoids.

Two Phase 5 acceptance criteria collide on it: "that grep is empty" and "`git diff
Scripts/Game/GameMode/Virtualization/` is empty". **The freeze wins**, on the same reasoning Phase 4
used for the stale comment at `OVT_OccupyingFactionManager.c:443`:

- "Core stays frozen" is one of the four hard-floor quality bars (§2.1) and the empty core diff is an
  explicit criterion of this phase; a comment edit would show up in it and invite the question "what
  *else* changed in core?".
- **Phase 6 is the phase authorised to touch core** (the observer API, with the full additive ritual).
  It can re-word one comment at zero marginal risk, in a diff that is already being read closely.
- The grep's *intent* holds completely: **no live caller survives anywhere in the tree.** What is left
  is one historical citation, and the other five were rewritten.

The other five were rewritten to name what happens now, not the deleted symbol:
`OVT_TownVehicleSourceConfig.c:272`, `OVT_GMGroupRegistry.c:71`, `OVT_GMWaypointWalk.c:6`,
`OVT_RecruitManagerComponent.c:2489`, `OVT_TEST_InitSuite.c:4688`. **Hand the core one to Phase 6.**

### ⚠ Deviation: shared registration plumbing was HOISTED onto the base spawning module

Phase 5 needed the same owner-key composition, spawning-index lookup, GM tagging and faction-key
resolution the infantry module already had. Rather than a second copy (≈50 lines that would drift),
five members moved from `OVT_InfantrySpawningDeploymentModule` up to
`OVT_BaseSpawningDeploymentModule`: `OWNER_SYSTEM`, `SPAWN_DISTANCE_GLOBAL`, `GetSpawningIndex()`,
`TagForGameMaster()` and `ResolveFactionKey()`, plus a new `BuildOwnerKey(string moduleName)`.

- `OWNER_SYSTEM` **had** to move, not be duplicated: two spawning modules disagreeing about that string
  would each be unable to reclaim the other's groups, and only one of them would ever notice.
- `m_sModuleName` did **not** move — it is an `[Attribute]` on each concrete class and moving it would
  change the authored config surface, which D1 freezes. That is why `BuildOwnerKey` takes the name as a
  parameter and each subclass keeps a one-line `GetOwnerKey()`.
- Blast radius traced first: `OVT_BaseSpawningDeploymentModule` has exactly **two** subclasses
  (infantry, vehicle) and every other reference in the tree is a `Cast` or an `array<>` element type.
  Unqualified references to an inherited `static const` from a subclass compile clean (verified with a
  dedicated compile-check run before anything was built on it).

### ⚠ Deviation: the directory is STILL not a net deletion, and here are the real numbers

The Phase 5 criterion asks for a net deletion across `Scripts/Game/GameMode/Deployments/` for the
feature overall, on the strength of the 460-line file going. Measured as **total lines in the
directory** (the metric Phase 3's table used), against `HEAD`:

| Scope | Total lines | Code-only lines (blank + comment stripped) |
|---|---|---|
| **Phase 5 alone** | 571→1010 vehicle, 507→551 multi-town, 120→211 base, 660→614 infantry, 460→0 API = **+64** | **−162** |
| **Feature overall** (HEAD → now) | 5545 → 6754 = **+1209** | 3865 → 3960 = **+95** |

So the criterion is **not met on total lines** and is essentially flat on code. The gap is
documentation, exactly as it was in Phase 3: the vehicle module's own code went 402 → 560 (**+158**) for
machinery that is genuinely new — reclaim, crew↔vehicle pairing, per-member seating, the deletion veto
and the vehicle prune — while its file went 566 → 1010, i.e. **+286 lines of comment** on the file that
now carries the stolen-vehicle guarantee. The multi-town module is a real deletion on both measures
(code −72). Trimming the headers to satisfy the metric would strip the "why" notes off the two files
`base-defense-migration` will copy. **Reported rather than gamed.**

## T5.10 — `RegisterGroup(` fixture sweep, Phase 5 additions — 2026-08-17

Movement's D12 discipline. A fixture is safe only if **(a)** it registers a null / empty / DEFEND-only
plan, or **(b)** it registers and unregisters inside **one frame**.

| Site | Case | Plan | Verdict |
|---|---|---|---|
| — | **`Deployments_VehiclePatrolCrewsResolve` (NEW, T5.8)** | — | **no footprint at all.** The case registers NOTHING: it resolves both shipped configs out of the registry, asks each faction's group registry to resolve the authored crew name, and clones the module to compare one integer. Nothing to clean up, nothing for the movement tick to walk. |

Phase 5 adds **no** new `RegisterGroup(` call site under `Scripts/Game/Tests/`; the table from T4.8 and
T3.10 stands unchanged.

## T3.10 — Phase 3 retired symbols, with grep verdicts — 2026-08-17

Every symbol Phase 3 was asked to delete, and what a fresh grep says about it. Run over the whole of
`Scripts/`, not just the deployments directory, because several had callers nobody expected.

| Retired symbol | Where it lived | Verdict |
|---|---|---|
| `IsPlayerInRange()` | `OVT_DeploymentComponent.c:294` | **gone tree-wide.** Its only caller was the proximity block that went with it. |
| `DeactivateDeployment()` | `OVT_DeploymentComponent.c:204` | **gone tree-wide.** Zero callers outside the block it served. A block comment stands where it was, so nobody re-adds one. |
| `m_aProcessedGroups` | `OVT_PatrolBehaviorDeploymentModule.c` (9 references) | **gone tree-wide.** |
| `ApplyPatrolBehaviorToGroup` / `ApplyPatrolBehaviorToExistingGroups` / `CheckForNewGroups` / `IsGroupProcessed` / `ForceReapplyPatrolBehavior` | `OVT_PatrolBehaviorDeploymentModule.c` | **gone tree-wide.** `ForceReapplyPatrolBehavior` was public and had zero external callers. |
| `CheckIfUnitsEliminated()` | base virtual on `OVT_BaseSpawningDeploymentModule.c:114`, infantry override, **vehicle override** | **gone tree-wide.** The base virtual is deleted. ⚠ The vehicle module's override had to move with it, so it became the private `CheckIfVehiclesEliminated()` marked `// Phase 5 rewrites this` — behaviour byte-identical, one call site updated. |
| `IsGroupAlive()` / `GetAliveGroupCount()` / `CheckGroupStatus()` | `OVT_InfantrySpawningDeploymentModule.c` | **gone tree-wide.** Strength questions now go to `virt.GetAliveMemberCount(handle)`, which is mask-first. |
| `m_aSpawnedGroups` (infantry) / `m_iSpawnedCount` (infantry) / `SpawnInfantryGroups()` / `GetGroupPrefabFromFaction()` (infantry) | `OVT_InfantrySpawningDeploymentModule.c` | **gone from this module.** The vehicle module keeps its own identically-named copies; those are Phase 5's. |
| `ClearGroupWaypoints` / `CreateMoveWaypoint` / `CreateDefendWaypoint` / `CreatePatrolWaypoint` | `OVT_BaseBehaviorDeploymentModule.c` | **gone tree-wide.** All four had zero callers and all four made it easy to violate core D6; a block comment records why they must not come back. |
| `OVT_EntitySpawningAPI` (from the infantry module) | `:84`, `:176`, `:373` | **gone from the infantry module.** File retained for Phase 5 — the vehicle module is still its caller. |

**Acceptance greps, run after the edits:**

- `grep -rn "IsPlayerInRange\|DeactivateDeployment\|m_aProcessedGroups\|ApplyPatrolBehaviorToGroup\|CheckIfUnitsEliminated\|IsGroupAlive" Scripts/Game/GameMode/Deployments/` → **empty**, comments included. The tombstone comment left where the deactivate path was is deliberately worded without the identifier, so the grep stays a real gate rather than one that always trips on its own epitaph.
- `grep -rn "OVT_EntitySpawningAPI" .../OVT_InfantrySpawningDeploymentModule.c` → **empty**.
- `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/ | wc -l` → **8** (was 9).
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**. `git diff Configs/` → **empty**.
- `OVT_PatrolHarassmentStabilityModifier.c` → **not edited**; its three calls (`GetInstance`, `GetDeploymentNearPosition`, `GetSpawnedUnitsEliminated`) are all still there and all still mean what they meant.

### ⚠ Deviation: the waypoint-API grep is NOT empty, and cannot be until Phase 5 — ✅ CLEARED BY PHASE 5

**Resolved 2026-08-17.** All 9 residual hits are gone: 7 went with the multi-town module's waypoint
authoring (T5.1) and 2 with the file (T5.7). The grep is empty. The rest of this section is kept as the
record of why it could not be empty in between.

`grep -rn "AddWaypoint\|RemoveWaypoint\|GetWaypoints\|GivePatrolWaypoints" Scripts/Game/GameMode/Deployments/`
returned **9 hits in 2 files** after Phase 3, not zero. Phase 3's acceptance criterion asks for zero, but
that was only reachable by doing Phase 5's work, and doing it early would have broken vehicle patrols outright.

| Residual | Why it stays | Cleared by |
|---|---|---|
| `OVT_MultiTownPatrolBehaviorDeploymentModule.c:225,229,239,247,262,277,405` | The vehicle patrol's crews are **not registered groups yet** — they are still hand-spawned, so these waypoints are the only thing that makes a vehicle patrol drive anywhere. Deleting them now would leave both shipped vehicle configs inert between Phase 3 and Phase 5. | **T5.1** |
| `OVT_EntitySpawningAPI.c:387,390` (`CleanupGroup`'s detach loop) | Its last remaining caller is the vehicle module. Editing a file Phase 5 deletes, to move a grep, would be an unrequested behaviour change in vehicle teardown. | **T5.7** (file deleted outright) |

The criterion's *intent* holds in full: **no code path that touches a registered group creates or removes
a waypoint.** Every residual belongs to hand-spawned groups the core does not own.

### ⚠ Deviation: `git diff --stat` on the deployments directory is +68, not a net deletion

Phase 3's own delta across `Scripts/Game/GameMode/Deployments/`, per file (total lines):

| File | Before | After | Δ |
|---|---|---|---|
| `Modules/OVT_PatrolBehaviorDeploymentModule.c` | 259 | 224 | **−35** |
| `Modules/OVT_BaseBehaviorDeploymentModule.c` | 216 | 189 | **−27** |
| `OVT_DeploymentComponent.c` | 627 | 589 | **−38** |
| `OVT_DeploymentConfig.c` | 223 | 222 | −1 |
| `Modules/OVT_BaseSpawningDeploymentModule.c` | 120 | 120 | 0 |
| `Modules/OVT_VehicleSpawningDeploymentModule.c` | — | — | +5 (the Phase 5 shim note) |
| `OVT_DeploymentManager.c` | — | — | +7 (guard removed, doc comment added) |
| `Modules/OVT_InfantrySpawningDeploymentModule.c` | 503 | 660 | **+157** |
| **Total** | | | **+68** |

The infantry module is the whole of it, and it is **documentation, not machinery**: its non-blank,
non-comment line count went 349 → 375 (**+26**), against +131 lines of comment on what is now the
epic's central seam. The machinery deletion is real — one poll, one processed-group list, one
agent-count elimination path, one delete-everything teardown and one proximity toggle are gone. A
first pass at the module was trimmed by 69 lines before landing (the rebuy's duplicate convergence was
folded into `ConvergeGroups(bool fromNearestBase)`); trimming the remaining ~130 to satisfy the metric
would mean stripping the "why" notes off the one file every later feature copies. The directory-level
net deletion arrives in **Phase 5**, when `OVT_EntitySpawningAPI.c` (429 lines) and the multi-town
waypoint authoring go.

## T3.10 — `RegisterGroup(` fixture sweep — 2026-08-17

`grep -rn "RegisterGroup(" Scripts/Game/Tests/`, re-run after Phase 3's two new cases. Movement's D12
discipline: a fixture is safe only if **(a)** it registers a null / empty / DEFEND-only plan, or **(b)**
it registers and unregisters inside **one frame**. Anything else is walked by the movement tick.

| Site | Case | Plan | Verdict |
|---|---|---|---|
| `Init:3540`, `Init:3562` | `RegisterRefusesUnknownComposition` | — | **safe** — both registrations are *refused* (`-1`); no record is booked |
| `Init:3746` | `RegisterBuildsDormantGroup` | `null` | **safe** by (a) **and** (b) |
| `Init:3903`, `Init:3905` | `GetAllHandlesEnumeratesRegistry` | omitted → `null` | **safe** by (a) **and** (b) |
| `Init:4223` | `VirtualMovement_TickAdvancesDormantGroup` | PATROL, movable | **safe, and walked ON PURPOSE** — being walked is the case's subject; `spawnDistanceOverride = 0` |
| `Init:4418` | `VirtualMovement_StationaryPlanIsNeverAdvanced` | DEFEND | **safe** by (a); `spawnDistanceOverride = 0` |
| `Init:4621` | `VirtualMovement_ManagerResolvesAndDoesNotLeak` | DEFEND | **safe** by (a); `spawnDistanceOverride = 0` |
| `Init:4763` | `Virtualization_WaypointsAreOwnedAndDeleted` | PATROL + MOVE, cycling | **safe by (b) only** — genuinely movable, but unregistered in the same frame and asserts nothing about position |
| `Init:4908` | `DeathsFlipMaskAndWipeRecord` | omitted → `null` | **safe** by (a) + (b) |
| `Init:5096` | `MaskDrivesSlotSelection` | omitted → `null` | **safe** by (a) + (b); deliberately materialises a member, which the movement tick's `IsSpawned` gate skips |
| **`Init:7527`** | **`Deployments_EnsureGroupsIsIdempotent` (NEW, T3.9)** | `null` | **safe** by (a) **and** (b); `spawnDistanceOverride = 0` so the autotest camera cannot materialise it either |
| `Persistence:3913` | wiped group | `null` | **safe** by (a); asserts absence, never position |
| `Persistence:4012` | resurrection group | `null` | **safe** by (a) |
| `Persistence:4411` | BOGUS group | `null` | **safe** by (a) |
| `Persistence:4639` | the save/reload fixture | DEFEND ×2, cycling | **safe** by (a) — the types were changed to DEFEND by movement's T3.1 for exactly this reason; **do not revert them to PATROL** |

Phase 3's other new case (`Deployments_TownPatrolPlanCycles`) **registers nothing at all** — it asks the
shipped config's patrol module for a plan and asserts its shape — so it has no fixture footprint.

## T1.9 verdict — the observer spike (GATE for Phase 6) — 2026-08-17

**Server-side `InsertObserverSP(key, 0, 0, entity)` IS HONOURED on the authority. Phase 6 does NOT re-plan onto `InsertObserverMP`.** Measured by `OVT_TEST_Init_Virtualization_ServerObserverSPSpike` (All 237 green):

> `T1.9 VERDICT: server-side InsertObserverSP(key, 0, 0, entity) is HONOURED, application DEFERRED (invisible same-frame, visible within the settling budget) | settled after 1 frame(s), removal settled after 1 | has(probe): before=false sameFrame=false settled=true afterRemove=false | GetObserversSP(): before=1 sameFrame=1 settled=2 (two inserts, one key) afterRemove=1`

- `HasObserverWithinRangeSq` — the same query `ChimeraAIGroup.HasObserverInRange` uses — answers true at the observer's position; the effect survives across frames indefinitely once applied.
- **Application is deferred by exactly 1 frame, both insert and remove.** Any consumer that expects its own effect on the calling frame reads a false negative; a remove-then-check reads a false leak (the first version of the spike case did exactly that and went red on it). Phase 6's `HasEntityObserver` must answer from core's own `map<EntityID, int>`, never by querying the engine.
- The SP key is an identity ("insert or update" per the engine header): two inserts on one key settle at +1, never +2 — an idempotent `AddEntityObserver` is safe.
- The SP key space has zero vanilla script users; the spike used key 770019.

## T1.8 verdict — BUG-028 Init case dropped, accessor not added — 2026-08-17

The Init world cannot drive it: (1) `RegisterDeployment` only inserts when the faction's array already exists (`OVT_DeploymentManager.c:807-811` early-outs on null) and the arrays are created lazily only inside `EvaluateFactionDeployments`, which is unreachable in an Init world (kill switch / `m_bInitialized` / 0-players gates); (2) `ForceCreateDeployment` leaks a repeating `CallLater(UpdateDeployment, ~10 s, true)` into the shared test world once the marker is deleted. Accessor `GetFactionDeploymentIdCount` was NOT added (no consumer).

**⚠ Adjacent latent defect found (not fixed, no bug filed — in-dev feature rule):** any deployment created before its faction's first evaluation — notably every deployment restored by `ApplyPersistedDeployment` on load — is silently absent from `m_mFactionDeployments` forever (RegisterDeployment early-outs on the missing array), so `m_iMaxDeploymentsPerFaction` under-counts continued campaigns. Mirror image of BUG-028. **Phase 2's manager work should consider minting the faction array in `RegisterDeployment` instead of early-outing.**

## T1.2 sibling-sweep verdict — 2026-08-17

- `OVT_ReinforcementBehaviorDeploymentModule.c` — correct, not edited: all four time values ms-authored (`m_fCheckInterval` 60000, `m_fInitialDelay` 300000, `m_fReinforcementCooldown` 120000), comparisons against raw `GetWorldTime()` deltas. Cosmetic only: debug prints suffix ms values with "s".
- `OVT_DeploymentManager.c` `m_iEvaluationInterval` — correct (ms, consumed by `CallLater`).
- No other file under `Deployments/` calls `GetWorldTime()` or declares an Interval/Timeout/Cooldown/Delay field. The seconds-vs-ms defect existed in exactly the two places T1.1/T1.2 named.

## Gotchas / Discoveries

- **⚠ The shipped perimeter geometry is broken today, and Phase 2's factory does NOT reproduce the bug.** `GivePatrolWaypoints` (`OVT_OverthrowConfigComponent.c:594-595`) reads `dir.VectorToAngles()[1]` — index 1 is **pitch**, not yaw ([0] is yaw; verified against the engine's own `vector.c` doc examples) — and feeds it back in as a pitch (`Vector(0, angle, 0).AnglesToVector()`). With yaw pinned at 0 the four "90° apart" points come out as `(0,0,1)`, `(0,1,0)`, `(0,0,-1)`, `(0,-1,0)` from the centre: two land due north/south at `radius`, and **two land vertically above and below the centre**, which `FindNearestRoad` then flattens back onto the centre. `OVT_VirtualPlanFactory.BuildPerimeterPlan` uses `vector.Direction(...).ToYaw()` + `vector.FromYaw()` and produces the square the plan describes. Phase 5 deletes the old helper's call site, so no fix is owed there — but do not "restore parity" with it, and expect the town patrol's footprint to visibly change.
- **Observer application is deferred 1 frame** (insert AND remove) — see T1.9 verdict. Consumer rule for Phase 6 recorded there.
- `Print` with one very long concatenation → `Formula too complex` compile error; build long lines from short `string.Format` appends.
- Phase 1 added `OVT_DeploymentComponent.SetResourcesInvested(int)` (absolute setter beside `SetThreadLevel`-style API) — `ReinforceDeployment` only increments; T1.3's stamp needed an absolute write.
- `grep -rn "DestroyDeployment" Scripts/` now shows exactly two lines (component method + `DeleteDeployment`'s call) — the plan's "two manager call sites" counted the wrapper-internal one T1.7 deleted.

### Phase 3 additions

- **The convergence has TWO eliminated gates, and a rebuy has to clear both.** `ConvergeGroups()` refuses
  to register when `m_bSpawnedUnitsEliminated` **or** `m_ParentDeployment.GetSpawnedUnitsEliminated()` is
  set (§3.5). `CheckAllSpawningModulesEliminated()` computes the deployment flag as "**all** spawning
  modules are eliminated", so in a config with two spawning modules, clearing only the rebuying module's
  flag and then re-running the check leaves the deployment flag **still set** and the rebuy silently buys
  nothing. `Reinforce()` therefore clears both (`SetSpawnedUnitsEliminated(false)` on the parent too),
  converges, and lets one final `CheckAllSpawningModulesEliminated()` recompute the truth on both the
  success and the failure path. The shipped Town Patrol and the Phase 4 Tower Garrison each have one
  spawning module, so this is latent today — and it would have bitten the first config that did not.
- **`OnCleanup` reclaims before it unregisters.** A deployment deleted before its module ever converged
  (a restored one whose condition fails on its first evaluation) holds an empty handle list while the
  registry still holds its groups. Unregistering only the local list would orphan them for the rest of
  the campaign with no owner left to find them, so cleanup calls `ReclaimHandles()` first. The owner
  entity is still alive at that point — `DestroyDeployment()` deletes it *after* module `Cleanup()`.
- **A partially wiped patrol tops back up — but only at a load or a re-activation, never per tick.**
  `EnsureGroups()` is *converge to wanted*, so a deployment holding 3 of 4 groups registers a 4th on its
  next convergence. `UpdateDeployment` does **not** call it; the callers are activation (once),
  the records-restored fan-out (once per load) and the rebuy. This is a strict improvement on the old
  behaviour, where the force was rebuilt in full every single time the last player walked out of 1750 m
  and came back. Only a **fully** wiped force is held down, and that is F4's actual claim.
- **`BuildVirtualPlan()` is answerable off a config template with no deployment behind it.** The centre
  falls back to the group's own position when it resolves to `vector.Zero` — parity with the vanilla
  helper's `if (center[0] == 0) center = aigroup.GetOrigin()`. That is what lets the Init tier assert the
  shipped Town Patrol's plan shape without creating a marker entity and leaking a repeating 10 s
  `UpdateDeployment` into the shared test world (the T1.8 verdict).
- **The perimeter plan is road-snapped by the module, not by the factory** — the factory is world-free.
  `SnapPatrolPointsToRoads` walks the plan by waypoint TYPE rather than by index arithmetic (snap a
  `PATROL`, copy the previous position into a `WAIT`), so it survives a change to the factory's
  interleaving and can never make the three parallel arrays ragged.
- **`m_eImportance` is `SCR_EAISpawnImportance`, ComboBox, default `1` (NORMAL)** and IS copied in
  `CloneModule`. R5's standing trap: a forgotten copy ships the Phase 4 tower garrison at NORMAL and it
  quietly loses the AI budget race on a busy server.
- **Faction index → key happens in exactly one place**, `OVT_InfantrySpawningDeploymentModule.ResolveFactionKey()`:
  `GetGame().GetFactionManager().GetFactionByIndex(i).GetFactionKey()`, null-guarded at both steps. It
  deliberately does **not** go through `OVT_OverthrowFactionManager.GetOverthrowFactionByIndex()`, which
  dereferences `GetFactionByIndex(index)` unguarded and would VME on a stale index. The core resolves
  `(factionKey, groupName)` itself and warns if either fails.
- **Two attributes are now inert but still declared**, with `desc` strings that say so:
  `OVT_DeploymentConfig.m_bEnableProximityActivation` / `m_fActivationRange` (D1 freezes the shipped
  config surface) and `OVT_PatrolBehaviorDeploymentModule.m_fCheckInterval` / `m_bApplyToNewGroups`
  (there is no poll left to interval). T1.1's seconds-vs-milliseconds fix went with the poll it fixed.

### Phase 4 additions

- **🔴 A tower garrison registers on the NEAREST ROAD, and the snap searches 500 m.** The shared
  `GetRandomSpawnPosition` rolls a point 10–`m_fSpawnRadius` m from the anchor and then calls
  `OVT_WorldUtils.FindNearestRoad`, which ends in
  `roadManager.GetReachableWaypointInRoad(center, center, 500, roadPos)` — a **500 m** search radius. So
  `m_fSpawnRadius 15` bounds the *roll*, not the *result*: a garrison can end up materially further from its
  tower than the old code's fixed `tower.location + "5 0 0"`. This is pre-existing shared behaviour that
  Phase 3 deliberately kept unchanged (T3.2: "the ring + road snap stays"), and it cannot be turned off from
  a config, so Phase 4 did not touch it. **It is the first thing to look at in the §6 play-test**: if
  garrisons appear on the access road rather than at the tower, the fix is a module-level opt-out in a later
  phase, not a config value. DEFEND plans mean they then hold wherever they registered — the garrison will
  not walk to the tower.
- **🔴 A fully wiped tower garrison is COLLECTED, never rebought — DoD F15's second sentence is wrong.**
  `OVT_ReinforcementBehaviorDeploymentModule.ShouldReinforceModule()` returns
  `spawningModule.AreSpawnedUnitsEliminated()`, so the rebuy only ever fires for a **fully** eliminated
  module — a partially cleared force is never topped up by reinforcement, on *any* config, today or before.
  And on the Tower Garrison the capture module runs earlier in the same update pass, so by the time
  reinforcement evaluates its conditions the tower has already flipped, the control condition fails, and
  `m_bDeleteOnConditionFail` deletes the deployment instead of rebuying. That is exactly the loop §3.3
  describes and is correct — but **F15's "The same mechanism rebuys a partially-cleared tower garrison" is
  not true and never was.** Phase 8 should correct the claim rather than a play-tester filing it as a bug.
  (A partially cleared garrison *is* topped back up, but by `EnsureGroups()` converging at the next load or
  re-activation — Phase 3's note above — not by the reinforcement module.)
- **`m_bEnableReinforcement` is what keeps a lost tower's deployment collectable.** The
  `m_bDeleteOnConditionFail` branch lives *inside* `CheckReinforcement()`, and `OnUpdate` returns early when
  reinforcement is disabled. So a config that wants condition-driven teardown must enable reinforcement even
  if it never expects to rebuy anything. Non-obvious, and worth stating for `base-defense-migration`.
- **Behaviour-module order in a `.conf` is update order, and `.conf` files cannot carry comments.** The
  capture module must be authored **before** the reinforcement module (see T4.5). The only place that
  constraint can live is a class header — it is in
  `OVT_RadioTowerCaptureBehaviorDeploymentModule`'s. Any config that copies this precedent inherits the
  same requirement.
- **The capture decision is exposed as `EvaluateCapture(eliminated, pos, factionIndex)` rather than hidden in
  `OnUpdate`.** Same inputs, passed in. Without it, asserting "fires only on a real wipe, and exactly once"
  would need a live deployment marker entity, which leaks a repeating 10 s `UpdateDeployment` into the shared
  test world (the T1.8 verdict). `OnUpdate` is a three-line adapter over it.
- **`GetLocationTypeAtPosition` was split, not rewritten.** The precedence chain moved verbatim into
  `GetPrimaryLocationTypeAtPosition` and the public entry point is now `primary | (RADIO_TOWER when near a
  tower)`. Both are reachable **on purpose**: the Init case asserts `(full & primary) == primary`, which is
  the live proof that the bit was OR-ed in rather than replacing something — D19's actual claim. The shared
  300 m is now `RADIO_TOWER_RADIUS`, read by both halves, so they cannot drift.
- **A stale comment was left standing DELIBERATELY.** `OVT_OccupyingFactionManager.c:443` still reads
  "*or CheckRadioTowers would skip it forever and it would never garrison*". The **reason** is now wrong
  (CheckRadioTowers no longer garrisons anything) but the **behaviour it justifies is still required** — an
  un-restored tower must be handed to the occupying faction or the Tower Garrison config's control condition
  will refuse it forever. It sits inside `ApplyPersistedOccupyingFaction`, on which Phase 4's acceptance
  criterion demands an **empty `git diff`**, so it was not edited even for a comment. Hand the re-wording to
  Phase 8 or feature 5.
- **Pre-existing latent hazard, now with a second caller: `RandInt(min, max)` with `min == max`.**
  `CalculateGroupCount` rolls `RandInt(ceil(patrolGroupsMin*0.5), ceil(patrolGroupsMax*0.5))`, which on
  **Easy** (1, 2) is `RandInt(1, 1)` — an empty range, which logs
  `SCRIPT (E): RandomGenerator.RandInt: invalid parameters`. This already fires for the shipped Town Patrol
  on Easy; the Tower Garrison config simply adds a second caller. Not fixed here (the memory note
  `randint-max-exclusive` records the whole latent family, and `RandIntInclusive` is the correct fix when a
  bug is filed). On Normal — the shipped default — the roll is `RandInt(1, 2)` and is fine.
- **Every tower position is a candidate for the occupying faction.** `GetRadioTowerPositions` filters through
  `IsPositionRelevantToFaction`, which returns **true unconditionally** for the occupying faction
  (`OVT_DeploymentManager.c:702-705`), so no remote tower is silently filtered out of the candidate list.
  Checked because a hidden relevance gate would have been an R17-class silent failure.

### Phase 5 additions

- **🔴 `SCR_AIGroup.GetOnAgentAdded()` PASSES ONE ARGUMENT, NOT TWO — the vanilla doc comment is wrong.**
  `SCR_AIGroup.c:2264-2270` documents "*Invoker params are: AIGroup group, AIAgent agent*"; the actual
  invocation at `:2458-2461` is `Event_OnAgentAdded.Invoke(child)` — the **agent alone**. A handler
  written to the doc comment compiles clean and misbehaves at the wire, exactly like this project's
  recorded `Rpc()` arity blind spot. The group is recovered with `agent.GetParentGroup()`. Every
  existing Overthrow subscriber already does it the right way
  (`OVT_TownCivilianSourceConfig.OnCivilianAgentAdded(AIAgent)`); copy one of those, not the header.
- **The module order inverts for vehicles too, and that forced lazy route planning.**
  `ActivateDeployment()` activates **spawning** modules before **behaviour** modules, and the spawning
  module asks for a plan *while it registers*. So `OVT_MultiTownPatrolBehaviorDeploymentModule.OnActivate`
  — which is where `PlanPatrolRoute()` used to live — has not run yet when `BuildVirtualPlan()` is first
  called. The route is now planned on first ask (`EnsureRoutePlanned`), and `OnActivate` merely arms the
  completion check. **Any behaviour module that computes state in `OnActivate` and needs it in
  `BuildVirtualPlan` has this bug**; the town-patrol module escaped it only because its plan is a pure
  function of attributes.
- **Route completion needed a DEPARTURE LATCH, and the latch has to scale with the route.** Measuring
  "everyone is within 100 m of the final plan point" is true on the very first tick, because the crew is
  registered beside its truck at the base it is about to leave — the deployment would delete itself
  before moving. A fixed 300 m latch fixes that but introduces the opposite hole: a route whose only
  eligible town is just outside the 100 m exclusion never takes a crew 300 m from home, so the latch
  never trips and the deployment never finishes, never refunds and never deletes itself. The threshold
  is therefore **half the route's own reach, floored at 150 m and capped at 300 m**
  (`GetDepartureDistance()`). The shipped configs search 2500 m and always sit at the cap.
- **Route legs are `MOVE` points now, where the hand-authored version used `SpawnPatrolWaypoint`.** That
  is what implementation.md §3.8 asks for and it is the better fit for a vehicle (drive there, rather
  than patrol around there), but it IS a behaviour change worth watching in the play-test.
- **⚠ §3.2 of the plan is wrong about the vehicle modules' names.** It states "Both shipped vehicle
  modules leave `m_sModuleName` empty… the fallback is not theoretical". They do **not**: both
  `Deployment_VehiclePatrol_Light.conf:5` and `_Heavy.conf:5` author
  `m_sModuleName "Spawn patrol vehicle"`, so both crews key on the sanitised name
  `Spawnpatrolvehicle`, not on `m0`. The `"m" + index` fallback remains untested by any shipped config.
- **A reloaded vehicle patrol gets a NEW truck and re-drives its route.** Vehicles are not persisted;
  crews are. On the records-restored fan-out the module reclaims its crews (remembering exactly who
  died), spawns a fresh truck, moves the still-dormant crew to it and seats them. The core rebuilt their
  waypoints from the persisted plan at index 0, so the tour starts again. This is a deliberate,
  documented simplification — the alternative (persisting the truck and the crew's progress along the
  route) is not something this feature owns.
- **Crew↔vehicle pairing is keyed on the crew's group ENTITY id, and is rebuilt every session.** Under
  Route B a restored group's entity id is a new one, so the pairing map cannot survive a load and is
  swept (`PruneCrewVehicleMap`) on every reclaim. Vehicle identity is compared by resolving the id back
  to an **entity pointer** rather than by comparing `EntityID`s, which needs no assumption about how
  `EntityID` equality behaves.
- **`ScriptInvoker.Insert` does not de-duplicate.** The seating subscription is `Remove` then `Insert`
  on every pairing, because convergence runs more than once per session and a doubled subscription would
  try to seat each arriving crewman twice.
- **The vehicle module's `EnsureGroups()` is NOT called per tick** — only from activation and from the
  records-restored fan-out, same as the infantry module. So a destroyed truck is not silently replaced
  mid-session; that remains the reinforcement module's job on configs that carry one (neither shipped
  vehicle config does).

### Phase 6 additions

- **🔴 A FRESHLY SPAWNED ENTITY CAN HAVE NO `EntityID`, AND EVERY SUCH ENTITY SHARES THE SAME ONE.**
  Found by the Phase 6 suite run, not by planning: the Init round-trip case spawned two throwaway
  markers with `GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params)` in one frame, added
  an observer for the *first*, and `HasEntityObserver` then answered **true for the second**. The only
  way that map lookup can hit is if both entities returned the same `EntityID` — i.e.
  `EntityID.INVALID`, which is what an entity that is not world-registered answers with. The engine's
  own header for `IEntity.GetID()` carries a worked example that prints `0` for a script-created
  object, and `EntityID` is a `handle64` whose sanctioned validity test is
  `id != EntityID.INVALID` (the idiom vanilla's `SCR_DestructibleIdentificator` uses,
  `SCR_MPDestructionManager.c:11,43`).
  **Consequences, all of them acted on:**
  1. **Core now refuses an unkeyable entity** in `AddEntityObserver` (WARNING + false) and answers a
     quiet false in `Has`/`Remove`. Storing one is a wrong-answer bug *and* a leak at the same time:
     two entities share one entry, the second add hijacks the first's key, and removing either removes
     the other's observer. Silent, in every direction.
  2. **Any consumer must add its observer a frame after the entity is spawned**, never from inside the
     entity's own initialisation — an `OnPostInit` on a component of the entity being spawned runs one
     call deep inside the spawn that created it. The recruit wiring does one call-queue hop
     (`CallLater(InstallObserver, 0, false)`) and cancels it in `OnDelete`.
  3. **Two candidate causes, both fixed, because the suite cannot be re-run cheaply to separate them:**
     (a) a bare *class* spawn may never be world-registered, so the markers are now spawned from a real
     prefab (the config's wait-waypoint, which `OVT_Global.SpawnEntityPrefab` also untracks for
     persistence); and (b) the markers stood **3 km** off the fixture position, plausibly outside the
     small autotest world's bounds, so the offset is now **150 m**. Prefab-spawned entities demonstrably
     *do* get valid distinct ids in this world — core's own `m_mHandleByGroup` has keyed group entities
     on `group.GetID()` read immediately after the prefab spawn since Phase 3, and the whole epic
     depends on it — which is what makes (a) the more likely cause.
  4. **The case now asserts the precondition** (both ids valid and distinct, waited for up to 30 frames)
     and fails naming the discovery if it is ever not met, plus a new claim that
     `FindEntityByID(entity.GetID())` resolves back to the entity — which is the load-bearing assumption
     of core's stale-entity sweep. If that were false the sweep would delete every observer within two
     seconds of parking it and the feature would silently do nothing.
- **The observer sweep had to go ABOVE the ambient tick's own early-out.** `AmbientTick` returns
  immediately when there are no ambient sources (`!m_mAmbientSources || m_aAmbientOrder.IsEmpty()`).
  The two halves are unrelated, so a manager holding observers in a world with no ambient source
  registered — which is every world until `civilians` authors one — would never have swept a single
  stale entry. The sweep call sits directly under the tick's self-cancel block and above that return.
- **The sweep only runs if the ambient tick is running, and that tick is installed in `PostGameStart`.**
  Init-tier worlds never run `PostGameStart` (this project's standing note), so **the sweep does not
  run in the test world at all** — which is why the Phase 6 Init cases assert only on the map, never on
  the sweep, and why `RemoveEntityObserver` (not the sweep) is the contract consumers are told to obey.
- **`HasEntityObserver` answers from core's map on purpose, and a "better" implementation would be a
  regression.** Anyone who later "fixes" it to ask `ObserversSystem` re-introduces the exact false
  negative/false leak the T1.9 gate case was rewritten to survive. There is now an Init case that goes
  red if they do.
- **The observer is parked from `OnPostInit`, which also runs on clients.** The parked-recruit group
  prefab is replicated, so the marker component initialises on every machine; the wiring is guarded by
  `Replication.IsServer()` *and* fails closed inside core (the observer map is allocated behind the
  same server guard as the registry). Two guards, deliberately, because the failure mode of the wrong
  answer here is a client parking an observer nothing will ever remove.
- **`OnDelete` removes the observer unconditionally — it does not ask whether it added one.** A remove
  for an entity core never parked an observer for is a silent `false`, so the "did I?" bookkeeping
  would only re-derive an answer the manager already holds — and doing it unconditionally also cleans
  up an observer that arrived by some route the component did not write.
- **The observer key is minted before the engine call and booked after it**, so a refusal can never
  leave the map claiming an observer that was not parked. Re-adding an entity that already has one
  re-inserts on the *same* key rather than skipping: the engine documents the key as "insert or
  update", the gate case measured two inserts on one key settling at +1, and re-inserting is the only
  thing that would repair an `EntityID` the engine recycled for a different entity (the one stale case
  the sweep cannot see, because `FindEntityByID` succeeds on the *new* entity).
- **⚠ The one-frame deferral is a hazard at teardown, not just a query wrinkle.** An entity destroyed in
  the *same frame* its observer is removed leaves the engine holding a removed-but-not-yet-applied entry
  for a frame. Nothing in the shipped path does that (vanilla's delete-when-empty defers the group
  deletion by a frame of its own), but the Init round-trip case deliberately waits 5 frames between
  removing the observer and deleting the marker it followed — the same discipline the T1.9 gate case
  used. **Do not "simplify" that away in a later case.**

### Phase 7 additions

- **🔴 THE SHARED PERSISTENCE GATE CANNOT RELOAD A DEPLOYMENT, AND NOTHING IN THE PLAN SAID SO.**
  `OVT_PersistenceManagerComponent.ReapplyLatestSaveData()` builds `PersistenceLoadRequest` with
  `Instances = {owner}` — the **game mode entity, and only that** — and its own header spells the
  limit out: *"WHAT IT DOES NOT COVER. Anything outside the game mode entity's record — world
  entities, characters, vehicles, placeables."* A deployment marker is a world entity with its own
  `EntityPersistenceConfig` (`Overthrow.conf:189-203`, `SelfSpawn 1`, priority 35000), so its
  component serializer's `Deserialize` is **never** re-run by the suite's reload seam. Everything the
  existing virtual-group cases assert works because the virtualization manager is a *component of the
  game mode*; deployments are not.
  **Consequences, all acted on in Phase 7:**
  1. T7.2/T7.3/T7.4 assert the **restore half** — `ApplyPersistedDeployment()`, the method the
     marker's own `Deserialize` calls and the one its header calls "where every side effect of
     restoring a deployment lives" — with a **real save** taken alongside so the write half runs for
     real over live state. The split is stated at the top of `OVT_TEST_DeploymentRoundTripFixture`
     rather than buried.
  2. "The bytes on disk read back as the values that went in" is **not** asserted anywhere automated
     for deployments, and cannot be in this harness. It is covered by inspection (T7.7) and belongs
     with the continue-flow claims the suite header already parks as manual.
  3. **Do not "fix" this by widening the reload seam.** Adding instances to that request means naming
     persistence-framework types inside `Scripts/Game/Tests/`, which the suite's assertion rule
     (Decision 4) forbids outright, or changing the persistence manager's public API for a test's
     benefit. Both are worse than the honest split.
- **A deployment fixture is only safe if it is ELIMINATED, not if it is short.** `CreateDeployment` /
  `InitializeDeployment` arm a repeating 8–12 s `UpdateDeployment`, whose first tick calls
  `ActivateDeployment()` → `EnsureGroups()` → `RegisterGroup(... SPAWN_DISTANCE_GLOBAL ...)`. In the
  Persistence world that is up to four real groups at 1750 m with the autotest camera inside the ring,
  carrying a **cycling perimeter plan** the movement tick will walk. Marking the deployment *and every
  spawning module* eliminated is the only construction that survives a stalled host. Corollary, and
  the reason T7.3's dirty step clears **only the module flags**: clearing the deployment-level flag on
  a live marker re-opens that window for as long as the dirty→assert gap lasts.
- **`Town Patrol` is the only shipped config a test may safely stand up.** The registry override
  authors `m_bDeleteOnConditionFail 0` for it, so its reinforcement module cannot delete the fixture's
  own marker mid-case. `Tower Garrison` authors `1` (with a 60 s initial delay) and would eventually
  collect itself out from under a case.
- **A fixture that must NOT be stored is spawned bare; one that must be stored goes through
  `CreateDeployment`.** `OVT_PersistenceTracking.Track()` is what puts a marker in a save point, and
  it is called *inside* `CreateDeployment` — which is the only way a test can get a tracked marker
  without naming a persistence type. A marker spawned straight off `manager.m_DeploymentPrefab` is
  untracked, which is a feature: it cannot leave a stray deployment record in the shared CI save, and
  it is the **only** way to reach the fresh-restore branch (`m_DeploymentConfig` doubles as the
  "already built" flag, so a `CreateDeployment`-made marker always takes the idempotent branch and
  `InitializeDeployment` — the method that reads the wipe-out flag — never runs again).
- **The shipped marker prefab authors no `OVT_DeploymentConfig`**, which is what makes the fresh
  branch reachable at all. The fixture asserts it rather than trusting it: if someone ever authors one
  onto `Prefabs/GameMode/OVT_Deployment.et`, three Phase 7 cases say so by name instead of quietly
  testing the wrong branch.
- **`owned` is a reserved keyword and it cost a compile round here too.** `int owned = …;` fails with
  `Broken expression (missing ';'?)` naming the line and nothing else. The project memory already
  records it; it is repeated in a comment at the site because the error text points at punctuation.
- **The autotest camera / test-world extent bound where a marker fixture may stand.** Phase 6 found
  that a script-spawned entity outside the small test world's registered extent answers
  `EntityID.INVALID` — and *every* such entity answers the same one. The deployment manager keys
  `m_aActiveDeployments` on exactly that id, so an out-of-bounds fixture would share a slot with the
  next one. All three marker offsets are ≤ 75 m from the town anchor and the fixture asserts the id is
  valid before going on.
- **Scope every registry assertion to your own owner key, never to `GetGroupCount()`.** The
  Persistence tier runs a **started campaign** and, since Phase 3 removed the evaluator's guard, the
  live deployment wave registers its own groups in that world while cases run. A global count is not a
  stable baseline there; `FindGroupsByOwner(ownerSystem, key)` is.

---

## Session Notes

### 2026-08-17 (Phase 8 — help & documentation sync, COMPLETE; docs-only, no suite run)

**One-line:** every player-facing claim about patrols, garrisons and radio towers is now either backed
by a `file:line` in its own fact-check comment or rewritten; the four player-visible behaviour changes
have a home in the Field Manual; the epic and master rows are current; the kill-switch ledger balances
at 5. **The wiki half did not happen and is owed** — see "Owed" below.

#### Surface 1 — Tutorial popups (`Configs/Tutorials/`) — NO CHANGE, and that is the finding

All 15 entries' bodies were read out of the `.st` (the `.conf` files carry only `#OVT-Tutorial_*`
keys). **Not one tutorial popup makes a claim about a patrol, a garrison or a radio tower.** The only
adjacent entry is `OVT-Tutorial_BasesFirstCapture_Body`, and it is about the **base QRF** — a system
this feature deliberately does **not** migrate (its spawner is one of the two production kill-switch
guards still standing). Its claims re-verified TRUE:

| Claim | Verdict | Citation |
|---|---|---|
| "capture bases … reach the flag at the center and trigger a battle" | ✅ verified | `OVT_QRFControllerComponent.CheckUpdatePoints` (points move toward whichever side holds the middle) |
| "decided by how many players (and recruits) hold the middle … against how many occupying soldiers are still there" | ✅ verified | same |
| "the occupying faction keeps reserves and spends a surplus of them on retaking a base" | ✅ verified, **citation drifted** | `OVT_OccupyingFactionManager.c:1217-1226` (`m_iResources > 2000 && m_bCounterAttackTimeout == 0 && rand > 0.9` → `StartBaseQRF` at `:1224`); the comment said `:1178-1194` |

Only the **Comment** was edited (re-cite + two notes): the trigger described in its DELIVERY
CONSTRAINT block was stale (it says `BASE_CONTROL_CHANGE`; `basesFirstCapture.conf` binds
`PLAYER_ENTER_BASE`, `OVT_TutorialTrigger.c:44`), and a standing note now says explicitly that nothing
in this string is affected by the virtualization migration, so a future editor does not bolt
patrol-persistence copy onto a base-QRF tip.

**No new tutorial popup was added, and could not have been.** `OVT_TutorialTrigger.c:13-44` is the
whole event catalog and it has no tower, patrol, garrison or "enemy group wiped" event —
`PLAYER_ENTER_BASE` is the closest and it is bases-only. **Gap handed to the tutorial-system feature:**
a `PLAYER_ENTER_RADIO_TOWER` (or a generic location-proximity) trigger is what a tower tip would need.
Not hacked in here; this phase edits content, not framework.

#### Surface 2 — Field Manual (`Configs/FieldManual/Categories/FM_Overthrow.conf`)

**(a) One sentence CORRECTED.** `OVT-FieldManual_BaseCapture_Text5` read *"Towns and radio towers are
fought over in much the same way"*. False for towers after Phase 4: a tower has no QRF, no
reinforcement wave and no ticking control counter; it flips the instant its garrison's last man dies
(`OVT_RadioTowerCaptureBehaviorDeploymentModule.c:89`, `:122-124`). Rewritten to split towns from
towers and to name the two real tower paths (cleared garrison; specops retake,
`OVT_BaseUpgradeSpecops.c:38-65`). Its Comment records the correction and why.

**(b) One new entry, `"Patrols and Garrisons"`** — The Resistance category, after "Capturing Bases".
Entry GUID `{6B5A11C000000001}`, pieces `…002`–`…009` (a fresh, tree-unique `6B5A11C0` series; verified
by grep before use). 8 pieces: intro text, then three headed sections. It carries **all four** T8.2
points, in reference voice, no imperatives, no em-dashes, no rich-text markup:

| Piece | Point | Fact-check citations (recorded in each key's `Comment`) |
|---|---|---|
| `_Text` (intro) | what standing forces exist and which cost resources | `Deployment_TownPatrol.conf` (`m_iBaseCost 0`, `m_iCostPerGroup 0`), `Deployment_VehiclePatrol_Light.conf:21` (25), `_Heavy.conf:20` (50), `Deployment_TowerGarrison.conf` (20 / 10), gate at `OVT_DeploymentManager.c:386-391` + `:750-752` |
| `_Text2` ("Losses Stay Lost") | **① dead members stay dead**, across despawn AND save/load | `OVT_VirtualizationManagerComponent.c:1813-1828` `GetMemberAlive`, `:1886-1900` `ReportMemberKilled`, `:731` `SetOVTSlotMask`, `:1029-1033` (mask → save payload), `:1180` `ApplyPersistedMask`; replacement path `OVT_ReinforcementBehaviorDeploymentModule.c:123-128` + `OVT_InfantrySpawningDeploymentModule.Reinforce:479-495` |
| `_Text3` ("Patrols Keep Moving") | **② patrols are not where you left them** | `OVT_VirtualMovementManagerComponent.c:355` (advanced only while NOT spawned), `:658` (`SetPosition` is the only mutation); `OVT_PatrolBehaviorDeploymentModule.c:16-18`, `:28`, `:78-85` (PERIMETER walks, DEFEND never does); `Deployment_TownPatrol.conf` authors no `m_ePatrolType` → attribute default PERIMETER; `Deployment_TowerGarrison.conf` authors DEFEND |
| `_Text4` ("Radio Towers") | **③ towers are taken by really clearing the garrison; proximity never flips one** | `OVT_RadioTowerCaptureBehaviorDeploymentModule.c:89` (no eliminated flag → no capture), `:91` (fired-once latch), `:103-114` (range + still-ours guards), `:122-124` (`ChangeRadioTowerControl`); the flag itself only ever comes from the core's wipe bookkeeping, `OVT_VirtualizationManagerComponent.c:1886-1900` |
| `_Text5` | **④ towers may be found ungarrisoned; a garrison can appear later; creation pauses** | `OVT_DeploymentManager.c:386-391`, `:750-752` (affordability), `:309` (`GetPlayerCount() == 0`), `:313` (`m_CurrentQRF`) — D9's two kept early returns |

Deliberate wording choices, so a later editor does not "simplify" them into lies:

- `_Text2`'s last sentence says **"a patrol"** wiped to the last man can be replaced, never "a
  garrison". A fully wiped **tower** garrison is *not* rebought: the capture module runs earlier in the
  same update pass, the tower has already flipped, and the control condition then fails so
  `m_bDeleteOnConditionFail` collects the deployment. This is the same correction the Phase 4 note
  makes to **DoD F15's second sentence**, now reflected in shipped player text.
- `_Text5` says **"a battle"**, not "a QRF": that is the word this manual already uses
  (`BaseCapture_Text2`), and `m_CurrentQRF` covers base and town QRFs alike.
- No resource **numbers** are quoted anywhere. T4.5 records the authored costs as a starting point
  expected to move in play-test tuning; a quoted number would go stale on the first tuning pass.
- The word "despawn" never appears. Players do not have that word; "stops being watched" does the job.

**(c) Tile art is a placeholder.** The new entry points at
`{CF6B203430123E78}…/Tiles/default_ui.edds`. Every other entry has a bespoke tile. **Owed: one
`patrols-and-garrisons_ui` tile** (and the GUID swapped into `FM_Overthrow.conf`).

#### Surface 3 — the wiki — NOT DONE, OWED

The `wikijs_*` MCP tools were **not exposed to this session at all** (no connection-status, get-page,
search or update tool was available), so no page could be fetched, let alone written. Note the
crash-recovery instruction could therefore **not** be satisfied either: **the wiki may or may not
carry partial edits from the crashed session, and this session could not look.** Whoever picks this up
must `get_page` each target and read it before editing (search returns wrong pageIds; `update_page`
requires `tags` and can write while reporting failure — re-fetch to confirm).

**Six points to publish**, four player-facing (identical in substance to the Field Manual entry above,
long-form) and two operator-facing:

1. Dead members stay dead: a shot-up patrol or tower garrison returns at the strength it was left at,
   with the same survivors, across both despawn and save/load.
2. Patrols keep walking while unobserved; a town patrol will not be where it was last seen. Tower
   garrisons hold their post (DEFEND plan).
3. A radio tower changes hands only when its garrison is wiped. Walking away and returning no longer
   resets the fight, and proximity alone never flips a tower.
4. Towers may be found ungarrisoned when the occupying faction is short of resources; a garrison can
   appear later. Creation costs resources and pauses entirely while a QRF is running or no players are
   connected (existing forces are unaffected).
5. **Operator:** a GM free camera counts as an observer and keeps registered content near it
   materialised — as does a **parked recruit squad**, which holds a whole town's patrols *and* its
   tower garrison spawned with no player anywhere near. Off-switch:
   `m_bRecruitGroupsAreObservers` on the virtualization manager, **default ON**.
6. **Operator:** tower garrisons no longer vanish map-wide during a QRF. The old range test ANDed
   `!m_CurrentQRF`, so any QRF anywhere deleted every tower garrison for its duration; that branch is
   gone (D12).

Player pages must stay in player language (no class names); item 5's attribute name is the one
exception and belongs in an operator/config page, not a player page.

#### Surface 4 — epic + master bookkeeping (T8.4)

- `docs/features/virtualization/epic-overview.md`: status 3/5 → **4/5**; the `integration` row rewritten
  (62/62, Phases 1–8 complete pending final review, All **255** green 2026-08-17, what shipped, what is
  owed); the `base-defense-migration` row now states that **the deployments↔virtualization seam is
  proven** and that **`Deployment_TowerGarrison.conf` is its worked precedent**, with the two traps that
  precedent recorded (behaviour-module order in a `.conf` **is** update order; `m_bEnableReinforcement`
  is what keeps a lost deployment collectable) and a pointer that the two remaining production
  kill-switch guards are that feature's to remove. Rollup + one-line summary refreshed.
- `docs/overview.md`: the epic's **single** row updated (173/173 = 50 + 39 + 22 + 62). No child feature
  was listed separately.
- `docs/features/virtualization/core/api.md` §6 (`m_iMilitarySpawnDistance` prose) — **rewritten, and
  it deviates from the task wording on purpose.** The task said to name "base upgrades, QRF" as the
  remaining readers. A tree-wide grep says otherwise: the **only** production read is
  `OVT_BasePatrolUpgrade.c:98` (inside `PlayerInRange()` at `:96-99`, called by the defence-position,
  sniper-position and tower-guard upgrades). **The QRF reads it nowhere** — `OVT_QRFControllerComponent`
  has no reference to it and spawns off its own trigger and its own ranges. The other two hits are a
  comment in `OVT_BaseSpawningDeploymentModule.c:14` and the declaration itself
  (`OVT_OverthrowConfigComponent.c:213`, with its own comment at `:56`), plus one test at
  `OVT_TEST_Campaign_GMGroupRegistry.c:374`. The prose now says exactly that. **No script file was
  touched**: `git diff Scripts/Game/GameMode/Virtualization/` is unchanged by this phase.
- **The T7.7 finding-5 correction was propagated** (the plan's wrong sentence, not just this file's
  note): `api.md` §8's Route B callout and `integration/implementation.md` T7.7 both said *"no vanilla
  `AIGroup`/`AIUnit`/`AIWaypoint` record for anything core owns"*. Both now read **"no core-owned AI
  record is ever self-spawned back"** — the records exist, written with `SelfSpawn 0`, and nothing
  rebuilds an entity from one. Both carry a dated note pointing at T7.7 finding 5 so the change reads
  as a correction rather than a drift. (`grep -rn AIWaypoint docs/` turned up no other statement of the
  wrong claim; the remaining hits are BUG-116/118, RFG-007, the persistence API reference and
  `gm/waypoint-viz`, all of which say records exist and are all correct.)

#### Surface 5 — kill-switch ledger (T8.5)

**Balances at 5.** Full verdict recorded in its own section above the T7.1 sweep.

#### Files changed by this phase

| File | Change |
|---|---|
| `Configs/FieldManual/Categories/FM_Overthrow.conf` | +1 entry, 8 pieces, 9 new GUIDs. Text values and new inline objects only — no framework, no structure moved. Braces re-checked balanced. |
| `Language/localization_Overthrow.st` | **+9 new items**, **1 changed `Target_en_us`**, **6 Comment re-cites**. |
| `docs/features/virtualization/core/api.md` | §6 prose, §8 Route B callout |
| `docs/features/virtualization/epic-overview.md` | rows 4 + 5, status, rollup, summary |
| `docs/overview.md` | one epic row |
| `docs/features/virtualization/integration/implementation.md` | T7.7 sentence corrected |
| `docs/features/virtualization/integration/tasks.md`, `context.md` | this bookkeeping |

**Zero `.c` files touched**, so `tools/compile-check.sh` was not the gate here; it is also unrunnable in
this session (WSL interop is broken — Windows binaries cannot execute, the script dies on a
`%USERPROFILE%` resolution error). Docs-only phase, no suite run, per the plan.

#### ⚠️ Localization keys — a Workbench re-export is REQUIRED

The `.st` is the source; `Language/localization_Overthrow.<lang>.conf` are generated exports and were
**not** touched. Until the user re-exports in Workbench, the new page renders raw keys.

**New (9):** `OVT-FieldManual_OccupyingForces_Title`, `_Text`, `_Head`, `_Text2`, `_Head2`, `_Text3`,
`_Head3`, `_Text4`, `_Text5`.
**Changed (1):** `OVT-FieldManual_BaseCapture_Text5` (`Target_en_us` rewritten).
**Comment-only (6, no re-export needed for these but they ride along):**
`OVT-Difficulty_Easy_Desc`, `OVT-Difficulty_Hard_Desc`, `OVT-Difficulty_Insane_Desc`,
`OVT-Faction_US_Occupying`, `OVT-Faction_USSR_Occupying`, `OVT-Tutorial_BasesFirstCapture_Body`.

The 6 comment-only edits are stale-citation repairs found during T8.1: `OVT_OccupyingFactionManager.c:508`
(the patrol-group read site) **no longer exists** — it went with the deleted legacy tower-garrison spawn
block in Phase 4 — and `OVT_InfantrySpawningDeploymentModule.c:224` moved to **`:416`**,
`OVT_BaseUpgradeDefensePosition.c:48` to **`:47`**. **Every claim those citations supported is still
true** (difficulty presets still scale patrols and garrisons monotonically; the occupier still
garrisons bases and towers and patrols towns); only the pointers were dead. Nothing was cut as
unverifiable in this phase.

#### Owed, and deliberately left

1. **Wiki sync (T8.3)** — tools unavailable; copy drafted above. **The crashed session's wiki state was
   never verified** and must be re-fetched page by page.
2. **Workbench localization re-export** — 9 new + 1 changed key.
3. **A bespoke Field Manual tile** for "Patrols and Garrisons" (currently `default_ui.edds`).
4. **No tutorial popup for towers/patrols** — the trigger catalog has no event that could fire one
   (`OVT_TutorialTrigger.c:13-44`). Belongs to the tutorial-system feature, not here.
5. **The stale comment at `OVT_OccupyingFactionManager.c:443`** (Phase 4 left it deliberately inside an
   empty-diff-gated method) is **still stale** and was **not** touched: this phase's remit is
   player-facing text and it is a code comment inside a file this phase must not modify. Still owed to
   `base-defense-migration`.
6. `OVT_EGroupOrigin.RADIO_TOWER_GARRISON` remains declared with no producer (T4.9 item 4) — correct,
   do not touch the enum.

### 2026-08-17 (Phase 7 code complete — All owed)

- Phase 7 by `component-developer-advanced`: T7.1–T7.7. **2 files edited (the shared persistence gate,
  the Campaign GM-registry case), 4 cases added, 1 kill-switch guard removed, 1 real save point
  decoded.** Compile **0** (6132 files, unchanged). No production code touched at all — `git diff`
  over `Scripts/Game/GameMode/`, `Scripts/Game/Persistence/` and `Configs/` is exactly what Phase 6
  left behind.
- **T7.1 ran first and came back clean**: 18 real `RegisterGroup(` call sites under
  `Scripts/Game/Tests/`, every one safe under movement's D12 rule, nothing needed changing. Table
  above. The three new deployment-marker fixtures register nothing at all *by construction*, which is
  the entry future phases should copy.
- 🔴 **The phase's headline finding is structural and was not in the plan**: the suite's reload seam
  reaches the **game mode entity only**, so a deployment marker — a world entity with its own
  persistence config — can never be handed its stored payload back inside this harness. T7.2/T7.3/T7.4
  therefore assert the **restore half** through `ApplyPersistedDeployment()` (the exact method the
  marker's `Deserialize` calls) with a real save taken alongside, and the split is written at the top
  of the new fixture class instead of being left for a reader to discover. Full note in Gotchas.
- New cases and the claim each carries:
  - `..._DeploymentRecord_SurvivesSaveAndReapply` (T7.2) — four scalars + config resolution + the
    virtual key, and the key is **not re-derived**: the planted key uses coordinates no marker stands
    at, and the case asserts that precondition before it asserts the key. The only case in the tree
    that takes a real save point with a **tracked** deployment carrying a v2 key.
  - `..._DeploymentEliminated_RegistersNoGroups` (T7.3, G4's teeth) — the flag-before-
    `InitializeDeployment` ordering asserted at the **module** level (nothing else sets that on a
    fresh restore), `EnsureGroups()` registering zero, and — after the reload — a re-applied payload
    re-marking live modules.
  - `..._DeploymentVersion1Payload_StillLoads` (T7.4) — an empty-key payload restores and mints its
    key from its own marker, once; **and** a later re-apply of that same keyless payload does *not*
    clobber the key the session now holds (one `if`, and without it a live deployment's whole force
    is orphaned).
  - `..._DeploymentOwnedGroups_ReclaimAfterReload` (T7.5) — the reclaim contract: two composed
    deployment owner keys differing only in module tag survive storage **verbatim**, `@` and `#`
    included, and each answers for exactly its own handles. Also the only place the `"m" + index`
    positional fallback tag is ever stored and read back. Core's two member-survival cases are **not**
    duplicated.
- **T7.6: guard removed, nothing re-pointed.** The case never asserted on `RADIO_TOWER_GARRISON`
  (checked before editing), so §3.7's re-point clause did not fire. Its producer moved from a
  kill-switched base upgrade to the deployment wave, so the wait budget was re-derived (25 s → 55 s,
  `timeoutS` 60 → 90) and the failure text now prints a **deployment ledger** as well as the base
  ledger, because "registry empty" has three interesting causes now. Kill-switch ledger **6 → 5**, and
  `Scripts/Game/Tests/` is clean of the tag.
- **T7.7 decoded 40 blobs across 5 worlds.** The prize: a **pre-feature save with 23 version-1
  deployment records**, field for field, `virtualKey` count **0** — the migration path T7.4 asserts
  is real and in the wild. Also: the save context is **name-keyed** (renaming a serializer local
  renames a stored key — a live hazard for anyone tidying one); the registry payload stores owner
  keys as plain strings; and the plan's "no `AIWaypoint` record for anything core owns" is **wrong**
  — records exist and are meant to, they are simply `SelfSpawn 0`. **One T7.7 claim is
  inspection-owed**: no v2 deployment payload exists in any blob yet (the CI world saves ~1 s into
  the campaign, before the evaluator's first run), so "a stored `virtualKey` appears" needs a decode
  after the next play-test. Two-minute check, on the manual list.
- Deviations, all recorded in Gotchas with reasoning: three of four cases assert the restore half
  only (the seam cannot do otherwise); T7.3's dirty step clears **module** flags only, deliberately
  leaving the deployment gate set so no fixture can ever register during a stall; T7.2 uses
  `CreateDeployment` (tracked) while T7.3/T7.4 use a bare marker (untracked, and the only way to the
  fresh-restore branch).
- Next: orchestrator runs **All** — including the newly un-guarded Campaign case, which is the one to
  read first if the run is red (its failure text now explains itself). Then Phase 8 (help & docs sync),
  which owes two corrections this phase found: DoD F15's rebuy claim (Phase 4) and the `AIWaypoint`
  sentence (T7.7 finding 5).

### 2026-08-17 (Phase 6 code complete — suites owed)

- Phase 6 by `component-developer-advanced`: T6.1–T6.6. **The feature's one and only core change.**
  3 files edited (core manager, the recruit group marker component, the Init suite), 2 Init cases
  added, 3 docs updated. Compile **0** (6132 files, unchanged).
- Built to the **T1.9 verdict**, not around it: the key is treated as an identity (idempotent add),
  `HasEntityObserver`/`GetEntityObserverCount` answer from core's `map<EntityID, int>` and **never**
  from the engine, and nothing anywhere expects its own effect on the calling frame.
- Core diff is **+305 / −2** and every removed line is the reworded waypoint-ownership comment Phase 5
  handed over. `grep -rn "OVT_EntitySpawningAPI" Scripts/` → **0**, closing Phase 5's recorded
  deviation. `grep -rn "InsertObserverSP" Scripts/ --include=*.c | grep -v Tests` → **one call site**,
  with the null guard on the line immediately above it *and* at the top of the method.
- Observer key base **771000** (spike owns 770019). Keys are never persisted and never replicated.
- Consumer wiring detects server-side creation with the component's own **`OnPostInit` +
  `Replication.IsServer()`**, which then **defers the add by one call-queue hop** — the group prefab is
  replicated, so the event runs everywhere and the guard is what makes it the server's; the hop is what
  guarantees the group entity is world-registered and has a real `EntityID` by the time core is asked
  to key on it (see the ⚠ id finding in Gotchas). No manager plumbing, no registry, no subscription
  into the recruit transfer paths (D16); the pending hop is cancelled and the observer removed in the
  same `OnDelete` that already cleans the group's waypoint.
- Deviations, both recorded above: **one public method beyond the four** (`GetRecruitGroupsAreObservers()`,
  because the new attribute is `protected` and the gate is read by the consumer, not inside
  `AddEntityObserver` — one consumer's knob must not disable another's); and the null guard is
  **repeated** immediately above the `InsertObserverSP` call as well as at the top of the method, which
  is what the phase gate asks for and is worth it for a call that hard-freezes the client.
- `git diff Configs/` carries **only Phase 4's registry entry** — Phase 6 touched no config and no
  prefab (the new attribute takes its default from the class, so the game-mode prefab needed no edit).
  Kill-switch guards re-verified: **6**, unchanged.
- ⚠ Handed to the play-test: **T6.6's cost note** (above) — a parked squad now pins a 3.5 km circle of
  registered groups awake, which is the requirement and the budget risk at the same time.
- **First suite run found a real defect in this phase and it is fixed** (2026-08-17):
  `..._EntityObserverRoundTrip` went red with "HasEntityObserver was true for an entity no observer was
  ever added for" — two freshly spawned marker entities were sharing one `EntityID`. Core now refuses an
  entity whose `GetID()` is `EntityID.INVALID`, the recruit consumer defers its add by a frame so it can
  never be the caller that trips it, and the case spawns prefab-backed markers 150 m (not 3 km) out and
  asserts the id precondition before anything else. Full finding in Gotchas → Phase 6 additions. The
  other three reds in that run (`GetAllHandlesEnumeratesRegistry`, `MissingFactionRecordIsDropped`,
  `RegisterBuildsDormantGroup`) were 500 ms step-timeouts on a visibly degraded host — same signature as
  the Phase 3 stall — and were left alone for the orchestrator to judge on the re-run.
- Next: orchestrator re-runs **All**; then Phase 7 (save compatibility + persistence-tier coverage).

### 2026-08-17 (Phase 5 code complete — suites owed)

- Phase 5 by `component-developer-advanced`: T5.1–T5.8. **6 files edited, 1 file deleted (460 lines),
  5 stale comments rewritten, 1 Init case added.** Compile **0** (6133 → 6132 files).
- **T5.5's verdict was read off the engine source before the teardown was written** (above):
  `HasHeldMember` protects the group's own AI crewmen, never players, so `UnregisterGroup` and the
  vehicle's player-occupancy/ownership check are two separate questions and both are asked. The
  ownership check is `OVT_PlayerOwnerComponent.GetPlayerOwnerUid()` — the same one the modded
  `SCR_GarbageSystem` refuses to collect on.
- **T5.7's gate came back clean**: exactly two live callers, both in the vehicle module being rewritten.
  The file is gone; kill-switch guards **7 → 6**.
- Files: `Modules/OVT_VehicleSpawningDeploymentModule.c` (rewritten),
  `Modules/OVT_MultiTownPatrolBehaviorDeploymentModule.c` (rewritten),
  `Modules/OVT_BaseSpawningDeploymentModule.c` (+5 hoisted members),
  `Modules/OVT_InfantrySpawningDeploymentModule.c` (−46, uses the hoisted members),
  `OVT_EntitySpawningAPI.c` (**deleted**), `OVT_TEST_InitSuite.c` (+1 case),
  plus comment-only edits in `OVT_TownVehicleSourceConfig.c`, `OVT_GMGroupRegistry.c`,
  `OVT_GMWaypointWalk.c`, `OVT_RecruitManagerComponent.c`.
- Deviations, all recorded above with reasoning: **one** `OVT_EntitySpawningAPI` comment survives inside
  the frozen core and was left for Phase 6 rather than break the empty-core-diff gate; shared
  registration plumbing was hoisted onto the base spawning module rather than copied; the directory is
  **+1209 total lines / +95 code lines** feature-wide, so the "net deletion" criterion is not met and the
  real numbers are tabled instead; the vehicle module gained reclaim/pairing that T5.3 does not name,
  because without it a continued campaign would double-register every crew (a hard-floor quality bar).
- Two additions beyond the task wording, both defensive: a player's **recruit** riding in a deployment
  truck also vetoes its deletion, and the route-completion departure latch **scales with the route**
  instead of being a fixed 300 m.
- 🔴 One upstream doc defect found and worked around: `SCR_AIGroup.GetOnAgentAdded()`'s header claims two
  invoker parameters and the engine passes one (Gotchas above).
- Next: orchestrator runs **All**; then Phase 6 (AI observers — the one additive core change; it should
  also re-word `OVT_VirtualizationManagerComponent.c:707`).

### 2026-08-17 (Phase 4 code complete — suites owed)

- Phase 4 by `component-developer-advanced`: T4.1–T4.9. **2 new module classes, 1 new shipped config + its
  registry entry, 3 new Init cases, and a 73-line deletion inside the occupying faction manager.** Compile **0**.
- **T4.1 survey ran first and came back clean on all five claims** (above), which is what made the manager
  edit a pure deletion of `[NonSerialized]`, unread state. `git diff` on `OVT_PersistedRadioTower`, the
  serializer, `RplSave`, `RplLoad` and `ApplyPersistedOccupyingFaction`: **all empty**.
- **T4.2 came back MATCH for both factions**, so the config uses `light_patrol` and no faction config was
  edited. `git diff Configs/` is exactly the one appended registry entry, plus the two new untracked files.
- Files: `Modules/OVT_RadioTowerControlConditionDeploymentModule.c` (new),
  `Modules/OVT_RadioTowerCaptureBehaviorDeploymentModule.c` (new),
  `Configs/Deployment/Deployment_TowerGarrison.conf` + `.meta` (new),
  `Configs/Deployment/overthrowDeployments.conf` (+2), `OVT_DeploymentManager.c` (D19 split),
  `OVT_OccupyingFactionManager.c` (−73/+22), `OVT_TEST_InitSuite.c` (+3 cases).
- Deviations, all recorded above with reasoning: `m_bReinforceFromNearestBase 0` was authored although the
  plan does not name it (the `true` default would rebuy a tower's garrison at a distant base with a DEFEND
  plan); `EvaluateCapture()` was exposed as a seam so the headline claim is assertable without a marker
  entity; `GetPrimaryLocationTypeAtPosition` is reachable so "OR-ed in, not replaced" is a live assertion;
  the stale comment at `OVT_OccupyingFactionManager.c:443` was left alone because the phase gate demands an
  empty diff on the method containing it.
- Two findings for the play-test, both flagged 🔴 in Gotchas: the 500 m road snap on garrison registration,
  and **DoD F15's rebuy claim being untrue** for every config, not just this one.
- Manager net: **+22 / −73**. The raw deletion matches the plan's "~70 lines"; the additions are the
  anti-revival header on `CheckRadioTowers` (R16) and the tombstone where the garrison field was.
- Next: orchestrator runs **All**; then Phase 5 (vehicle patrols).

### 2026-08-17 16:05
- Feature started via /autorun-feature (Discord). Docs scaffolded from implementation.md (8 phases, 62 tasks).
- Epic context loaded: movement's "For `integration`" seam contract (plan-is-the-opt-in; resume-from-index-0; vehicle-borne groups stay live) is the movement-side input; core api.md §10 `integration` table is the core-side contract.
- Working tree at start: only `managers.layer` modified (pre-existing) + this feature's docs untracked. Concurrent-session risk R8 noted — re-grep file:line refs before each phase.
- Next: Phase 1 delegation.

### 2026-08-17 (Phase 3 gate)
- First All run red: `OVT_TEST_Init_Virtualization_AmbientRollCountOverrideIsCalled` timed out. Investigated before re-running: **total engine silence for 105 s** (no script OR platform output) starting immediately after a user-settings save commit, then the harness woke and stamped the timeout; the case produced zero output; the feature's diff to `OVT_TEST_InitSuite.c` is purely additive (+686/-0) and does not touch that case; no evaluator/registration activity in the window. Verdict: **host-level main-thread stall (I/O hitch, profile dir is under OneDrive), not a code regression**. One justified re-run → **All 245 green in 80 s** (stalled run took 173 s). If this case ever times out again with the same silence signature, suspect the settings-commit I/O path, not the ambient tick.

### 2026-08-17 (Phase 3 code complete — suites owed)
- Phase 3 by `component-developer-advanced`: T3.1–T3.10. 8 files edited, 2 Init cases added, compile **0**.
  **This is the first code in the campaign that registers virtualization groups.**
- Shape as built: `EnsureGroups()` → `ConvergeGroups(bool fromNearestBase)` → `ReclaimHandles()` +
  `RegisterGroups()`. One convergence serves activation, the records-restored fan-out and the rebuy; the
  rebuy differs only by its spawn anchor and by raising `m_iActualGroupCount` first.
- Deviations, all recorded above with reasoning: the waypoint-API grep is **not** empty (9 residual hits,
  all Phase 5's — deleting them now would leave both vehicle configs inert); `git diff --stat` on the
  deployments directory is **+68**, not a net deletion (all of it doc comment on the infantry module,
  whose code-only count moved +26); the vehicle module needed one renamed shim
  (`CheckIfVehiclesEliminated`) because the base virtual it overrode was deleted.
- Manager fan-outs (T3.7) needed **no code change** — Phase 2 had already wired
  `OnVirtualRecordsRestored` → `deployment.EnsureGroups()` and `OnVirtualGroupWiped` →
  `deployment.OnVirtualGroupWiped(handle)`; Phase 3 made both sides real.
- Next: orchestrator runs **All**; then Phase 4 (tower garrison config).

### 2026-08-17 17:20 (Phase 2 complete)
- Phase 2 by `component-developer-advanced`: 3 new files, 5 edited, compile 0, **All 243 green** (237 + 6 Logic).
- Notable deviations (all sound): `NextKeyOrdinal` probes live deployment keys rather than counting (restored deployments carry persisted keys without asking for an ordinal); `OwnerKey` sanitises the module tag (this is what makes the `("a#b","c")` vs `("a","b#c")` non-ambiguity true); `Sanitise` strips rather than substitutes (`TowerGarrison@x_z`); `ApplyPersistedDeployment`'s key param has no default (serializer is the only caller — safer for Phase 7 cases); added symmetric inert `OnVirtualGroupWiped` fan-out beside `EnsureGroups`.
- ⚠ Found: vanilla perimeter geometry bug (pitch-for-yaw) — see Gotchas. Town patrol footprint will visibly change once plans come from the factory.
- Rollback caveat: a v2 save read on a pre-Phase-2 build leaves one unconsumed trailing string per deployment payload (forward-compat is the supported direction, same as the tree's three existing v1→v2 serializers).
- Next: Phase 3 (advanced) — Town Patrol migration.

### 2026-08-17 16:30 (Phase 1 complete)
- Phase 1 by `component-developer`: T1.1–T1.7 landed as planned (one addition: `SetResourcesInvested` absolute setter on the component). T1.8 dropped with recorded verdict; T1.9 gate answered.
- First All run red on the spike case itself: it asserted synchronous observer semantics, but the engine defers application. Case rewritten for deferred semantics (bounded settle polls, assert-once); second run **All 237 green**. Per-policy: one fix iteration, one re-run.
- Baseline note: All was 236 before this feature; 237 with the spike case.
- Next: Phase 2 (advanced) — scaffolding; consider the RegisterDeployment latent defect noted above.
