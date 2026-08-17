# Base Defense Migration - Context & Decisions

**Last Updated:** 2026-08-18 (**post-completion amendment A1** built: authored base perimeters, free garrisons, AT road overwatch — full record at the bottom of this file. Phase 8 complete before it. Wiki sync OWED, see T8.3 below; loc re-export OWED)
**Current Phase:** ALL 8 PHASES COMPLETE + AMENDMENT A1 — Ready for Review (built 2026-08-18 via /autorun-feature; A1 2026-08-18 from the user's play-test)

> **Phase 4 gate note (orchestrator):** first run failed 1/272 — the new Persistence case was named `..._BaseDefenseDeployment_...`, and the suite runs **alphabetically**, so its real save landed before `..._Capability_SaveGameProducesASave`'s "no save yet" precondition. Renamed to `..._DeploymentBaseDefense_...` (sorts with the other Deployment cases, after the Capability gate) and added the naming rule to the suite's case-list comment. Re-run: **272/272 green**.
**Status:** ✅ Complete (Ready for Review)

**Epic:** `virtualization` (feature 5/5, the closer — the kill switch leaves with this feature)
**Plan:** `implementation.md` (8 phases, 68 tasks). Requirements: `requirements.md`. Core api.md is 🔒 FROZEN — this feature asks core for nothing.

---

## Quick Status

**What's Done:**
- ✅ Plan complete (2026-08-17), four binding user decisions recorded verbatim in implementation.md §5 (D1 per-concern configs, D2 elevated placement generalized, D3 specops dropped, D4 save conversion = value refund)
- ✅ tasks.md + context.md scaffolded
- ✅ **Phase 1 (T1.1-T1.8): the evaluator escalates.** Blanket 100 m veto deleted, the name-scoped dedup moved INTO the per-config filter through a new world-free `OVT_DeploymentSelection`, ceiling raised to 400 on the game-mode prefab, `OVT_NoPlayersNearbyConditionDeploymentModule` shipped, 2 Logic + 3 Init cases added. Compile **0**. Suite run owed (the orchestrator's, not the phase agent's). Full record below.
- ✅ **Phase 2 (T2.1-T2.9): the modules, the providers and the registry entries.** Three additive seams + `m_bSnapToRoad` on the shipped infantry module, `OVT_DeploymentPlacement`/`OVT_DeploymentPlacementProvider` + three providers, `OVT_PlacedInfantrySpawningDeploymentModule`, `OVT_CompositionSpawningDeploymentModule` (+ `OVT_EDeploymentSlotType`), `OVT_ParkedVehicleSpawningDeploymentModule`, `WasRestoredFromSave()`, 9 appended registry entries **per faction**, 4 Init cases. Compile **0**, negative-control verified. **Nothing authors any of it yet** — no deployment config was touched. Full record below.

- ✅ **Phase 3 (T3.0–T3.7): the three garrison-patrol configs, and S1 in the classification.** `BASE` now ORs into `GetLocationTypeAtPosition()` within 250 m of a base centre (T3.0, decision S1 — **Q1 is answered**); `Deployment_BaseGarrisonPatrol` / `_BaseHeavyPatrol` / `_BaseATSection` authored and registered; `OVT_BaseUpgradeDefensePatrol` + its conf entry and `OVT_BaseUpgradeTownPatrol` + `OVT_OccupyingFactionManager.RecoverResources()` deleted; 2 Init cases. Compile **0**, negative-control verified. Full record below.

- ✅ **Phase 5 (T5.1–T5.8): static content ships and the base-upgrades config is down to ONE entry.** `Deployment_BaseCheckpoints` / `_BaseFortifications` / `_BaseParkedVehicles` authored and registered; `OVT_BaseUpgradeCheckpoints` + `OVT_BaseUpgradeComposition` + `OVT_BaseUpgradeParkedVehicles` + `OVT_SlottedBaseUpgrade` deleted with their five conf entries; the build decision and the slot lottery extracted into pure statics; 2 Init cases. Compile **0**, negative control verified. Full record below — including the **DEFEND-anchor fix** (found in Phase 5, fixed in Phase 5 by orchestrator decision; it also repairs Phase 4's shipped `Deployment_BaseDefensePositions.conf`), which added a 3rd Init case.

- ✅ **Phase 4 (T4.1–T4.10): exact placement ships, and the first three base-upgrade classes of the migration proper are gone.** `Deployment_BaseDefensePositions` / `_BaseTowerGuards` / `_BaseSniperPositions` authored and registered; `OVT_BaseUpgradeDefensePosition` + `OVT_BaseUpgradeTowerGuard` + `OVT_BaseUpgradeSniperPosition` deleted with their three conf entries; the placement decision extracted into four pure static methods on the placed module; 3 Init cases + 1 Persistence case. Compile **0**. Full record below.

- ✅ **Phase 6 (T6.1–T6.10): ONE funding path, specops dropped, legacy saves convert.** The base-spend
  loop, the +5 s per-base distribution and the conditional drip are all gone; 80 % of every tick and the
  opening budget go to the deployment pool through a single credit point; `OVT_BaseUpgradeSpecops` and
  its conf entry are deleted; a pre-migration save's upgrade value is summed and refunded (deferred —
  see the ordering hazard below). 2 Logic + 1 Init + 1 Persistence case. Compile **0**, negative control
  verified. Full record below.

- ✅ **Phase 7 (T7.1–T7.11): the retirement sweep, and the epic's closing act.** The `BaseUpgrades/`
  directory, its config, `OVT_BaseUpgradesConfig`, the base controller's upgrade half, 13 legacy
  faction attributes with every authored value, `m_iMilitarySpawnDistance`, the GM class-name
  formatter and the Campaign-tier base-upgrade test are all **deleted**; the GM snapshot is re-pointed
  at the deployments anchored at each base; the QRF spawn queue is **restored** (one deleted line);
  and `OVT_VirtPlaytestKillSwitch.c` is gone — `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` returns
  **nothing**. Compile **0**, negative control verified. Full record below.

- ✅ **Phase 8 (T8.1–T8.5): help & documentation sync.** Field Manual fact-checked and corrected (3
  false sentences cut, 3 paragraphs widened, 9 comments re-cited), a `Defending a Base` section added
  to the Patrols and Garrisons page, `api.md` §6 corrected, the epic rolled to 5/5 and the closing
  ledger recorded. Full record at the bottom of this file.

**What's Next:**
- ✅ **Amendment A1 (2026-08-18) built** — `PERIMETER_BASE` walks the base controller's authored square (`m_fPerimeterRadius` / `m_fPerimeterRotation` ± 10°, no road snap), garrison patrol + tower guards are free at game start, the AT section became a placed road overwatch, and the base marker draws its square in Workbench. Compile `0`; **suite run owed to the orchestrator**. Full record at the bottom of this file.
- 🔴 **The Workbench perimeter viz has never been looked at** (it is `#ifdef WORKBENCH` + selected-only, so no suite can reach it) — see the A1 record's "what still needs a human".
- 🔴 **The wiki sync (T8.3) is OWED** — the `mcp__wikijs__*` tools were not available to the Phase 8
  agent. The exact content to publish is written out in the Phase 8 record below.
- 🔴 **A Workbench localization re-export is OWED** (3 new keys, 6 edited).
- ~~🔴 AN OPEN DESIGN QUESTION (Q1)~~ — **CLOSED by S1, implemented in Phase 3 T3.0.** The measurement below ("Towns shadow bases") stands as the record of why: 4 of Eden's 10 bases and the Init test world's only base classified as `TOWN` and could be offered no `BASE`-only config. The plan's §3.2 assumption that "no classification change is needed" was **false as written**, and Phase 3 corrected it in three lines.

**Blockers:**
- None. Phase 3 answered Q1 by implementing S1; every later phase's `BASE` configs inherit the fix.

---

## Key Files

### Will change
- `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c` — evaluator escalation (Phase 1)
- `Scripts/Game/GameMode/Deployments/Modules/OVT_InfantrySpawningDeploymentModule.c` — additive seams (Phase 2)
- `Scripts/Game/GameMode/Deployments/Modules/` — 4 new module classes + 3 placement providers (Phases 1–2)
- `Configs/Deployment/Deployment_Base*.conf` — NINE new configs (Phases 3–5)
- `Configs/Factions/{USSR,US}_OverthrowData.conf` — new group/vehicle/composition registry entries (Phase 2), legacy array deletion (Phase 7)
- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` — funding single-path + save conversion (Phase 6)
- `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c` — shrunk to slot registry + base data (Phase 7)

### Deleted (all done)
- ✅ `Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/` — 12 files, phases 3–7, directory gone
- ✅ `Configs/BaseUpgrades/` (+ `.meta`) + `Scripts/Game/Configuration/OVT_BaseUpgradesConfig.c` (Phase 7)
- ✅ `Scripts/Game/GameMode/Virtualization/OVT_VirtPlaytestKillSwitch.c` (Phase 7 — **EPIC END**)
- ✅ `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_BaseUpgrades_ConfigLoaded.c` (Phase 7)

### Frozen / must not change
- `Scripts/Game/GameMode/Virtualization/` (except the kill-switch file deletion) — G8/Q9
- The four shipped deployment configs (byte-identical — I1); GM record classes + RPC; save payload field order (Q11)

---

## Important Decisions

(Plan-level decisions D1–D10 live in `implementation.md` §5 — recorded there verbatim, not duplicated here. Session decisions land below.)

### S1 — Q1 resolution: BASE bit ORs in within 250 m of a base centre (fallback decision, user not reached)
**Date:** 2026-08-17
**Context:** Q1 below — 4 of Eden's 10 bases classify TOWN (towns tested first, 500 m radius) and could be offered no BASE-only config. The Discord bridge's ask path was down (2 transport timeouts), so the orchestrator took the recommended option per the away protocol and announced it on the progress channel (which was up) with an explicit veto invitation.
**Decision:** In `GetLocationTypeAtPosition`/`GetPrimaryLocationTypeAtPosition`, OR the `BASE` bit into the result for any position within **250 m** of a base centre — deliberately the same radius as the `HasExistingDeploymentOfType` name-scoped dedup.
**Rationale:** Radius equality makes force doubling geometrically impossible: any position that spuriously gains the BASE bit is ≤250 m from the base centre, so the dedup sees the base's own copy of a config and refuses a second. The 4 shadowed base centres always get the bit (distance 0); their town centres (323–481 m away) never do. Rejected: 500 m OR-in (doubles at Levie, 460 m), full candidate dedup (more code, moves town patrols), accepting the regression (~40 % of bases undefended vs legacy).
**Impact:** Phase 3 implements this alongside the first BASE config, plus an Init case asserting a town-shadowed base position qualifies for a BASE config. **User can veto/revise — flagged in the final report.**

---

## Gotchas & Learnings

### Inherited from siblings (binding, from integration/movement context)
- **`CloneModule` copies attributes by hand and silently drops what it forgets** — every new/edited module hand-copies its own + all inherited attributes; T2.8 asserts clone fidelity.
- **`SCR_AIGroup.GetOnAgentAdded()` passes ONE argument** (the vanilla doc comment is wrong); recover group via `agent.GetParentGroup()`.
- **`.conf` module order is update order and `.conf` files cannot carry comments** — spawning first, behaviour, reinforcement last among behaviour, then conditions.
- **Init-tier worlds never run `PostGameStart`** — tick cases install the tick themselves; autotest camera is an observer — use `spawnDistanceOverride = 0` (Manual policy); scope registry assertions to your own owner key.
- **Deployment test fixtures must be `SetSpawnedUnitsEliminated(true)`** on the deployment AND every spawning module before anything ticks.
- **Logic-tier grep matches comments too** — no manager/world/entity/`OVT_Global` identifiers anywhere in Logic files.
- **`RandInt` is max-exclusive; `out`/`owned` are reserved; `vector.Distance` is +1 ULP at 1000/2000 m; PrintFormat max 3 string params.**
- **No `maxAttempts` anywhere** — suites are deterministic; every new case carries a can-fail proof.

### From Phase 2 (binding on the config phases 3–5)

- **The new module attribute names, so a config phase authors them right the first time:**
  - `OVT_PlacedInfantrySpawningDeploymentModule` — `m_Placement` (a provider object), `m_fSearchRadius`
    (default 280 = `baseRange`), plus all thirteen infantry attributes.
  - `OVT_CompositionSpawningDeploymentModule` — `m_sCompositionTag`, `m_eSlotType`
    (`OVT_EDeploymentSlotType`: `SMALL`/`MEDIUM`/`LARGE`/`ROAD_SMALL`/`ROAD_MEDIUM`/`ROAD_LARGE`),
    `m_bFillAmmoBoxes`, plus all thirteen. ⚠ **The slot enum's road members are `ROAD_MEDIUM` /
    `ROAD_LARGE`**, not `MEDIUM_ROAD` — §3.2 of the plan writes them the other way round in the
    checkpoint row.
  - `OVT_ParkedVehicleSpawningDeploymentModule` — `m_sModuleName`, `m_sVehicleType`, `m_iVehicleCount`,
    `m_iCostPerVehicle`, **`m_eParkingType`** (author `PARKING_TRUCK` for the truck config, or it asks
    for a car spot and parks nothing).
- **`m_bSnapToRoad 0` must be authored on every base config whose groups garrison a PLACE.** It
  defaults to `1` (the shipped behaviour), so forgetting it is silent and puts the force up to 500 m
  away on a road — with a DEFEND or null plan they then hold *there*.
- **A composition module with `m_iMaxGroupCount 0` builds the structure and no guard** — that is how
  the ammo cache and the MG nest are authored.
- **A placed module ignores `m_iMinGroupCount` and the difficulty band**: it wants exactly as many
  groups as the provider offers posts, capped by `m_iMaxGroupCount`. Author the cap, not the floor.
- **Registry names now available on both factions:** groups `heavy_infantry`, `at_team`, `sniper`,
  `sniper_team`, `bunker_team`; vehicles `car`, `truck`; compositions `MediumCheckpoint`,
  `LargeCheckpoint` (alongside the existing `SmallBunker`, `AmmoCache`, `MGNest`).

---

## Testing Approach

- **Logic (Fast):** `OVT_TEST_Logic_BaseDefenseEscalation.c` (Phase 1), `OVT_TEST_Logic_BaseDefenseConversion.c` (Phase 6) — world-free maths.
- **Init (Fast):** additions per phase to `OVT_TEST_InitSuite.c` — seams, plan shapes, clone fidelity, slot bookkeeping.
- **Persistence (All):** two cases on the shared gate — base-defense deployment round trip (Phase 4), legacy-payload conversion (Phase 6). Restore-half assertions only; do NOT widen the reload seam.
- **Campaign:** `OVT_TEST_Campaign_BaseUpgrades_ConfigLoaded` **deleted** in Phase 7; `OVT_TEST_Campaign_GMGroupRegistry`'s header and failure ledger re-pointed at deployments (its base-upgrade ledger went with the base upgrades).
- **Suites:** All `{6A6E2A002F53A581}` after phases 1–7; skipped for Phase 8 (docs-only). Run by the orchestrator only.
- **Manual:** §6 play-test steps 1–14 + MP pass — owed to the user at the end (see tasks.md "Needs Human Verification").

---

## Next Steps

### Immediate
1. ✅ Phases 1–7 complete — compile `0` at every phase; **All suite run owed to the orchestrator**
2. 📋 Phase 8 (help & docs sync) — docs-only, suite skipped
3. 🔴 The manual play-test (§6 steps 1–14 + an MP pass) is the only evidence for placement quality,
   fortification pacing, the GM base panel, the restored QRF spawner and legacy-save conversion

### Future (after feature)
- User play-test §6 (keep a **pre-migration save before Phase 6**) + dedicated-server MP pass
- Pacing/tuning pass (step 14) feeding config costs and ceilings

---

## Open Questions

- ✅ **Q1 (CLOSED 2026-08-18 — option (a) at 250 m, per decision S1, implemented in Phase 3 T3.0): do bases shadowed by a town get base defense? YES.** The force-doubling cost that made option (a) unsafe at 500 m disappears at 250 m, because that is the dedup radius itself. Original write-up kept below for the record.
- 🔴 ~~**Q1 (Phase 1, must be answered before Phase 3): do bases shadowed by a town get base defense?**~~
  See "Towns shadow bases" below for the measurements. Three options were considered by the Phase 1
  agent and none of them is its call to make:
  **(a)** OR the `BASE` bit into `GetLocationTypeAtPosition()` the way `integration` OR-ed in
  `RADIO_TOWER`. One line, fixes 4 Eden bases - but it also makes a *town centre* within 500 m of a
  base acceptable to `BASE`-only configs, so a base and its town become **two candidate positions
  that both qualify**, and with the 250 m dedup the two are far enough apart (460 m at Levie) to each
  buy their own copy of every base config. That is force doubling, which the feature's own quality
  bar forbids outright. `integration` rejected the union for exactly this reason
  (`OVT_DeploymentManager.c:1065-1071`).
  **(b)** Deduplicate the candidate list instead - drop a town candidate that is within 500 m of a
  base candidate, or vice versa - and then (a) is safe. More code, and it changes where town patrols
  can be created.
  **(c)** Do nothing and accept that ~40 % of Eden's bases have no base-defense deployments. This is
  a **regression against the legacy system** being replaced: `SpendResources()` ran per base
  controller and never asked what kind of place the base was, so today all 10 bases fortify.
  Whatever is chosen, it wants a Phase 3 Init case asserting a *town-shadowed* base can be offered a
  base config.

---

## Session Notes

### 2026-08-18 (orchestrator) — WB viz crash #2 and the CreateArrow rewrite
- The member-buffer fix did NOT hold: Workbench crashed again on base selection (native illegal-write AV ~11 s after Eden load, no script frames, minidump unsymbolised — log `logs_2026-08-18_03-42-14`).
- **The viz no longer uses the CreateLines family at all.** `DrawPerimeterSquare` now draws each square as four `Shape.CreateArrow` edges (copy-safe — the attack arrows' proven primitive; bare `ONCE` calls, vanilla precedent `SCR_PowerLineJointEntity.c:163`), heads showing walk direction: solid cyan authored square (head 8), two faint ±10° squares (head 4), start arrow kept. The three member vertex buffers and the perimeter Shape handles are deleted. Compile 0.
- Standing rule (also in orchestrator memory): **per-frame `_WB_` viz in this repo uses copy-safe Shape calls only** — CreateLines crashed twice (locals AND member buffers) and is banned here until someone symbolises the second crash.
- User re-test owed: select a base in WB — expect 3 arrow-edged squares + start arrow, no jitter, no crash. Note `OVT_StartCameraPos.c:32,54` (pre-existing, unrelated) still passes a LOCAL buffer to `CreateTris` — selecting a start-camera entity may crash WB for that separate reason.

### 2026-08-17 23:20 (orchestrator)
- **Phase 1 gate: All suite green — 262/262.** First run had 2 reds (`OVT_TEST_Campaign_GMWaypointWalk`, `OVT_TEST_Campaign_ShopCivilianStock`), both the documented vanilla `Setup_Checkpoint` 500 ms I/O flake (error-type `TestResultTimeout` in Setup, no assertion text — the movement feature's known signature); single policy re-run was fully green. Compile 0.
- Q1 (towns shadow bases) put to the user via Discord before Phase 3.

### 2026-08-17 22:45
- Feature started via /autorun-feature (Discord). implementation.md → In Progress; tasks.md (68 tasks/8 phases) + context.md scaffolded.
- Baseline at start: v1.5 branch, All suite 257 green (integration + civilians complete), 5 `OVT-VIRT-PLAYTEST-ONLY` guards in tree.
- Next: Phase 1 implementation agent.

### 2026-08-17 - PHASE 1 BUILT (T1.1-T1.8). Compile `0`. Suite run owed.

**What shipped, in one line each:**

| Task | What landed |
|---|---|
| T1.2 | The blanket 100 m proximity veto is **deleted** from `IsPositionSuitableForDeployment()`, which is now a ground trace and nothing else. `MIN_DEPLOYMENT_DISTANCE` is **gone tree-wide** (`grep -rn "MIN_DEPLOYMENT_DISTANCE" Scripts/` → nothing, including comments) |
| T1.3 | `m_iMaxDeploymentsPerFaction 400` authored on `Prefabs/GameMode/OVT_OverthrowGameMode.et`. Class default untouched at 100. `git diff Prefabs/` is exactly this one line |
| T1.4 | `FindBestDeploymentConfig()` collects the already-present names per config **inside the filter loop** and picks through the new `OVT_DeploymentSelection`. The caller's identical check is kept, with a comment saying it is now a redundant belt-and-braces guard and must never go back to being a `continue` on the POSITION |
| T1.5 | `OVT_NoPlayersNearbyConditionDeploymentModule` - static gate at 320 m, runtime `return true` with the asymmetry spelled out in the class header and again on the method |
| T1.6 | `TestSuites/Logic/OVT_TEST_Logic_BaseDefenseEscalation.c`, 2 cases: the 7-rung acquisition ladder (priority beats offer order, offer order breaks ties, exhaustion answers nothing) and the defensive-input case (null/empty/ragged/unrelated-name/priority-has-no-ceiling) |
| T1.7 | 3 Init cases: `..._BaseLocationTypeIsReachable`, `..._NoPlayersNearbyGatesCreationOnly`, `..._EscalationBuysTheNextConfig` |

**Two files were touched beyond the task list, both because they asserted the rule that was deleted:**
`OVT_DeploymentVirtualKey.c`'s header claimed "the creation path enforces a 100 m minimum
separation", and `CollectSeedCandidates()`'s header gave that veto as one of its two reasons for
enumerating per location kind. Both corrected in place; the second reason still stands on its own.

**One deliberate widening:** `FindBestDeploymentConfig()` is now **public** (was `protected`), because
the phase's headline claim is only assertable as a live claim by calling it. Same justification, same
wording, as `GetPrimaryLocationTypeAtPosition()` carries from `integration` Phase 4. Nothing outside
the evaluator and that one Init case calls it.

**Deferred, on purpose:** §7's "`MissingCount`-style clamping for placement: wanted vs available
posts" bullet is listed under this phase's Logic file but describes the *placement* module, which
does not exist until Phase 2/4. `OVT_DeploymentVirtualKey.MissingCount` is already asserted by
`OVT_TEST_Logic_DeploymentVirtualization_MissingCount`. Add the post-count clamp case in Phase 4,
beside the provider it belongs to.

---

## T1.1 - the read-only survey, run BEFORE any edit - 2026-08-17

Every expectation the plan set, checked against the tree rather than trusted.

| Claim | Expected | Verdict |
|---|---|---|
| Callers of `IsPositionSuitableForDeployment` | 1 | ✅ **exactly 1** - `FindDeploymentCandidates` (was `:692`). Plus the declaration. No test, config or prefab reference |
| Callers of `FindBestDeploymentConfig` | 1 | ✅ **exactly 1** - `EvaluateFactionDeployments` (was `:614`). Two other hits are comments: `OVT_DeploymentManager.c:421` (the seeding pass, naming it as the match it copies) and `OVT_TEST_InitSuite.c:7994` (a case header). **Now 2 call sites**: the new Init case calls it too |
| `m_iAllowedLocationTypes` authored in `Configs/Deployment/` | TOWN ×1, RADIO_TOWER ×1, BASE ×2 | ✅ **exact match.** `Deployment_TownPatrol.conf:27` TOWN; `Deployment_TowerGarrison.conf:42` RADIO_TOWER; `Deployment_VehiclePatrol_Light.conf:20` and `Deployment_VehiclePatrol_Heavy.conf:19` BASE. `overthrowDeployments.conf` overrides **none** of them, so the registry inherits all four verbatim |
| `OVT_DeploymentConfig.m_iResourceAllocation` readers | zero | ✅ **zero, confirmed.** `OVT_DeploymentConfig.c:44` is the declaration and nothing reads it. Every other tree hit is `OVT_BaseUpgrade`'s unrelated same-named field: its declaration (`OVT_BaseUpgrade.c:4`), its two readers (`OVT_BaseUpgrade.c:51`, `OVT_BaseControllerComponent.c:311`) and its 9 authored values in `Configs/BaseUpgrades/overthrowBaseUpgrades.conf`. **Not authored in any deployment config. NOT deleted this phase** - it dies in the Phase 7 sweep if still unread, and deleting it now would be a `.conf` parse risk for no gain |

**No concurrent session had added a caller**, so the §3.4 design applied as written.

---

## 🔴 Towns shadow bases - the classification finding the T1.7 survey turned up - 2026-08-17

**MEASURED, not suspected.** `GetPrimaryLocationTypeAtPosition()` is a **first-match precedence
chain** that tests towns before bases, and `OVT_TownData.IsWithinTownBounds()` is a hardcoded **500 m
radius** (`OVT_TownManagerComponent.c:61-65` - it does **not** read the town's authored
`m_iTownRange`). `GetBasePositions()` offers the evaluator a base's **own centre** and nothing else
(`OVT_DeploymentManager.c:720-737`). So a base whose centre is within 500 m of a town centre
classifies as `TOWN` and can never be offered a `BASE`-only config.

Eden, measured off `Worlds/MP/OVT_Campaign_Eden_Layers/{bases,towns}.layer` (10 bases, 20 towns):

| Base | Nearest town | Distance | Classifies as |
|---|---|---|---|
| `#OVT-Base_Erquy` | Erquy | **323 m** | 🔴 TOWN |
| `#OVT-Base_Lamentin` | Lamentin | **372 m** | 🔴 TOWN |
| `#OVT-Base_Levie` | Levie | **460 m** | 🔴 TOWN |
| `#AR-MapLocation_MontfortCastle` | St Pierre | **481 m** | 🔴 TOWN |
| `#OVT-Base_Gravette`(MilitaryHospital) | Gravette | 689 m | BASE |
| `#OVT-Base_Chotain` | Chotain | 803 m | BASE |
| `#OVT-Base_StPhillipe` | St Phillipe | 930 m | BASE |
| `#AR-MapLocation_PowerPlant` | Tyrone | 1154 m | BASE |
| `#AR-MapLocation_Airport` | St Phillipe | 1239 m | BASE |
| `#OVT-Base_TrainingGrounds` | Chotain | 2229 m | BASE |

**4 of 10.** And the **Init test world's only base** is 114 m from its only town
(`OVT_Campaign_Test_Layers/default.layer`: base `55.331 128.241`, town `168.378 146.334`), so it is
shadowed too - which is why the T1.7 case had to be written as "a position **in a base's ring**
classifies as BASE" rather than the plan's "a base position classifies as BASE". The plan's wording
would have shipped a red suite.

**This is not new and this phase did not cause it** - it is true at HEAD for the two shipped vehicle
patrol configs, which today simply never deploy at those 4 bases. It becomes serious in Phase 3,
when **nine** configs depend on it and the system being replaced (`SpendResources()`, per base
controller, no classification anywhere) fortified all 10. Recorded as **Open Question Q1** above with
the three options and their costs. **Phase 1 did not act on it** - option (a) is one line and would
introduce force doubling, which is precisely the class of change that needs a decision rather than an
implementation.

The new Init case **prints the shadow count on every run**, so the number cannot be quietly lost.

---

## T1.3 - the ceiling arithmetic - 2026-08-17

Counted off the Eden layers rather than estimated:

```
bases.layer   OVT_BaseControllerComponent  x 10      (the plan said ~11; it is 10)
towns.layer   OVT_TownControllerComponent  x 20
radio towers                               x  2      (per integration A1; the tower entities are
                                                      not in a .layer file - they are discovered by
                                                      QueryEntitiesBySphere over the whole world)

fully fortified occupying faction after the migration:
     10 bases x 9 base configs   =  90
   + 20 town patrols             =  20
   +  2 tower garrisons          =   2
   +  2 vehicle patrols          =   2   (both m_iMaxInstances 1, so 2 map-wide, not 2 per base)
                                  ----
                                   114   against a class default of 100  ->  IT BITES
```

The class default has **no headroom at all** (114 > 100), and the ceiling is enforced in two places:
`EvaluateFactionDeployments()` returns outright when the per-faction list is at the cap, and
`SeedFreeConfig()` breaks with a WARNING naming the config. Hitting it means some location silently
goes without.

**Authored 400** on the prefab - ~3.5x headroom. Sized for: a larger map, a modded registry with more
base configs, and the fact that the ceiling is **per faction**, so the resistance's own deployments
(a later feature) do not draw on the same number. The per-pass cap
(`MAX_DEPLOYMENTS_PER_EVALUATION = 10`, ~6 minutes to establish 114 from cold at one pass per 30 s)
is **left alone** per the plan, and is a play-test question, not a compile-time one.

⚠ The class default in `OVT_DeploymentManager.c:34-35` is **unchanged at 100**, on purpose: a mod
that instantiates the component without the Overthrow prefab keeps today's behaviour.

---

## Phase 1 design notes worth keeping

**The escalation is one pure function, and it is `OVT_DeploymentSelection.SelectNextConfigIndex()`**
(`Scripts/Game/GameMode/Deployments/OVT_DeploymentSelection.c`, new). The evaluator builds three
parallel arrays - suitable names, their priorities, and the names this position already holds - and
the helper answers an index. That shape was chosen over a `continue` inside the filter loop
specifically so the Logic tier can assert the contract; a `continue` would have left the tier with
nothing to test but the manager itself.

**Two behaviour changes came free with the rewrite, both improvements, both asserted:**
1. **The priority ceiling is gone.** The old selection started at `int bestPriority = 999` and
   compared `<`, so a config authored at priority 999 or above could never be chosen by anything,
   with no error and no log line. The new one has no sentinel.
2. **Ties are documented and asserted** as "first in registry order wins", which was already the old
   behaviour but was nowhere written down.

**What the phase did NOT change, and would have been easy to:** `HasExistingDeploymentOfType`'s
250 m radius (still a default argument, still 250), the per-pass cap, the candidate sort, the chance
roll, and `GetTotalResourceCost`'s unused difficulty multiplier.

⚠ **A COST THIS PHASE ADDS, measured by reasoning rather than by a profiler - watch for it in the
play-test.** Two things compound:
1. the candidate list **no longer shrinks as deployments are created**. The 100 m veto used to take a
   position off `FindDeploymentCandidates`' list for good once anything stood there; now every town,
   base and tower is a candidate on every pass, forever;
2. `HasExistingDeploymentOfType` is now called **once per suitable config per candidate** instead of
   once per candidate, and it is a linear walk of every live deployment with a `FindEntityByID` +
   `Cast` + `vector.Distance` per entry.

Worst case on a fully fortified Eden: ~10 bases x 9 suitable configs x ~114 live deployments ≈ 10 k
entity lookups, plus towns, **per 30 s evaluation pass**. That is very likely a few milliseconds and
fine - but it is a new O(candidates x configs x deployments) term where there was an
O(candidates x deployments) one, and it grows with the map. If a 30 s hitch ever shows up in a
profile, the cheap fixes in order are: (a) break out of the per-config loop once every suitable
config is known present, (b) hoist one name-keyed index of live deployments per pass instead of
walking the list per query. Neither is worth doing on speculation.

**Fixture discipline for the new Init case** (`..._EscalationBuysTheNextConfig`): it appends **two
hand-built configs to the live registry** and removes them again, and creates **two real
deployments** which are `SetSpawnedUnitsEliminated(true)` on the deployment and every spawning module
the instant they exist, then deleted in the same `Execute()` frame - the standing rule from
`integration` T7.1. Teardown runs on every path including the red ones and deletes the **deployments
before** it takes the configs back out of the registry. It registers **no groups**, so
`grep -rn "RegisterGroup(" Scripts/Game/Tests/` is unchanged by this phase.

⚠ **`[Attribute]` defvalues do NOT apply to `new`.** A hand-constructed `OVT_DeploymentConfig` has
`m_fChance = 0` (so it is essentially never offered), `m_iMaxInstances = 0` and `m_iBaseCost = 0`;
a hand-constructed condition module has `m_fMinPlayerDistance = 0` (so it accepts every position).
Both new Init cases set every field they depend on by hand and say why. This will bite every future
fixture in this framework.

---

## 2026-08-17 — PHASE 2 BUILT (T2.1–T2.9). Compile `0`. Suite run owed.

**Nothing in the game uses any of this yet, on purpose.** Phase 2 ships the *machinery*; the configs
that author it are Phases 3–5. `git diff Configs/Deployment/` is empty and every existing case still
sees exactly today's behaviour.

| Task | What landed |
|---|---|
| T2.1 | `m_bSnapToRoad` (default **1** = shipped behaviour) + three protected virtuals on `OVT_InfantrySpawningDeploymentModule`: `ResolveSpawnPosition(anchor, index)` (called by `RegisterGroups` instead of the roller), `OnGroupRegistered(handle, pos)` (right after `TagForGameMaster`), `OnGroupReclaimed(handle)` (right after the reclaim's `TagForGameMaster`). All copied in `CloneModule` |
| T2.2 | `OVT_DeploymentPlacement` (position + angles + `GetTransform`) and `OVT_DeploymentPlacementProvider` (`[BaseContainerProps]`, one virtual, a written four-point contract) + `OVT_TowerCoverPostPlacementProvider`, `OVT_SniperMarkerPlacementProvider`, `OVT_BaseDefendPositionPlacementProvider` |
| T2.3 | `OVT_PlacedInfantrySpawningDeploymentModule` — post-per-handle, re-hooked on every reclaim, arrival counter zeroed by the group's own `GetOnMembersDespawning()` |
| T2.4 | `OVT_CompositionSpawningDeploymentModule` + `OVT_EDeploymentSlotType` (6 values) — one structure per module, ever; slot claimed only after a successful spawn |
| T2.5 | `OVT_ParkedVehicleSpawningDeploymentModule` on `OVT_BaseSpawningDeploymentModule` — parks once, never replaces |
| T2.6 | `OVT_DeploymentComponent.WasRestoredFromSave()` — runtime bool set at the top of `ApplyPersistedDeployment`, on **every** branch. Serializer, its version and its field order untouched (`git diff` on the serializer is empty) |
| T2.7 | 9 appended entries **per faction**: groups `heavy_infantry`/`at_team`/`sniper`/`sniper_team`/`bunker_team`, vehicles `car`/`truck`, compositions `MediumCheckpoint`(40)/`LargeCheckpoint`(60). GUID prefix `6AB1C7D4`, grep-verified unused repo-wide before use |
| T2.8 | 4 Init cases: `..._BaseDefenseRegistryEntriesResolve`, `..._SnapToRoadOptOutStaysInRadius`, `..._PlacementProvidersAnswerEmptyNotNull`, `..._NewModuleClonesCarryEveryAttribute` |

**Compile verified by negative control, not by trust.** A deliberate undefined-symbol was appended to
`OVT_PlacedInfantrySpawningDeploymentModule.c`; `compile-check.sh` reported it at the right file and
line and exited 1. Removed, re-run, exit 0. Worth doing because the check's 5 s runtime and constant
"6142 files" line look like a cache.

---

## T2.1 — the `CloneModule` audit verdict — 2026-08-17

**VERDICT: the existing copy list was COMPLETE. Nothing was missing before this phase, and one line
was added.**

Audited by listing every `[Attribute]` declared on `OVT_InfantrySpawningDeploymentModule` and on both
of its base classes, then matching each against `CloneModule`:

| Attribute | In the copy list? |
|---|---|
| `m_sModuleName`, `m_sGroupType`, `m_iMinGroupCount`, `m_iMaxGroupCount`, `m_bScaleByTownSize`, `m_fSpawnRadius`, `m_iCostPerGroup`, `m_bAllowReinforcement`, `m_iReinforcementCost`, `m_bSpawnAtNearestBase`, `m_bReinforceFromNearestBase`, `m_eImportance` | ✅ all 12 present |
| `m_bSnapToRoad` (new this phase) | ✅ added |
| `OVT_BaseSpawningDeploymentModule` — declares **no** `[Attribute]`, only two `static const` | n/a |
| `OVT_BaseDeploymentModule` — declares **no** `[Attribute]` | n/a |

Two non-attribute members are **correctly not copied** and that is worth stating so nobody "fixes" it:
`m_aHandles` (registry-derived, rebuilt by every convergence) and `m_iActualGroupCount` (a clone must
roll its own force size — copying a template's would freeze every deployment of that config at one
number). `m_iSpawnedEver` likewise.

⚠ **The real trap is one level up and it is now written into the code.** `CloneModule` is **not
chained** — each subclass `new`s its own instance and copies by hand — so
`OVT_PlacedInfantrySpawningDeploymentModule` and `OVT_CompositionSpawningDeploymentModule` each
reproduce all thirteen inherited lines verbatim before adding their own. A comment above the parent's
copy list now says that anything appended there has to be appended in both subclasses, and
`OVT_TEST_Init_Deployments_NewModuleClonesCarryEveryAttribute` asserts all four modules field by
field with non-zero/non-empty/non-false probe values (a `false` probe would be indistinguishable from
a dropped copy).

---

## T2.7 — the prefab picks, and why — 2026-08-17

The registry resolves **one name to one prefab**; three of the five legacy sources were arrays. Every
pick, with its reason:

| Registry name | Legacy source | USSR pick | US pick | Why this one |
|---|---|---|---|---|
| `heavy_infantry` | `m_aHeavyInfantryPrefabSlots` (5 USSR / 4 US) | `Group_USSR_FireGroup` | `Group_US_FireTeam` | **Index [0]**, which is literally what `OVT_BaseUpgradeDefensePosition.BuyGuard` used (`m_aHeavyInfantryPrefabSlots[0]`, `:94`) — the defense-positions config is this name's main consumer. It is also the only *general-purpose* entry; the rest are role specialists (MG team, GL team, suppression team, maneuver group) and a name meaning "heavy infantry" should not silently resolve to a two-man GL team |
| `at_team` | `m_aGroupATPrefabSlots` (2 each) | `Group_USSR_Team_AT` (**index [1]**) | `Group_US_Team_LAT` (index [0]) | 🔴 **The USSR pick is deliberately NOT [0].** USSR's `[0]` is `Group_USSR_RifleSquad` — a plain rifle squad with no AT weapon, which looks like an authoring accident in the legacy array. The registry name promises AT and the array's other entry delivers it. US's `[0]` already IS the LAT team, so both factions now resolve `at_team` to an actual AT team. Recorded because it is the one place a prefab was NOT taken "verbatim from index 0" |
| `sniper` | `m_aGroupSniperPrefab` (single) | `Group_USSR_Sniper` | `Group_US_Sniper` | No choice to make |
| `sniper_team` | `m_aGroupSniperTeamPrefab` (single) | `OVT_Group_USSR_SniperTeam` | `Group_US_SniperTeam` | No choice to make |
| `bunker_team` | `SmallBunker` composition's `m_aGroupPrefabs` (1 each) | `Group_USSR_BunkerTeam` | `Group_US_BunkerTeam` | No choice to make |
| `car` | `m_aVehicleCarPrefabSlots[0]` | `UAZ469` | `M151A2` | Plan says `[0]` |
| `truck` | `m_aVehicleTruckPrefabSlots[0]` | `Ural4320_transport` | `M923A1_transport` | Plan says `[0]` |

**Costs.** Group costs are authored on the same scale as the three existing entries (`light_patrol`
10 by default, `light_fireteam` 15, `rifle_squad` 30): `heavy_infantry` 25, `at_team` 35, `sniper` 20,
`sniper_team` 30, `bunker_team` 20. **Vehicle costs are not invented** — they are the legacy
valuations exactly: `OVT_BaseUpgradeParkedVehicles.GetResources()` valued a car at
`baseResourceCost × 3` and a truck at `baseResourceCost × 6`, which on Normal (`baseResourceCost` 15)
is **45** and **90**. Composition costs are the legacy hardcoded checkpoint prices, 40 and 60
(`OVT_BaseUpgradeCheckpoints.c:12,21` and `:34,41`).

⚠ **Nothing reads `m_iCost` on either registry today.** `GetGroupCostByName` / `GetVehicleCostByName`
have **zero** callers tree-wide; what actually charges the pool is each module's own
`m_iCostPerGroup` / `m_iCostPerVehicle`. The registry costs are authored to be right rather than to
be read, so a future consumer inherits sane numbers.

⚠ **`GetRandomVehiclePrefab()` / `GetRandomGroupPrefab()` also have zero callers**, which is why
appending `car` and `truck` (both at the default `m_iWeight 1`) is safe today. If a future feature
ever picks a *patrol* vehicle at random from the whole registry it will now sometimes draw an unarmed
soft-skin. The fix at that point is a weight of 0 on the scenery entries, not a separate registry.
**Appending is otherwise provably safe**: every live consumer of `GetAvailableGroupNames()` takes the
FIRST resolvable name (`OVT_VirtualizationManagerComponent.ResolveDebugTestGroupName`,
`OVT_TEST_VirtualizationFixture.FindComposition`) or the first with ≥2 members, and appending cannot
change what comes first.

---

## Phase 2 design notes worth keeping

**A placed group's post is the index of its handle in `m_aHandles`, and that is the whole assignment
rule.** `ResolveSpawnPosition` uses `m_aHandles.Count()` (the index the handle is about to occupy,
because a group is only inserted on success), and both `OnGroupRegistered` and `OnGroupReclaimed` use
`m_aHandles.Find(handle)`. One rule, three call sites, no map to keep in step. The consequence — a
wipe that removes a handle from the middle shifts the survivors one post along — is deliberate and
harmless: posts within one concern are interchangeable and it is core's survivor mask, not the post
index, that remembers *who* is alive. Pinning a post to a handle across a load would need a persisted
map, which D7/D10 forbid.

**The provider is queried at most once per convergence pass**, and the invalidation point is
`ReclaimHandles` rather than `EnsureGroups`, because `ReclaimHandles` is the first thing
`ConvergeGroups` does on **both** ways in (activation and the reinforcement rebuy). Invalidating in
`EnsureGroups` would have left a rebuy reading a stale post list. Without the cache a placed module
would fire one sphere query per held group per 10 s tick.

**`OnCleanup` needed a releasing latch.** The base class's `OnCleanup` calls `ReclaimHandles` before
it unregisters — which fires `OnGroupReclaimed` and would have re-hooked every group on the way out.
`m_bReleasing` suppresses that, and the unhook runs after `super.OnCleanup()` off
`m_mHookedGroupByHandle` (kept separately from `m_aHandles` precisely because the base class clears
the handles). This matters because `UnregisterGroup` retires a group with held members **in place**
rather than deleting it, so a group entity can outlive the module that subscribed to it — integration's
D7 dangling-call hazard, reached from the other direction.

**`GetOnMembersDespawning()` is real and is what resets the arrival counter** (`SCR_AIGroup.c:2288`,
invoked at `:2894` with the GROUP as its single argument — unlike `GetOnAgentAdded`, whose header lies
about its arity). `PlacementSlotForArrival` still wraps modulo the group's `m_aUnitPrefabSlots.Count()`
as a belt to that braces: a missed despawn notification would otherwise march each new arrival another
1.2 m sideways, forever.

**Two `OVT_DeploymentPlacementProvider` implementations answer NO heading (`vector.Zero` angles) and
one answers a full one.** Snipers face where their marker points, because that is the entire reason a
designer placed a curated marker. Tower guards do not: a CoverPost sentinel's authored look direction
points *into* the cabin's firing slit, which is the exact place `WALKWAY_OFFSET` exists to get the
guard out of, so adopting it would undo the offset. Legacy used `Math3D.MatrixIdentity4` for the tower
guard for the same reason, and AI turns to face what it perceives anyway.

**`ReleaseAction()` housekeeping is DELIBERATELY NOT PORTED.**
`OVT_BaseUpgradeTowerGuard.ReleasePosts` (`:215-231`) walked a tower's smart actions on despawn and
force-released any that were inaccessible, because "a reserved post is only auto-released by the
action ending or its user dying — deleting a proxied guard would leave the post inaccessible forever
and block every future BuyGuard on this tower". **The new guards never take a smart action.** The
tower provider *reads* the sentinel to find out where the walkway is and never occupies it, and the
config gives these groups no waypoint at all (legacy parity — an idle group keeps full threat and
attack reactions, which is the configuration proven to engage). With no reservation there is nothing
to leak, so porting the release would be dead machinery guarding a state the new code cannot enter.
🔴 **This is a play-test item, not a proof:** during the §6 pass, check a base whose tower guards have
cycled through several despawn/re-materialise rounds and confirm the tower still yields a post —
`IsActionAccessible()` returning false on every sentinel of a tower would show up as that tower
silently dropping out of `ResolvePlacements` and its guard never coming back. If that is ever seen,
the fix is to port `ReleasePosts` onto the placed module's despawn hook, which already exists
(`OnPlacedGroupDespawning`).

**Deliberate divergences from the plan, all small, all deliberate:**
1. **`OVT_ParkedVehicleSpawningDeploymentModule` has a FOURTH attribute, `m_eParkingType`.** The plan
   lists three. A parking spot is sized by the building (`OVT_ParkingComponent.GetParkingSpot(mat,
   type)`) and legacy hard-coded `PARKING_CAR` in `BuyCar` and `PARKING_TRUCK` in `BuyTruck`; without
   the attribute every truck would ask for a car spot, find none and the base would park nothing while
   logging nothing. Inferring it from the registry name would be a magic string.
2. **Checkpoints now man their turrets.** `SpawnDefaultOccupants({TURRET})` is applied for all six
   slot sizes, road included, at 7/15/23 m by size. Legacy `OVT_BaseUpgradeCheckpoints` never
   implemented `Setup()`, so its checkpoints' guns stood empty. The plan's T2.4 says "7/15/23 m by
   slot size" without carving out the road sizes, and a checkpoint with a crewed gun is what a
   checkpoint is for.
3. **The composition module ignores `OVT_FactionComposition.m_aGroupPrefabs`.** Only `m_aPrefabs` and
   `m_iCost` are read. A guard here is a registered, virtualized group and must come from the faction
   GROUP REGISTRY via the module's inherited `m_sGroupType` — core resolves `(factionKey, groupName)`
   and never a raw prefab (`api.md` §3). The `SmallBunker` group prefab is preserved as the new
   `bunker_team` registry entry, so nothing is lost. Stated in the attribute's header.
4. **The snap-ON half of the Init snap case is PRINTED, not asserted.** What road-snapping does
   depends on a road network the test world does not guarantee, and the plan's own wording is that
   snap ON "does not necessarily" stay inside the radius. The case asserts the OFF half hard (exact
   anchor altitude + horizontal distance in the roll's own 10..radius band, over 12 samples) and logs
   two diagnostic lines saying whether this world can distinguish the two paths at all — which is
   exactly what the recorded fail proof depends on. Asserting it would have been a flake.

⚠ **A RESTORED composition deployment cannot re-find its own structure, and one narrow case notices.**
The module holds the composition as a runtime `EntityID` and nothing links a deployment to the
structure it built across a save (D7 forbids the payload change that would). On a restored
deployment `GetComposition()` is null, so `ResolveSpawnPosition` falls back to the deployment
position. **This is invisible for the restored guards themselves** — core re-creates them at their
persisted positions (at the composition) and `ReclaimHandles` finds them, so `ResolveSpawnPosition`
is never called for them; it is only called for the *shortfall*. The one case that notices is a
**reinforcement bought for a restored composition deployment**: that replacement group anchors on the
deployment marker rather than on the bunker, up to `baseRange` away, and with a DEFEND plan holds
there. Small, real, and the designed fix is D7's already-costed serializer version 3 (append the
built slot positions). **Not built** — YAGNI until a play-test says otherwise. Watch for it in §6.

**D6's reinforcement cast is satisfied by construction and was re-verified.**
`OVT_ReinforcementBehaviorDeploymentModule.GetMissingUnitsCount()` casts to
`OVT_InfantrySpawningDeploymentModule` (`:186-197`); both new group-holding modules subclass it, so
both are rebuyable. `OVT_ParkedVehicleSpawningDeploymentModule` deliberately is **not** — it holds no
groups and has nothing to rebuy, which is why its config authors no reinforcement module (§3.2 row 9
already says so).

**A test-only subclass was needed and is the right shape.**
`OVT_TEST_SnapProbeInfantryModule` (declared in the Init suite, **not** `[BaseContainerProps]`) exists
solely to reach the protected `ResolveSpawnPosition`. Widening the production method to public for a
test would have changed the class's contract to make the test easy; a subclass is what the seam is
for. Contrast Phase 1, which *did* widen `FindBestDeploymentConfig` to public — that was a method the
evaluator's headline claim could only be asserted by calling directly, and it was recorded as a
deliberate widening.

**Fixture footprint of the four new Init cases: NONE.** None registers a group, creates a deployment,
spawns an entity or mutates a registry. Three are pure reads of shipped configs / bare `new` objects;
the provider case runs read-only sphere queries at a position ≥2 km clear of every town, base and
tower. `grep -rn "RegisterGroup(" Scripts/Game/Tests/` is unchanged by this phase, and there is
nothing for the movement tick to walk.


---

## 2026-08-18 — PHASE 3 BUILT (T3.0–T3.7). Compile `0`. Suite run owed.

| Task | What landed |
|---|---|
| **T3.0 (S1)** | `GetLocationTypeAtPosition()` ORs the `BASE` bit in via a new `IsNearBaseCentre()` against a new `BASE_CLASSIFICATION_RADIUS = 250`. The precedence chain (`GetPrimaryLocationTypeAtPosition`, including its own 500 m base branch) is **byte-untouched** — the OR sits beside the existing `RADIO_TOWER` one, in the consumer-facing method, so every gate that asks the classification benefits at once (the evaluator's `CanUseLocationType` filter at `:428`, the free-seed pass at `:1049`, and the T1.7 Init case) |
| T3.1 | `Deployment_BaseGarrisonPatrol.conf` — `light_patrol`, `m_bSnapToRoad 0`, `m_fSpawnRadius 50`, 1–2 groups, PERIMETER r 280, Reinforcement (`m_bDeleteOnConditionFail 1`), BaseControl (500 / require), NoPlayersNearby. `OCCUPYING_FACTION` / `BASE` / prio 1 / chance 100 / maxInstances -1 / `m_bFreeAtGameStart 0` |
| T3.2 | `Deployment_BaseHeavyPatrol.conf` (`heavy_infantry`, threat 25, prio 5) and `Deployment_BaseATSection.conf` (`at_team`, threat 50, prio 6) — thin variants, identical in every other line |
| T3.3 | Three entries appended to `overthrowDeployments.conf` in the Tower Garrison shape (inheritance line, empty override body). GUID prefix `6AB2D3E1`, grep-verified unused repo-wide before use |
| T3.4 | `OVT_BaseUpgradeDefensePatrol.c` deleted **with** its `overthrowBaseUpgrades.conf` entry — `git diff` on that conf is exactly one entry removed |
| T3.5 | `OVT_BaseUpgradeTownPatrol.c` deleted; `OVT_OccupyingFactionManager.RecoverResources(int)` deleted with it (verdicts below) |
| T3.6 | 2 Init cases: `OVT_TEST_Init_Deployments_BasePatrolConfigsCyclePerimeter`, `OVT_TEST_Init_Deployments_TownShadowedBaseAcceptsBaseConfig` |

**Compile verified by negative control**, the Phase 2 way: a deliberate undefined symbol appended to the
Init suite reported at the right file and line and exited 1; removed, re-run, exit 0.

### T3.0 — where S1 landed, and why THERE

`GetLocationTypeAtPosition()`, not `GetPrimaryLocationTypeAtPosition()`. The precedence chain is a
first-match chain whose ordering four shipped configs already depend on; inserting a base test into it
would have changed what a town centre near a base classifies **as**, not just what it also carries.
The OR-in adds a bit and moves nothing, which is the same argument `integration` made for
`RADIO_TOWER` and is why both fixes sit on the same three lines. Every production consumer that gates
on location type calls the outer method (`:428` the evaluator filter, `:1049` the free-seed pass), so
one edit covers them all; the inner method stays reachable purely so "the bit was OR-ed in, nothing
was replaced" remains assertable.

⚠ **The radius equality is the safety argument and is now pinned in three places** — the constant's own
header, the method header, and `EXPECTED_RADIUS` in the new Init case. Any position that gains the bit
is ≤250 m from the base centre, which is exactly `HasExistingDeploymentOfType()`'s dedup reach, so the
dedup always sees the base's own copy of a config and refuses a second. **Raising this constant
re-opens force doubling** at every base whose town centre falls between the old and new radii (Levie's
is at 460 m). It is not a tuning knob.

**Side effect worth knowing:** the T1.7 case's printed shadow count now reads **0** in the Init world
(its base is 114 m from its town, so the centre gains the bit). The count was a diagnostic for the
problem S1 fixes; it is not an assertion and does not go red.

### T3.5 / T3.4 — the retired-symbol grep verdicts

`grep -rn "OVT_BaseUpgradeDefensePatrol\|OVT_BaseUpgradeTownPatrol\|RecoverResources" Scripts/ Configs/`:

| Hit | Verdict |
|---|---|
| `OVT_BaseUpgradeDefensePatrol` | **ZERO** anywhere. The class, its conf entry and its two `Scripts/Game/Controllers/README.md` lines are gone |
| `OVT_BaseUpgradeTownPatrol` | **ZERO** anywhere. The comment at `OVT_GMRecords.c:61` named it as an example `m_sType` value and now names `OVT_BaseUpgradeCheckpoints` instead — a class that still exists, so the example stays true until Phase 5 |
| `OVT_OccupyingFactionManager.RecoverResources(int)` | **DELETED.** Its only caller was `OVT_BaseUpgradeTownPatrol.c:96`, which went with it. Nothing else in the tree called it |
| `OVT_MultiTownPatrolBehaviorDeploymentModule.RecoverResources()` (`:491`) + `m_bRecoverResourcesOnComplete` | 🟢 **SURVIVOR, UNRELATED AND JUSTIFIED.** A `protected` method on a deployment behaviour module that credits the **deployment pool** via `manager.AddFactionResources()` — it never touched `m_iResources` and never called the manager method that was deleted. Same name, different class, different economy |
| Three comments naming `RecoverResources` (`OVT_DeploymentManager.c:1289`, `:1343`, `OVT_TEST_PersistenceRoundTripSuite.c:5340`) | 🟢 **SURVIVORS, CORRECT.** All three are about the surviving module method above (they explain why `m_iResourcesInvested` must be stamped) |

### T3.2 — the deliberate divergence from legacy

`OVT_BaseUpgradeDefensePatrol.Spend()` chose its group type as
`if (GetNumGroups() == 0 || threat > 50) → ANTI_TANK; else if (threat > 25) → HEAVY_INFANTRY;`. The
`GetNumGroups() == 0` half meant **the first group bought at every base was an AT team regardless of
threat**, on a brand-new campaign with zero threat, forever — a base's opening defense was an AT
section and its later groups were light infantry. That reads as an authoring accident, not a design:
nothing else in the legacy system escalates *downwards*, and the AT branch's own second condition
(`threat > 50`) states the intended gate. **It is deliberately not reproduced.** The three configs
gate purely on threat: garrison patrol at 0, heavy at 25, AT at 50, which is the escalation the
requirements asked for. A base at zero threat now opens with light infantry.

**Two smaller authoring decisions, recorded because they are invisible in the diff:**

1. **All three configs price a group at `m_iCostPerGroup 60`**, including the heavy and AT variants.
   That is legacy parity, not an oversight: `Spend()` charged `baseResourceCost × 4` (= 60 on Normal)
   per group **whatever type it bought**. The faction registry's own `m_iCost` values (`heavy_infantry`
   25, `at_team` 35) are not read by anything (T2.7's verdict), so pricing the variants higher here
   would have been a new balance decision taken inside a migration phase. Pacing is a play-test item.
2. **`m_iBaseCost 20`** on all three, matching the Tower Garrison config's shape. The legacy system had
   no per-upgrade base cost at all, so any value is new; 20 keeps the opening purchase cheap relative
   to the 60-per-group body while still being non-free, which `m_bFreeAtGameStart 0` requires to mean
   anything.

### Phase 3 design notes worth keeping

**The three configs are byte-identical apart from four lines** (`m_sDeploymentName`, module names,
`m_sGroupType`, `m_iMinimumThreatLevel`, `m_iPriority`). That is on purpose and should stay that way:
they are one concern at three threat tiers, and the escalation contract in §3.4 depends on their
priorities being the only thing that orders them. If a future tuning pass differentiates them, do it
in a way a reader can see at a glance — the diff between the files IS the documentation, since `.conf`
files cannot carry comments.

**`m_bSnapToRoad 0` is authored on all three**, per the Phase 2 gotcha. With a PERIMETER plan a snapped
garrison would still walk, so this one would not have been *visible* the way it is for the static
configs in Phases 4–5 — it would just have started its circle from up to 500 m off the base.

**Fixture footprint of the two new Init cases: NONE.** Neither registers a group, creates a deployment,
spawns an entity or mutates a registry. One reads shipped config templates and asks the patrol module
for a plan (no marker, no `UpdateDeployment` leak — the `TownPatrolPlanCycles` shape); the other runs
pure classification queries against live campaign data. `grep -rn "RegisterGroup(" Scripts/Game/Tests/`
is unchanged by this phase.

**The S1 case carries a real negative control**, and it is the half that would catch a future radius
raise: a probe 260 m from the base centre taken **toward** the town (so the precedence chain still
answers TOWN there, and a BASE bit could only have come from the OR-in) must carry no BASE bit. Where
a world's geometry cannot produce such a probe the control prints a line and stands down rather than
asserting something else — the Phase 2 snap-case discipline.

---

## 2026-08-18 — PHASE 4 BUILT (T4.1–T4.10). Compile `0`. Suite run owed.

| Task | What landed |
|---|---|
| T4.1 | `Deployment_BaseDefensePositions.conf` — placed module + `OVT_BaseDefendPositionPlacementProvider`, `heavy_infantry`, `m_iMaxGroupCount 5` (= `defenseGroupsBaseMax`), `m_iCostPerGroup 60` (= `baseResourceCost × 4`), `m_fSearchRadius 280`; `OVT_PatrolBehaviorDeploymentModule` **DEFEND**; Reinforcement (`m_bDeleteOnConditionFail 1`); BaseControl (500 / require); NoPlayersNearby. `OCCUPYING_FACTION` / `BASE` / prio **2** / chance 100 / maxInstances -1 / `m_bFreeAtGameStart 0` |
| T4.2 | `Deployment_BaseTowerGuards.conf` — placed module + `OVT_TowerCoverPostPlacementProvider`, `sniper`, `m_eImportance HIGH`, cost **15** (= `baseResourceCost`), max 4 groups. **No behaviour module at all** beyond Reinforcement. Prio 2 |
| T4.3 | `Deployment_BaseSniperPositions.conf` — placed module + `OVT_SniperMarkerPlacementProvider`, `sniper_team`, HIGH, cost **30** (= `baseResourceCost × 2`), max 4 groups. No behaviour module. Prio 2 |
| T4.4 | Three entries appended to `overthrowDeployments.conf` in the Phase 3 shape (inheritance line, empty override body). GUID prefix **`6AB4E5F2`**, grep-verified unused repo-wide before use |
| T4.5 | `OVT_BaseUpgradeDefensePosition.c`, `OVT_BaseUpgradeTowerGuard.c`, `OVT_BaseUpgradeSniperPosition.c` **deleted with their three `overthrowBaseUpgrades.conf` entries**. Six surviving mentions elsewhere rewritten (table below) |
| T4.6 | Four pure statics on `OVT_PlacedInfantrySpawningDeploymentModule` — `PostForGroup`, `SlotForArrival`, `ArrivalTransform`, `PlacementForArrival` — plus a public instance `ResolvePlacements(position, radius, factionIndex)`. **The production path was re-routed through them**, it is not a parallel copy |
| T4.7 | 3 Init cases: `..._PlacedBaseConfigsHoldTheirPosts`, `..._PlacedArrivalPlacementIsStable`, `..._SniperMarkerThreatGateFilters` |
| T4.8 | 1 Persistence case on the shared gate: `OVT_TEST_PersistenceRoundTrip_DeploymentBaseDefense_SurvivesSaveAndReapply` |

### T4.5 — the retired-symbol grep verdicts

`grep -rn "OVT_BaseUpgradeDefensePosition\|OVT_BaseUpgradeTowerGuard\|OVT_BaseUpgradeSniperPosition" Scripts/ Configs/` → **EMPTY**. Six live mentions had to be re-pointed to get there, and each is a decision rather than a find-and-replace:

| Hit | Verdict |
|---|---|
| `Configs/BaseUpgrades/overthrowBaseUpgrades.conf:3-14` (three entries) | **Deleted with the classes**, same change-set. `git diff` on that conf is exactly three entries removed; the file now holds **six** entries (ParkedVehicles, Checkpoints, three Compositions, Specops) |
| `Scripts/Game/Controllers/README.md:40-41` | **Deleted.** Same treatment as Phase 3 gave `OVT_BaseUpgradeDefensePatrol`'s two lines |
| `OVT_BaseControllerComponent.c:239` — *"Towers are handled by OVT_BaseUpgradeTowerGuard"* | **Re-pointed at `OVT_TowerCoverPostPlacementProvider` / `Deployment_BaseTowerGuards.conf`.** ⚠ **The exclusion this comment justifies is load-bearing and must NOT be removed with the class**: `FindSlots()` skips `MDT_TOWER` sentinels so they never enter `m_aDefendPositions`, and the defend-position provider reads exactly that list. Delete the skip and every tower is manned twice — once by the tower provider, once by the defend-position provider |
| `OVT_SniperPositionComponent.c:2` | **Header rewritten** to name the provider and the config, and to say out loud that the provider is now `m_iMinimumThreat`'s **only reader** |
| `OVT_TowerCoverPostPlacementProvider.c`, `OVT_SniperMarkerPlacementProvider.c`, `OVT_BaseDefendPositionPlacementProvider.c`, `OVT_PlacedInfantrySpawningDeploymentModule.c` (7 citations) | **Reasons kept verbatim, dead `file:line` citations replaced** with a pointer to `implementation.md` §3.3, which preserves them. A citation into a deleted file is worse than no citation: it reads authoritative and cannot be checked |
| `OVT_GMIconFormat.c:73` + `OVT_TEST_Logic_GMIconFormat.c:53` | 🔶 **DEVIATION — re-pointed at `OVT_BaseUpgradeParkedVehicles` / `"Parked Vehicles"`.** Both were illustrative strings for a pure string transform, not references; the plan's T7.9 says this test "needs no change", but Phase 4's acceptance criterion says the grep must be **empty** and the criterion wins. Two words in, two words out, so the assertion's strength ("the prefix goes and the words separate") is unchanged. ⚠ **Phase 5 deletes that class too and will have to re-point again**, and Phase 7 finally — at which point the honest end state is a shape example that names no class, the way `OVT_TEST_Logic_GMIconFormat.c:60` already uses `"OVT_BaseUpgradePatrols"` |

`grep -rn "m_ProxiedGroups\|m_iProxedResources\|m_ProxiedPositions" Scripts/` → **21 hits, all in `OVT_BasePatrolUpgrade.c`** (was 39 across four files). Checkpoints, Composition and Specops still subclass it but contain **no direct hit of their own**, so the count strictly decreased and the surviving file is the single one the criterion allows.

### T4.6 — the shape that was actually built, and the one signature that changed

The plan asked for `ResolvePlacements(position, radius, factionIndex, threat)` and `PlacementForArrival(groupIndex, arrivalIndex)`. What shipped:

```
array<ref OVT_DeploymentPlacement> ResolvePlacements(vector position, float radius, int factionIndex)   // instance
static OVT_DeploymentPlacement PostForGroup(posts, groupIndex)
static int  SlotForArrival(arrivalIndex, spread)
static void ArrivalTransform(post, arrivalIndex, spread, out mat[4])
static vector PlacementForArrival(posts, groupIndex, arrivalIndex, spread)
```

Two deliberate deviations:

1. **No `threat` parameter, and that is a behavioural argument rather than a convenience one.** Threading threat in from the module would mean the module resolving it, and the only threat a module has is `m_ParentDeployment.GetThreatLevel()` — a **snapshot taken when the deployment was created and persisted with it**. The sniper gate would then freeze a base's coverage at whatever the campaign looked like the day the deployment appeared, which is the exact opposite of the provider's own documented promise that "a base grows sniper teams as threat rises". The provider keeps reading the live occupying threat through its own `GetOccupyingThreat()`, and the Init case reaches it with a test subclass (the Phase 2 `OVT_TEST_SnapProbeInfantryModule` precedent) instead.
2. **`PlacementForArrival` takes the posts and the spread as arguments.** It has to: the plan's own justification for the seam is that the claim be assertable "without a live deployment marker", and a two-argument form would have to read module state that only a live deployment can populate. `MEMBER_SPACING` and `FALLBACK_SPREAD` were widened from `protected const` to `static const` so the case asserts against the production numbers rather than copies of them.

⚠ **These are not a parallel implementation.** `AssignPost()`, `ResolveSpawnPosition()` and `OnPlacedAgentAdded()` were re-routed through them in the same change; `EnsurePosts()` is now nothing but the per-pass cache plus the deployment's own position and faction. A second implementation that agreed on the day it was written and drifted afterwards would be worse than no test.

### T4.1–T4.3 — the config authoring decisions that are invisible in a diff

1. **`m_fSpawnRadius 0` on all three, and it is honest rather than lazy.** `m_fSpawnRadius` is read by exactly one method, `GetRandomSpawnPosition()`, which a placed module **never calls** — `ResolveSpawnPosition` is fully overridden and its only fallback is the bare deployment anchor. Zero reads as "this module does not roll a ring", which is the truth. `m_bSnapToRoad 0` is authored for the same defensive reason even though the snap is equally unreachable: it costs one line and makes a future class swap (placed → plain infantry) safe instead of silently moving a garrison up to 500 m onto a road. The Init case asserts it.
2. **`m_iMinGroupCount 1` is authored but ignored.** `CalculateGroupCount` on the placed module answers `min(posts, m_iMaxGroupCount)` and deliberately skips both the floor and the difficulty band — you cannot man three posts when the world offers two. Authored anyway so the value a reader sees is not the class default of 1 by accident.
3. **`m_iMaxGroupCount 4` on tower guards and sniper positions is a NEW number.** Legacy had no cap at all: it manned every tower and every marker it found within `baseRange`, one group each. 4 is a ceiling, not a target — the provider almost always offers fewer — and it exists so a modded map with fifteen towers around one base cannot buy fifteen sniper groups in one purchase. **Defense positions is capped at 5, which is `defenseGroupsBaseMax` and therefore legacy parity.** If a play-test finds a base whose towers are not all manned, this is the number.
4. **`m_fPatrolRadius 0` on the defense-positions behaviour module** — matching what `BuildDefendPlan(centre, 0)` is actually called with. The DEFEND branch ignores the attribute entirely; zero is what the code uses and 200 (the class default) would be a number that means nothing.
5. **Neither `m_fInitialDelay` nor `m_fCheckInterval` is authored on the reinforcement modules**, matching Phase 3. That leaves the class defaults 300 000 ms / 60 000 ms — and the 300 s initial delay is **what makes the T4.8 persistence fixture safe** despite `m_bDeleteOnConditionFail 1`. A future tuning pass that shortens it will start that case failing intermittently; the header says so and says the fix is to pick a different config, not to lengthen the timeout.
6. **`m_iBaseCost 20` on all three**, the Phase 3 shape. Legacy had no per-upgrade base cost, so any value is new.

### T4.7 / T4.8 — what the four new cases actually claim

| Case | Tier | The claim, and the thing it would catch |
|---|---|---|
| `..._PlacedBaseConfigsHoldTheirPosts` | Init (Fast) | All three resolve + `IsValidConfig` + priority 2 + BASE bit + a placed module with a provider + the authored importance + `m_bSnapToRoad 0`; **tower/sniper build NO plan, defense positions builds a one-point non-cycling DEFEND plan**. The plan is resolved through the same walk production uses (every behaviour module in order, first non-null wins) rather than by counting modules — the reinforcement module IS a behaviour module, and a case that only looked for a patrol module would pass if some future behaviour module started answering a plan |
| `..._PlacedArrivalPlacementIsStable` | Init (Fast) | Six claims on the pure statics: post selection wraps; **two materialisations agree**; a missed despawn notification wraps instead of marching men sideways forever; the step is exactly `MEMBER_SPACING`; a post's authored heading turns the step with it (a spotter placed one metre "east" of an east-facing marker stands in front of the sniper); negative indices cannot read off the front of the array |
| `..._SniperMarkerThreatGateFilters` | Init (Fast) | The per-marker gate, **at its boundary**: below → manned, **equal → manned**, above → withheld, and the placement carries the marker's own rotation. An off-by-one flip to `<=` leaves every always-on marker (`m_iMinimumThreat 0`, the authored default) unmanned at threat 0 on a brand-new campaign — the most player-visible break and invisible to a middle-of-range test |
| `..._DeploymentBaseDefense_SurvivesSaveAndReapply` | Persistence (All) | The five persisted values + `FindConfigByName` + **two things the existing Town Patrol case cannot reach**: the restored deployment still carries a live placed module **with its `m_Placement` provider** (the `CloneModule` trap, on the one attribute whose loss is completely silent), and `WasRestoredFromSave()` is true — D7's gate, which **nothing else in the tree asserted** |

🔴 **The T4.8 seam limit is restated in the case header and NOT worked around.** `ReapplyLatestSaveData` passes `Instances = {gameMode}` only, so a deployment marker's `Deserialize` is never re-run by it. The case takes a **real save** over a fixture created through the manager's own creation path (the write half is genuine) and asserts the **restore half** through `ApplyPersistedDeployment()`, exactly as integration's T7.2–T7.4 do. Widening the seam would mean naming persistence-framework types inside `Scripts/Game/Tests/`, which the suite's assertion rule forbids.

### T4.9 — `RegisterGroup(` fixture sweep, FULL RE-SWEEP

**Total unchanged at 19 hits (18 call sites + 1 comment), and every line number is identical to integration's T7.1 sweep** — nothing moved, so those verdicts carry, re-verified rather than copied. **Phase 4 adds no registration site at all.**

Safety grounds, as integration defined them: **(a)** the plan has nothing movable in it, so the movement tick cannot walk the group; **(b)** the registration is released inside the same `Execute()` frame.

| Site | Case | Plan | Verdict |
|---|---|---|---|
| `Init:3540`, `Init:3562` | `Virtualization_RegisterRefusesUnknownComposition` | — | **safe** — both registrations are *refused* (`-1`); no record is booked |
| `Init:3746` | `Virtualization_RegisterBuildsDormantGroup` | `null` | **safe** by (a) **and** (b) |
| `Init:3903`, `Init:3905` | `Virtualization_GetAllHandlesEnumeratesRegistry` | omitted → `null` | **safe** by (a) **and** (b) |
| `Init:4223` | `VirtualMovement_TickAdvancesDormantGroup` | PATROL, movable | **safe, and walked ON PURPOSE** — being walked is the case's subject; `spawnDistanceOverride = 0` |
| `Init:4418` | `VirtualMovement_StationaryPlanIsNeverAdvanced` | DEFEND | **safe** by (a); `spawnDistanceOverride = 0` |
| `Init:4621` | `VirtualMovement_ManagerResolvesAndDoesNotLeak` | DEFEND | **safe** by (a); `spawnDistanceOverride = 0` |
| `Init:4765` | `Virtualization_WaypointsAreOwnedAndDeleted` | PATROL + MOVE, cycling | **safe by (b) only** — genuinely movable, but unregistered in the same frame and asserts nothing about position |
| `Init:4910` | `Virtualization_DeathsFlipMaskAndWipeRecord` | omitted → `null` | **safe** by (a) + (b) |
| `Init:5098` | `Virtualization_MaskDrivesSlotSelection` | omitted → `null` | **safe** by (a) + (b); deliberately materialises a member, which the movement tick's `IsSpawned` gate skips |
| `Init:7928` | `Deployments_EnsureGroupsIsIdempotent` | `null` | **safe** by (a) **and** (b); `SPAWN_DISTANCE_NEVER` (= 0, Manual policy) — re-verified this phase |
| `Init:8418` | `Deployments_TowerCaptureOnlyOnRealWipe` | `null` | **safe** by (a) **and** (b); `SPAWN_DISTANCE_NEVER`, importance HIGH — re-verified this phase |
| `Persistence:3918` | `VirtualGroupsWiped_DoNotComeBack` — the wiped group | `null` | **safe** by (a); asserts absence, never position |
| `Persistence:4017` | same case — the resurrection group | `null` | **safe** by (a) |
| `Persistence:4165` | *(not a call site — the sweep note in a case header)* | — | — |
| `Persistence:4420` | `VirtualGroups_SurviveSaveAndReload` — the BOGUS group | `null` | **safe** by (a) |
| `Persistence:4648` | same case — the save/reload fixture | DEFEND ×2, cycling | **safe** by (a) — the types were changed to DEFEND by movement's T3.1 for exactly this reason; **do not revert them to PATROL** |
| `Persistence:6253` | `DeploymentOwnedGroups_ReclaimAfterReload` | `null` | **safe** by (a); `SPAWN_DISTANCE_NEVER`, all handles released before the case reports on every path |
| **— (none)** | **`Deployments_PlacedBaseConfigsHoldTheirPosts` (NEW)** | — | **no footprint.** Pure reads of shipped config templates; asks behaviour modules for a plan off the template, the `TownPatrolPlanCycles` shape. Nothing created, registered or mutated |
| **— (none)** | **`Deployments_PlacedArrivalPlacementIsStable` (NEW)** | — | **no footprint.** Bare `OVT_DeploymentPlacement` objects and static calls. No world access at all |
| **— (none)** | **`Deployments_SniperMarkerThreatGateFilters` (NEW)** | — | **safe, one restored mutation.** Read-only sphere queries plus **one int written onto a world-authored marker's `OVT_SniperPositionComponent.m_iMinimumThreat` and restored on every path including the red ones**, inside a single `Execute()` with no yield between. After this phase the provider is that field's only reader. Where a world authors no marker the case prints and stands down |
| **— (no `RegisterGroup(`)** | **`DeploymentBaseDefense_SurvivesSaveAndReapply` (NEW, T4.8)** | — | **safe on both grounds.** It creates a **real deployment** (the second case in this suite to do so), so: (a) `MakeInert()` runs in the same statement block as `CreateDeployment` and marks the deployment **and every spawning module** eliminated — `InitializeDeployment` only arms a `CallLater`, it converges nothing synchronously, so no tick can fire in between; and (b) the reinforcement module's 300 s initial delay is longer than the case's entire 60 s budget, so its `m_bDeleteOnConditionFail` branch can never collect the fixture. Teardown runs on every path including the red ones |

### T4.10 — `ReleaseAction()` was NOT ported. Confirmed, unchanged, and here is the play-test that replaces it

Phase 2 recorded this; Phase 4 re-checked it against the class before deleting the class, and **the verdict stands**.

`OVT_BaseUpgradeTowerGuard.ReleasePosts()` walked a tower's smart actions on despawn and force-released any that were inaccessible, because *"a reserved post is only auto-released by the action ending or its user dying — deleting a proxied guard would leave the post inaccessible forever and block every future BuyGuard on this tower"*. **The new guards never take a smart action.** `OVT_TowerCoverPostPlacementProvider` *reads* the sentinel to find out where the walkway is and never occupies it, and `Deployment_BaseTowerGuards.conf` authors **no behaviour module**, so the group gets no waypoint and nothing ever calls `ReserveAction`. With no reservation there is nothing to leak; porting the release would be dead machinery guarding a state the new code cannot enter.

🔴 **THIS IS A PLAY-TEST ITEM, NOT A PROOF, AND IT IS THE ONE PHASE 4 CLAIM NO SUITE CAN REACH.** During the §6 pass:

1. find a base with a watchtower and let its tower guards materialise;
2. drive away past the 1750 m ring and back, **at least three times**, so the group de-materialises and re-materialises repeatedly;
3. after each round, confirm a guard is standing on the tower's open-air walkway — not on the ground beneath it, and not missing.

**The failure signature is the tower silently dropping out of the provider's answer**: `FindCoverPost()` requires `IsActionAccessible()`, so a leaked reservation makes the tower offer no post at all and its guard simply never comes back, with nothing in the log. If that is ever seen, the fix is to port `ReleasePosts` onto the placed module's despawn hook, which already exists (`OnPlacedGroupDespawning`).

### Phase 4 design notes worth keeping

**The tower/sniper configs authoring no behaviour module is not an omission and reads like one.** A `.conf` cannot carry a comment, so the only thing standing between a future author and "helpfully" adding a DEFEND module is `OVT_PlacedInfantrySpawningDeploymentModule`'s class header (which now names all three configs and says which gets what) and `..._PlacedBaseConfigsHoldTheirPosts`, which goes red naming the module type that answered. Both were written for that reader.

**A DEFEND plan and a null plan are equally immobile, and the difference is what the AI does when it arrives.** Both are un-walkable by the movement tick, so "garrisons never wander" holds either way. The reason defense positions get DEFEND and tower guards get nothing is legacy parity in both directions: the legacy defense-position guard was given `SpawnDefendWaypoint(pos)`, and the legacy tower guard was deliberately given nothing because every available post waypoint parks him at a smart action that is a pose loop with no fire node.

**The two `HIGH` importances are the only thing standing between a tower guard and not existing.** `SCR_EAISpawnImportance` decides who wins the AI spawn budget on a busy server, and a guard that loses is indistinguishable in play from a guard whose placement failed. It is authored on two configs, copied in `CloneModule`, and now asserted in two places (the clone case from Phase 2, the authored value from Phase 4).

**Compile verified by negative control**, the Phase 2/3 way: a deliberate undefined symbol appended to the Init suite reported at the right file and line and exited 1; removed, re-run, exit 0. The check's 5 s runtime and constant "6137 files" line look like a cache and are not. (6140 → **6137** this phase: three files deleted.)

---

## 2026-08-18 — PHASE 5 BUILT (T5.1–T5.8). Compile `0`. Suite run owed.

| Task | What landed |
|---|---|
| T5.1 | `Deployment_BaseCheckpoints.conf` — **two** composition modules, `LargeCheckpoint` on `ROAD_LARGE` and `MediumCheckpoint` on `ROAD_MEDIUM`, each with one `light_patrol` guard at `m_iCostPerGroup 60` and `m_bSnapToRoad 0`; `OVT_PatrolBehaviorDeploymentModule` **DEFEND**; Reinforcement (`m_bDeleteOnConditionFail 1`); BaseControl (500 / require); NoPlayersNearby. `OCCUPYING_FACTION` / `BASE` / prio **3** / chance 100 / maxInstances -1 / `m_bFreeAtGameStart 0` |
| T5.2 | `Deployment_BaseFortifications.conf` — **three** composition modules on `SMALL` slots: `SmallBunker` + a `bunker_team` guard (cost **45**), `AmmoCache` with `m_bFillAmmoBoxes 1` and **no** guard, `MGNest` with no guard. Reinforcement + BaseControl + NoPlayersNearby, **no behaviour module at all** (§3.2 row 8). Prio **4** |
| T5.3 | `Deployment_BaseParkedVehicles.conf` — `truck` ×1, `m_iCostPerVehicle 90`, **`m_eParkingType PARKING_TRUCK`**, BaseControl + NoPlayersNearby, **no reinforcement module**. Prio **10** |
| T5.4 | Three entries appended to `overthrowDeployments.conf` in the Phase 3/4 shape (inheritance line, empty override body). GUID prefix **`6AB5F6A3`**, grep-verified unused repo-wide before use. `ls Configs/Deployment/Deployment_Base*.conf` → **9**; `grep -c` in the registry → **9** |
| T5.5 | The four legacy classes **deleted with their five conf entries**. `Configs/BaseUpgrades/overthrowBaseUpgrades.conf` now holds **exactly one** entry, `OVT_BaseUpgradeSpecops`. Ten surviving mentions elsewhere re-pointed (table below) |
| T5.6 | `OVT_ECompositionBuildDecision` + `static DecideBuild(...)` + `protected ApplyBuildDecision(...)` on the composition module — **the production path was re-routed through them**, not copied. 1 Init case |
| T5.7 | `static RollFreeSlotIndex(slots, filled)` + `static ClaimSlot(filled, slotId)` — likewise re-routed, not copied. 1 Init case |

**Compile verified by negative control**, the Phase 2/3/4 way: a deliberate undefined symbol appended to
the composition module reported at the right file and line and exited 1; removed, re-run, exit 0. File
count 6137 → **6133** (four files deleted).

### T5.2 — the composition-tag verdict: NOTHING HAD TO BE ADDED

The plan warned that `SmallBunker` / `AmmoCache` / `MGNest` might be missing from the faction
composition registry. They are **all present on both shipped factions**, with the exact tag spellings
the config authors, and their `m_iCost` values are what the plan's cost arithmetic assumes:

| Tag | USSR | US | `m_iCost` | Guard prefab? |
|---|---|---|---|---|
| `SmallBunker` | `Bunker_S_USSR_01` | `Bunker_S_US_01` | **10** | yes — preserved as the `bunker_team` group-registry entry (Phase 2 T2.7), and the module ignores `m_aGroupPrefabs` on purpose |
| `AmmoCache` | `AmmoCache_S_USSR_03` | `AmmoCache_S_US_03` | **9** | no |
| `MGNest` | `MachineGunNest_S_USSR_01_PKM` | two US variants | **15** | no |
| `MediumCheckpoint` | `Checkpoint_M_USSR_01` | `Checkpoint_M_US_01` | **40** | no (Phase 2 added these two) |
| `LargeCheckpoint` | `Checkpoint_L_USSR_01` | `Checkpoint_L_US_01` | **60** | no |

**`git diff Configs/Factions/` is unchanged by this phase** — the only faction edits in the working
tree are Phase 2's nine appended registry entries per faction.

### T5.5 — the retired-symbol grep verdicts

`grep -rn "OVT_SlottedBaseUpgrade\|OVT_BaseUpgradeComposition\|OVT_BaseUpgradeCheckpoints\|OVT_BaseUpgradeParkedVehicles" Scripts/ Configs/` → **EMPTY**. Ten live mentions had to be dealt with:

| Hit | Verdict |
|---|---|
| `Configs/BaseUpgrades/overthrowBaseUpgrades.conf` (5 entries) | **Deleted with the classes**, same change-set. The file now holds **one** entry |
| `OVT_BaseControllerComponent.c:199` — `OVT_BaseUpgradeComposition.Cast(upgrade)` inside `FindUpgrade(type, tag)` | 🔶 **THE ONLY REAL CODE REFERENCE, and it is now a documented dead branch.** `tag` could only ever match the slotted-composition upgrade, so with that class gone the tagged lookup answers null for every input. Rewritten as an early `if(tag != "") return null;` with a header saying why. **Behaviour is identical** — the old loop also returned null once no composition existed in `m_aBaseUpgrades`. The parameter stays because `OVT_PersistedBaseUpgrade.tag` is a persisted field and `OVT_OccupyingFactionManager.c:725` still passes it; Phase 6 turns that replay block into a value refund and Phase 7 deletes the method |
| `Scripts/Game/Controllers/README.md:36,38,39,41` | **Deleted**, the Phase 3/4 treatment. The section now lists the three classes that still exist |
| `OVT_GMRecords.c:61` | **Re-pointed again.** Phase 3 moved it from `OVT_BaseUpgradeTownPatrol` to `OVT_BaseUpgradeCheckpoints`; it now names `OVT_BaseUpgradeSpecops` and says outright that it is the last one left |
| `OVT_DevAmmoBoxComponent.c:4`, `SCR_InventoryStorageManagerComponent.c:8` (the BUG-001 / vanilla-VME guard) | **Re-pointed at `OVT_CompositionSpawningDeploymentModule.FillAmmoBoxes`**, which is the same code doing the same unsafe-by-vanilla thing. ⚠ The modded guard is still load-bearing and must not be removed with the legacy class — the new module inserts through a box's own manager exactly as the old one did |
| `OVT_NavmeshRebuild.c:12` | **Re-pointed at `OVT_CompositionSpawningDeploymentModule`** — same reason, same call (`RebuildNow` on a freshly spawned structure) |
| `OVT_CompositionSpawningDeploymentModule.c` (7 citations) + `OVT_ParkedVehicleSpawningDeploymentModule.c` (3) | **Reasons kept verbatim, dead `file:line` citations replaced** with a pointer to `implementation.md` §3.3 — the Phase 4 rule. A citation into a deleted file reads authoritative and cannot be checked |
| `OVT_GMIconFormat.c:73` + `OVT_TEST_Logic_GMIconFormat.c:53` | 🔶 **RE-POINTED A THIRD TIME, and this time at nothing.** Phase 4 moved both from `OVT_BaseUpgradeTowerGuard` to `OVT_BaseUpgradeParkedVehicles` and predicted this. Both now use the synthetic **shape example** `"OVT_BaseUpgradeExampleName"` → `"Example Name"`: two capitalised words in, two words out, so the assertion strength is unchanged, and **no class deletion can invalidate it again**. Both places say so out loud. ⚠ **Phase 7 hand-off:** the literal `UPGRADE_PREFIX = "OVT_BaseUpgrade"` still lives in `OVT_GMIconFormat.c:23` and is unavoidable while the formatter exists, so Q4's `grep -rn "BaseUpgrade"` needs either that method deleted with the GM base-upgrade panel or a written exception for the prefix constant and its shape example |

`grep -rn "m_ProxiedGroups\|m_iProxedResources\|m_ProxiedPositions" Scripts/` → **21 hits, all in
`OVT_BasePatrolUpgrade.c`** — unchanged from Phase 4, because the four deleted classes carried no
direct hit of their own. `OVT_BaseUpgradeSpecops` is now its only surviving subclass.

**`OVT_SlottedBaseUpgrade` had exactly one subclass, grep-proven before deletion**: listing every
`^class` in the directory showed `OVT_BaseUpgradeComposition : OVT_SlottedBaseUpgrade` and nothing
else, and that class went in the same change-set. The directory now holds three files —
`OVT_BaseUpgrade`, `OVT_BasePatrolUpgrade`, `OVT_BaseUpgradeSpecops`.

### 🔴 T5.3 — PARKED VEHICLES ARE NEVER COLLECTED, AND THAT IS A TRAP FOR THE NEXT AUTHOR

`Deployment_BaseParkedVehicles.conf` authors **no `OVT_ReinforcementBehaviorDeploymentModule`**, and
that has a consequence nothing in the `.conf` can express (`.conf` files cannot carry comments):

> **`m_bDeleteOnConditionFail` lives INSIDE `OVT_ReinforcementBehaviorDeploymentModule.CheckReinforcement()`.**
> A deployment with no reinforcement module therefore has **no collection path at all** — its
> conditions are still evaluated, and failing them still stops it doing anything, but **nothing ever
> deletes it**. It persists for the life of the campaign.

For parked trucks that is exactly right and deliberate: there is nothing to rebuy, a truck the player
stole must not be replaced, and the module latches after its one parking pass anyway. But it means:

- **a base captured by the resistance keeps its parked-vehicle deployment marker forever.** The base
  control condition fails, so the deployment does nothing; it just never goes away. It counts against
  `m_iMaxDeploymentsPerFaction` (400 after Phase 1, so ~11 markers is noise) and it shows in the GM
  deployment list;
- **any future config that drops the reinforcement module inherits this silently.** If a later author
  wants "no rebuy" *and* "collect me when my conditions fail", the reinforcement module has to stay
  with `m_bEnableReinforcement 0` and `m_bDeleteOnConditionFail 1` — not be removed.

The note is repeated in `OVT_ParkedVehicleSpawningDeploymentModule`'s class header, which is the only
place a reader of that config will find it.

### T5.6/T5.7 — the composition path's persistence claim, decoded

Which entity is tracked, which is not, and what each consequence actually is. This is now also written
into the module's class header, because it is the whole D7 argument:

| Thing | Tracked by | Comes back from a save? | Consequence |
|---|---|---|---|
| The composition **entity** (bunker / cache / MG nest / checkpoint) | `OVT_PersistenceTracking.Track()` in the module + vanilla's `Composition.conf` prefab rule on `CompositionBase.et` | ✅ **yes**, before any deployment ticks | The structure is standing on load. The module must therefore build **nothing** — `DecideBuild(... restoredFromSave: true ...) = NEVER`, latched |
| The **slot claim** | `OVT_BaseControllerComponent.m_aSlotsFilled` → occupying-faction serializer → `InitBaseControllers()` restores it verbatim | ✅ **yes**, independently of any deployment | A new deployment at that base rolls for a slot and refuses every one on the list, so a legacy campaign's structures and a new deployment's structures coexist without overlapping. **This is the entire coexistence mechanism and T5.7 is its assertion** |
| The **guard group** | virtualization core, keyed on the deployment's owner key | ✅ **yes**, at its persisted position (which is the composition's) | `ReclaimHandles` finds it; the survivor mask means dead men stay dead |
| The **link** from this module to the structure it built (`m_CompositionId`) | **nothing — runtime only** | ❌ **no** | On a restored deployment `GetComposition()` is null and `ResolveSpawnPosition` falls back to the deployment anchor. **Invisible for the restored guards** (core re-creates them where they were, so `ResolveSpawnPosition` is never called for them) — visible **only** for a reinforcement bought FOR a restored composition deployment, which anchors on the marker instead of on the structure. Costed in D7 (serializer v3, append the built slot positions) and deliberately **not built** |
| The **turret crew** | nothing — `SpawnDefaultOccupants` runs once, at build time | ❌ **no** | ⚠ **Legacy re-manned turrets on load** (`Deserialize` called `Setup()`); the new module does not, because a restored deployment returns before it reaches `ManTurrets`. Whether the vanilla persistence of the composition brings its own crew back is a **play-test question** (§6 step 7): look at whether a restored MG nest still has a gunner. If it does not, the fix is one call — run `ManTurrets(GetComposition().GetOrigin())` on the restored branch — but it needs the link above to find the structure, which is why it is recorded here rather than guessed at |

### ✅ A DEFEND PLAN ANCHORED ON THE DEPLOYMENT MARKER, NOT ON THE POST — found in Phase 5, **FIXED in Phase 5** (orchestrator decision)

**MEASURED IN THE CODE, NOT SUSPECTED.** `OVT_PatrolBehaviorDeploymentModule.GetPatrolCenter()`
returns `m_ParentDeployment.GetPosition()` whenever there IS a parent deployment (`:186`); the
`groupPosition` fallback at `:75-76` only applies when there is none, i.e. off a config *template*.
`BuildDefendPlan(centre, 0)` puts the single DEFEND point at that centre, and core spawns the waypoint
there verbatim (`OVT_VirtualizationManagerComponent.CreatePlannedWaypoint`, surface-snapped only).

So for **any config whose spawning module places groups away from the marker**, the DEFEND waypoint is
at the base marker rather than where the men stand:

| Config | Where its groups are put | Where its DEFEND waypoint goes | Verdict |
|---|---|---|---|
| `Deployment_TowerGarrison.conf` (shipped, frozen) | at the marker | at the marker | ✅ correct — this is why nobody has seen it |
| `Deployment_BaseDefensePositions.conf` (**Phase 4**) | on `m_aDefendPositions` posts, up to `baseRange` 280 m out | at the base marker | 🔴 the guards are teleported to their posts and then told to hold the base centre |
| `Deployment_BaseCheckpoints.conf` (**Phase 5**) | on the checkpoint composition, on a road slot | at the base marker | 🔴 same |

Legacy did the opposite in both cases: `OVT_SlottedBaseUpgrade.AddWaypoints` used
`aigroup.GetOrigin()` and `OVT_BaseUpgradeCheckpoints.AddWaypoints` used the checkpoint's own origin.

**Originally deferred to the play-test; the orchestrator directed that it be fixed in Phase 5 instead**,
on the grounds that it breaks the feature's own "guards hold their posts" quality bar. It is fixed.

#### 🔴 THE VERDICT THE FIX DEPENDED ON WAS *NOT* "the tower garrison is unaffected"

The obvious fix — anchor DEFEND on `groupPosition` unconditionally — **would have regressed
`Deployment_TowerGarrison.conf`**, and the measurement that proves it was already in the tree.

That config authors **no `m_bSnapToRoad`**, so it takes the shipped default `1`: its groups are rolled
onto a ring 10–15 m from the marker and then handed to `OVT_WorldUtils.FindNearestRoad`, whose search
is **500 m wide and ignores `m_fSpawnRadius` entirely** (the attribute's own header records that
`integration` **MEASURED** a tower garrison registering on its access road instead of at its tower).
Today the marker-anchored DEFEND point silently *papers over* that: the men materialise on the road and
walk back to the tower. Anchor DEFEND on the group and they hold **the road**, permanently. Marker and
post do **not** reliably coincide for that config.

So the anchor cannot be a constant — it depends on whether the group's position was **chosen** or
**rolled**.

#### The change, in three parts

1. **`OVT_BaseSpawningDeploymentModule.StationsGroupsDeliberately()`** — new, returns **`false`** (the
   shipped answer). "Did this module choose each group's position, or roll one near the marker?"
2. **Overridden `true`** on `OVT_PlacedInfantrySpawningDeploymentModule` (positions come from a
   placement provider's post) and `OVT_CompositionSpawningDeploymentModule` (positions are the
   structure it just built). Nothing else overrides it.
3. **`OVT_PatrolBehaviorDeploymentModule`, DEFEND branch only:**
   `BuildDefendPlan(centre, 0)` → `BuildDefendPlan(holdPoint, 0)`, where `holdPoint` is `groupPosition`
   when `GroupsAreStationedDeliberately()` and `centre` otherwise. **PERIMETER and every other type are
   untouched**, and `GetPatrolCenter()` is still called in the same place for every type so no
   side-effect ordering moved. No new attribute, so `CloneModule` is unchanged.

`GroupsAreStationedDeliberately()` answers **true when there is no deployment at all** (a config
template), which keeps the template path identical to what every shipped plan-shape case already
asserts. It returns true if **any** spawning module says so — exact for every shipped config, since
none mixes a deliberate module with a rolling one; the limitation is written on the method.

#### Consumer verdicts (grep-verified, every `.conf` in `Configs/Deployment/`)

| Config | Patrol module | Type | Spawning module | Verdict |
|---|---|---|---|---|
| `Deployment_TowerGarrison.conf` (**shipped, frozen**) | 1 | **DEFEND** | plain infantry, **road snap ON** | ✅ **behaviour unchanged** — anchors on the marker exactly as before. This is the config the discriminator exists for |
| `Deployment_TownPatrol.conf` (**shipped, frozen**) | 1 | PERIMETER (class default `1`) | plain infantry | ✅ untouched — DEFEND branch not reached |
| `Deployment_VehiclePatrol_Light/Heavy.conf` (**shipped, frozen**) | **0** | — | vehicle | ✅ untouched — no patrol behaviour module at all |
| `Deployment_BaseGarrisonPatrol` / `_BaseHeavyPatrol` / `_BaseATSection` | 1 each | PERIMETER | plain infantry | ✅ untouched |
| `Deployment_BaseDefensePositions.conf` (**Phase 4**) | 1 | **DEFEND** | **placed** | 🎯 **FIXED** — guards now hold their defend positions instead of the base flag |
| `Deployment_BaseCheckpoints.conf` (**Phase 5**) | 1 | **DEFEND** | **composition** ×2 | 🎯 **FIXED** — guards hold their checkpoint |
| `_BaseTowerGuards` / `_BaseSniperPositions` / `_BaseFortifications` / `_BaseParkedVehicles` | **0** | — | placed / composition / parked | ✅ untouched — they author no behaviour module by design |

**No `.conf` was edited.** `git diff Configs/Deployment/` is still exactly the three appended registry
entries; the four shipped configs and all nine base configs are byte-identical to before the fix. The
only other `BuildVirtualPlan` implementor, `OVT_MultiTownPatrolBehaviorDeploymentModule`, never builds a
DEFEND plan; `BuildDefendPlan` has exactly one production caller.

#### It IS reachable from the Init tier — but only through a probe, and that matters

A config **template** cannot distinguish the two anchors: with `m_ParentDeployment` null,
`GetPatrolCenter()` returns `vector.Zero` and the existing fallback makes `centre` **equal to**
`groupPosition`, so both branches answer the same point. That is exactly why the shipped plan-shape
cases could not see this defect, and why a template-based assertion would have had **no can-fail
proof**. `OVT_TEST_Init_Deployments_DefendPlansHoldTheStation` therefore drives a probe subclass
(`OVT_TEST_DefendAnchorProbeModule`) that overrides the two protected inputs — the Phase 2/4 precedent —
and asserts **both directions**: a stationed group holds its own position (red if the fix is reverted),
a rolled group holds the marker (**red if the fix is made unconditional — the tower-garrison regression,
caught**), the template path is unchanged, and PERIMETER still builds a multi-point cycling plan.

⚠ One recorded honesty note: the template claim is a **regression guard with no independent fail
proof** — it holds by two routes at once — and the case header says so rather than inventing a proof.
PERIMETER's centre-dependence is deliberately **not** asserted geometrically, because
`SnapPatrolPointsToRoads` moves every corner onto a road up to 500 m away and any exact ring claim would
be a flake.

**Still owed to the play-test (§6 steps 4–6):** stand at a base with defense positions or a checkpoint,
let the guards materialise, watch for 30 s and confirm they stay on their posts; and at a radio tower,
confirm the garrison still ends up at the tower rather than on the access road. **The road snap on
`Deployment_TowerGarrison.conf` is still live and still unfixed** — this change stops it becoming
permanent, it does not remove it. The clean remedy remains one line (`m_bSnapToRoad 0` on that config),
and it needs whoever owns I1's byte-freeze to authorise unfreezing it.

### T5.1–T5.3 — the config authoring decisions that are invisible in a diff

1. **Guard prices are legacy `unit count × baseResourceCost`, per the Phase 4 convention.** Checkpoint
   guards `light_patrol` at **60** — the same price `Deployment_BaseGarrisonPatrol.conf` pays for the
   same group name, and what legacy `BuyPatrol` charged for the 4-man `Group_*_LightFireTeam` it used
   at checkpoints. Bunker guards `bunker_team` at **45**, because that prefab holds exactly **3**
   characters (MG / AMG / MG, counted off `Prefabs/Groups/OPFOR/Group_USSR_BunkerTeam.et`) and 3 × 15
   = 45. The faction registry's own `m_iCost` values are still read by nothing (T2.7's verdict), so
   pricing here is the only pricing.
2. **The composition's own cost is NOT authored in the config** — `GetResourceCost()` adds
   `OVT_FactionComposition.m_iCost` on top of the guards. So the totals are: Checkpoints
   20 + (60+60) + (60+40) = **240**; Fortifications 20 + (45+10) + 9 + 15 = **99**; Parked Vehicles
   20 + 90 = **110**. Changing a checkpoint's price means editing the faction config, not the
   deployment config, and that is worth knowing before hunting for a number that is not there.
3. **`m_iMaxGroupCount 0` on the ammo cache and the MG nest, with `m_sGroupType ""`.** `GetResourceCost`
   is `m_iMaxGroupCount × m_iCostPerGroup`, and `CalculateGroupCount` clamps to `[min, max]`, so 0
   means "structure only" through both paths without a special case. The empty group type is authored
   rather than left at the class default `light_patrol` so a reader cannot mistake it for a guard that
   failed to appear. `m_bAllowReinforcement 0` on both, for the same honesty.
4. **`m_fSpawnRadius 0` and `m_bSnapToRoad 0` on every composition module**, the Phase 4 argument
   verbatim: a composition module never calls `GetRandomSpawnPosition()` (its `ResolveSpawnPosition` is
   fully overridden and falls back to the bare anchor), so 0 reads as "this module does not roll a
   ring", and the snap opt-out costs one line and makes a future class swap safe.
5. **`m_iBaseCost 20` on all three**, matching Phases 3 and 4. Legacy had no per-upgrade base cost, so
   any value is new; 20 keeps the entry price cheap relative to the module bodies while still being
   non-free, which `m_bFreeAtGameStart 0` requires to mean anything.
6. **Fortifications authors NO behaviour module at all** (§3.2 row 8), so the bunker team gets no
   waypoint — the tower-guard shape, and the reason Phase 4 gave for it applies unchanged. It also
   means the bunker team is the one guard in this phase the DEFEND-anchor finding above does **not**
   touch.
7. **`m_eSlotType` uses the enum's real spellings `ROAD_LARGE` / `ROAD_MEDIUM`.** The plan's §3.2
   checkpoint row writes them `LARGE_ROAD` / `MEDIUM_ROAD`, which do not exist; Phase 2's context note
   flagged this and it was authored from the enum, not from the table.

### ⚠ TWO COMPOSITION MODULES ON ONE CONFIG MUST HAVE DIFFERENT `m_sModuleName` VALUES

`GetOwnerKey()` is `BuildOwnerKey(m_sModuleName)`, so the module NAME is the owner-key scope. Two
modules on one deployment sharing a name would each reclaim the other's guards through
`FindGroupsByOwner` on every convergence, and one of them would end up holding all of them while the
other rebought forever. The five new composition modules are named "Large Road Checkpoint", "Medium
Road Checkpoint", "Small Bunker", "Ammo Cache" and "MG Nest"; the rule is now in the module's class
header, because a `.conf` cannot carry the warning.

### T5.6 / T5.7 — what the two new cases actually claim

| Case | Tier | The claim, and the thing it would catch |
|---|---|---|
| `..._CompositionNeverBuildsTwice` | Init (Fast) | The **full truth table** of `DecideBuild` — template → SKIP, fresh → BUILD, already-attempted → NEVER, **restored → NEVER**, eliminated → SKIP, and **restored-AND-eliminated → NEVER** (the precedence row: if the eliminated SKIP won there, the reinforcement that clears the flag would build a second structure beside the restored one). Plus the **latch**, on a real module instance through a test subclass: NEVER records itself, SKIP does not, only BUILD returns permission, and BUILD does **not** latch (the production path latches later, once it has a slot, so a module that could not find its base controller yet keeps its free retry) |
| `..._CompositionSlotClaimsAreRespected` | Init (Fast) | Both halves of the coexistence mechanism: a claimed slot is **never** offered (invariant over 60 rolls with 2 of 3 claimed), a fully claimed base answers −1 on **every** roll rather than doubling up, an empty claim list **always** answers (the deterministic liveness row — without it a function that simply always refused would satisfy every other claim), and the **claim round-trip**: `ClaimSlot` records once, refuses a duplicate, and the slot is gone from the lottery afterwards. Plus four defensive rows (null/empty slot list, null claim list, null claim on `ClaimSlot`) — the empty guard is load-bearing because `RandInt(0, 0)` is an engine error |

**Both cases are structurally unable to be flaky.** No `maxAttempts`; the random half is asserted as an
invariant over samples plus a deterministic liveness row, never as "try until one passes".

### Fixture footprint of the two new Init cases: NONE

Neither registers a group, creates a deployment, spawns an entity or mutates any registry.
`..._CompositionNeverBuildsTwice` touches no world at all (bare module objects and static calls).
`..._CompositionSlotClaimsAreRespected` **reads** the world to borrow three distinct, valid `EntityID`
values (base markers first, then read-only `STATIC` sphere queries around the bases and towns) and
builds its own arrays from them — nothing is spawned, moved or modified, and where a world cannot
offer three distinct ids the case prints and stands down rather than asserting something weaker (the
Phase 2/3 discipline). `grep -rn "RegisterGroup(" Scripts/Game/Tests/` is **unchanged** by this phase,
so Phase 4's 19-site sweep table carries verbatim.

### Hand-offs to Phase 7 created by this phase

- **Four faction attributes lost their last reader** and are now declaration-plus-authored-values only:
  `m_aLargeCheckpointPrefab`, `m_aMediumCheckpointPrefab`, `m_aVehicleCarPrefabSlots`,
  `m_aVehicleTruckPrefabSlots`. Their content survives as the `MediumCheckpoint` / `LargeCheckpoint`
  compositions and the `car` / `truck` vehicle-registry entries. D9 deletes them **with their authored
  values in both faction configs and in `Prefabs/GameMode/OVT_FactionManager.et`**, in one commit —
  deleting the attribute alone produces a parse warning on every load.
- **`m_aGroupInfantryPrefabSlots` still has two readers**, both test fixtures
  (`OVT_TEST_InitSuite.c:1603-1606, :1840-1843`), exactly as D9 already predicted. Re-point them at the
  group registry when the arrays go.
- **`OVT_GMIconFormat.UPGRADE_PREFIX`** — see the T5.5 table's last row.
- **`FindUpgrade`'s dead `tag` branch** goes with the method itself in T7.x.

---

## 2026-08-18 — PHASE 6 BUILT (T6.1–T6.10). Compile `0`, negative control verified. Suite run owed.

**This phase rewrote the occupying faction's economy loop, deleted the last live legacy spend path and
changed what a pre-migration save means.** Nothing else in the campaign was touched: QRF, the
counter-attack roll, threat decay, tower sabotage, `RplSave`/`RplLoad` and the four persisted payload
classes are all byte-identical.

| Task | What landed |
|---|---|
| T6.1 | Read-only survey, verdicts below. The survey found the phase's one genuine landmine (the pool-restore ordering hazard) before a line was written |
| T6.2 | `CheckUpdate`'s sorted-bases spend loop is **deleted**. `GainResources()`, the 80 % computation, `UpdateKnownTargets()`, threat decay, the counter-attack block and BOTH early returns are unchanged. The loop is replaced by `TransferDefenseShareToPool(newResources)` |
| T6.3 | `DistributeInitialResources()` and its +5 s `CallLater` are **deleted**. `NewGameStart()` now credits `SeedOpeningDeploymentResources()` once, gated on `m_bDistributeInitial`, which it then clears. **No `CallLater` was needed** — reasoning below |
| T6.4 | `AllocateDeploymentResourcesIfNeeded()` and its call from `GainResources()` are **deleted**. `AllocateDeploymentResources()` keeps a header naming its three callers as the only ones there may ever be |
| T6.5 | `OVT_BaseUpgradeSpecops.c` **deleted**; `overthrowBaseUpgrades.conf` down to an empty `m_aBaseUpgrades { }`; `UpdateSpecops()` and both call sites **deleted**. Loss list below |
| T6.6 | `ApplyPersistedBaseUpgrades` **sums instead of copying** and returns the value; `ApplyPersistedOccupyingFaction` accumulates across every base and queues ONE refund; the upgrade-replay block in `InitBaseControllers` is **deleted** with the `slotsFilled` restore kept verbatim; `WriteBase` writes an **empty** `upgrades` array and `WriteUpgrade()` is deleted with it |
| T6.7 | `TestSuites/Logic/OVT_TEST_Logic_BaseDefenseConversion.c`, 2 cases — the legacy value sum (incl. the idempotence claim) and the funding split's conserved-total walk |
| T6.8 | `..._LegacyBaseUpgrades_ConvertToDeploymentResources` on the Persistence round-trip gate |
| T6.9 | `OVT_TEST_Init_Deployments_DefenseFundingLandsInThePool` — three claims: the seed lands in the pool, a tick's transfer conserves the total, a starved reserve is clamped |

**Two new source files:** `Scripts/Game/GameMode/Managers/Factions/OVT_BaseDefenseConversion.c` (pure,
world-free — the Phase 1 `OVT_DeploymentSelection` shape) and the Logic case file.

---

## T6.1 — the read-only survey, run BEFORE any edit — 2026-08-18

### Every writer of `m_iResources`

| Writer | Verdict |
|---|---|
| `NewGameStart` `m_iResources = maxQRF` | **KEPT VERBATIM.** The reserve is still what QRF sizing and the counter-attack roll draw on |
| `ApplyPersistedOccupyingFaction` `m_iResources = resources` | **KEPT VERBATIM** |
| `RecoverResources(int)` | **ALREADY GONE** — deleted in Phase 3 with its only caller |
| `CheckUpdate`'s per-base loop (`m_iResources -= spent`, the `<= 0` clamp) | **DELETED.** Replaced by `TransferDefenseShareToPool()` |
| `UpdateSpecops` (`m_iResources -= upgrade.SetTarget(target)` + two clamps) | **DELETED** with the method |
| `GainResources` `m_iResources += newResources` | **KEPT VERBATIM** |
| `AllocateDeploymentResourcesIfNeeded` `m_iResources -= toAllocate` | **DELETED** with the method |
| `OVT_QRFControllerComponent.c:299-300` | **UNTOUCHED** — epic-level exclusion |

### Every reader outside the manager

| Reader | Verdict |
|---|---|
| `OVT_GMRequestComponent.c:553` (`int ofResources = occupying.m_iResources`) | **UNTOUCHED.** The GM campaign panel shows the reserve and the deployment pool side by side and is still truthful — the reserve simply stops being a defense budget |
| `OVT_QRFControllerComponent.c:223` (QRF sizing) | **UNTOUCHED** |
| `OVT_BaseUpgradeSpecops.c:55` | **GONE WITH THE CLASS** |
| `OVT_OccupyingFactionManagerSerializer.c:134` | **UNTOUCHED** — the reserve is still field 2 of the payload |
| `OVT_TEST_PersistenceSuite.c:1491, :1602` (two pass-throughs) | **UNTOUCHED.** Both read the live reserve and hand it straight back into `ApplyPersistedOccupyingFaction`, so neither depends on what the reserve is spent on |

### Every caller of `AddFactionResources`

Expected one; found **two production call sites plus the declaration**, and the second is legitimate:

| Site | Verdict |
|---|---|
| `OVT_OccupyingFactionManager.AllocateDeploymentResources()` | **THE manager's single credit point.** Three internal callers now (opening seed, tick transfer, legacy refund) and a header saying there must never be a fourth without a written reason |
| `OVT_MultiTownPatrolBehaviorDeploymentModule.c:507` | **THE DEPLOYMENT FRAMEWORK'S OWN REFUND** — a module handing back resources for a patrol it collected. Correctly outside the manager's accounting; untouched |
| `OVT_TEST_InitSuite.c:8982, :9149` (+ the two new fixtures' `RestorePool`) | Test fixtures planting and restoring a borrowed pool. Not production paths |

### `StartBaseQRF`'s callers after the specops drop

| Caller | Verdict |
|---|---|
| `OVT_OccupyingFactionManager` counter-attack roll | ✅ **SURVIVES**, untouched |
| `OVT_CampaignRequestComponent.c:177` player-initiated capture | ✅ **SURVIVES**, untouched |
| `OVT_BaseUpgradeSpecops.c:47` | Gone with the class — this is D3's documented loss |

### 🔴 `m_aKnownTargets` / `UpdateKnownTargets` — VERDICT: **BOTH STAY. Specops was NOT their only consumer.**

The plan allowed `UpdateKnownTargets()`'s call to be deleted "only if specops was its only consumer".
It was not, and the grep is unambiguous:

```
GetThreatByLocation()  reads m_aKnownTargets  (OVT_OccupyingFactionManager.c)
  ├─ OVT_DeploymentManager.c:989   ← the DEPLOYMENT EVALUATOR's own candidate threat score
  ├─ OVT_MapThreatGrid.c:66        ← the player-facing map threat overlay
  └─ GetBaseThreat()               ← was the deleted sort loop's key
GetNearestKnownTarget()  reads m_aKnownTargets  ← used by UpdateKnownTargets itself
```

Deleting the call would have frozen the target list at whatever `PostGameStart()` computed, so the
evaluator's threat sort and the player's threat overlay would never learn about a captured base, a
taken tower or a new FOB. **The call is kept in `CheckUpdate` with a comment saying exactly this**, so
nobody removes it as a specops leftover later.

Two things DID lose their last reader and are left for Phase 7 rather than swept here:
`GetBaseThreat(OVT_BaseData)` (a public accessor, one line, trivially useful) and
`OVT_BaseData.sortBy` (a `[NonSerialized()] [SortAttribute()]` field). Neither is dead weight worth a
scope excursion in the highest-risk phase.

---

## 🔴 THE ORDERING HAZARD THIS PHASE FOUND AND DESIGNED AROUND — read before touching the refund

**`ApplyPersistedOccupyingFaction` CANNOT credit the deployment pool inline.** It was written that way
first, and the survey caught it:

```
Configs/Systems/Persistence/Overthrow.conf, ComponentSerializers, in order:
   ...
   7. OVT_OccupyingFactionManagerSerializer     ← the legacy refund is computed here
   ...
   9. OVT_DeploymentManagerSerializer           ← ApplyPersistedFactionResources() does
                                                  m_mFactionResources.Clear() and refills from the save
```

An inline `AddFactionResources` would therefore be **wiped microseconds later, silently**, and every
legacy campaign would load with its entire investment gone — with a cheerful "converted N resources"
line in the log saying otherwise. This is a hazard the phase *introduced*: nothing credited the pool
during deserialization before it.

**The design:** `ApplyPersistedOccupyingFaction` accumulates into `m_iPendingLegacyRefund` (never
persisted, never replicated) and calls `QueueLegacyUpgradeRefund()`. Two delivery points, armed by
whether a campaign is already running:

| Situation | Discriminator | Delivery |
|---|---|---|
| **Loading a save point** (the real player path) | `HasGameStarted()` is **false** — `RestoreStartedCampaign()` only *schedules* `DoStartGame()` | `PostGameStart()` calls `CreditPendingLegacyRefund()`. Provably after the whole load, with no assumption about how many frames a load takes |
| **Re-applying to a live session** (`ReapplyLatestSaveData`; `PostGameStart` will never run again) | `HasGameStarted()` is **true** | `CallLater(CreditPendingLegacyRefund, 0)` — next frame, after the one synchronous re-application |

`CreditPendingLegacyRefund()` zeroes the pending amount before crediting, so arming both is safe:
whichever reaches it first wins and the other is a no-op. The Persistence case asserts all three halves
— the apply queues the right amount, the apply does **not** move the pool, and the credit point
delivers it exactly once.

---

## The conserved-total claim, stated so it can be checked

> **After Phase 6, every resource that leaves the occupying faction's reserve for defense arrives in the
> deployment pool, and nothing is created or destroyed on the way:**
> `reserve_after + pool_after == reserve_before + pool_before + tick`

It holds **unconditionally**, in every state including the degenerate ones, because
`TransferDefenseShareToPool()` clamps the share to the reserve before moving it
(`if(toSpend > m_iResources) toSpend = m_iResources;`). That clamp is **parity, not caution** — the
per-base loop it replaced clamped each base's budget the same way. Asserted twice: as pure arithmetic
over a six-tick walk in the Logic tier, and against the live managers in the Init tier.

**Exactly one path credits the pool** (`AllocateDeploymentResources`, three internal callers) and
**exactly one spends it on defense** (the deployment evaluator). Grep-checkable:
`grep -rn "AddFactionResources" Scripts/` answers the manager's credit point, the deployment
framework's own patrol refund, and test fixtures.

---

## `LEGACY_GROUP_VALUE` — the derivation, and why it is an approximation

```
LEGACY_GROUP_VALUE = OVT_BaseDefenseConversion.LEGACY_GROUP_SIZE (4) × difficulty.baseResourceCost
                   = 60 on the shipped Normal difficulty (baseResourceCost 15)
```

The deleted valuation priced a **live** group at `agents × baseResourceCost`, and the shipped group
compositions run **2–6 men**, so 4 is their middle. It is an **approximation by design — value-parity,
not entity-identity**: the requirement is that a loaded campaign gets back the value it had invested,
not that the same men come back. Nothing anywhere reconstructs a legacy group. The sentence is pinned
on the constant itself.

`GetLegacyGroupValue()` is null-safe on difficulty: deserialization can land before the campaign's
difficulty settings resolve, and a refund of 0 is a far better failure than a VME during a load.

**What converts to ZERO, and why (all three asserted in the Logic tier):**

| Record shape | Converts to | Why |
|---|---|---|
| Composition / checkpoint (`{type, tag, pos}`, `resources 0`, no groups) | **0** | The structure is a `OVT_PersistenceTracking`-tracked world entity that comes back from the save on its own, and its slot claim comes back in `slotsFilled`. Refunding would pay for it twice |
| Parked vehicles / specops / town patrol | **0** | All three `Serialize()`d to null and were never in a payload |
| An already-converted (empty) array | **0** | **This is the entire idempotence mechanism.** After one save on this build the payload holds an empty array. There is no flag |

**The one exposure, stated so nobody reads it as a bug:** re-applying the *same pre-migration save*
twice in one session credits twice, because nothing has rewritten the payload yet. Taking a single save
after loading a legacy campaign closes it permanently. Recorded in `ApplyPersistedBaseUpgrades`' header.

---

## D3 — the specops loss list (what a player stops getting)

`OVT_BaseUpgradeSpecops` is deleted with no deployment replacement. What goes with it:

1. **Target-driven special-forces squads.** `UpdateSpecops` walked `m_aKnownTargets` (player FOBs,
   camps, captured bases, resistance-held radio towers), found the nearest occupied base and gave its
   specops upgrade a target; the upgrade bought a `SPECIAL_FORCES` group and waypointed it there.
   Nothing in the deployments framework enumerates player-held targets, so there is no cheap equivalent.
2. **The specops `StartBaseQRF` hook.** Arriving within 20 m of a resistance-held **base** triggered a
   QRF on it. QRFs still happen — the counter-attack roll and player-initiated capture both survive,
   verified above — they just no longer arrive *because a specops team walked to your base*.
3. **The 600 s radio-tower recapture timer.** Arriving at a resistance-held **broadcast tower** sent a
   `"RadioTowerCapture"` notification and counted `m_iCaptureTimer = 600000` down to
   `ChangeRadioTowerControl`. 🔴 **The occupying faction loses its only way to take a radio tower back.**
   Towers are now a one-way ratchet for the player until someone builds a replacement.

**What is GAINED by the deletion, and it is not nothing:** at HEAD the class was already broken in a way
that cost resources for no effect. `SetTarget` still debited the reserve and still spawned groups
(`UpdateSpecops` was never kill-switched), but its `OnUpdate` — the arrival check, the QRF hook and the
capture timer — ran only from `OVT_BaseControllerComponent.UpdateUpgrades`, which **is** kill-switched.
So the occupying faction was paying for specops squads that spawned, stood around and did nothing.
Deleting the class fixes a live resource-and-AI leak.

If it is ever wanted back, the shape is a `Deployment_SpecopsRaid.conf` with a target-position condition
module — a feature, not a migration, and it belongs to the occupying epic.

---

## Deliberate divergences from the plan, all small, all deliberate

1. **The opening seed runs inline in `NewGameStart()` — NO `CallLater`.** The plan allowed keeping the
   deleted method's +5 s timer "if bases are not discovered at that point". They are: `Init()` →
   `InitializeBases()` runs a synchronous world query long before `DoStartNewGame()`, which is why
   `NewGameStart`'s own existing loops can already walk `m_Bases` and print a count. The 5 s deferral
   existed because **spending** needed each controller's discovered slots (`InitBase()` runs out of
   `PostGameStart`); a **credit** needs only `m_fStartingResourcesMultiplier`, an `[Attribute]` readable
   the instant the controller entity exists. The reasoning is in the code comment.
2. **`m_bDistributeInitial` is now load-bearing rather than decorative.** `NewGameStart()` checks it and
   clears it after seeding, so the flag means "this campaign has not had its opening allocation yet" and
   is monotonic. `ApplyPersistedOccupyingFaction` still clears it on a restore, exactly as documented —
   a continued campaign never re-seeds. (Structurally it also cannot: `RestoreStartedCampaign()` runs
   `DoStartGame()` and never `DoStartNewGame()`. The flag is the second line of defence.)
3. **`GetBase(EntityID)` is now null-safe on the marker.** It dereferenced `FindEntityByID()`'s result
   directly and threw when a marker had gone away — which is *why* the occupying-faction serializer
   carries its own `FindBaseController()` and says so in its header. Every caller already null-checked
   the RESULT; this makes those checks reachable rather than decorative, and the new seed loop depends
   on it.
4. **`WriteUpgrade()` is deleted, not left orphaned.** It lost its only caller and takes an
   `OVT_BaseUpgradeData` parameter, a type Phase 7 deletes — so it had to die by Phase 7 anyway.
   `OVT_PersistedBaseUpgrade` / `OVT_PersistedBaseUpgradeGroup` / `OVT_PersistedBase.upgrades` are
   **untouched** and still READ by the conversion.
5. **The refund is deferred rather than credited inline** — see the ordering hazard above. This is the
   one place the phase's shipped code differs from §3.6's flow diagram, and it differs because §3.6's
   flow would have silently lost every refund.
6. **`OVT_TEST_Campaign_BaseUpgrades_ConfigLoaded` was rewritten, not left to go red.** It asserted
   `m_aBaseUpgrades.Count() >= 1`, which an empty config cannot satisfy, and its failure path
   dereferenced `m_BaseUpgradesConfig.m_aBaseUpgrades` unguarded (a VME once the array can be null). It
   now asserts the WIRING claim it was written for — the prefab→conf reference resolves and
   `InitializeBase()` builds a runtime list that MIRRORS the config, whatever the config holds. Phase 7
   (T7.9) still deletes the file.

---

## Comment debt this phase deliberately did NOT pay, and why

- **`OVT_PersistedBase`'s class header** still says "an occupying-faction base stores its UPGRADES and
  filled slots (its garrison is a product of those upgrades and is rebuilt by replaying them)". The
  second half is now false. It is left **byte-identical on purpose**: the phase's acceptance criterion
  is an EMPTY `git diff` on `OVT_PersistedBase` / `OVT_PersistedBaseUpgrade` / `OVT_PersistedRadioTower`,
  and a prose-only diff would trip a literal check. **Phase 7's T7.10 comment sweep should fix this
  sentence** — added to its list here so it is not lost. The truth is written on the serializer's own
  class header and on `WriteBase`, which is where a reader of the write path lands.
- **Three surviving `SpendResources` mentions in comments** (`OVT_DeploymentManager.c:1151`,
  `OVT_TEST_InitSuite.c:10652, :10900`) all name `OVT_BaseControllerComponent.SpendResources`, which
  still exists and is the acceptance grep's documented exception. They go with the method in T7.3.
- **Every OTHER deleted symbol name was scrubbed from comments tree-wide**, including one historical
  citation in `Scripts/Game/Modded/SCR_InventoryStorageManagerComponent.c`, because this project treats
  the acceptance greps literally (the Phase 1 precedent: "`MIN_DEPLOYMENT_DISTANCE` is gone tree-wide,
  including comments"). The comments still describe the deleted machinery — they just describe it
  instead of naming it.

---

## Acceptance criteria — verified 2026-08-18

| Criterion | Result |
|---|---|
| `tools/compile-check.sh` exit 0 | ✅ **0**, and proven non-cached by a negative control (a deliberate undefined symbol appended to the Persistence suite reported at the right file and line, exit 1; removed, exit 0) |
| `grep -rn "SpendResources\|DistributeInitialResources\|AllocateDeploymentResourcesIfNeeded\|UpdateSpecops\|OVT_BaseUpgradeSpecops" Scripts/` | ✅ **4 hits, all `OVT_BaseControllerComponent.SpendResources`** — its declaration plus three pre-existing comments naming it. The other four symbols return **nothing at all**, comments included |
| `git diff` on `OVT_PersistedBase`, `OVT_PersistedBaseUpgrade`, `OVT_PersistedRadioTower`, serializer write/read ORDER, `RplSave`, `RplLoad` | ✅ **empty.** The serializer's first diff hunk starts at line 81 — past all four payload classes (which end at line 67). `Serialize()`/`Deserialize()` bodies are in no hunk. `RplSave`/`RplLoad` live in the manager and its diff contains zero `RplSave`/`RplLoad`/`writer.Write`/`reader.Read` lines |
| `grep -rn "AddFactionResources" Scripts/` | ✅ the declaration, the manager's single credit point, the deployment framework's own refund, and test fixtures |
| `git diff Scripts/Game/GameMode/Virtualization/` | ✅ **empty**, and no untracked files there either |
| `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/ \| wc -l` | ✅ **5**, unchanged |

---

## What Phase 6 does NOT prove, and what the play-test has to answer

- **Whether the pacing feels right.** The opening seed is one number (`baseResourcesPerTick` + Σ per-base
  `startingResources × multiplier`) and the evaluator spends it at up to 10 deployments per 30 s pass,
  so a cold Eden takes minutes to fortify where the legacy system built instantly at +5 s. That is a
  tuning judgement, not an assertion.
- **Whether a real pre-migration save converts to a sensible number.** The suites assert the arithmetic
  and the seam; only a real legacy save point loaded in a real session shows whether the refund buys a
  recognisable amount of defense. ⚠ **The pre-migration save the user was asked to keep is now the only
  way to test this.**
- **That the deferred refund really lands on the LOAD path.** The Persistence tier drives the credit
  point directly (one frame, deterministic); the `PostGameStart` delivery point is exercised only by a
  real save-and-continue, which the harness cannot do. Watch the log for
  `Legacy base-upgrade refund credited: N resources` on the first continue of a legacy save.
- **The GM panel's base-upgrade rows are now EMPTY**, because `OVT_GMSnapshotBuilder.BuildBases()` still
  walks `controller.m_aBaseUpgrades` and the config has no entries. Expected, and Phase 7's T7.8
  re-points it at the deployments anchored on each base.

---

## 2026-08-18 — PHASE 7 BUILT (T7.1–T7.11). Compile `0`, negative control verified. Suite run owed.

## 🎉 THE VIRTUALIZATION EPIC'S KILL SWITCH IS GONE

```
$ grep -rn "OVT-VIRT-PLAYTEST-ONLY\|DISABLE_LEGACY_AI_SPAWNS\|OVT_VirtPlaytestKillSwitch" Scripts/
$ echo $?
1
```

Nothing. `Scripts/Game/GameMode/Virtualization/OVT_VirtPlaytestKillSwitch.c` is deleted and the
directory holds six files, none of which is a play-test switch. This is the last acceptance line of
the whole five-feature epic, and it is empty.

| Task | What landed |
|---|---|
| T7.1 | Read-only sweep, disposition table below. It found two things that changed the plan: `FormatUpgradeType` had **zero production callers** (so the GM formatter could be deleted outright rather than exempted), and `OVT_GMRecords.c`'s `git diff` was **already non-empty** before this phase |
| T7.2 | `Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/` (dir gone), `Scripts/Game/Configuration/OVT_BaseUpgradesConfig.c`, `Configs/BaseUpgrades/` (+ `.meta`, dir gone), and the `m_BaseUpgradesConfig` reference on `Prefabs/Controllers/OVT_BaseController.et` — **511 lines of deleted files** |
| T7.3 | The base controller lost `m_aBaseUpgrades`, `m_BaseUpgradesConfig`, `UPGRADE_UPDATE_FREQUENCY`, the `InitializeBase` copy loop, `UpdateUpgrades()` + its `CallLater`, `FindUpgrade()` and `SpendResources()` — **both kill-switch guards left INSIDE those deletions and neither was un-commented.** −94/+25 lines, and the +25 is a class header saying what survives and why |
| T7.4 | The QRF guard line deleted, `SpawnFromQueue()` otherwise untouched. `git diff --numstat` = `0 1` |
| T7.5 | The kill switch deleted. Ledger above |
| T7.6 | `GetRandomGroupByType()` + **13 legacy faction attributes** deleted with **every authored value** in both faction configs (−31 lines each) and `Prefabs/GameMode/OVT_FactionManager.et` (−48 lines, 0 added). `OVT_SpawnGroupJobStage` and the two Init fixtures re-pointed at the registry FIRST |
| T7.7 | `m_iMilitarySpawnDistance` deleted; three comments citing it rewritten; the test diagnostic that read it went with the ledger it belonged to. `grep` → **0 hits tree-wide** |
| T7.8 | `OVT_GMSnapshotBuilder.BuildBases()` enumerates the deployments anchored at each base; `GetRegisteredGroupCount()` added to `OVT_BaseSpawningDeploymentModule` (0) and overridden on the infantry module. Record classes, field order and the RPC **untouched** |
| T7.9 | `OVT_TEST_Campaign_BaseUpgrades_ConfigLoaded.c` deleted; `OVT_TEST_Campaign_GMGroupRegistry.c` header + failure ledger re-pointed at deployments; `OVT_EGroupOrigin` **not renumbered** |
| T7.10 | Four comment sites re-worded (the radio-tower reason, `ApplyPersistedBaseUpgrades`' header, `OVT_PersistedBaseUpgrade`'s header, `OVT_PersistedBase`'s header) — **every persisted payload class body byte-identical, verified field by field against HEAD** |

**Net for this phase: 511 deleted-file lines + 377 deleted edit lines against 236 added edit lines,
almost all of the additions being class headers explaining what is no longer there.**

---

## T7.1 — the disposition table, produced BEFORE anything was deleted — 2026-08-18

Every symbol the plan named, every surviving reference, and what happened to it. **"Survivor" always
carries a reason; nothing is left standing by omission.**

### `OVT_BaseUpgrade` / `OVT_BasePatrolUpgrade` (the classes)

| Reference | Disposition |
|---|---|
| `BaseUpgrades/OVT_BaseUpgrade.c`, `BaseUpgrades/OVT_BasePatrolUpgrade.c` | **DELETED** with the directory (T7.2) |
| `OVT_BaseControllerComponent` — the runtime list, the copy loop, the tick, the lookup, the spender | **DELETED** (T7.3) |
| `OVT_GMSnapshotBuilder.BuildBases()` — walked `controller.m_aBaseUpgrades`, cast to `OVT_BasePatrolUpgrade` for `GetNumGroups()` | **RE-POINTED** at the deployments anchored at the base (T7.8) |
| `OVT_TEST_Campaign_BaseUpgrades_ConfigLoaded.c` | **DELETED** (T7.9) |
| `OVT_TEST_Campaign_GMGroupRegistry.DescribeSpawnState()` + `DescribeProximity()` | **DELETED** with the base-upgrade ledger they printed; the deployment ledger beside them is the one producer left (T7.9) |
| `Scripts/Game/Controllers/README.md` "Base Upgrades (RETIRED — awaiting deletion)" section | **DELETED**, replaced by a "Base defense" paragraph naming the configs |
| `OVT_MapLocationType.c:2` — *"Follows the same pattern as OVT_BaseUpgrade"* | **RE-WORDED** — it described the hybrid config/code pattern, which needs no example |
| `OVT_DeploymentManager.c`, `OVT_TEST_InitSuite.c` ×2, `OVT_BaseControllerComponent.c` — comments naming `SpendResources()` | **RE-WORDED** to describe the per-base priority sweep without naming it. `grep -rn "SpendResources"` → **0** |
| `OVT_GMRecords.c:39, :52, :67` — comments naming `OVT_BaseUpgrade.GetResources()`, `OVT_BasePatrolUpgrade.GetNumGroups()`, `OVT_BaseUpgradeData.pos` | **RE-WORDED** — see the `OVT_GMRecords.c` note below for why editing it was the right call |

### The persisted and wire classes — **SURVIVORS, all recorded**

| Symbol | Why it stays |
|---|---|
| `OVT_PersistedBaseUpgrade`, `OVT_PersistedBaseUpgradeGroup`, `OVT_PersistedBase.upgrades` | **Save format.** `upgrades` is field 4 of 5 in a POSITIONAL binary context; removing it shifts `garrison` and every existing save reads a base's garrison out of an upgrade array. Still READ once, to convert a pre-migration campaign's value into a pool refund. Class bodies byte-identical (verified field by field against HEAD) |
| `OVT_BaseUpgradeData`, `OVT_BaseUpgradeGroupData`, `OVT_BaseData.upgrades` | **Named as an expected survivor by the plan's own Q4**, and live: `ApplyPersistedBaseUpgrades()` CLEARS `base.upgrades` on every path as the runtime half of the conversion's structural idempotence, and `OVT_TEST_PersistenceRoundTrip_LegacyBaseUpgrades_ConvertToDeploymentResources` asserts exactly that |
| `OVT_GMBaseUpgradeRecord`, `m_aBaseUpgrades` on the builder / `OVT_GMCampaignState` / `OVT_GMRequestComponent`, `RpcDo_BaseUpgrade`, `SendBaseUpgrade` | **On the Game Master wire.** Renaming them moves the RPC for no behavioural gain. A row is a DEPLOYMENT now and the class headers say so |
| `ApplyPersistedBaseUpgrades()` (the method name) | It converts a payload literally called `upgrades`; the name is what a reader of the save format looks for |
| `OVT_TEST_PersistenceRoundTripSuite.c:7188` — `composition.type = "OVT_BaseUpgradeComposition"` | 🔶 **PAYLOAD REALISM, NOT A REFERENCE.** The fixture stands in for a PRE-MIGRATION save point and that is the literal string such a save carries. Nothing in the conversion matches on it (`OVT_BaseDefenseConversion.ConvertedValue` sums banked resources and group counts and never sees a class name). A comment now says so, so nobody "fixes" it to a config name no legacy save could contain |

### `FindUpgrade` / `m_aBaseUpgrades` / `m_BaseUpgradesConfig`

| Reference | Disposition |
|---|---|
| `OVT_BaseControllerComponent.FindUpgrade(type, tag)` **and its Phase 5 dead `tag` branch** | **DELETED** together, exactly as Phase 5 predicted. Its last caller died in Phase 6 |
| `OVT_OccupyingFactionManagerSerializer.c:15` — *"FindUpgrade(type, tag) matches it back to the live upgrade object"* | **RE-WRITTEN.** `OVT_PersistedBaseUpgrade`'s header now says nothing replays one and what `type` is actually read for |
| `m_BaseUpgradesConfig` on the component and on `Prefabs/Controllers/OVT_BaseController.et:5` | **DELETED** together (the attribute and its authored value in one change-set). The component block on the prefab survives with an empty override body — deleting the block would remove the component from the base |
| `Configs/BaseUpgrades/overthrowBaseUpgrades.conf` GUID `{0756DED5D4018095}` | **No surviving reference** anywhere outside `docs/` |

### `GetRandomGroupByType` and the legacy faction attributes (D9)

**Two readers had to be dealt with first, and both were, before a single attribute was deleted.**

| Reader | Disposition |
|---|---|
| `OVT_SpawnGroupJobStage.c:30` — the live, non-base-upgrade reader | **RE-POINTED** at `OVT_Faction.GetGroupPrefabByType()`, a new registry-backed mapper. Its default branch still falls through to `m_aGroupPrefabSlots`, the vanilla-catalog-derived array, which **survives** and has 8 other readers |
| `OVT_TEST_InitSuite.c:1603-1606, :1840-1843` — two fixtures using the arrays as prefab fallbacks | **RE-POINTED** at `GetGroupPrefabByName("light_patrol")` then `("heavy_infantry")`. The primary path (`m_aGroupPrefabSlots[0]`) is unchanged, so what the fixtures actually spawn on the test world does not move |

Then, in ONE change-set, **13 attributes and every authored value**:

| Attribute | Authored in | Replaced by |
|---|---|---|
| `m_aGroupInfantryPrefabSlots` | both confs, prefab ×2 | `light_patrol` |
| `m_aHeavyInfantryPrefabSlots` | both confs, prefab ×2 | `heavy_infantry` |
| `m_aGroupATPrefabSlots` | both confs, prefab ×2 | `at_team` |
| `m_aGroupSpecialPrefabSlots` | both confs, prefab ×2 | `heavy_infantry` (see the mapping note) |
| `m_aGroupSniperPrefab` | both confs, prefab ×2 | `sniper` |
| `m_aGroupSniperTeamPrefab` | both confs (**not** in the prefab) | `sniper_team` |
| `m_aLightTownPatrolPrefab` | both confs, prefab ×2 | `light_patrol` |
| `m_aTowerDefensePatrolPrefab` | both confs, prefab ×2 | `light_patrol` (integration's hand-off, closed) |
| `m_aMediumCheckpointPrefab` / `m_aLargeCheckpointPrefab` | both confs, prefab ×3 | the `MediumCheckpoint` / `LargeCheckpoint` composition entries (Phase 2 T2.7) |
| `m_aVehicleCarPrefabSlots` / `m_aVehicleTruckPrefabSlots` | both confs, prefab ×2 | the `car` / `truck` vehicle-registry entries (Phase 2 T2.7) — Phase 5's hand-off, closed |
| `m_aGroupMGPrefab`, `m_aGroupATPrefab`, `m_aGroupFRAGPrefab`, `m_aHeavyTownPatrolPrefab`, `m_aSpecOpsPatrolPrefab` | **nowhere** | nothing — swept as confirmed dead (declaration only, no reader, no authored value anywhere in the tree) |

`grep -rn` for all 17 names over `Scripts/ Configs/ Prefabs/ Worlds/` → **0 hits each**, comments included.

### `m_iMilitarySpawnDistance` (integration's fourth hand-off)

| Reference | Disposition |
|---|---|
| `OVT_OverthrowConfigComponent.c:213` (the `[Attribute]`) | **DELETED.** Not authored in any prefab, config or world (re-verified tree-wide, `.git`/`docs` excluded); **absent from `RplSave`/`RplLoad`**, which stream only `m_Difficulty.*` and `m_ConfigFile.*` — so `CONFIG_STREAM_VERSION` stays at **3** |
| Its last production reader, the base-upgrade proximity gate | Died with the class in T7.2 |
| `OVT_OverthrowConfigComponent.c:56`, `OVT_BaseSpawningDeploymentModule.c:14` (comments) | **RE-WORDED**. `virtualizationSpawnDistance` is now documented as the campaign's only spawn distance |
| `OVT_TEST_Campaign_GMGroupRegistry.c:50, :374` (header + diagnostic) | **DELETED** with `DescribeProximity()` |
| `docs/features/virtualization/core/api.md` §6 | **NOT TOUCHED** — api.md is frozen for this phase; T8.4 owns that sentence and now has to say **no** system reads it |

### `baseResourceCost`

**SURVIVOR, entirely legitimate, no action.** It is a difficulty setting (`Difficulty_Easy` 20 /
`Difficulty_Normal` 15) with three surviving consumer groups after this phase, none of them a base
upgrade: QRF sizing (`OVT_QRFControllerComponent` ×3), the legacy-save conversion's per-group value
(`OVT_BaseDefenseConversion.LegacyGroupValue` + its two callers and two tests), and the authored
config values themselves. The only reader that died was `SpendResources()`'s allocation multiply.

---

## The GMIconFormat decision (T7.8), and the fact that settled it

**`FormatUpgradeType()` and `UPGRADE_PREFIX` are DELETED, not exempted.** Phase 5 handed Phase 7 a
choice: delete the formatter with the GM base-upgrade panel, or write Q4 an exception for the literal
`UPGRADE_PREFIX = "OVT_BaseUpgrade"`. The sweep found the fact that decides it:

```
$ grep -rn "FormatUpgradeType" Scripts/
Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMIconFormat.c:159   ← the test
Scripts/Game/UI/GM/OVT_GMIconFormat.c:90                                ← the declaration
```

**Zero production callers.** No widget renders a base-upgrade row; the records reach
`OVT_GMCampaignState` and stop there. And after T7.8 the field it formatted (`m_sType`) carries a
deployment config's **display name** — `"Base Tower Guards"`, authored readable — so prefix-stripping
a `OVT_BaseUpgrade`-shaped class name would be dead code guarding a state the new builder cannot
produce. It went, with `UPGRADE_PREFIX`, `ASCII_UPPER_A` and `ASCII_UPPER_Z` (used by nothing else).

The Logic case lost its two `ExpectUpgradeType` assertions and the helper, and its header says why
rather than going quiet. **Nothing else in that file changed**, and one free-text `reason` argument
that happened to read `"OVT_BaseUpgradePatrols"` now reads `"Base Garrison Patrol"` — an opaque string
either way, so the assertion's strength is identical. `grep -rn "BaseUpgrade" Scripts/Game/UI/` → **0**.

---

## 🔴 T7.4 vs T7.3 — THE ASYMMETRY IS DELIBERATE. DO NOT "FIX" IT.

Three `[OVT-VIRT-PLAYTEST-ONLY]` guards were removed and they were removed **two different ways**:

| Guard | Operation | Why |
|---|---|---|
| `OVT_BaseControllerComponent.UpdateUpgrades()` | the guard left **inside a deletion** — the whole method went | It ticked classes that no longer exist |
| `OVT_BaseControllerComponent.SpendResources()` | the guard left **inside a deletion** — the whole method went | Un-guarding it would revive the legacy spender **beside** the deployment pool and double every base's force. That is the exact defect G2 exists to prevent |
| `OVT_QRFControllerComponent.SpawnFromQueue()` | **the guard line alone deleted; the method restored to service** | QRF is an epic-level exclusion. It was never migrated, it has no deployment replacement, and its spawner must come back **exactly as it was**. `git diff` on that file is `0 1` — one deleted line, nothing else |

A reviewer seeing "two guards deleted with their code, one guard deleted alone" is looking at the
correct diff. The rule is not "remove the guard" — it is **"the guard leaves with whatever it was
protecting the campaign from"**, and for QRF that was nothing.

---

## `OVT_GMRecords.c` — why the empty-diff criterion could not be met literally, and what was done

The phase's acceptance criterion asks for an **empty** `git diff` on the GM record classes. It was
**already non-empty when this phase started**: Phase 6 had rewritten the `m_sType` doc comment to say
*"this list is EMPTY until `OVT_GMSnapshotBuilder.BuildBases()` is re-pointed at the deployments"* —
a sentence **T7.8 makes false**.

So the literal criterion was unreachable either way, and the choice was between a file with a false
sentence and a file that tells the truth. The truth won:

- **What the criterion is actually about is intact and was verified mechanically**: the four record
  classes' FIELD DECLARATIONS are byte-identical to HEAD, in the same order, compared field by field
  by script (comments and blank lines stripped); `git diff` on
  `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` (the RPC) and
  `Scripts/Game/GameMode/GM/OVT_GMCampaignState.c` (the client mirror) is **completely empty**.
- The diff on `OVT_GMRecords.c` is **comment-only** — `git diff | grep -v '^[+-]\s*//'` returns
  nothing but the hunk headers.
- Four comment sites were corrected: the base-record aggregate header, the three field comments that
  named deleted methods, and the record header. They now say a row is a deployment, and that the
  class names are historical because they are on the wire.

---

## T7.8 — the GM re-point, and the two judgement calls inside it

```
BuildBases()  per base marker:
    deployments = deploymentManager.GetDeploymentsInRadius(marker.GetOrigin(),
                        OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS)   // 250 m
    per deployment -> one OVT_GMBaseUpgradeRecord
        m_sType      = deployment.GetDeploymentName()      (config display name)
        m_iResources = deployment.GetResourcesInvested()
        m_iGroups    = sum over spawning modules of GetRegisteredGroupCount()
    OVT_GMBaseRecord.m_iUpgrades = the number of rows emitted
```

1. **The radius is `BASE_CLASSIFICATION_RADIUS`, not `baseRange` and not a new number.** It is the
   same 250 m within which the evaluator considers a position to BE this base (Phase 3's S1), so
   exactly the deployments that could have been bought for the base are the ones listed — and because
   it equals `HasExistingDeploymentOfType`'s dedup radius, two bases can never claim the same
   deployment. Reusing the constant means the panel cannot drift away from the classifier.
2. **No faction filter, deliberately.** The most interesting moment to read a base's row is right
   after a capture, when the previous owner's deployments are still standing and their control
   condition has not collected them yet; filtering by faction would blank the panel in exactly that
   state. The cost, stated in the method header: a deployment placed for a different location kind
   within 250 m of a base centre (a town patrol at a town-shadowed base) also appears in that base's
   rows. That is still the truth about the ground.

**The accessor is on the module BASE class and answers 0 there** (`GetRegisteredGroupCount()`),
overridden on `OVT_InfantrySpawningDeploymentModule` to return `GetGroupCount()`. The builder
therefore needs no cast, a parked-vehicle module contributes 0 without a special case, and a future
module type is counted the day it is written. The builder's read-only rule is preserved — the header's
list of "mutators you must not call from here" was updated too (it named the deleted upgrade spenders).

**The old NON-EMPTY filter is gone and `m_iUpgrades` now equals the row count.** It existed because a
base ran a dozen upgrades from the first tick with most of them at zero forever; a deployment only
exists once it has been bought, so every row is worth sending. A consumer that read "12 upgrades,
4 rows" as "8 are empty" now reads "3 deployments, 3 rows".

---

## T7.6 — the enum→registry map, and the one judgement call in it

`OVT_Faction.GetGroupPrefabByType(OVT_GroupType)` replaces the deleted resolver. `OVT_GroupType` stays
because it is `OVT_SpawnGroupJobStage`'s **authored surface** — a job config picks a rough shape of
force and should not have to know a faction's registry names.

| `OVT_GroupType` | Registry name | Note |
|---|---|---|
| `LIGHT_INFANTRY` | `light_patrol` | ⚠ **A REAL BEHAVIOUR CHANGE, and an improvement.** The old switch had **no case** for LIGHT_INFANTRY, so it fell through to `m_aGroupPrefabSlots.GetRandomElement()` — a random draw from the faction's whole vanilla GROUP catalog. The one authored consumer, `Configs/Jobs/assassinateTraitor.conf`, therefore guarded its traitor with anything from a sentry team to a rifle squad. It now gets a light patrol, every time |
| `HEAVY_INFANTRY` | `heavy_infantry` | Same prefab the legacy array's `[0]` gave |
| `ANTI_TANK` | `at_team` | Now always an actual AT team (the legacy USSR array's `[0]` was a plain rifle squad — see T2.7) |
| `SPECIAL_FORCES` | `heavy_infantry` | 🔶 **The judgement call.** No `special_forces` registry entry exists on either faction. `heavy_infantry` is the only general-purpose combat group **both** shipped factions define; the legacy USSR value was `Group_USSR_ManeuverGroup`, itself a member of the heavy-infantry array, and the legacy US value was a sniper team, which reads as an authoring accident rather than a design worth inheriting. **Nothing authors SPECIAL_FORCES anywhere**, so this is a documented default, not a live behaviour change |
| `SNIPER` | `sniper` | Same prefab |

**The fallback is load-bearing, not defensive.** An unresolvable name (a modded faction with no such
entry — and note `rifle_squad` exists on USSR but **not** on US, so partial registries are real in this
very tree) falls through to `m_aGroupPrefabSlots.GetRandomElement()`, the vanilla catalog. A faction
that ships no registry entry gets a group of some sort instead of an empty `ResourceName` and a silent
no-spawn.

### What was NOT swept, and why — the principle, stated so the next author can apply it

**This feature retires what this feature replaced.** Under that rule the checkpoint prefabs and the
car/truck arrays went (Phase 2 authored their registry replacements in the same change-set that
authored the group names), and the following did **not**, even though `grep` proves every one of them
is **declaration-only with zero readers and zero comments**:

```
m_aVehiclePrefabSlots        m_aVehicleLightPrefabSlots   m_aVehicleHeavyPrefabSlots
m_aTripodLightPrefabSlots    m_aTripodHeavyPrefabSlots    m_sFlagPrefab
```

Nothing in this feature replaced any of them; they are a modder-facing authored surface in the
"Faction Vehicles" / "Faction Objects" categories, and retiring them is one coherent decision for a
faction-registry pass rather than a half-sweep taken inside a base-defense retirement. **Recorded here
so it is a decision and not an oversight.**

⚠ **Two pre-existing oddities in `Prefabs/GameMode/OVT_FactionManager.et` were seen and left alone**,
because they predate this feature and fixing them is not this phase's remit: it authors
`m_aFlagPolePrefab` (an attribute `OVT_Faction` does not declare **at all**) and `m_aGroupPrefabSlots`
(a plain member with no `[Attribute]`, rebuilt from the vanilla catalog at `Init()`). Both are values
the engine cannot bind. Worth a look in a faction-config pass.

---

## Verification — every acceptance criterion, run 2026-08-18

| Criterion | Result |
|---|---|
| `tools/compile-check.sh` | ✅ **exit 0**, and proven non-cached by a negative control (an undefined symbol appended to `OVT_Faction.c` reported at `OVT_Faction.c:615` with exit 1; removed, exit 0) |
| `grep -rn "OVT-VIRT-PLAYTEST-ONLY\|DISABLE_LEGACY_AI_SPAWNS\|OVT_VirtPlaytestKillSwitch" Scripts/` | ✅ **COMPLETELY EMPTY** 🎉 |
| `ls Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/ Configs/BaseUpgrades/` | ✅ both **"No such file or directory"** |
| `grep -rn "BaseUpgrade\|GetRandomGroupByType\|m_iMilitarySpawnDistance" Scripts/ Configs/ Prefabs/` | ✅ `GetRandomGroupByType` **0**, `m_iMilitarySpawnDistance` **0**; `BaseUpgrade` survives **only** as the recorded persisted-payload, live-data and GM-wire names in the table above, plus the one legacy-save fixture string |
| `git diff` on the GM record classes and the RPC | ✅ RPC (`OVT_GMRequestComponent.c`) and client mirror (`OVT_GMCampaignState.c`) **completely empty**; `OVT_GMRecords.c` **comment-only**, field declarations byte-identical field by field (see the note above for why it was not left alone) |
| `git diff Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c docs/features/virtualization/core/api.md` | ✅ **empty** |
| `git diff Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` | ✅ **`0 1`** — exactly one deleted line |
| `grep -rn "SpendResources\|DistributeInitialResources\|AllocateDeploymentResourcesIfNeeded\|UpdateSpecops"` | ✅ **0 hits**, comments included (Phase 6's four documented exceptions are now gone with the method) |
| `grep -rn "m_ProxiedGroups\|m_iProxedResources\|m_ProxiedPositions\|BuyPatrol" Scripts/` | ✅ **0 hits** — banked-value proxying is gone from the tree (G3) |
| Persisted payload class bodies | ✅ `OVT_PersistedBase` / `OVT_PersistedBaseUpgrade` / `OVT_PersistedBaseUpgradeGroup` / `OVT_PersistedRadioTower` **byte-identical to HEAD**, verified by script |
| Net deletion | ✅ **511 lines of deleted files** + 377 deleted edit lines vs 236 added edit lines (almost all class headers explaining what left). ⚠ The FEATURE-wide `git diff --stat` is net **positive** — phases 1–6 added nine configs, seven module/provider classes and ~30 test cases. The *retirement sweep* is the pure deletion |

---

## What Phase 7 does NOT prove, and what the play-test has to answer

- **That the GM base panel reads sensibly.** The re-point compiles and the accessor is exercised by
  nothing automatic — `OVT_GMSnapshotBuilder` is multiplayer-only and no suite can open the Game
  Master editor. §6 step 2 and F14 are the only evidence: open the GM view at a fortified base and
  expect one row per deployment with its config name and a non-zero invested cost, and
  `m_iUpgrades` equal to the row count.
- **That the QRF spawn queue really works again.** It has been silenced since the epic began. §6
  step 10 / F15: trigger a QRF and expect waves. **This is the single highest-value manual check in
  the phase** — nothing else in the tree exercises `SpawnFromQueue()`.
- **That a base still initialises with no upgrade config on its prefab.** `InitBase()` now calls
  `InitializeBase()` and installs no timer; the slot registry is built by the same two world queries
  as before. A compile cannot see a prefab that failed to bind.
- **That the assassinate-traitor job still spawns a guard group.** Its group type resolves through the
  registry now instead of the vanilla catalog. Accept the job and expect a light patrol beside the
  traitor.
- **That no faction config warns on load.** Attributes and authored values were deleted together, in
  one change-set, in all three files — but only a real load prints the parse warning if one was missed.
  Watch the first campaign start's log for `OVT_Faction`.

---

## 2026-08-18 — PHASE 8 DONE (T8.1–T8.5). Docs-only; suite deliberately skipped.

### T8.5 — THE EPIC'S CLOSING LEDGER (run on the working tree, 2026-08-18)

```
$ grep -rn "OVT-VIRT-PLAYTEST-ONLY\|DISABLE_LEGACY_AI_SPAWNS" Scripts/
(no output, exit status 1)

$ ls Scripts/Game/GameMode/Virtualization/
OVT_AmbientSpawnSourceConfig.c
OVT_AmbientSpawnSourceInstance.c
OVT_AmbientSpawnSourceRegistry.c
OVT_VirtualGroupRecord.c
OVT_VirtualizationManagerComponent.c
OVT_VirtualizationMath.c
```

`OVT_VirtPlaytestKillSwitch.c` is gone. Two further greps run in the same pass and worth pinning:
`SpendResources` and `m_iMilitarySpawnDistance` both return **nothing** from `Scripts/`, and
`OVT_BaseUpgrade` matches **only** the legacy save-payload classes D4 keeps on purpose
(`OVT_OccupyingFactionManager.c:5,14,30,680`, the serializer's mirror classes, and one Persistence
fixture string). Recorded in `docs/features/virtualization/epic-overview.md` under
"Epic Closing Ledger" as well as here.

### T8.1 — the fact-check, with verdicts

Scope: every sentence in `Configs/Tutorials/` and `Configs/FieldManual/` touching bases, garrisons,
fortifications or attacking a base. Surfaces audited: 15 tutorial configs (only
`basesFirstCapture.conf` is in scope; the other 14 mention no base defence) and the single Field
Manual category file, whose in-scope pages are **Capturing Bases**, **Patrols and Garrisons** and
**FOBs and Building**.

| String | Verdict | Why |
|---|---|---|
| `Tutorial_BasesFirstCapture_Body` (5 sentences) | **KEPT, all 5** | Re-verified: the restricted-zone/wanted clause; the flag (`OVT_CaptureBaseAction` is authored on `Prefabs/Structures/Military/Flags/BaseFlag_US.et:18`, `BaseFlag_USSR.et:28` and `Prefabs/Controllers/OVT_BaseController.et:74`); the QRF point count; the counter-attack (`OVT_OccupyingFactionManager.c:1420-1429`, still spending `m_iResources`, epic-level exclusion). Its **Comment** was stale and IS corrected: it claimed base garrisons were not migrated and that the kill switch still guarded their spawners |
| `FieldManual_BaseCapture_Text` / `_Text2` / `_Text3` / `_Text4` | **KEPT** | QRF machinery, `ChangeBaseControl`, flag material and the resistance-side affordances are all untouched by this feature |
| `FieldManual_BaseCapture_Text5` | 🔴 **CORRECTED** | "it is taken back by a specialist team sent out from an occupied base" was the deleted `OVT_BaseUpgradeSpecops`. The **only** writer of `ChangeRadioTowerControl` left in the tree is `OVT_RadioTowerCaptureBehaviorDeploymentModule.c:124`, which always passes `config.GetPlayerFactionIndex()`, so no path returns a tower to the occupier. New clause says exactly that. "A place can change hands more than once" narrowed to "Bases and towns" for the same reason |
| `FieldManual_FOBs_Text5` | 🔴 **CORRECTED** | "sends troops out to look at them when it has soldiers to spare" was `UpdateSpecops` + `OVT_BaseUpgradeSpecops.OnUpdate`, both deleted. `UpdateKnownTargets()` **survives** (`OVT_OccupyingFactionManager.c:1394-1397`) but its only consumer is `GetThreatByLocation:1286-1310`. Note the asymmetry the new wording respects: a FOB target scores `5 × distanceFactor` (`:1300-1303`), a CAMP target scores **nothing** |
| `FieldManual_OccupyingForces_Text` (opening) | 🔴 **CORRECTED + WIDENED** | Base defence added to the list of standing forces; "the same reserves it spends on its bases" replaced by "one and the same pool", which is now literally true (G2) |
| `FieldManual_OccupyingForces_Text2` (Losses Stay Lost) | 🔴 **WIDENED** | Now names base defence alongside patrols and tower garrisons, and carries the headline promise ("a base is left in the state you left it in"). Last sentence "a patrol" → "a force" |
| `FieldManual_OccupyingForces_Text3` (Patrols Keep Moving) | 🔴 **WIDENED** | Base perimeter patrols are `PERIMETER`/280 and so **are** walked while dormant; defence positions and checkpoints are `DEFEND`; tower guards and sniper positions author no behaviour module at all |
| `FieldManual_OccupyingForces_Text4` / `_Text5` (Radio Towers) | **KEPT**; citations re-pointed | Claims unchanged. The comment's `OVT_DeploymentManager` line numbers had drifted (`:386-391`→`:652-657`, `:750-752`→`:1069-1071`, `:309`→`:573`, `:313`→`:577`) |
| Difficulty ×3 and faction ×2 campaign-setup descriptions | **KEPT**; citations corrected | Out of the two audited surfaces, but their comments cited the deleted `OVT_BaseUpgradeDefensePosition.c:47`. 🔴 **`defenseGroupsBaseMax` now has ZERO readers tree-wide** (still authored in all five `Difficulty_*.conf`, still declared at `OVT_DifficultySettings.c:45`). The player claim survives on `patrolGroupsMin/Max` alone (`OVT_InfantrySpawningDeploymentModule.c:486`, was `:416`) plus resource scaling into the pool. **Flagged as a tuning question for the play-test, not fixed here** |

**Counts: 3 sentences cut/corrected as false, 3 paragraphs widened, 9 strings' comments re-cited,
0 invented mechanics added, 0 tutorial popups created.**

### T8.2 — what was added, and where

No new tutorial popup. The base tutorial fires on `PLAYER_ENTER_BASE`, is capped at a 460 px panel,
is already over its own two-sentence budget and is **not wrong**; the standing rule is to prefer an
existing entry, and the Field Manual page that already owns this subject is **Patrols and Garrisons**.
That page gained one section, `Defending a Base`, of two paragraphs, placed between "Patrols Keep
Moving" and "Radio Towers":

- **`_Text6`** — the nine concerns in authored priority order, "bought and paid for on its own",
  "thickens up over a campaign rather than arriving finished", and the two threat-gated escalations.
  "Roughly in that order" is deliberate hedging and is honest: priority-2 ties break on registry
  order and every purchase is also cost- and condition-gated.
- **`_Text7`** — no fortification while a player stands in the base (creation gate only), a freshly
  taken or starved base is lightly held, losses last, survivors are re-placed on their posts on
  every materialisation, a wholly wiped position can be rebought, and clearing a post is settled
  only when the base changes hands. Its comment carries a **DO NOT** for future editors: a killed
  guard does not come back.

Every claim in both paragraphs carries a `file:line` in the string's `Comment`, per the project's
"cite it or cut it" rule.

⚠ **LOC RE-EXPORT OWED.** Three keys added and six edited in `Language/localization_Overthrow.st`
(list in the phase report). They are invisible in-game until the user re-exports in Workbench. This
stacks with the standing re-export note from `integration` and `civilians`.

### T8.3 — WIKI SYNC NOT DONE: the wikijs MCP tools were unavailable in this session

`mcp__wikijs__*` is not mounted in the Phase 8 agent's tool set (`wikijs_connection_status`,
`wikijs_search_pages` and `wikijs_get_page` all return "No such tool available"). This is a
**tool-availability gap, not a wiki outage** — nothing was written and no page was left half-edited.
The in-game side was completed in full. **The wiki work is OWED**, and the content is specified below
so it can be pasted without re-deriving it.

**Target page:** search `Documentation` for the page that already covers occupying forces / base
defence (candidates seen in earlier features' notes: a patrols-or-garrisons page, `difficulty`, and
`overthrow-config`). **Read it before writing** — search returns wrong `pageId`s; `update` needs the
`tags` parameter and can report failure while still writing, so re-read after writing.

*Player-facing points (same as the Field Manual, longer form):*
1. A base's defence is what you left it as: men killed there stay dead across leaving, returning and
   reloading a save.
2. Bases fortify over time, concern by concern (perimeter patrol → fighting positions → tower guards
   → sniper positions → checkpoints → fortifications → heavy patrol at threat 25 → AT section at
   threat 50 → parked vehicles), and a base a player is standing in never fortifies while watched.
3. A freshly taken or resource-starved occupier base can be lightly defended and thickens later.
4. Tower and sniper posts are manned again after a respawn: the survivors are teleported back onto
   their posts on every materialisation, and a position wiped to the last man can be rebought.
   Clearing a post is not permanent unless the base is taken.
5. Specops raids no longer happen. Enemy special forces no longer walk to a player FOB, and the
   occupying faction no longer has any way to retake a radio tower.

*Operator-facing notes (belong on the config/difficulty page, not the player page):*
6. Base defence now spends the **same deployment resource pool** as every other deployment. There is
   exactly one spender; the legacy per-base budget is deleted and 80 % of every resource tick is
   credited to the pool unconditionally.
7. `m_iMaxDeploymentsPerFaction`, authored **400** on `Prefabs/GameMode/OVT_OverthrowGameMode.et`
   (class default is still 100), is the ceiling that decides whether a big map can fully fortify.
   Eden needs ~114. A map that hits the ceiling logs a WARNING naming the config that was refused.

### T8.4 — epic bookkeeping done

- `docs/features/virtualization/epic-overview.md`: status → ✅ Complete (5/5), row 5 filled in at
  69/69 with what shipped, Master Overview Rollup refreshed, closing ledger section added.
- `docs/overview.md`: the epic's single row → ✅ Complete (5/5), 242/242.
- `docs/features/virtualization/core/api.md` §6: the sentence that still named the base-upgrade
  spawners as `m_iMilitarySpawnDistance`'s last reader is corrected to "**no system reads it any
  more; the attribute was deleted in Phase 7**", grep-proven. **No signature or contract text was
  touched** — api.md stays frozen.

---

## 2026-08-18 — AMENDMENT A1 (post-completion): authored base perimeters, free garrisons, AT road overwatch. Compile `0`. Suite run owed.

**The feature was already Ready for Review and the user had play-tested it** (all groups spawn in the
right places, tower guards and snipers included). A1 is the tuning pass that came back from that
play-test. The request, verbatim-summarised:

> Make the initial garrison groups and tower guards free at game start. The garrison waypoints aren't
> great — the road positions make sense for town patrols but not the base garrisons. Make them a
> little authored: two options on base controller prefabs for perimeter radius and rotation, giving a
> square at that radius rotated by the rotation with some randomness to increase/decrease the rotation
> a little, plus a debug viz in the editor (same as the arrows for attack direction) to see the square
> with rotation. Any deployments with patrol type = perimeter use this perimeter. **The AT sections
> should NOT patrol the perimeter** — they should be placed where checkpoints would be (whether or not
> there is one) but off to the side with an offset.

**MID-TASK DESIGN CHANGE (user, before implementation got far).** The original brief gated on
"PERIMETER within 250 m of a base". That was **superseded**: the two behaviours are now two explicit
enum members, so a config says which one it wants and neither can drift into the other.

| Sub-task | What landed |
|---|---|
| A1.1 | `m_bFreeAtGameStart 1` on `Deployment_BaseGarrisonPatrol.conf` and `Deployment_BaseTowerGuards.conf`. Free-seed verdict below |
| A1.2 | `m_fPerimeterRadius` (280, = `baseRange`) and `m_fPerimeterRotation` (0) on `OVT_BaseControllerComponent`, beside the QRF attack-direction attributes |
| A1.3 | `OVT_VirtualPlanFactory.BuildSquarePerimeterPlan()` + two protected pure helpers (`StartCornerIndex`, `NormalizeDegrees`) |
| A1.4 | `OVT_PatrolType.PERIMETER_BASE` **appended** to the enum; `OVT_PatrolBehaviorDeploymentModule.BuildAuthoredSquarePlan()`; `PERIMETER_ROTATION_JITTER_DEG = 10`; ground snap, no road snap; WARNING + un-authored fallback when no base is in range |
| A1.5 | `DrawPerimeterSquare()` inside the existing `#ifdef WORKBENCH` block — cyan square + two faint ±10° squares + a start arrow at corner 0 |
| A1.6 | `OVT_RoadSlotOverwatchPlacementProvider` (new) + `Deployment_BaseATSection.conf` rewritten to placed + DEFEND |
| A1.7 | 1 Logic case (new), 1 Init case (new), 3 Init cases extended |
| A1.8 | This record + the tasks.md section |

---

### A1.1 — the free-seed / NoPlayersNearby verdict

**BOTH are true, and which one applies depends on whether anybody is connected yet.**

`SeedFreeDeployments()` fires at **+9 s after `PostGameStart()`** and it **does** ask every condition
module the creation-time question (`SeedFreeConfig` → `PassesSeedConditions` →
`EvaluateStaticCondition`), so `OVT_NoPlayersNearbyConditionDeploymentModule`'s 320 m gate **is
consulted during seeding**. There is no bypass and none was added.

- **Dedicated server, nobody joined yet** (`OVT_DeploymentManager.c:332-337`, and the condition
  module's own header): `GetPlayerProximity()` answers `float.MAX` with no players connected, so the
  gate passes for every base and the whole baseline lands before the first join. This is the intended
  and normal path.
- **Single-player / hosted, player already spawned at +9 s**: if the player happens to be within
  320 m of a base centre, that base's garrison and tower guards simply **do not seed**, and are bought
  later by the ordinary evaluator — which asks the *same* static condition, so they arrive as soon as
  he leaves. Nothing is lost, nothing double-spawns (the 250 m same-name dedup covers both paths), and
  the rule the module exists for is preserved: **a player never watches a garrison appear.**

Overthrow's player start is a random town house, so a base within 320 m of the start position is
possible but uncommon; the failure mode is "this one base fortifies a few minutes later", which is
exactly the behaviour the config would have had before A1 anyway.

---

### A1.4 — the two perimeter types, and the verdict at every comparison site

`OVT_PatrolType` is now `{ DEFEND, PERIMETER, PERIMETER_BASE }`. **PERIMETER_BASE is APPENDED**, and
the enum carries an append-only warning: the members' integer values are what the `.conf` files carry,
so inserting one in the middle would silently re-point every authored value.

`grep -rn "OVT_PatrolType\." Scripts/` — every site, with its verdict:

| Site | Verdict |
|---|---|
| `OVT_PatrolBehaviorDeploymentModule.c` DEFEND / PERIMETER_BASE / PERIMETER branches | **EXTENDED.** The new branch runs before the `!= PERIMETER` bail-out, so plain PERIMETER is byte-for-byte today's code |
| `OVT_OverthrowConfigComponent.GivePatrolWaypoints()` | **EXTENDED** to treat PERIMETER_BASE as an ordinary PERIMETER. This is the LEGACY hand-authored waypoint path and its only caller is `OVT_SpawnGroupJobStage`, which spawns at a JOB location — there is no base controller to read a square off, and falling through would give the group **no waypoints at all**. No shipped job authors PERIMETER_BASE (the member did not exist), so nothing's behaviour changes today |
| `OVT_TEST_InitSuite` `..._TownPatrolPlanCycles` | **EXTENDED** — it now asserts Town Patrol is still plain `PERIMETER`. It never asserted the type before, so flipping the town config would have been silent |
| `OVT_TEST_InitSuite` `..._BasePatrolConfigsCyclePerimeter` | **REWRITTEN** — asserts `PERIMETER_BASE` on garrison + heavy, plus the authored-square geometry, plus the AT section's new placed shape |
| `OVT_TEST_InitSuite` `..._DefendPlansHoldTheStation` probes (`:12940`, `:12995`) | **LEFT.** Those probes assert the DEFEND *anchor* rule, which A1 does not touch |
| `OVT_TEST_Campaign_GMWaypointWalk` (comment only) | **LEFT.** It walks a hand-built vanilla cycle and never reads the enum |
| The movement tick / `CreatePlannedWaypoint` / the GM waypoint walk | **LEFT, and nothing to extend.** They switch on `OVT_EVirtualWaypointType`, not on the patrol type. A PERIMETER_BASE plan is 4 `PATROL` + 4 `WAIT` + `m_bCycle` — the *same kind* of plan PERIMETER builds — so every downstream consumer treats them identically by construction. That equivalence is deliberate and is stated in the module's class header |

**Design decisions recorded:**

- **±10° jitter, `PERIMETER_ROTATION_JITTER_DEG`**, rolled **fresh on every plan build** — so two
  garrisons at one base, and the same garrison rebought after a wipe, do not tread one line. Small on
  purpose: the point of an authored square is that a designer decided where it goes.
- **250 m lookup radius is `OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS`, referenced
  and not re-declared** — the same constant decision S1 pinned. Raising it to "fix" a distant base
  would re-open the force-doubling hole S1 closed.
- **The square's ORIENTATION is authored; the START CORNER is not.** `BuildSquarePerimeterPlan` walks
  the four authored corners but begins at the one nearest the bearing from the group to the centre —
  the same convention `BuildPerimeterPlan` uses, and for the same reason (a group sets off across the
  area rather than turning on the spot, and two garrisons approaching from different sides do not walk
  in lockstep). The Logic case asserts both halves: the corners do not move, the start does.
- **No base in range warns and still builds a plan** (un-snapped square, rotation 0). A mis-authored
  config that patrols the wrong shape is far easier to notice than one whose groups stand still.
- 🔴 **GROUND SNAP, NOT WATER AVOIDANCE.** Corners are clamped with `GetSurfaceY()` — the same clamp
  core applies when it spawns the waypoint entity — done at authoring time as well so the *persisted*
  plan carries standable positions. **A coastal base with a large authored radius can still put a
  corner in the sea and nothing moves it**, because relocating one corner would stop the square being
  the square that was authored. The Workbench viz is the mitigation.

---

### A1.6 — the AT provider's side-pick, and why it is not an index

`OVT_RoadSlotOverwatchPlacementProvider` reads the **same two slot sets the checkpoint modules use** —
`m_LargeRoadSlots` and `m_MediumRoadSlots` — and **deliberately ignores `m_aSlotsFilled`**: the ask is
"where checkpoints *would* be, whether or not there is one", so a base that has not bought its
checkpoints yet and one that has bought them all offer the AT section the same posts. Nothing here
claims a slot, so the provider can never stop a checkpoint being built later.

**THE SIDE IS A PURE FUNCTION OF THE SLOT'S OWN POSITION** (`SideForSlot()` folds the rounded world
X+Z to a parity, ±1). The alternative — alternate by list index — was **rejected**: the provider is
re-asked on every convergence pass, after every load and after every re-discovery of a base's slots,
so a destroyed slot or a differently-ordered query would flip a team across the road for no reason a
player could see, and placement stability across materialisations is a promise of the placed module.
Rounding to the metre first is what stops a float wobble flipping a team between two passes.
Neighbouring slots rarely share a parity, so in practice this *is* the "alternate sides" that was
asked for — bought without an ordering dependency.

Two more properties, both asserted: the offset is taken along the **slot's own right vector** (a slot
carries the road's rotation, so its local +X is "across the road" — a world-X step would put a team in
the middle of any north-south road), and the post **faces back at the slot**, so the team overwatches
the approach from the first frame. This is the second shipped provider to answer a real heading.

⚠ **THE PROVIDER NEEDS NO `CloneModule` AND MUST NOT GET ONE.**
`OVT_PlacedInfantrySpawningDeploymentModule.CloneModule()` copies `m_Placement` **by reference**,
deliberately (its own comment says why: a provider holds no state between calls, and cloning a
polymorphic config object by hand would need a copy method per provider that a mod's provider would
not have). So `m_fSideOffset` survives cloning because the clone shares the same object. Adding a
clone method here would be dead code that implies the opposite contract.

`Deployment_BaseATSection.conf` is now: `OVT_PlacedInfantrySpawningDeploymentModule` +
`OVT_RoadSlotOverwatchPlacementProvider` (`m_fSideOffset 15`, `m_fSearchRadius 280`) + `at_team`,
behaviour **DEFEND** (the Phase 5 stationed-group anchor holds them on their posts), Reinforcement +
BaseControl + NoPlayersNearby unchanged, `m_iMinimumThreatLevel 50`, priority 6, cost unchanged.
`m_fSpawnRadius` went 50 → 0 for parity with the other three placed configs (a placed module never
reads it). GUID prefix `6AB6A7B4`, grep-verified unused repo-wide before use.

---

### A1.7 — coverage

- **Logic:** `OVT_TEST_Logic_DeploymentVirtualization_SquarePerimeterPlan` (new, same file as the
  other three plan builders). Five claims: shape, the rotation is obeyed (0 vs 45 are different
  points), the square is a square (radius, adjacent = r√2, opposite = 2r), only the START corner
  follows the walker, and a negative rotation folds to the same square as its positive twin. ⚠ For the
  rotation-45 half the walker is moved **onto the diagonal too**: a bearing exactly halfway between
  two corners is the one input where the start-corner rounding is on a knife edge, and pinning a walk
  order against it would pin a tie-break rather than the geometry.
- **Init, rewritten:** `..._BasePatrolConfigsCyclePerimeter` — garrison + heavy assert
  `PERIMETER_BASE` **by name** and then measure the plan **against the live base controller**: every
  corner within 1 m of the authored radius (a road-snapped corner would miss by tens or hundreds of
  metres — that assertion *is* the "no road snap" proof) and within ±11° of an authored corner
  bearing. The AT section asserts placed module + the new provider by type + `m_fSideOffset > 0` +
  one-point non-cycling DEFEND + threat 50 + priority 6.
- **Init, new:** `..._RoadSlotOverwatchIsOffsetAndStable` — the offset is the authored distance and is
  perpendicular to the slot's facing (two headings, one of them 37° so a world-X bug cannot hide), the
  post looks back at the slot, the side follows the slot and not the order (three slots resolved
  forwards then backwards), the shipped config really authors 15, and two consecutive live resolves
  are identical. The transforms are hand-built because **the Init tier never runs
  `InitBaseControllers()`, so a live road-slot list does not exist there at all** — the live half is
  the never-null + repeatability contract, with the count printed rather than asserted.
- **Init, extended:** `..._TownPatrolPlanCycles` (+ plain `PERIMETER` assertion),
  `..._PlacementProvidersAnswerEmptyNotNull` (+ the fourth provider),
  `..._FreeAtGameStartIsAuthored` (+ the two newly-free base configs).
- **Persistence: untouched**, as instructed. No serializer, RPC or save-payload change — the new
  attributes live on a level-authored component and the plan payload's shape is unchanged.

**Compile verified by negative control, not by trust:** a deliberate undefined type was appended to
`OVT_RoadSlotOverwatchPlacementProvider.c`; `compile-check.sh` reported it at that exact file and line
and exited 1. Removed, re-run, exit **0**.

---

### 🔴 A1 CRASH FIX (2026-08-18, same day) — the viz killed Workbench, and the rule that came out of it

**REPORTED:** the debug square rendered with jittering geometry for a moment and then Workbench died.

**ROOT CAUSE, CONFIRMED BY READING THE CODE:** `DrawPerimeterSquare()` built its three vertex buffers
as **method-LOCAL fixed arrays** (`vector authored[4]; vector jitterMin[4]; vector jitterMax[4];`) and
handed them straight to `Shape.CreateLinesLoop`. **The `Shape.CreateLines` family REFERENCES the
caller's array; it does not copy it.** The shape is rendered after `_WB_AfterWorldUpdate` has already
returned, so the render thread was reading vertices out of a dead stack frame — reused memory, hence
the jitter, then the crash.

**The jitter is the proof, not just a symptom.** Nothing in the viz is rolled: `PERIMETER_ROTATION_
JITTER_DEG` is a `static const` and `PerimeterCorner()` is pure, so the fifteen points are identical on
every frame by construction. Deterministic geometry that visibly moves can *only* be memory
corruption. (Re-verified while fixing: no randomness anywhere in the viz, and the shape handles were
already stored in members exactly like `m_aDirectionArrowCenter`.)

**Why the QRF attack arrows beside it were always fine:** `Shape.CreateArrow` takes two `vector`s **by
value and copies them**. There is no buffer. The three arrow calls in the same method are not a
licence to use locals for a line strip, and the code now says so where somebody would look.

**THE FIX** (`OVT_BaseControllerComponent.c`):
1. The three vertex buffers are now **class members** — `m_aPerimeterAuthored`,
   `m_aPerimeterJitterMin`, `m_aPerimeterJitterMax`, each `vector[PERIMETER_VIZ_POINTS]` — carrying a
   loud hard-rule comment on the declaration.
2. `CreateLinesLoop` → plain **`CreateLines`** with a **5-point closed strip** (corner 0 repeated at
   index 4). `CreateLinesLoop` would close the square for us, but the only vanilla *per-frame `_WB_`*
   precedent uses plain `CreateLines`, and after a crash the closest thing to the proven idiom is
   worth more than one saved line. It also removes `CreateLinesLoop` from the equation entirely.
3. The start arrow stays a `CreateArrow` built from locals — safe, and now commented as to why.

**VANILLA PRECEDENT THIS IS COPIED FROM:** `SCR_PowerLineJointEntity` declares
`protected vector m_aDebugLine[POINTS];` as a **member** (`:22`) and passes it straight to
`Shape.CreateLines(..., m_aDebugLine, POINTS)` (`:163`) from its own per-frame `_WB_AfterWorldUpdate`.
Exact same problem, exact same answer. (`grep -rl CreateLines` ∩ `grep -l _WB_` over
`/mnt/n/Projects/Arma 4/ArmaReforger/scripts/` gives 7 files; this is the only per-frame one.)

---

### ⚠ THE STANDING RULE, and it reconciles a contradiction already in this repo

> **A buffer passed to the `Shape.CreateLines` / `CreateLinesLoop` / `CreateSeparateLines` /
> `CreateTris` family must (a) OUTLIVE THE FRAME and (b) be EXACTLY `num` long.**
> Member storage, sized exactly to what you pass. Locals are for the copy-safe calls only
> (`CreateArrow`, `CreateLine`, `CreateCircle`, `CreateSphere`, `Create`).

`OVT_GMWaypointRenderer.c:353-357` carries a comment from the 2026-08-16 play-test saying the
opposite — *"a CreateLines strip fed from a member fixed array … did not render at all … vanilla only
ever passes a LOCAL array sized exactly to num"*. **Both observations are real and they are not in
conflict; the generalisation in that comment is what is wrong.** Read the whole sentence: what failed
there was a member array with **`num` SMALLER THAN THE DECLARED SIZE** — a partially-filled buffer,
which renders nothing. The renderer's own fix (per-leg `CreateLine`) is still right for its case, for
its own second reason (z-fighting on the highlighted leg). What does not follow is "therefore use a
local": `SCR_PowerLineJointEntity` passes a member array per frame and works, because it passes the
**whole** array. This viz does the same — 5 declared, 5 passed.

So the two data points combine into the one rule above, and neither file should be "corrected"
towards the other.

---

### 🟡 A PRE-EXISTING INSTANCE OF THE SAME HAZARD, FOUND BY THE SWEEP — NOT TOUCHED

`Scripts/Game/Entities/OVT_StartCameraPos.c:32,54` declares `vector points[12];` as a **method local**
inside `_WB_AfterWorldUpdate` and passes it to `Shape.CreateTris(..., points, 4)`. Same family, same
buffer-lifetime hazard, same per-frame `_WB_` context. Its sizing is correct (4 triangles = 12
vertices), so only the storage is wrong.

**Deliberately NOT changed here** — it predates amendment A1, it is in an unrelated file, and a
crash-fix is the wrong place for unscoped edits. It is a three-line change (move `points` to a member)
whenever somebody wants it. ⚠ Worth knowing during the A1 re-test: selecting a start-camera-pos entity
in Workbench could crash for this reason and look like the perimeter fix having failed.

---

### 🔴 A1 — what still needs a human

- **THE WORKBENCH VIZ NEEDS A SECOND LOOK AFTER THE CRASH FIX.** `DrawPerimeterSquare()` lives inside
  `#ifdef WORKBENCH` and draws only when the entity is selected (`CALL_WHEN_ENTITY_SELECTED`, the same
  condition the QRF arrows use), so **no suite can ever reach it**. **Open a world layer in Workbench,
  select the base marker entity, and confirm:** one solid cyan square at `m_fPerimeterRadius`, two
  fainter cyan squares rotated ±10°, a short cyan arrow from the marker to corner 0 — and **no crash,
  and no vertex jitter**. Jitter returning would mean the buffers are being copied somewhere on the
  way to the call; a square that renders as an open "C" would mean the closing repeat point was lost.
- **Authoring the squares per base is a design pass, not a code task.** Every base ships at the class
  defaults (280 m, 0°) until somebody sets them, which is exactly parity with the old `baseRange`
  patrol radius — so nothing regresses if nobody ever touches them.
- **Play-test the AT posts.** Confirm the teams stand beside a road slot rather than in it, look at
  the road, and come back to the same side after a despawn/re-materialise cycle.
