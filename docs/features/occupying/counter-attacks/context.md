# Counter Attacks - Context & Decisions

**Last Updated:** 2026-08-19 (Phase 10 built bar the wiki; the suites and a localization re-export owed. ⚠ A **play-test pacing fix** landed after everything below — the phase timeout is now an IDLE clock, an operation in flight holds it, and an unfinished operation is refunded through the deployment framework; **Q6 was amended with its reason**. Top of Session Notes. T8.9 looks DONE in the working tree - see the Phase 10 note. ⚠ A **World Editor crash** reported against this feature was fixed the same day, and a **play-test fix for deployments materialising inside a live battle** landed after it - both are at the top of Session Notes and both owe a suite run)
**Current Phase:** Phase 10 — help & documentation sync (T10.3 wiki BLOCKED, see the session note)
**Status:** 🟡 In Progress

---

## Quick Status

**What's Done:**
- ✅ Requirements written 2026-08-17, formalised after a design discussion
- ✅ Implementation plan written 2026-08-18 by `solution-architect`, every `file:line` verified that day
- ✅ **Counter-attack QRF mode addendum, 2026-08-19** — the silent siege (§3.9), the daylight window (D17), Phase 9 added, docs phase renumbered 9 → 10
- ✅ `tasks.md` scaffolded — 106 tasks across 10 phases

- ✅ **Phase 1 built, 2026-08-19** — both legacy triggers deleted, `counterAttackTimeout` retired with all four authored values, twelve new difficulty fields authored across five presets, `OVT_ObjectivePhaseRules` opened with its two difficulty-consuming statics, one Logic case and one Init case added. `tools/compile-check.sh` exit 0.

- ✅ **Phase 2 built, 2026-08-19** — `OVT_ObjectiveDirectorComponent` on the game-mode prefab, its records and enums, the pure selection statics, the state machine with its three early returns, the one reset path, two pure-read helpers on the occupying faction manager (with the town manager's duplicated tower loop re-pointed at one of them), and a version-first serializer of its own. Five Logic cases, three Init cases and one Persistence case added; 19 can-fail faults injected and compiled. `tools/compile-check.sh` exit 0.

- ✅ **Phase 3 built, 2026-08-19** — the per-faction anchor store on the deployment manager, one line in the
  evaluator's candidate loop, and the director's push/drop. ⚠ **The plan's §3.5 pseudo-code was refused**:
  it biases the candidate's constructor argument, which would have biased ELIGIBILITY as well as ordering
  (see the T3.1 verdict below). Three Logic cases and three Init cases added; ten can-fail faults injected
  and compiled. `tools/compile-check.sh` exit 0.

- ✅ **Phase 4 built, 2026-08-19** — `OVT_InsertionGeometry` (pure), the `OVT_DeploymentSourceProvider`
  seam with its nearest-controlled-base implementation, the per-faction convoy cap on the deployment
  manager, and `OVT_InsertionSpawningDeploymentModule` itself: source → truck → crew → seat → drive →
  drop → return → despawn, with **five** independent diversions onto the walk path. Two registry entries
  per faction appended. Three Logic cases and four Init cases added; 22 can-fail faults injected and
  compiled. `tools/compile-check.sh` exit 0. **No config authors the module yet** —
  `git diff Configs/Deployment/` is empty.

- ✅ **Phase 5 built, 2026-08-19** — the two prerequisite registry fixes, the objective condition module,
  the two hold-a-place behaviour modules, the appended support modifier (LAST entry) with its
  index-carrying handler, both new deployment configs, **four thin registry variants carrying the ramp**,
  and the director's `TickHarassment()` finally spending: `ForceCreateDeployment` **plus**
  `SubtractFactionResources`, in one method, in one place. Four Init cases added; **19 can-fail faults
  injected and compiled**. `tools/compile-check.sh` exit 0.

- ✅ **Phase 6 built, 2026-08-19** — the T6.1 survey (which overturned the plan's assumed cost join), the
  navmesh-queue-then-delete pair **extracted** into `OVT_ResistanceFactionManager.DestroyPlacedItem()`
  and its four copies collapsed into one, the cost join added beside it, `NextTargetIndex` +
  `BasePhase2Gate` in the pure tier, `OVT_BaseSabotageBehaviorDeploymentModule`, one config + one
  registry entry, one broadcast preset with two `.st` keys, and the director's base branch:
  `SendSabotageOperation()` on the SAME cadence through the SAME `CreateObjectiveDeployment()`.
  One Logic case and four Init cases added; **28 can-fail faults injected and compiled**.
  `tools/compile-check.sh` exit 0.

- ✅ **Phase 7 built, 2026-08-19** — the curated-site marker (component + entity + prefab, no world-layer
  instances), `Prefabs/Bases/OVT_OccupyingFOB.et`, the pure `OVT_FOBSiting` statics and the director's
  deterministic 24-point siting lattice, `OVT_FOBRaiseSpawningDeploymentModule` with its
  `WasRestoredFromSave()` gate **and** a walking-arrival path the plan did not describe, the anchor source
  provider, both FOB configs + two registry entries, the spend ceiling inside `CreateObjectiveDeployment()`,
  the starvation rule, the held dismantle action with a fifth server-validated verb on
  `OVT_CampaignRequestComponent`, and one teardown reached by every exit. One Logic case, five Init cases
  (four new + one seam) and one new Persistence case added, plus the stranded-objective half appended to
  the Phase 2 round trip; **24 can-fail faults injected and compiled**. `tools/compile-check.sh` exit 0.

- ✅ **Phase 8 built, 2026-08-19 (bar T8.9's `.layout` slice)** — the forward base joins the battle's wave
  sources, `OVT_QRFBearing` (pure) biases each wave's landing zone toward the source that sent it, the
  director's Phase 3 gate fires `StartBaseQRF`/`StartTownQRF` and polls the battle's end, the daylight
  window lands as a pure predicate with its consts on the director, and the Game Master wire gains a
  **new** campaign record pair with both version bumps. Two Logic cases for the bearing, one for the
  window, one for the panel formatter, one Init case for the client store and one for the gate;
  **18 can-fail faults recorded**. `tools/compile-check.sh` exit 0.
  ⚠ **The `.layout` rows, the widget caching and `RenderAll` are NOT done** — they are a `ui-developer`
  slice and everything it consumes is listed in the session note below.

- ✅ **Phase 9 built, 2026-08-19** — the two enums, `OVT_QRFSiege` (pure, now **six** statics: the ring
  pair, the publish predicate, the minutes pair and the neutralised pair), the three-stage machine on
  `CheckUpdateTimer`, the single-pass spend with its **one** debit and its three loop bounds, the ring as
  a fourth index-parallel array, one `Defend` waypoint replaced by `SearchAndDestroy` at the BATTLE
  transition, the 🔴 zero-agent-is-ALIVE early end behind its seen-alive arming latch, `m_bQRFRevealed` +
  `IsQRFEngaged()` + `RevealQRF()` +
  `OnQRFEngaged()` on the manager, the three world gates moved and the two left alone on purpose, four
  client conjuncts, the HUD's minutes form, and two broadcast presets. Four Logic cases and three Init
  cases added in two new files; **19 can-fail faults injected and compiled**. T9.11's line-by-line
  standard-path re-check came back clean. `tools/compile-check.sh` exit 0.
  🔴 **The All gate came back 1/359 red on the first cut and it was a real D16 defect** — a failed
  `FindEntityByID` was being read as proof of death. Fixed on the caller (the seen-alive arming latch);
  the fixture that caught it was wrong too and was corrected. See the session note.

- ✅ **Phase 10 built, 2026-08-19 (bar T10.3)** — the in-game help now agrees with the shipped machine. Four
  false or stale sentences cut or corrected (the tutorial's surplus-roll clause, `BaseCapture_Text`'s and
  `BaseCapture_Text5`'s retired counter-attack mechanics, `BaseCapture_Text5`'s "no way of taking a tower
  back", and `OccupyingForces_Text5`'s "while a battle is RUNNING" which is now `IsQRFEngaged()`), two
  entries extended (`BaseCapture_Text2`'s accepted tell, `OccupyingForces_Text4`'s recapture path), and one
  new Field Manual page, **Counter Attacks**, authored in The Resistance category with 11 new keys. Every
  sentence carries a `file:line` in its `Comment`. No gameplay code, config or prefab touched;
  `tools/compile-check.sh` exit 0. ⚠ **T10.3 (wiki) is BLOCKED** — see the session note.

- ✅ **Play-test fix, 2026-08-19 — deployments are paused NEAR a battle, and only near one.** Tower
  guards were materialising at a contested base mid-battle. Not a regression: `occupying/qrf`'s
  "the contested base loses its defenders" and `virtualization/base-defense-migration`'s "existing
  deployments are unaffected" are both deliberate and cannot both hold at the objective. The user chose
  **local-only** suppression — `IsQRFEngaged()` + occupying faction + within `QRF_RANGE`, materialisation
  only, standing groups left alone. New pure `OVT_DeploymentBattleSuppression`, a 1 s reconciliation on
  the deployment manager, and one guard on the rebuy. Frozen core untouched. Three cases added (two
  Logic, one Init); **six can-fail faults injected and compiled**. `tools/compile-check.sh` exit 0.

**What's Next:**
- 📋 **T10.3** — the wiki sync. The page content is drafted in the Phase 10 session note below and needs a
  session in which the wikijs MCP tools are actually available.
- ⚠ **T8.9 looks COMPLETE but its checkbox is still unticked.** `UI/Layouts/GM/GMPanel.layout` now carries
  `Row_Objective`/`Label_Objective`/`Value_Objective` and `Row_Phase`/`Label_Phase`/`Value_Phase`, and
  `OVT_GMPanelUIComponent.c` has both `FindText` calls (`:167`) and both `SetText` calls (`:413-417`) through
  `OVT_GMPanelFormat`. Phase 10 did **not** tick the box - it is another agent's slice and may be mid-flight.
  Whoever owns it should confirm and tick, or say what is still missing.
- 📋 **Phase 10** — help & documentation sync. `help-docs-sync`.

**Blockers:**
- None

---

## Key Files

**Owned by this feature (all new):**
- `Scripts/Game/GameMode/Objectives/` — the director, its pure statics (`OVT_ObjectiveSelection`, `OVT_ObjectivePhaseRules`, `OVT_FOBSiting`), its records and its serializer
- `Scripts/Game/GameMode/Deployments/Modules/` — six new modules (insertion + its FOB-raise subclass, town harassment, tower recapture, base sabotage, objective condition)
- `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFSiege.c` — the siege's pure statics (Phase 9)
- `Configs/Deployment/Deployment_Objective*.conf` — six new configs
- `Prefabs/Bases/OVT_OccupyingFOB.et` (P7, the structure + the held dismantle action, `Duration 15`
  authored IN the prefab), `Prefabs/GameMode/OVT_FOBPosition.et` (P7, the curated-site marker - **no world
  layer instances anywhere, by instruction**)
- `Scripts/Game/Components/OVT_FOBPositionComponent.c`, `Scripts/Game/Entities/OVT_FOBPosition.c` (P7)
- `Scripts/Game/UserActions/OVT_DismantleEnemyFOBAction.c` (P7)

- `Scripts/Game/GameMode/Deployments/OVT_DeploymentBattleSuppression.c` — the pure battle-suppression
  rule (play-test fix, 2026-08-19)

**Edited by this feature:**
- `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c` — the shared removal path `DestroyPlacedItem()` (P6, four copies collapsed into it) and the cost join `GetStructureCost()` (P6)
- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` — trigger deletions (P1), two read helpers (P2), the reveal flag + `IsQRFEngaged()` (P9)
- `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` — FOB wave source + bearing bias (P8), the siege mode (P9)
- `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c` — the objective anchor (P3), the insertion cap (P4), the engaged gate (P9), the battle-suppression reconciliation (play-test fix, 2026-08-19)
- `Scripts/Game/GameMode/Deployments/Modules/OVT_InfantrySpawningDeploymentModule.c` — the rebuy's battle guard in `CanReinforce()` (play-test fix, 2026-08-19)
- `Scripts/Game/Configuration/OVT_DifficultySettings.c` + `Configs/Difficulty/*.conf` — eleven fields in, one out (P1)
- `Scripts/Game/Components/Controller/OVT_CampaignRequestComponent.c` — the fifth verb, `DismantleEnemyFOB` (P7)
- `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` + the GM panel (P8)

**Frozen — must stay byte-identical every phase:**
- `Scripts/Game/GameMode/Virtualization/`, `Scripts/Game/GameMode/VirtualMovement/`, `docs/features/virtualization/core/api.md`
- The thirteen shipped deployment configs

---

## Important Decisions

The plan's §5 carries D1–D17 in full. The ones that will bite an implementer first:

- **D2 — retire first.** Phases 1–7 leave the occupying faction with **no offensive trigger at all**. That is the plan, not a bug: v1.5 is unreleased and the user accepted temporary passivity in dev play-tests. Play-tests in that window evaluate defence and deployments, not pressure.
- **D3 — re-selection is a flag, never inline.** `OnBaseControlChange` fires *before* the affiliation is applied, so a handler reading ownership inline reads the old owner.
- **D4 — tick counters, not wall-clock deadlines.** An early return freezes every director timer by construction.
- **D5 — the anchor biases ordering, never eligibility.** No anchor must be byte-identical to today.
- **D8 — poll `m_CurrentQRF` going null; never add a second `m_OnFinished` subscriber.** The manager deletes the controller entity inside the invoker's own dispatch.
- **D14 — the siege is a mode on the existing QRF controller, not a second controller.** The cost is accepted: a mistake breaks player-initiated battles too, which is why Phase 9 is advanced and why F17 is a play-test criterion in its own right.
- **D15 — three flags, three questions.** `m_bQRFActive` = a battle exists (blocks new battles); `m_bQRFRevealed` = the client knows (HUD, map, travel, respawn); `IsQRFEngaged()` = the shooting is on (economy, deployments, civilians). Every new flag defaults so a **standard** battle takes no new path.
- **D16 — the early end resolves ambiguity toward ALIVE.** Zero agents with a live entity counts as alive; `AllNeutralised(0,0)` is false. A false positive hands the resistance the objective for free.
- **D17 — the daylight window is a pure hour predicate on the Phase 3 gate**, with consts on the director rather than difficulty fields. Being outside the window is not a failure: no starvation tick, no reselect, log once.

---

## Gotchas & Learnings

Carried in from the plan and from `virtualization/base-defense-migration` — every one of these has already cost someone a debugging session:

- **The objective anchor biases ORDERING, never ELIGIBILITY — and `OVT_CandidatePosition` makes that easy to get wrong.** Its constructor seeds `sortBy` and `threatLevel` from one number, but only `sortBy` may ever be biased: `threatLevel` is handed to `CheckDeploymentConditions()`, where `m_iMinimumThreatLevel` is a hard gate three shipped configs author above zero, and is then stamped onto the deployment and persisted. Biasing it compiles, sorts identically, and quietly lets an objective-adjacent position buy configs it cannot afford. **§3.5's own pseudo-code does exactly that** — the invariant three paragraphs below it is the binding statement, not the snippet.
- **A director that has no objective did not necessarily go through `ResetObjective()`.** A re-selection during harassment that finds nothing selectable reaches `EnterIdle()` → `ClearObjectiveRecord()` directly. Anything that must be undone when the objective ends belongs in `ClearObjectiveRecord()`, which is the choke point **both runtime paths** share. ⚠ **Construction is not one of them any more** (2026-08-19): `OnPostInit()` calls `ClearObjectiveRecordFields()` instead, because "there is no objective now" is an *event* and construction is the *absence* of one — see the crash note below. The funnel is unchanged for every live path.
- 🔴 **A component's `OnPostInit()` RUNS IN THE WORLD EDITOR, where the game's managers do not exist.** Loading a world in the editor constructs the game mode entity's components without starting a game, so `GetGame().GetFactionManager()` is **null**, and so is anything that resolves through it. `OVT_Global.GetConfig()` and `OVT_Global.GetDeploymentManager()` both answer non-null in that context, so a "did the manager resolve?" guard on the component you called is **not** evidence that the manager *it* resolves through exists. An `OnPostInit()` may allocate its own state and nothing else; every question about the campaign belongs behind `Init()`, `PostGameStart()` or the tick. Cost: a VM exception on world load in the editor, from a director that was only trying to clear an anchor it had never pushed.
- **`CloneModule` copies attributes by hand and silently drops what it forgets.** Six new modules here; every one asserts clone fidelity. ⚠ It is **not chained** — a subclass repeats its parent's whole list. `OVT_InsertionSpawningDeploymentModule` now carries **23** lines (13 inherited + 10 own); anything appended to `OVT_InfantrySpawningDeploymentModule` has to be appended there too.
- **A spawning module registered under one owner key owns EVERYTHING found under it.** `ReclaimHandles` clears and re-derives `m_aHandles` from `FindGroupsByOwner` every convergence, so any second kind of group a module registers (the insertion module's truck crew) needs its **own** owner key — and its wipe needs intercepting before `super.OnVirtualGroupWiped`, or a dead driver marks the whole force eliminated.
- **A group registered with a null plan is a GARRISON, and a garrison at the source base is men who never leave it.** Anything that registers a force away from its objective must supply a plan pointing at that objective, at registration time — core builds the waypoints there and owns them from then on.
- **The core has no post-registration setter for `spawnDistanceOverride`, and `api.md` is frozen.** The insertion module writes `OVT_VirtualGroupRecord.m_iSpawnDistanceOverride` directly and re-stamps `SetLifecyclePolicy` itself (`RestoreGlobalSpawnRing`). Both halves are required: the policy is this session, the record is every future load. If a second consumer ever needs this, ask core for a setter instead of copying the write.
- **`SCR_AIGroup.GetOnAgentAdded()` passes ONE argument** — recover the group via `agent.GetParentGroup()`.
- **`ScriptInvoker.Insert` does not de-duplicate** — `Remove` then `Insert`, always.
- **`.conf` module order is update order, and `.conf` files cannot carry comments** — spawning, behaviour, reinforcement last among behaviour, then conditions.
- **A behaviour module that is NOT last may not delete its own deployment inline.** `DestroyDeployment()` clears `m_aActiveModules` and deletes the owner while `UpdateDeployment()` is mid-`foreach` over weak references to those modules. Use `OVT_BaseBehaviorDeploymentModule.RequestDeploymentCollection()`, which defers one frame and cancels itself in `OnCleanup()`.
- **Count a deployment's force through HANDLES, never through `GetSpawnedEntities()`** — the latter sees only materialised groups, so a dormant force reads as dead. `CollectRegisteredHandles()` + `GetAliveMemberCount()` + `GetPosition()` are true dormant or spawned.
- **A public counter/mutator on the director may NEVER change phase.** Only `DirectorTick()` moves the machine; a phase entry re-arms the phase timeout, so a transition from a public method silently overwrites planted timers and can save a phase nobody asked for. Cost two red cases in two suites (D4 and G6) from one line.
- **"The town is soft enough" is not "this ramp softened it."** Gate a phase transition on a fact about the WORLD that only this feature creates (the `ObjectiveHarassment` modifier on the town), never on the director's own success counter — anything can raise a counter, and a fixture arranging a mid-ramp state does.
- **A fixture that drives `DirectorTick()` must plant a non-zero operation countdown** — a phase entry arms it to zero, so the next tick spends from the shared world's pool, and nothing refunds a deleted deployment.
- **`ForceCreateDeployment` does not debit the pool** — a forcing caller must call `SubtractFactionResources` itself, and only after a create that returned non-null.
- **A placed/built structure leaves the world through `OVT_ResistanceFactionManager.DestroyPlacedItem()` and nowhere else.** `OVT_NavmeshRebuild.Queue(entity)` **then** `SCR_EntityHelper.DeleteEntityAndChildren(entity)`, in that order: `Queue()` measures at CALL time and rebuilds a second later, so reversing them leaves the carve in the navmesh forever and the AI refuses to cross ground that is now empty. The pair used to be copied four times in that one file; it is one method now. ⚠ It is the MECHANISM — `RemovePlacedItem`'s owner-or-officer check is the AUTHORIZATION and stays exactly where it is. A new caller decides at the call site who may ask; nobody gives this a `playerId`.
- **A live structure's type string does NOT match its config entry's `m_sName`** — `"GuardTower"` vs `"Guard Tower"`, `"Bunker"` vs `"Bunkers"`, `"VehicleGarage"` vs `"Garage"`; seven of the eight shipped buildables disagree. **The join is by PREFAB**, `OVT_ResistanceFactionManager.GetStructureCost()`, and its precondition is that no prefab appears in two config entries.
- **There is no registry of placed structures.** Discovery is a `QueryEntitiesBySphere` in the `OVT_ItemLimitChecker.CountItemsForLocation` shape, and its radius for a BASE (**500 m**) has to be matched exactly, or a structure the placement limit counted is one nothing else can ever find.
- **`OVT_Modifier.m_iIndex` is positional** — a new modifier entry goes at the **end** of its config or every live save's town modifiers shift by one.
- **Init-tier worlds never run `PostGameStart`** — a case needing a tick installs it itself.
- **Deployment fixtures must be `SetSpawnedUnitsEliminated(true)`** on the deployment **and every spawning module** before anything ticks; the autotest camera is an observer.
- **The Logic-tier rule is a directory-wide grep that does not distinguish code from comments** — `OVT_Global` and `GetGame().GetGameMode` may not appear anywhere under `TestSuites/Logic/`, prose included.
- **`new` does not apply `[Attribute()]` defvalues** — a hand-built subject needs every field set explicitly.
- **`RandInt` is max-exclusive; `out` and `owned` are reserved; `vector.Distance` is +1 ULP at 1 000/2 000 m; `PrintFormat` takes at most 3 string params.**
- **`Rpc()` arity is a compile-check blind spot (BUG-090)** — a wrong argument count compiles clean and dies silently at the wire.
- **Never hand-edit `Language/*.conf`** — they are Workbench build output. Edit only the `.st` master and report that a re-export is owed.
- **No `maxAttempts` in any test** — the suites are deterministic and a red is real.
- **A restored deployment RAISES NOTHING, and the latch is a separate requirement from the gate.**
  `OVT_FOBRaiseSpawningDeploymentModule.DecideRaise()` refuses on `restoredFromSave` (D11 - otherwise a
  campaign grows one flagpole per load) **and** on `alreadyAttempted` (otherwise a reinforcement rebuy,
  which clears the eliminated flags and re-runs the convergence, puts a second one beside the first).
  Neither substitutes for the other.
- **`OnInsertionArrived()` is reached ONLY from the truck-arrives path.** Five things divert an insertion
  onto foot and none of them calls it, so anything that has to happen "when the force gets there" needs a
  second trigger polling the registered handles at the deployment position. The FOB raise module carries
  one; a future consumer of the hook will need the same.
- **`DEFEND` anchors on where a group is REGISTERED, and an insertion registers at the SOURCE.** A DEFEND
  patrol module on any insertion-based config parks the force at the base it set out from, permanently.
  The insertion module's own cycling fallback march is what holds a force at its objective.
- **The director's forward-base state does NOT replicate (G12), so no client-side code may ask it where
  the base is.** `IsFOBUp()` is false and `GetFOBPosition()` is the zero vector on every remote client. A
  user action asks `CanDismantleFOBAt(callerPos, ownerEntityPos, ...)` about the entity it is attached to;
  the server asks the overload that reads the record. One body, two entry points.
- **`m_FOB.spent` is a COUNTER, and the ceiling is inactive during harassment.** It arms when the forward
  base's own deployment is SENT (so the structure's cost is inside the budget) and disarms on every reset.
  `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` must stay empty - do not quote the
  name in a comment either.
- **A fixture that drives `DirectorTick()` past a give-up must stop AT the grace window.** The tick that
  abandons an objective leaves the machine IDLE, and the IDLE branch of the very next tick selects a real
  objective and arms its operation countdown to zero - so one extra tick buys a real deployment with real
  resources. Both persistence cases drive exactly `FOB_RELINK_ATTEMPTS` and plant a countdown afterwards.
- **The forward-base structure is found for teardown by PREFAB RESOURCE NAME read off the config**, never
  by the raise module's `EntityID`: that link is runtime-only and is gone after any load.
- 🔴 **The wave landing-zone bearing runs TARGET → SOURCE, and the inverse is not a compile error.**
  `GetLandingZone` builds `checkpos = qrfpos + (dir * distance)`, so `dir` points from the objective OUT
  TO the landing zone. Backwards, every wave lands on the far side of the objective from the men who sent
  it and it reads in play as a pathing bug. The convention is `GetRandomDirection`'s own: 0° = North =
  `-Z`, 90° = East = `+X`, so the bearing is `atan2(dx, -dz)` on `source - target`.
- **`OVT_Component` already declares `m_Time` and fills it in its own `OnPostInit`.** A component that
  re-declares it shadows the base class's copy with one nothing else fills in. Read the world hour off the
  inherited field, re-resolving lazily (edit mode skips the fill), exactly as the occupying faction
  manager does.
- **`SetFailure` takes at most THREE parameters after the format string**
  (`SCR_AutotestCaseBase.c:88`) — the same ceiling `PrintFormat` has. A fourth is a compile error, so fold
  the extra value into the literal.
- **A new scalar on `OVT_GMCampaignState` needs THREE edits, and the one that has no symptom is `Clear()`.**
  Missing from `CopyFrom` the row never fills in and somebody notices in a minute; missing from `Clear` the
  row keeps showing the PREVIOUS campaign's value after a second campaign starts in the same client
  session. `CopyRecords` is for the four per-entity arrays only and a campaign scalar does not belong in it.
- **A Game Master record added to the fan is TWO constant bumps, not one.** `CAMPAIGN_RECORD_COUNT` is what
  `SendSnapshotEnd` reports as the total sent, and `WIRE_VERSION` is what makes a mismatched client refuse
  to stage instead of showing a permanently-truncated snapshot. Bumping one without the other is silent.
- **A `#`-prefixed key handed to `TextWidget.SetText` is resolved**, so a formatter may answer either a
  localization key or a proper noun and the call site needs no branch.
- 🔴 **A failed `FindEntityByID` is a reading about the LOOKUP, not about the ENTITY.** An entity that
  is not world-registered answers `GetID()` with `EntityID.INVALID` and every unregistered entity shares
  that value (`OVT_InactiveRecruitGroupComponent.c:76-83`), so an id captured in the same frame as its
  spawn can fail to resolve while the thing is perfectly alive. Anything that concludes "dead" from a
  null lookup needs separate evidence that the id was ever good — the QRF siege carries an arming latch
  (`m_bSiegeForceSeenAlive`) for exactly this, after the first cut shipped the bug and its own Init case
  caught it.
- 🔴 **A group with a LIVE ENTITY and ZERO AGENTS is ALIVE, and `AllNeutralised(0, 0)` is FALSE.**
  "Zero agents = dead" is a known-bad prune in this engine (spawn queue and dormancy both produce it).
  Getting it wrong ends a siege mid-muster and hands the resistance the objective for free, silently.
  The rule lives in `OVT_QRFSiege.GroupNeutralised` so it can be asserted, not only commented.
- 🔴 **A siege can never resolve without passing through `BATTLE`, and that is load-bearing.** Scoring
  is gated on `m_iTimer <= 0`, which `SILENT_DEPLOY` (clock parked at 120 000) and `MUSTER` (reaching
  zero *is* the transition) both make unreachable. The civilian suppression is a **paired** invoke —
  BATTLE and again at the finish — so a path that skipped BATTLE would leave a town permanently empty.
  **Nothing may introduce one.**
- **The QRF controller's spawn arrays are INDEX-PARALLEL and there are now FOUR of them.**
  `m_aSpawnQueue` / `m_aSpawnPositions` / `m_aSpawnTargets` / `m_aSpawnRingSlots`. `SendTroops` clears
  all four and `SpawnFromQueue` pops index 0 off all four. Missing one gives every later group somebody
  else's slot, with no symptom but groups standing in the wrong place.
- **The QRF debit is the only place a battle spends the war chest, and it sits OUTSIDE the mode branch.**
  Both `SendWave` paths fall through to it exactly once. Double-debiting or skipping it is BUG-027's
  shape in a new place.
- **`m_iTimer < 105000` is a STANDARD-mode expression, not a general "wait for the world to despawn".**
  A mode whose clock does not move makes it permanently false. Any new mode needs its own drain
  condition.
- **A loop that cycles a source list on the server thread needs an iteration counter as well as a
  budget.** `baseResourceCost` misauthored to 0 makes the per-source slice 0, which makes the inner
  allocation loop false on entry — forever. A zero-progress break plus a hard pass ceiling, both.
- **`AddWaypoint` APPENDS, and a `Defend` waypoint is never completed.** Remove every held waypoint
  before adding the assault order or the group defends its ring slot for the rest of the battle. The
  vanilla shape is `GetWaypoints` → `RemoveWaypoint` in a loop, and it does **not** delete the waypoint
  entities.
- **`SetFailure` takes at most three parameters after the format string.** A fourth is a compile error;
  fold two values into one string at the call site.
- ⚠ **`OVT_OccupyingFactionManager.m_CurrentQRFBase` and `m_CurrentQRFTown` are NEVER CLEARED.** Both
  finish handlers reset the `m_iCurrentQRFBase` / `m_iCurrentQRFTown` **indices** to -1 and leave the
  object handles holding the last battle's place. Anything new that reads a handle must check the
  matching index first, or a base battle will name whatever town was fought over previously.
- **`SetLifecyclePolicy(Manual)` IS THE ONLY WAY TO STOP A REGISTERED GROUP MATERIALISING, and it
  despawns nothing.** It assigns the policy, moves the distance fields **only for positive arguments**
  and clears the `FRAME` mask (`SCR_AIGroup.c:2915-2951`), so `LifecycleTick()` stops being called and
  a request already in the engine's queue is refused at dispatch by
  `Modded/SCR_AIGroup.RefusesUnrequestedManualSpawn()`. `ForceDespawn()` is **not** an alternative:
  `api.md` says force is a nudge and not a pin, so the group's own 1 Hz tick undoes it a second later —
  a despawn-on-sight loop is a flicker, not a suppression. ⚠ **Pass NO distances in either direction:**
  omitted/negative arguments leave the group's bands alone, so the release call restores exactly what
  core stamped, and nothing here needs a copy of core's despawn hysteresis (protected, no getter).
- 🔴 **`SPAWN_DISTANCE_GLOBAL` RESOLVES TO the global ring, so `spawnDistance >= GetGlobalSpawnDistance()`
  matches EVERY ordinary deployment group.** A guard written that way to spare the always-materialised
  registrations (the insertion module's riding passengers) skips the entire shipped world instead and
  suppresses nothing at all — with no error, no log line and no test that would notice. It has to be
  **strictly** wider. Caught in review of the 2026-08-19 play-test fix, not by a case.
- **A LIVE POLICY CHANGE IS SESSION-ONLY; `m_iSpawnDistanceOverride` IS IN THE SAVE.** Write the record
  and a campaign saved mid-battle comes back with pinned garrisons forever. Write only the group entity's
  policy and a load restores every authored ring for free. The insertion module writes **both** halves
  because it needs its ring across loads; anything transient must write **neither**.
- 🔴 **"The objective must not be penalised for waiting" is NOT "the phase stops".** The daylight
  wait holds the **phase timeout only**. Starvation and the operation cadence keep running, because
  starvation is the resistance's counterplay and answers to the world rather than to the clock — freeze it
  and a forward base cut off at 22:00 survives until dawn and attacks anyway (F7 regression). D17's
  original wording said otherwise and was corrected; see the Phase 8 session note.

---

## Session Notes

### 2026-08-19 — PLAY-TEST FIX: the phase timeout is now an IDLE clock, an operation in flight holds it, and an unfinished operation is REFUNDED

#### The report, in real time at 6x

```
13:03:02  Objective: base 'Levie' at score 69.1 (the only candidate)
13:34:14  Created 'Objective Sabotage' ... Sent for 100 resources
13:34:26  Insertion: driving 2426 m to a landing zone
13:37:56  Insertion is on foot: transport stopped 1561 m short
13:43:08  (W) Objective 'Levie' ended: harassment ran out of time without reaching the forward-base gate
13:43:28  Objective: base 'Levie' at score 75.4 (the only candidate)
```

**Three separate defects in six lines, and the loop had no exit.** `m_iPhaseTimeoutTicks` is 240 in-game
minutes, which at this time multiplier is about 40 real ones. The first 31 of them were spent unable to
afford a single 100-resource operation; the create is refused and the countdown is re-armed only on
success, so it correctly retried every minute — and the phase clock, counting from the phase ENTRY,
ran the whole time. The operation that finally launched had five minutes of budget left and a
fifteen-to-twenty minute walk in front of it. It was deleted mid-walk with its truck, its 100 resources
were never returned, the same base was re-selected 20 seconds later, and **the ramp could never have
reached Phase 2 from that state**.

⚠ **THE POVERTY WAS SELF-INFLICTED AND IS NOT AN INCOME DEFECT.** Confirmed by the user the same day: an
early-game campaign on Easy, a QRF cheated in for Levie Base, then a Game Master wipe of *both* the base
and the QRF, which left the occupying faction depleted. *"In a normal game they would have much more
time to accrue resources before I'm able to take a base... I don't think it's an issue. If there is an
issue with resource depletion it will show up in a real non-cheat play-test."* **No income investigation
was carried out and none is owed** — the `/give-resources` admin command exists to paper over exactly
this during development. What the poverty did do is expose the pacing defect, which is real regardless of
how the pool got empty.

#### Fix A — the phase timeout measures IDLENESS, not phase age

`m_iPhaseTimeoutTicks` keeps its name, its value (240) and its job as **a backstop, not a pacing knob** —
but it now counts in-game minutes since the objective last did anything, and **its `desc:` was rewritten
to say so**, because a field whose meaning changed and whose description did not is a trap.

**What counts as progress, and where it is defined:** `TickObjectiveIdleClock(bool created)` on the
director, called LAST by each of the two ramp phase handlers. Three signals, exactly as specified:

| Signal | How it is detected |
|---|---|
| **An operation was created** | `SendNextOperation()` / `SendNextFOBOperation()` changed from `void` to `bool` and the handler passes the answer straight in |
| **An operation reported complete** | `ConsumeReportedOperations()` compares `harassmentSuccesses`/`sabotageSuccesses` against `m_iProgressHarassmentMark`/`m_iProgressSabotageMark` |
| **A phase transition** | `EnterPhase()`, which already re-armed the clock, now does it through `SetPhaseTimeout()` |

🔴 **THE SUCCESS SIGNAL IS PULLED BY THE TICK, NEVER PUSHED BY THE COUNTER, AND THAT IS D4.**
`OnHarassmentSuccess()` and `OnSabotageSuccess()` are public, are called from a deployment's own update,
from a restore and from fixtures arranging a state, and **two shipped cases pin them as unable to move a
timer** — Phase 5 already paid for that lesson with two red suites. Re-arming from inside either of them
would have reintroduced it exactly. So the marks live on the director and the tick compares them.

⚠ **`SetPhaseTimeout()` now re-baselines the marks as well as setting the clock**, and that second half is
load-bearing. Planting a clock means "this much patience remains **as of now**"; without the re-baseline,
successes that were already banked (a fixture arranging "three completed", a restore adopting a saved
count, the very successes that opened a gate) would read as fresh news on the next tick and immediately
overwrite the value that was just planted. `ApplyPersistedObjective()` calls `SyncProgressMarks()` by hand
**after** both counters, because the payload writes the clock *before* them.

#### Fix B — the clock is HELD while an operation is in flight, and while the block is only affordability

**In-flight detection: `HasOperationInFlight()`.** It walks the director's own teardown ledger
(`m_aCreatedDeployments`) — the only count in the component that legitimately does, because the question
is not "is anything of this kind nearby" but "is something **we** sent still out there" — resolves each
entry with the same `GetDeploymentNearPosition(name, pos, TEARDOWN_LOOKUP_RADIUS)` lookup the teardown
uses, and answers true on the first one that both resolves and is **not** flagged
`GetSpawnedUnitsEliminated()`. A team the resistance killed is not "on its way"; its marker can outlive it
by a frame or a minute, and reading a dead team as work in progress would hold the clock open on a corpse
forever. Exposed as the public reader `IsOperationInFlight()` so a case can assert the state that changed
the outcome.

🔴 **THE SCOPE IS THE PART THAT KEEPS R1 ALIVE — `IsObjectiveOperationConfig()`.** Only the harassment
ladder, tower recapture, sabotage and **the forward-base supply party while `m_FOB.up` is still false**
count as operations. If a *standing* forward base or its garrison counted, the forward-base phase could
never time out at all: a base that is up with a full garrison and a counter-attack gate that will never
open is precisely the wedge the backstop exists to catch. An unclassified config fails safe to "not an
operation", which neither holds the backstop open nor pays anything back.

**⚠ ONE PREDICATE, TWO USES, AND THAT IS DELIBERATE.** "Is this operation in flight" and "is tearing this
down a recall rather than a write-off" are the same question about the same deployment, so both read
`IsObjectiveOperationConfig()` and can never disagree. `ResetObjective()` tears the ledger down **before**
`ClearFOBRecord()`, so the `!m_FOB.up` clause still reads the truth during a teardown.

**Affordability: the D17 shape, one layer up.** `CreateObjectiveDeployment()` sets a per-tick flag
`m_bBlockedOnAffordability` on **the pool test and nothing else**; `TickObjectiveIdleClock()` consumes it
and holds. This is the same argument that corrected D17 for the daylight wait — *freeze the clock the
director runs against itself when the block is not the objective's fault* — and it is written at both
sites with a pointer to the other. ⚠ The forward base's spend **ceiling** deliberately does NOT set the
flag: a spent ceiling is a decision the director made about itself, and a phase that can only ever hit
that should time out.

⚠ **THE HOLD COVERS THE WHOLE POVERTY SPELL, NOT ONE TICK IN FORTY-FIVE.** A refused create leaves
`nextOpTicks` at zero, so *every* minute of a poverty spell reaches the spender and is refused again. That
pre-existing retry is what makes a per-tick flag sufficient.

#### 🔴 WHAT HAPPENS TO AN OBJECTIVE THAT CAN NEVER BE AFFORDED — the state this design lets persist

Written down because it is the one place the backstop is deliberately switched off, and the second brief
asked for it explicitly:

- **It SITS.** The clock is held, so it is never abandoned. It is never abandoned because the next
  objective would be exactly as unaffordable — abandoning would churn targets and achieve nothing, which
  is the observed treadmill.
- **Nothing accumulates and nothing leaks while it waits.** No deployment is created, no resource moves in
  either direction, `m_aCreatedDeployments` does not grow, the anchor is not re-pushed, the cadence stays
  at zero, and the log line is latched to **exactly one per objective**.
- **It recovers by itself.** The first tick on which the pool can cover an operation creates one; that
  create is progress, the clock re-arms, and the ramp continues from where it stopped.
- **It is diagnosable from the log alone.** `LogAffordabilityBlock()` emits one WARNING naming the
  objective and saying the clock is held and why — then silence, then the ordinary
  `Sent '<config>' ... for N resources` line, which **is** the recovery marker. The play-test spent 31
  real minutes with nothing at all in the log; that is the gap this line closes.
- **It is not a silent wedge**, because the objective still answers to everything else: a control change
  re-selects, starvation still runs in the forward-base phase, and a Game Master can see the objective and
  its phase on the campaign panel.

#### Fix C — an unfinished operation is RECALLED, and the route taken for Q6

**Route chosen: through the deployment framework**, so `Q6`'s `Objectives/` clause stays literally true.
`OVT_DeploymentManagerComponent.RecallDeployment(deployment)` is new — "delete **and** refund what is
recoverable" — and `TearDownObjectiveDeployments()` calls it for entries `IsObjectiveOperationConfig()`
classifies as operations, and plain `DeleteDeployment()` for everything else.

**Why not a director-side credit.** `Q6` is written as a grep and the grep is the point: the director
subtracts and never credits. A credit in `Scripts/Game/GameMode/Objectives/` would have been a fourth kind
of funding path. ⚠ The comment explaining all this **does not spell the method's name**, because a bare
grep does not know a comment from a call and the clause is "empty, comments included".

**Q6 WAS AMENDED ANYWAY, WITH THE REASON, IN `implementation.md`** — its `Scripts/`-wide clause said "the
deployment framework's own refund" (singular) and there are now two, both inside the framework
(`RecallDeployment` and the patrol module's `RecoverResources`). The `Objectives/` clause is unchanged and
still empty. A new check `7a` pins the exact five-line tree-wide answer.

**G5's "exactly once" is not weakened; it is structural on four counts:**

1. `RecallDeployment()` zeroes `m_iResourcesInvested` **before** it credits, so a second call reads 0.
2. The ledger is `Clear()`ed by the same teardown, so a lookup cannot find the deployment twice.
3. The deployment is destroyed, so it cannot be found by position either.
4. A force flagged eliminated refunds **nothing** — a team the player killed is a loss, not a recall, and
   paying for it would pay the occupying faction for losing a fight. Same rule the patrol module's
   `OnPatrolComplete()` already applies, for the same reason.

A *standing* forward base or garrison is never recalled at all: its money bought exactly what it was for.

#### What moved in the two tick handlers — and why the order is the fix

Both handlers now serve the **cadence** first, do their phase's work, and serve the **idle clock last**,
because it is the only step that has to know whether the tick accomplished anything. Served first (as it
was) it could only ever mean "minutes since the phase began", and it could abandon an objective on the
very tick that was about to advance the phase or send the operation the phase was waiting for.

| Handler | Order now |
|---|---|
| `TickHarassment()` | clear flag → cadence → gate out → at most one operation → idle clock |
| `TickFOB()` | counter-attack gate → clear flag → cadence → starvation → at most one operation → **return if waiting for daylight** → idle clock |

⚠ **EVERY TRANSITION IS STILL ON `DirectorTick()` BEHIND ITS THREE EARLY RETURNS.**
`TickObjectiveIdleClock()` returns a **verdict** and the caller resets; nothing was moved into a callback.
⚠ **The daylight wait is unchanged**: it still holds the clock and still does not test it, and it drops
the per-tick affordability flag on the way out so a refusal seen during a wait cannot leak forward.
⚠ **The idle clock refuses to run against a cleared objective** (`kind == NONE`) — `SendFOBOperation()`
can reset the objective mid-tick when there is nowhere to put a forward base, and without that guard the
clock would have called `ResetObjective()` a second time on an empty record, which with a refund in the
path is the one shape that could double-credit.

#### Tests — two cases changed (with the reason), three added

**CHANGED — and neither is a weakened assertion:**

1. `OVT_TEST_Init_ObjectiveOperations_GateNeedsTheRampsOwnDebuff`. It plants a clock, counts **three**
   harassment successes, then drives a tick and requires exactly one round served. Under the new contract
   a tick that sees three brand-new successes is a tick that observed progress and legitimately re-arms,
   so the case would have read 240 instead of 60. **One line added**: the clock is re-planted between the
   two halves, which is how a fixture says "those successes are arrangement, not this minute's work".
   *Both original assertions survive verbatim and both keep their full strength* — counting still moves no
   timer, and the "a value far above the planted one is a phase entry re-arming it" canary is still sharp
   precisely because the marks are baselined.
2. `OVT_TEST_PersistenceRoundTrip` (the objective round-trip case). Its fixture planted the clock at 137
   and *then* counted 3 + 2 successes. This world runs a live director on a repeating timer and the step
   spans several frames before the save settles, so one background tick would have re-armed the saved
   clock to 240 and `AssertBand` would (correctly) have reported a value higher than the one saved.
   **The two `Set*` calls were REORDERED to after the success loops.** No constant and no assertion
   changed.

**UNCHANGED and re-verified by hand against the new code:**
`OVT_TEST_Init_ObjectiveDirector_FreezesEveryTimerWhileABattleIsLive` (its tick has no progress, no
in-flight operation and a cadence that has not elapsed, so it still serves exactly one round) and
`OVT_TEST_Init_ObjectiveDirector_GateWaitsForDaylightThenFiresOnce` (the wait still holds the clock; the
`SetPhaseTimeout(77)` call after its sabotage loop now also baselines the marks, which is what keeps the
night half reading 77).

**ADDED — three Init cases in `OVT_TEST_Init_ObjectiveDirector.c`, sorting after the daylight case:**

- `..._IdleClockHoldsWhileTheFactionCannotPay` — a funded tick serves one round (so the held half is not
  vacuous), then the pool is emptied and the cadence dropped to zero: the clock must not move **and the
  pool must not move**, and the objective must survive. Two preconditions guard it: the rung must cost
  something, and the preset must allow at least one concurrent operation.
- `..._IdleClockRearmsWhenAnOperationReports` — an idle tick serves one round; `OnHarassmentSuccess()`
  moves **no** timer (D4); the next tick re-arms to `GetPhaseTimeoutTicks()`; the tick after that serves
  an ordinary round, proving the news is *consumed* rather than latched.
- `..._InFlightOperationIsHeldThenRefunded` — drives the director's **own** spend path with a planted pool:
  a create re-arms the clock, the operation reports as in flight, a clock planted at 1 is re-armed rather
  than expiring, the teardown returns the pool to exactly where it was, and a second buy that is flagged
  eliminated returns **nothing**. The refund is asserted as a pool delta, never as a literal cost.

**Can-fail proofs are recorded at each case** (P1–P2, R1–R3, F1–F4), with the exact edit and the exact
message. F4 is written as an **honest non-detection**: this case only ever creates harassment-phase
operations, so mis-classifying `FOB_GARRISON_CONFIG` as an operation is not caught by any tier and is
argued only in `IsObjectiveOperationConfig()`'s header. ⚠ **No `maxAttempts` anywhere**; every new case is
synchronous and drives `DirectorTick()` by hand.

**⚠ New public readers added for the tests to be able to say anything:** `GetPhaseTimeoutTicks()` (so a
case asserts a re-arm against the **authored** budget rather than a copy of 240) and `IsOperationInFlight()`
(side-effect free, in the shape `IsCounterAttackReady()` already established).

#### Play-test items this fix adds

- 🔴 **Does an operation now survive to reach its target?** The headline. Watch a sabotage or harassment
  team whose transport drops it far out and confirm the objective is still alive when they arrive.
- 🔴 **Does a broke director say so, exactly once?** Drain the pool (a Game Master wipe will do it) and
  look for one `cannot afford its next operation` WARNING, then no repeats, then a `Sent ...` line when
  the pool recovers — with the same objective still selected.
- **Does an abandoned objective give the money back?** Force a genuine idle timeout with an operation
  alive and confirm the deployment pool rises by the operation's cost. The `Recalled deployment ... N
  resources returned` line is the marker.
- **Does a wiped-out team return nothing?** Kill an objective operation outright, then end the objective,
  and confirm no refund line appears for it.
- **Does the ramp now reach Phase 2 at all?** The whole point. A campaign that previously looped on one
  base should now get a forward base up.
- **Save/reload mid-ramp.** Uncovered by the change's own cases: load a campaign saved with a live
  operation and confirm the restored clock is not immediately re-armed by the restored success counters.

#### Owed

- **The suites.** Not run here by policy (`.claude/test-policy.md`) and the user was mid-play-test.
- **Gate:** `tools/compile-check.sh` exit **0** (6183 files, Game module). Frozen paths verified empty.


### 2026-08-19 — PLAY-TEST FIX: deployments are paused NEAR a battle, and only near one

#### The report

*"During a player-initiated QRF on a base, tower guards appeared at the contested base mid-battle.
Deployments should be paused during QRF."*

#### 🔴 THIS WAS NOT A REGRESSION. TWO SHIPPED FEATURES' DESIGNS DISAGREED, ON PURPOSE, IN WRITING.

Both statements below are deliberate and both are still in their own feature's docs. They simply cannot
both hold at the place being fought over, and nobody noticed until the migration made the second one
true of base defence:

| Feature | What it says |
|---|---|
| `occupying/qrf` (`context.md:43`) | "all garrison/patrol/civilian spawn predicates check `!m_CurrentQRF` … **the contested base loses its defenders by design** (perf headroom)" |
| `virtualization/base-defense-migration`, via `EvaluateDeployments()`'s header | "Existing deployments' groups live entirely on the engine's lifecycle and are **unaffected by either** \[guard], which is a strict improvement on the old behaviour where the whole force also stopped being maintained" |

The QRF's sentence was written when base defence was **base upgrades**, which honoured it by not
spawning while a battle existed. `base-defense-migration` moved base defence onto **deployments**, and
a deployment's groups materialise on the engine's proximity lifecycle with no QRF predicate anywhere in
the path. `EvaluateDeployments()`'s QRF guard — which Phase 9 moved to `IsQRFEngaged()` and which is
still correct — only ever blocked **creating** deployments. What the player saw was an **existing**
garrison coming up by proximity, untouched by any gate.

#### THE USER'S DECISION, AND THE EXACT RULE AS BUILT

**Suppress only near the battle.** The contested area loses its defenders, exactly as the QRF has always
intended; the rest of the map keeps its forces maintained, preserving the migration's improvement.

The rule, all four conjuncts, is `OVT_DeploymentBattleSuppression.SuppressesMaterialisation()`:

1. **ENGAGED** — `OVT_OccupyingFactionManager.IsQRFEngaged()`, never "a battle exists". A counter-attack
   siege must suppress **nothing** during `SILENT_DEPLOY`/`MUSTER`, or the objective town emptying of
   its garrison becomes exactly the tell §3.9 exists to avoid. A **standard** battle is engaged from
   creation, so a player-initiated battle takes no new path — the same D15 discipline as every other
   Phase 9 gate.
2. **OCCUPYING FACTION ONLY** — compared by faction **key** (a virtualization record carries a key;
   indices are positional across saves). Resistance deployments are never suppressed: this battle's
   zone-control scoring deliberately counts resistance AI, so nerfing the player's own committed forces
   mid-battle would score them down for it.
3. **WITHIN `OVT_QRFControllerComponent.QRF_RANGE` (750 m) of `m_vQRFLocation`** — the constant is read
   from the battle component, never copied, so the suppressed circle is the same one the battle already
   scores, vetoes fast travel into and blocks respawn inside.
4. **NOT ALREADY UP** — a group with members in the world is left completely alone.

#### WHERE IT IS GATED, AND WHY THERE

**Two arms, both at the deployment/module seam. The virtualization core is untouched**
(`git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/
docs/features/virtualization/core/api.md` is empty).

**Arm 1 — materialisation, `OVT_DeploymentManagerComponent.TickBattleSuppression()`,** a new 1 000 ms
repeating call installed in `PostGameStart()` beside the 30 s evaluation:

- a suppressed group is parked on the engine's **`SCR_EAIGroupLifecyclePolicy.Manual`** policy.
  `SetLifecyclePolicy` assigns the policy, moves the distance fields **only for positive arguments**,
  and clears the `FRAME` mask — it never despawns anything (`SCR_AIGroup.c:2915-2951`). So
  `LifecycleTick()`, the only thing that turns observer proximity into a `RequestSpawn`, stops being
  called at all, and a request that was **already queued** when the policy flipped is refused at
  dispatch by `Modded/SCR_AIGroup.RefusesUnrequestedManualSpawn()`, which exists for exactly this state.
  Two independent layers, both already shipped, neither of them new code;
- **no distances are ever passed, in either direction.** That is the whole restore strategy: negative /
  omitted distances leave the group's bands untouched, so the release call puts back exactly the ring
  core stamped at registration. Nothing here needs a copy of core's despawn hysteresis (which is
  protected with no getter), and no module's deliberate re-banding is overwritten;
- **the record is never written.** `OVT_VirtualGroupRecord.m_iSpawnDistanceOverride` **is** in the save
  payload; the live policy is not. A campaign saved mid-battle therefore comes back with every group on
  the ring its config authored — which matches `occupying/qrf`'s own "a load mid-battle cleanly rolls
  back to no battle". This is deliberately **not** the insertion module's precedent, which writes both
  halves because it needs its ring to survive a load;
- **the release pass runs first and unconditionally**, reconciling the pinned list against the same
  predicate rather than firing from a battle-ended callback. One missed release is a garrison that never
  materialises again for the rest of the campaign, with no log line and no symptom but empty ground;
- scope is `FindGroupsBySystem(OVT_BaseSpawningDeploymentModule.OWNER_SYSTEM)`, **not** a walk of the
  deployment list. `CollectRegisteredHandles()` is overridden by the infantry module only, so a
  deployment walk would miss the vehicle module's crews — a base's armour crews would keep materialising
  while its garrison did not;
- two registrations are skipped by their resolved ring: `<= 0` (already Manual by design — releasing one
  would hand it a proximity lifecycle it was registered without) and **strictly wider** than the global
  ring (the insertion module's riding passengers and crew, which cannot be seated if they do not exist).
  ⚠ **Strictly wider, not "at least as wide": every ordinary group registers `SPAWN_DISTANCE_GLOBAL`,
  which RESOLVES TO the global ring, so `>=` there would suppress nothing at all.** That was caught in
  review, not by a test.

**Arm 2 — the rebuy, `OVT_InfantrySpawningDeploymentModule.CanReinforce()`:** refuses while
`IsBattleSuppressedAt(deployment position, controlling faction)`. This is the one path in the framework
that puts a **new** force on the ground during a battle and is the likeliest single cause of the
report — the player wipes a module, the reinforcement behaviour notices within 60 s, and the
replacement materialises on the spot because the player is standing right there. Gated in
`CanReinforce()` rather than in the behaviour module because `Reinforce()` consults it itself, so one
guard covers every caller; refused **before** anything is charged, because arm 1 would pin the new
groups dormant anyway and buying them would be resources leaving the pool with nothing to show for it
(BUG-027's shape).

#### WHAT IS PROVABLY UNAFFECTED

- **The QRF's own troops.** They are spawned straight from a prefab by
  `OVT_QRFControllerComponent.SpawnFromQueue()` (`GetGame().SpawnEntityPrefab`) and are **never
  registered with the virtualization core** — no record, no owner system, no survivor mask, no handle —
  so `FindGroupsBySystem` cannot see them and no deployment module owns them. Verified by reading the
  method, not assumed.
- **The siege's silent stages.** `IsQRFEngaged()` is `false` for a `COUNTER_ATTACK` controller until it
  reaches `BATTLE`; both arms read it. Pinned by an Init case that drives one siege from
  `SILENT_DEPLOY` to `BATTLE` and asserts the answer flips at exactly that transition and nowhere else.
- **Free-at-game-start seeding.** Deliberately left alone. It carries no QRF guard because it is the
  world's opening state rather than a decision, it charges nothing, and it runs at +9 s — a campaign
  cannot have an engaged battle then. Adding a gate there would only be able to *lose* baseline
  garrisons on a continued campaign loaded during… nothing, since a load rolls a battle back anyway.
- **`EvaluateDeployments()`'s creation guard.** Byte-identical. Its **header** gained a pointer
  paragraph so the next reader of "unaffected by either" learns where the exception now lives.
- **Civilians**, the economy tick, and every client-facing rule (`m_bQRFRevealed`). Untouched.

#### 🔴 FEASIBILITY VERDICT — DESPAWNING GROUPS THAT ARE ALREADY STANDING (asked for, NOT implemented)

**Feasible without touching frozen core, and cheap — about three lines — but it is a gameplay call, not
an engineering one, and it should be decided separately.**

- **Mechanism exists and is public.** `OVT_VirtualizationManagerComponent.ForceDespawn(handle)` calls
  `SCR_AIGroup.DespawnMembers()`. It would go in `ApplyBattleSuppression()`, and the `IsSpawned()` skip
  in `SuppressForcesAroundBattle()` would come out. **Strictly additive on what was built**, because a
  despawn alone is not sticky: the group's own `LifecycleTick` re-requests it a second later, so it only
  works *combined with* the Manual pin this change already applies.
- ✅ **The survivor-mask hazard is ALREADY MITIGATED, which is the one genuinely dangerous part.** The
  engine's `DespawnMembers` records `alive = GetAgentsCount()`, so a despawn landing during an
  in-progress refill writes the not-yet-spawned slots down as **dead**, permanently
  (`totalSlots - dormantDead` never re-corrects). `Modded/SCR_AIGroup.DespawnMembers()` overrides this
  and calls `ReassertOVTDormantCounts()` from the mask on every despawn, so a core-registered group is
  safe. Despawning is not dying and the per-slot mask is untouched — a garrison shot down to one man
  comes back as one man.
- 🔴 **THE REAL COST: held members are reported but NOT protected.** `api.md` §3 is explicit —
  "`ForceDespawn` reports a held member (in a vehicle, player-engaged) and despawns anyway". A garrison
  in a firefight with the player **is** player-engaged by definition, so this deletes the men the player
  is currently shooting at, mid-trade, with no death and no XP. `UnregisterGroup` is the call that
  respects held members, and it retires the group permanently — not what is wanted.
- 🔴 **Second cost: it throws the player's work away in appearance.** Under the legacy base-upgrade
  design this was invisible, because those defenders were spawned *by* the upgrade and the base was
  cleared before a capture could be started at all. Under deployments they are persistent, mask-tracked
  individuals — vanishing every one of them inside 750 m the instant the player presses "start base
  capture" is a visible event that the old design never produced.
- **Not attempted, per instruction.** If it is ever wanted, the honest middle ground is to despawn only
  groups with **no** held member (`SCR_AIGroup.HasHeldMember()` is public and is what `ForceDespawn`
  already consults for its log line) — which gets the perf headroom the original QRF design was after
  without deleting anybody under fire.

#### TESTS ADDED

- `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_DeploymentBattleSuppression.c` — **two Tier A
  cases**. The rule (unengaged suppresses nothing anywhere; engaged + occupying + inside is suppressed
  on the battle and one metre inside the ring; nothing one metre outside or 5 km away; the resistance
  and a third faction never; an empty occupying key never, including against an empty force key), and
  the ring on its own (positive, zero distance is inside, symmetric on all four horizontal axes).
- `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_DeploymentBattleSuppression.c` — **one Tier B case**
  for the wiring: no battle suppresses nothing; a standard battle suppresses the occupying faction
  inside its ring only and never the resistance; and 🔴 the same siege suppresses **nothing** in
  `SILENT_DEPLOY` and **does** suppress in `BATTLE`, which is what proves the gate is the shooting and
  not the mode. Fixture discipline copied from `OVT_TEST_Init_QRFSiege.c`; `Start()` is never called,
  both objective indices are forced to -1, and every manager field is restored before the first
  assertion.
- ⚠ **THE EXACT RING IS DELIBERATELY NOT ASSERTED.** `vector.Distance` is not correctly rounded (a full
  ULP high at 1 000 m and 2 000 m, measured), so a row planted at exactly 750 m would be asserting which
  side of a rounding error the build lands on. Both boundaries are pinned one metre either side, which
  is the claim that matters and cannot flake. Whoever probes `vector.Distance` at 750 m may tighten it.
- **Six can-fail faults injected and compiled, one at a time, each restored byte-identically**
  (`git diff` of both subjects verified clean afterwards): S1 drop the engagement guard, S2 invert the
  faction test, S3 drop the range test, S4 drop the empty-key guard, W1 compare the range the wrong way
  round, and 🔴 **B1 — swap `IsQRFEngaged()` for `m_bQRFActive` in `IsBattleSuppressedAt()`**, i.e. the
  exact D15 mistake this design turns on. **Every one exited `tools/compile-check.sh` with 0**, which is
  the point: none is a script error and nothing else in the tree would stop it shipping. The expected
  failure text is recorded per case; the suites were not run (`.claude/test-policy.md`).

#### WHAT IS OWED

1. **Play-test the original report.** Start a base capture, let the battle run, and walk the whole base:
   no new guards. Then let it finish and confirm the garrison comes back — that second half is the one
   that catches a missed release, and its failure mode is silent.
   ⚠ **DO NOT EXPECT THE GARRISON BACK WHILE YOU ARE STANDING ON IT.** Vanilla's
   `m_fVeryNearBlockDistance` is **150 m** and blocks a fresh materialisation for any observer inside
   it, precisely to avoid pop-in — and the release deliberately re-enters `ProximityDriven` through the
   transition that arms that block. So the correct observation is: **walk 200 m away and come back.** A
   garrison that is still missing then is a real missed release.
   ⚠ **WHAT ARM 1 ACTUALLY CATCHES, so a null result is not misread.** Everything within 750 m of the
   battle is also within the player's own 1 750 m spawn ring, so the groups it holds back are the ones
   that are **not up yet** at that moment: those still queued behind the engine's importance-ordered AI
   spawn budget (a base with four or five deployment configs never materialises all of them at once —
   that IS "guards appearing over the next few minutes"), those a deployment registers mid-battle on its
   first convergence, and anything that despawns and would otherwise come straight back. Groups already
   standing are left alone on purpose. The rebuy — the loudest single source of "a fresh squad appeared
   where I just killed one" — is stopped by arm 2 before it is even bought.
2. **Play-test a counter-attack siege** through `SILENT_DEPLOY` and `MUSTER` and confirm the objective's
   garrison is still standing throughout, then that suppression starts at the assault.
3. **A save taken mid-battle, reloaded**, to confirm no group comes back pinned. The design says it
   cannot (the pin is never written to the record and the battle does not persist), but it is one save
   to check and the failure would be permanent.
4. Run the suites. Record the observed reds for the six faults into the case headers.

#### FILES

- **New:** `Scripts/Game/GameMode/Deployments/OVT_DeploymentBattleSuppression.c`,
  `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_DeploymentBattleSuppression.c`,
  `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_DeploymentBattleSuppression.c`
- **Edited:** `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c` (the list, the interval, the
  allocation, the tick install, the restore-clears-the-list line, the suppression block, the public
  `IsBattleSuppressedAt()`, and a pointer paragraph on `EvaluateDeployments()`'s header),
  `Scripts/Game/GameMode/Deployments/Modules/OVT_InfantrySpawningDeploymentModule.c` (one guard in
  `CanReinforce()`), `docs/features/occupying/qrf/context.md` (the `:43` bullet **amended, not deleted**)
- `tools/compile-check.sh` exit 0 (6183 files).

---

### 2026-08-19 — 🔴 CRASH FIX: the director took the World Editor down on world load

#### The report

Loading a world in the **World Editor** — before pressing play, so no campaign exists — threw:

```
Virtual Machine Exception
Reason: NULL pointer to instance. Variable 'fm'
Class:    'OVT_OverthrowConfigComponent'
Function: 'GetOccupyingFactionIndex'
Stack trace:
  OVT_OverthrowConfigComponent.c:420   GetOccupyingFactionIndex
  OVT_ObjectiveDirectorComponent.c:3219 DropObjectiveAnchor
  OVT_ObjectiveDirectorComponent.c:3830 ClearObjectiveRecord
  OVT_ObjectiveDirectorComponent.c:461  OnPostInit
```

#### THE GENERAL LESSON, WHICH IS WORTH MORE THAN THE FIX

**A component's `OnPostInit()` runs in the World Editor, where the game's managers do not exist.**
Loading a world constructs the game mode entity's components without starting a game. `GetGame().GetFactionManager()`
is null there, and so is everything that resolves through it.

Two corollaries that are not obvious and both bit here:

1. **`OVT_Global.GetConfig()` and `OVT_Global.GetDeploymentManager()` BOTH answer non-null in the editor.**
   `DropObjectiveAnchor()` already guarded `if (!deployments || !config) return;` and sailed straight
   past it. A guard on the component you are calling is **not** evidence that the manager *that*
   component resolves through exists. Guard the dereference, not the handle.
2. **`Replication.IsServer()` is TRUE in the editor.** The method's first early return was no help either.

The standing rule: an `OnPostInit()` may allocate its own state and do nothing else. Every question
about the campaign belongs behind `Init()`, `PostGameStart()`, or the tick.

#### THE LATENT HALF — six unguarded dereferences, not one

`OVT_OverthrowConfigComponent` dereferenced `GetGame().GetFactionManager()` with **no null check** in
all six accessors of the `GetOccupying/GetSupporting/GetPlayer` × `FactionData/FactionIndex` family.
This is **pre-existing code this feature did not write**; the director's `OnPostInit()` was simply the
first caller in the codebase to reach one before a game had started. All six are guarded now, so the
next caller does not find the next one:

| accessor | answer with no faction manager |
|---|---|
| `GetOccupyingFactionData()` / `GetSupportingFactionData()` / `GetPlayerFactionData()` | `null` |
| `GetOccupyingFactionIndex()` / `GetSupportingFactionIndex()` / `GetPlayerFactionIndex()` | `-1` |

⚠ **AND -1 IS NEVER WRITTEN INTO THE CACHE.** `-1` is this component's *not-yet-resolved* sentinel — it
is what the three members are born holding, and what `OVT_OccupyingFactionManager` deliberately resets
them to when a campaign restarts (`OVT_OccupyingFactionManager.c:338`). Caching it as an **answer**
would make one unlucky early call permanent: the index would read -1 for the rest of the session, long
after the manager existed. Each guard returns *without touching the member*, so the real index is still
resolved by the first call that can see a faction manager. `FactionManager.GetFactionIndex(null)` is
documented to return -1, so an unknown faction **key** already behaved this way — only the missing
**manager** was unhandled.

None of the six changed behaviour in any context that previously worked: every one of them either
crashed or resolved, and the guard only replaces the crash.

#### THE DIRECTOR HALF — construction stops doing anchor work

`ClearObjectiveRecord()` split in two:

- `ClearObjectiveRecord()` = `DropObjectiveAnchor()` + `ClearObjectiveRecordFields()`. **Still the single
  funnel every RUNTIME "there is no objective now" path goes through** — `ResetObjective()` and
  `EnterIdle()`. That design is unchanged and its header still says so.
- `ClearObjectiveRecordFields()` = the record fields and the two counter-attack latches, no anchor work.
  **One caller, `OnPostInit()`.** At construction the component has never pushed an anchor, so there is
  definitionally nothing to drop; routing construction through the funnel was pure work in the one
  context where none of its dependencies exist.

`DropObjectiveAnchor()` and `PushObjectiveAnchor()` now read the index into a local and bail on `< 0`,
so `DropObjectiveAnchor()` is finally as safe as its own header always claimed. That guard is not
decoration: the anchor store is a `map<int, ...>`, so **a negative index is a perfectly valid key** —
an unguarded push files a bias against a faction that does not exist, which nothing ever reads and
nothing ever clears.

#### VERDICT ON THE DIRECTOR'S OTHER FACTION-INDEX CALLERS — all safe, checked rather than assumed

Thirteen call sites in total. Twelve were traced to their entry points:

- **Nine behind `DirectorTick()`** — `SendHarassmentOperation`, `SendTowerRecaptureOperation`,
  `SendSabotageOperation`, `StartCounterAttackOnBase`, `SendFOBOperation`, `SendFOBGarrisonOperation`,
  `CollectFOBExclusions` (via `ResolveFOBSite`), `IsFOBSourceBaseHeld` and `CountAliveFOBGroups` (both
  via `TickFOBStarvation`), plus `SelectObjective`. The tick is installed in `PostGameStart()` and its
  first landing is one in-game minute later.
- **One behind a player action** — `OnFOBDismantledByPlayer`, reached from
  `OVT_CampaignRequestComponent.c:245`.
- **`PushObjectiveAnchor`** — reached only from `EnterPhase()`, i.e. from `CommitObjective()` (tick) or
  `ResolveRestoredObjective()` (tick, behind `m_bRestorePending`).
- **`DropObjectiveAnchor`** — the one that was reachable at construction. Fixed above.

`Init()` and `OnDelete()` were checked too: `HookControlChanges()` / `UnhookControlChanges()` touch
invokers only, and `ClearFOBRecord()` → `ClearFOBRuntimeState()` is field assignment. No other file in
`Scripts/Game/GameMode/Objectives/` declares an `OnPostInit()` at all.

#### THE TEST — one case added, and the honest limit on what a suite can see

**The crash itself is NOT reproducible in any suite, and no case here pretends to reproduce it.** Every
autotest world has a running game with a faction manager, there is no script API to remove one, and no
case can re-run a component's `OnPostInit()`. **The only true reproduction is loading a world in the
World Editor**, which no suite covers.

What *is* testable is the invariant the fix turns on, so
`OVT_TEST_Init_ObjectiveAnchor_NegativeFactionIndexNeverReachesTheStore` was added: plant
`config.m_iOccupyingFactionIndex = -2`, drive `CommitObjective()` (the live machine's one push site),
assert **nothing** was filed at -2 *or* at the real index, then restore the index and drive the same
path again to prove the drive really does reach the push site. Everything is put back before anything
is asserted.

⚠ **-2, NOT -1, AND -1 WOULD ASSERT NOTHING** — -1 is the config's not-yet-resolved sentinel, so
planting it sends the accessor off to resolve the real index and the case would push a perfectly good
anchor.

🔴 **CAN-FAIL PROOF OWED.** The fixing agent was explicitly barred from running the suites, so the proof
is recorded in the case header as a one-line mutation rather than as an observed red: delete
`if (occupyingIndex < 0) return;` from `PushObjectiveAnchor()` — the tree recompiles clean, and the case
reports *"an unresolvable faction index must never reach the anchor store, but faction -2 reads back as
biased"*. Please record the observed failure into the header on the next suite run.

#### Manual verification owed

1. **World Editor** — load a world (any world; the report was on world load, before play). No VM
   exception. This is the actual repro and the only place it can be checked.
2. Enter play mode from the editor, start a campaign, and confirm the occupying faction still resolves
   (bases and towns take their owner) — i.e. the -1 return really did not poison the cache.

#### Files this fix touched

- `Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c` — six guards + the family header note
- `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c` — `OnPostInit`,
  `ClearObjectiveRecord` split, `PushObjectiveAnchor` / `DropObjectiveAnchor` index guards
- `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ObjectiveAnchor.c` — one case, plus the ordering note

`tools/compile-check.sh` exit 0 (6180 files).

---

### 2026-08-19 — Phase 10 built (help sync), and T10.3 blocked

#### 🔴 THE OWED LOCALIZATION RE-EXPORT NOW COVERS PHASES 5-10

`Language/localization_Overthrow.st` is the only file in `Language/` this feature has ever touched, and it
is a **source** file: the `Language/localization_Overthrow.<lang>.conf` exports are Workbench build output
and were not edited by any phase. **None of the keys added by Phases 5, 6, 8, 9 or 10 render in game until
the user re-exports the string table in Workbench.** Phase 10's own additions are the eleven
`OVT-FieldManual_CounterAttacks_*` keys; the six entries it *edited* already exist in the exports, so their
old English text will keep rendering until the same re-export happens.

#### T10.1 — WHAT WAS FALSE, AND WHY

Four sentences were false against the shipped tree. All four were traced to a `file:line` before being
touched, and the reason is recorded in each key's `Comment` so the next reader cannot undo it by accident.

| Where | The sentence | Why it was false |
|---|---|---|
| `OVT-Tutorial_BasesFirstCapture_Body` | "the occupying faction keeps reserves and spends a surplus of them on retaking a base the resistance holds" | The hourly surplus roll, `m_bCounterAttackTimeout` and the `counterAttackTimeout` difficulty field were all deleted in Phase 1. The comment on this key had been *re-citing* the dead branch (`:1420-1429`) through two previous correction passes. |
| `OVT-FieldManual_BaseCapture_Text` | "they may spend their reserves on a counter attack to take it back later" | Same retired mechanic. |
| `OVT-FieldManual_BaseCapture_Text5` | "when it has more than it needs it will pick a base the resistance took and come back for it, then wait a while before trying again" | Same retired mechanic, stated in the most detail of the three (the pick AND the timeout). |
| `OVT-FieldManual_BaseCapture_Text5` | "once it is lost the occupying faction has no way of taking it back" (radio towers) | True only between `base-defense-migration` deleting `OVT_BaseUpgradeSpecops` and this feature landing. `OVT_TowerRecaptureBehaviorDeploymentModule.c:100-106` calls `ChangeRadioTowerControl` with the deployment's own faction. |

One more was **not** false but had become misleading, and was corrected:

- `OVT-FieldManual_OccupyingForces_Text5` — "While a battle is **running** anywhere on the island ... no new
  force of any kind is put out." The evaluator's guard moved from `m_CurrentQRF` to `IsQRFEngaged()` in
  Phase 9, so during a counter-attack's silent and muster stages deployments **are** still created. Now
  reads "while a battle is actually being **fought**".

Two were extended rather than corrected: `OVT-FieldManual_BaseCapture_Text2` gained the accepted tell
("a counter attack that has not announced itself yet counts as one"), and
`OVT-FieldManual_OccupyingForces_Text4` gained the tower-recapture path and its player-presence pause.

⚠ **Everything else about QRFs, battles and base defence in both surfaces was re-read and survived.** The
scoring paragraph, the "Losses Stay Lost" paragraph, the two "Defending a Base" paragraphs, the fast-travel
paragraphs and the sleep paragraph are all still true: Phase 9's whole point was that a **standard** battle
takes no new path, and its T9.11 line-by-line re-check is the citation.

#### T10.2 — the new Field Manual page

**Counter Attacks**, in The Resistance category, authored between *Patrols and Garrisons* and *Sleeping* in
`Configs/FieldManual/Categories/FM_Overthrow.conf` (entry `{6B8C3F5D00000101}`, pieces `…0102`-`…010B`).
Four headers and six paragraphs: the objective and how it is chosen; the build-up (trucks, the stacking
support debuff, tower recapture, sabotage, and the fact that standing there pauses every one of those
clocks); what sabotage permanently costs (the T6.9 statement, with the cheapest-first order named as
"bunkers and tents before the garage" rather than as a price list); the forward base (unannounced, starved
three ways, dismantled by hand at a cost to them); and the siege in two paragraphs (silence, the ring, the
tell, daylight, the bearing; then the reveal, the thirty real minutes, the minutes-then-seconds countdown,
the early wipe-out win, and the teardown-and-reselect afterwards).

Two things were deliberately NOT put in a tutorial popup:

- **There is no trigger for it.** `OVT_TutorialEvent` (`OVT_TutorialTrigger.c:12-44`) has fourteen members
  and none of them fires on an objective being selected, a phase changing, or a counter-attack being
  revealed. A popup about the siege would need a new trigger, which is tutorial-system framework and
  belongs to that feature, not to a docs phase. **Reported as a gap.**
- **Volume restraint.** The one existing popup that talked about counter-attacks
  (`bases-first-capture`) was corrected in place and still points at the Capturing Bases page, which now
  names the Counter Attacks page in its own body.

#### ⚠ T10.3 — THE WIKI SYNC DID NOT HAPPEN, AND WAS NOT FAKED

**No `mcp__wikijs__*` tool was exposed to the Phase 10 session at all** - not even
`wikijs_connection_status`, so this is not an auth failure or a `RetryError`, it is the MCP server not
being connected. Nothing was written and nothing was invented. It is still owed, and the content below is
ready to paste. **Search before creating** (`wikijs_search_pages`, `wikijs_get_page_children`) - page paths
are flat-ish and the search is known to return wrong `pageId`s, so re-read each page before updating and
remember that an update needs `tags`.

**Page 1 - `counter-attacks` (new, player-facing).** The six paragraphs of the new Field Manual page,
expanded to wiki length, plus what the manual deliberately leaves out: the phase names as a player would
describe them, that villages/towers/FOBs are never the target, that the objective can still change during
the build-up but not after the forward base goes up, and that a live battle anywhere on the map freezes the
whole build-up.

**Page 2 - `difficulty` (existing, update in place).** The operator half:

- **Removed:** `counterAttackTimeout`. Any wiki text describing the hourly random counter-attack, the 10 %
  roll or the cooldown is now describing deleted code and must go.
- **Added, twelve fields** (Easy / Normal / Hard / Extreme / Insane):
  `objectiveHarassmentIntervalMinutes` 90/60/45/30/20 (in-game minutes between objective operations),
  `objectiveHarassmentMaxConcurrent` 1/2/2/3/4,
  `objectiveHarassmentHoldSeconds` 240/180/150/120/90 (how long a group must hold a town centre for the
  support debuff),
  `objectiveSabotageMissionsRequired` 6/5/4/3/2 - **INVERTED on purpose: easier settings demand MORE
  sabotage missions before the counter-attack is allowed, so a new player gets more warning**,
  `objectiveSabotageHoldSeconds` 180/120/100/80/60,
  `objectiveSabotageStructuresPerMission` 1/2/2/3/3,
  `objectiveTowerRecaptureHoldSeconds` 900/600/480/360/300,
  `objectiveFOBGarrisonMax` 1/2/3/5/6,
  `objectiveFOBCost` 400 on every preset (the forward base's spend **ceiling** is three times this, derived
  in `OVT_ObjectivePhaseRules.FOBBudgetCeiling`, not authored),
  `objectiveMaxConcurrentInsertions` 1/2/3/4/4 (live transport convoys at once, per faction),
  `objectiveStarvationMinutes` 45/30/25/20/15,
  `objectiveQRFResourceGate` 2000/1500/1200/1000/800 (reserve the faction must hold before Phase 3 fires).
- **Not difficulty fields, state this explicitly so nobody hunts for them:** the daylight window (5 and 15,
  constants on the director), the ring radii (100 and 150, `OVT_QRFSiege`), the muster window
  (1 800 000 ms, `OVT_QRFSiege.MUSTER_TIME_MS`) and the town support thresholds (50 % and 25 %,
  `OVT_ObjectivePhaseRules`).
- **Everything the objective director builds is bought from the SAME per-faction deployment pool** as base
  defence, patrols and tower garrisons. There is no third wallet: the forward base's "budget" is a counter
  checked against a ceiling, and the director never credits resources back.

**Page 3 - whichever existing page describes QRFs / battles (search `qrf`, `battles`, `occupying-forces`).**
It must say that a player-initiated battle is unchanged, and that a counter-attack is a different thing
wearing the same battle system: silent deployment, a 100-150 m ring, a thirty **real**-minute unscored
muster window that starts when the encirclement completes, an early resistance win if the whole force dies
inside it, and a daylight-only start.

**Page 4 - developer material under `development-documentation/`,** if the epic has a page there: the
director is `OVT_ObjectiveDirectorComponent` with its own version-first serializer, the QRF controller
gained a **mode** rather than a sibling class, and the three flags are distinct
(`m_bQRFActive` = a battle exists, `m_bQRFRevealed` = the client knows, `IsQRFEngaged()` = the shooting has
started).

#### T10.4 — epic bookkeeping, and two pointer disagreements flagged rather than fixed

`docs/features/occupying/epic-overview.md`: `counter-attacks` added as feature #5; `base-upgrades` marked
**retired in code** with the surviving legacy save-payload classes named; the Purpose, Build Order and
Master Rollup sections rewritten around the objective director; and four Tech Debt items closed
(BUG-026/027/029 the resource arithmetic, BUG-031 the landing zones, the stalled migration, and the
tower-recapture regression). `docs/overview.md`'s single occupying row replaced to match.

⚠ **Two pointer disagreements were FLAGGED, not fixed** - both are documentation-versus-code, neither is a
defect, and the player-facing text follows the code in both cases:

1. `requirements.md` says the forward base is starved by a strong resistance presence **at its source
   base**. The shipped `IsPlayerAtFOB()` (`OVT_ObjectiveDirectorComponent.c:2325-2332`) measures presence
   at **the forward base itself**, inside `baseCloseRange`. The Field Manual says "stay in strength on the
   forward base itself".
2. This file's own T6.9 block says the sabotage operation interval is "45 in-game minutes on Normal".
   `Configs/Difficulty/Difficulty_Normal.conf:10` authors `objectiveHarassmentIntervalMinutes 60`; 45 is
   **Hard**. No player-facing text quotes the number.

#### Files this phase touched

`Language/localization_Overthrow.st` (11 keys added, 6 edited), `Configs/FieldManual/Categories/FM_Overthrow.conf`
(one entry appended), `docs/features/occupying/epic-overview.md`, `docs/overview.md`, this file and
`tasks.md`. **No `.c`, no `.et`, no gameplay `.conf`, and no `Language/*.conf`.**
`tools/compile-check.sh` exit 0.

⚠ **The German and Ukrainian translations of the five edited entries are now stale** and say the old thing.
They were left in place rather than deleted, following the precedent of the two earlier correction passes
on `BaseCapture_Text5`; each affected `Comment` records it.


### 2026-08-19 — Phase 9 built (the silent siege)

#### T9.1 — THE READ-ONLY SURVEY. EVERY DESIGN CLAIM HOLDS; ELEVEN OF THE SEVENTEEN LINE NUMBERS DO NOT.

Written **before** any edit, per R18. Verified against the working tree on 2026-08-19.

`Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c`

| §3.9 cites | Really at | Verdict |
|---|---|---|
| `:15` `m_iTimer = 120000` | **15** | ✅ exact |
| `:64-79` `CheckUpdateTimer` | **64-79** | ✅ exact |
| `:110` `IsFightingFit` | **110** | ✅ exact |
| `:126` `if(m_iTimer <= 0)` | **126** | ✅ exact |
| `:245` `SendTroops` | **245** | ✅ exact |
| `:30-32` the three parallel spawn arrays | **30-32** | ✅ exact |
| `:309` `SendWave` | **332** | ❌ +23 |
| `:342` the follow-up `CallLater(SendWave, …)` | **365** | ❌ +23 |
| `:345-352` the debit | **368-374** | ❌ +23 |
| `:403` `AddWaypoint` | **426** | ❌ +23 |
| `:416-419` `GetRandomDirection`'s convention comment | **490-493** | ❌ +74 |
| `:418` `SpawnFromQueue` | **441** | ❌ +23 |
| `:434-443` the registry tag + the waypoint ladder | **457-466** | ❌ +23 |
| `:445-447` the three index-0 removes | **468-470** | ❌ +23 |
| `:487` `IsOceanAtPosition` in `GetTargetZone` | **513** | ❌ +26 |

`Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`

| §3.9 cites | Really at | Verdict |
|---|---|---|
| `:169` `m_bQRFActive` | **169** | ✅ exact |
| `:1064` `UpdateQRFTimer` | **1125** | ❌ +61 |
| `:1091`/`:1143` the two `Start()` calls | **1152** / **1204** | ❌ |
| `:1102-1103` the notification pair | **1163-1164** | ❌ |
| `:1162` the first `m_OnQRFTownChanged.Invoke` | **1223** | ❌ +61 |
| `:1165`/`:1204` `OnQRFFinishedBase`/`Town` | **1226** / **1265** | ❌ |
| `:1249` the second `m_OnQRFTownChanged.Invoke` | **1310** | ❌ +61 |
| `:1278` `SpawnQRFController` | **1339** | ❌ +61 |
| `:1418` the economy tick's `if(m_CurrentQRF) return;` | **1476** | ❌ +58 |
| `:1957` `RpcDo_SetQRFActive` | **1982** | ❌ +25 |
| `:2002` `RpcDo_SetQRFTimer` | **2027** | ❌ +25 |
| `:2008` `RpcDo_SetQRFInactive` | **2033** | ❌ +25 |

Elsewhere: `OVT_DeploymentManager.c:577` → **664**; `OVT_EconomyInfo.c:79` → **79** ✅ and `:286-289` → **283-287**;
`OVT_MapRestrictedAreas.c:327` → **327** ✅; `OVT_FastTravelService.c:108` → **108** ✅;
`OVT_RespawnService.c:220` → **220** ✅; `OVT_SleepService.c:227` → **227** ✅;
`OVT_GMPanelUIComponent.c:532` → **547** ❌.

**Nothing about the design changed. Every mechanism §3.9 describes is present and behaves as described.**

#### 🔴 THE STANDARD-MODE BASELINE — what a player-initiated QRF does, second by second

**This is the contract. T9.11 checks the finished code against it line by line.** Timings are from the
1 000 ms `CheckUpdateTimer` callqueue installed in `OnPostInit`; `t = 0` is the moment
`StartBaseQRF`/`StartTownQRF` is entered.

**t = 0, synchronously, in one frame**

1. `SpawnQRFController(loc)` spawns the prefab at the objective. `OnPostInit` sets `m_iPoints = 0` and
   `m_iWinningFaction = -1`, resolves `m_OccupyingFaction`, and — server only — installs
   `CheckUpdateTimer` on a **1 000 ms repeating** call, allocates `m_Groups`, `Replication.BumpMe()`, and
   installs `CheckUpdatePoints` on a **10 000 ms repeating** call. `m_iTimer` is its field default,
   **120 000**.
2. The caller writes `m_iLZMin`, `m_iLZMax`, `m_iPreferredDirection`, `m_iDirectionVariance` off the base
   or town controller. **Nothing else is configured, and `Start()` takes no arguments.**
3. `Start()` → `SendTroops()`:
   - clears the three parallel spawn arrays;
   - fills `m_Bases` with every occupying-held base more than 20 m away, then the forward operating base
     if one is up (P8), then the `qrfpos + "250 0 100"` sea fallback if the list is still empty;
   - budget = `m_OccupyingFaction.m_iResources`, floored at **400**, ceilinged at
     `maxQRF × {1,2,3,4,5,6}` by player count;
   - `m_iResourcesLeft = resources;` then **one** `SendWave()`.
4. `SendWave()`: `allocate = floor(m_iResourcesLeft / m_Bases.Count())` clamped to `16 × baseResourceCost`;
   for each source (breaking when the budget is gone) **one** `GetLandingZone(base)`, **one**
   `GetTargetZone(qrfpos)`, then `SpawnTroops` until `allocated >= allocate` or 6 iterations — each call
   queueing one group prefab + its LZ + its target and returning `8 × baseResourceCost`. Leftover budget
   schedules **one** follow-up `SendWave` in `RandInt(240 000, 480 000)` ms — the 4-to-8-minute waves.
   Then, **once per pass**, `m_iUsedResources += spent` and the debit
   `m_OccupyingFaction.m_iResources -= spent` clamped at zero.
5. Back in the starter: `m_OnFinished.Insert(OnQRFFinished…)`, the base/town handle is recorded,
   `m_bQRFActive = true`, `m_vQRFLocation`, `m_iCurrentQRFBase`/`m_iCurrentQRFTown`, the
   `BaseBattle`/`TownBattle`/`CityBattle` notification goes out **immediately** on both channels
   (`SendTextNotification` + `SendExternalNotifications`), then `Rpc(RpcDo_SetQRFBase|Town)` and
   `Rpc(RpcDo_SetQRFActive, m_vQRFLocation)`. The **town** path additionally fires
   `m_OnQRFTownChanged.Invoke(townId)` inline — the civilian suppression.

**t = 1 s … 16 s** — `CheckUpdateTimer` fires. `120 000 < 105 000` is false, so **nothing spawns**.
Each tick: `m_iTimer -= 1000`; `m_iTimer < 0` is false; `UpdateQRFTimer(m_iTimer)` → `m_iQRFTimer` plus
`Rpc(RpcDo_SetQRFTimer, timer)` **to every client, every second**; `Replication.BumpMe()`.

**t = 17 s** — `m_iTimer` is 104 000 on entry, `< 105 000` at last: the **first** `SpawnFromQueue`. (The
comment says 15 s; the strict `<` and the pre-decrement read make it **16**.) One group per second from
here on: spawn the prefab at its queued LZ, `m_Groups.Insert(id)`,
`OVT_GMGroupRegistry.Tag(group, QRF, -1, "QRF")`, then **four** scheduled waypoints on its queued target —
Scout +5 s, Scout +15 s, SearchAndDestroy +30 s, SearchAndDestroy +60 s — then index 0 comes off all
**three** arrays.

**t = 120 s** — `m_iTimer` reaches exactly 0. `0 < 0` is false, so this tick **does** publish `0`, and the
HUD flips from `#OVT-BattleStartsIn n` to `#OVT-BattleProgress`. **Exactly 120 timer broadcasts have been
sent.**

**t = 121 s onward** — every tick still calls `SpawnFromQueue` (draining any later wave), then
`m_iTimer -= 1000` goes negative, is clamped back to 0, and the method **returns before publishing**. No
further timer RPC is ever sent. `CheckUpdateTimer` is removed only when a winner is declared.

**t ≈ 120 s onward, every 10 s** — `CheckUpdatePoints`, whose whole body is behind `if(m_iTimer <= 0)`, so
it is inert for the first two minutes. Then: one pass over every AI agent in the world, counting
**fighting-fit** occupying characters within `QRF_POINT_RANGE` (220 m) and `QRF_RANGE` (750 m), and
fighting-fit non-player-controlled resistance AI within 220 m as recruits; one pass over players, giving
2 XP each and counting them within 220 m. `resistanceNum = playerNum + recruitNum`. Resistance present
with **no** occupying inside 750 m → **+5**; equal head counts → decay toward 0; otherwise ±1. Clamped to
±`QRFPointsToWin`, `UpdateQRFPoints` broadcasts, `m_iWinningFaction` follows the sign. On reaching either
bound: `m_OnFinished.Invoke()` then **both** callqueue entries are removed.

**While it runs — client:** the HUD panel (`m_bQRFActive`), the map restricted-area circle at
`m_vQRFLocation` **and** the suppression of the objective base's own circle, the fast-travel rules
(`QRFFastTravelMode`, FOB exempt), the respawn refusal inside 750 m, the sleep refusal, the GM panel row.
**Server:** the OF economy tick returns early (no gain, no spend, no threat decay), `OVT_DeploymentManager`
creates nothing, `DirectorTick()` returns early (D8's freeze), `OVT_GMRequestComponent` raises
`FLAG_DISTRIBUTION_SUPPRESSED_QRF`, and `OVT_CaptureBaseAction` / `OVT_StartUprisingAction` /
`OVT_CampaignRequestComponent` / `OVT_UprisingRequestComponent` all refuse a second battle.

**End:** `OnQRFFinishedBase`/`OnQRFFinishedTown` — control change if the winner differs, town modifiers
and support reset on the town path, `DeleteEntityAndChildren(m_CurrentQRF.GetOwner())`,
`m_CurrentQRF = null`, the three flags cleared, `Rpc(RpcDo_SetQRFInactive)`, and on the town path the
**second** `m_OnQRFTownChanged.Invoke(-1)`. Survivors are left standing (commit `e115965`).

#### ✅ T9.11 — THE STANDARD-PATH VERDICT, checked back against the note above line by line

**A `STANDARD` battle still does every one of the eighteen things listed.** Checked method by method,
after the phase was finished:

| Baseline item | Verdict |
|---|---|
| `OnPostInit` — two callqueue installs, `m_Groups`, `BumpMe`, `m_iTimer` default 120 000 | untouched |
| `Start()` takes no arguments and calls `SendTroops()` | body unchanged; one doc comment added |
| `SendTroops` — base list, FOB source, sea fallback, 400 floor, `maxQRF` × player ceiling | unchanged. Two additions: `m_aSpawnRingSlots.Clear()` at the top with the other three, and a `COUNTER_ATTACK`-guarded `BuildSiegeRing()` after `SendWave()` |
| `SendWave` — per-source slice, `GetLandingZone`/`GetTargetZone`, `ii < 6`, the Prints | verbatim, inside the `else` |
| the **4-8 minute follow-up wave** | verbatim, inside the `else` |
| 🔴 the **debit** (`m_iUsedResources`, `m_iResources -= spent`, clamp, Print, `return spent`) | **outside** the branch, runs exactly once in both modes |
| `CheckUpdateTimer` — `< 105000`, decrement, `< 0` clamp-and-return, publish, `BumpMe` | verbatim, after one early-returning mode branch. First spawn still at t ≈ +17 s; still exactly **120** timer broadcasts |
| `SpawnFromQueue` — spawn, `m_Groups.Insert`, `Tag(…, QRF, -1, "QRF")`, three index-0 removes | unchanged |
| the Scout / Scout / SaD / SaD ladder at 5/15/30/60 s | verbatim, inside the `else` |
| the fourth index-0 remove | guarded on `Count() > 0`; a standard battle never fills the array, so it is a no-op |
| `CheckUpdatePoints` — the whole scoring pass, XP, the ±5/±1/decay table, clamp, winner, `m_OnFinished`, both `Remove`s | unchanged. One added statement above it, guarded `COUNTER_ATTACK && MUSTER` |
| `KillAll`, `IsFightingFit`, `CreateWaypoint`, `AddWaypoint`, `ScheduleWaypoint`, `IsZeroVector`, `GetRandomDirection`, `GetTargetZone`, `GetLandingZone` | untouched |
| both starters — immediate notification on both channels, `m_bQRFActive`, the two `Rpc`s, the inline civilian invoke | all preserved on the default `STANDARD` argument; one appended `Rpc(RpcDo_SetQRFRevealed, true)` |
| HUD, map circle, fast travel, respawn | each gained `&& m_bQRFRevealed`, which is **true from creation** for a standard battle |
| the HUD's seconds line | byte-identical, and unreachable by the minutes branch because the crossover is strictly greater-than a countdown that starts at exactly 120 000 |
| economy tick, deployment evaluator | now `IsQRFEngaged()`, **true from creation** for a standard battle |

**The only behavioural change a player-initiated battle can see is none.** The two `protected` → `public`
changes (`CheckUpdateTimer`, `CheckUpdatePoints`) exist so the Init tier can drive exactly one tick, the
same way `DirectorTick()` is public, and change no behaviour at all.

⚠ **This is still not a substitute for playing one** (F17). The automated spine cannot see waves
arriving, a HUD countdown, or scoring.

#### The stage machine as built

```
CheckUpdateTimer()                         // 1 000 ms, unchanged cadence, unchanged install
  m_eMode == STANDARD        -> today's four statements, verbatim
  m_eMode == COUNTER_ATTACK  -> TickSiegeTimer(), return

TickSiegeTimer()
  SILENT_DEPLOY  queue not empty -> SpawnFromQueue(); return       // own condition; NO 15 s wait
                 queue empty     -> EnterMuster()
  MUSTER         m_iTimer -= 1000
                 <= 0            -> m_iTimer = 0; EnterBattle("the muster window ran out")
                 otherwise       -> PublishSiegeTimer()            // cadence, not every tick
  BATTLE         nothing; CheckUpdatePoints owns it

EnterMuster()    stage = MUSTER; m_iTimer = 1 800 000; last-published = -1;
                 manager.RevealQRF();  PublishSiegeTimer()          // the -1 forces this one out
EnterBattle()    idempotent; stage = BATTLE; m_iTimer = 0;
                 UpdateQRFTimer(0) UNCONDITIONALLY (the cadence must not withhold the one
                   broadcast that flips every panel to "#OVT-BattleProgress");
                 IssueAssaultOrders();  manager.OnQRFEngaged()
```

**Publication rate:** `OVT_QRFSiege.ShouldPublishTimer` — first value always, a clock that jumped up
always, every tick at or below 120 000 ms, otherwise every 10 000 ms. A 30-minute muster sends ~180
broadcasts instead of 1 800; a standard battle's 120 are untouched.

#### The ring: how it is computed and where it is stored

Computed **once**, in `BuildSiegeRing(qrfpos)`, called from the end of `SendTroops()` after `SendWave()`
has filled the queue and before any group has spawned. Nothing can run in between: `SpawnFromQueue` is
only ever reached from the 1 000 ms call, and a siege schedules **no follow-up wave**, so the queue is
final at that point and stays final.

- `N = m_aSpawnQueue.Count()`; slot `i` bearing `360/N * i` (`OVT_QRFSiege.RingSlotBearing`), radius
  rolled per slot in `[100, 150]`.
- `OVT_QRFSiege.RingSlotOffset` returns a **local offset** in the `0° = North = -Z` convention, scaled
  (not normalized). The controller adds it to the objective origin, keeping the objective's own height —
  exactly what `GetLandingZone` does with its direction vector.
- An ocean slot is walked **inward** in 15 m steps, 8 attempts, floor 25 m — never re-rolled onto a new
  bearing, because a re-rolled bearing would bunch every rejected group on a coastal town's landward
  side. Still wet after all eight → falls back to the objective centre, which is dry by definition.
- Stored in **`m_aSpawnRingSlots`, the FOURTH index-parallel array**, cleared beside the other three in
  `SendTroops` and popped at index 0 beside them in `SpawnFromQueue`. The pop is guarded on
  `Count() > 0` because a standard battle never fills it.

#### 🔴 THE ALL GATE CAME BACK 1/359 RED ON THE FIRST CUT, AND IT WAS A REAL D16 DEFECT

```
OVT_TEST_Init_QRFSiege_EarlyEndTreatsAResolvedEmptyGroupAsAlive
  → "🔴 A LIVE GROUP WITH ZERO AGENTS MUST COUNT AS ALIVE: the siege entered BATTLE
     against a force that had not been fought (stage 2)"
```

The Logic-tier statics were green, so `GroupNeutralised` and `AllNeutralised` were right in isolation.
**The defect was in the caller**, and it is the second unreliable reading in this check — the one §3.9
did not anticipate:

> `CheckSiegeWipedOut` resolves each tracked group with `world.FindEntityByID(id)` and read a **null
> answer as proof of death**. A null answer is ambiguous in exactly the way D16 says must never be
> trusted: the group really was deleted (death), **or the id never resolved in the first place**.

**The second case is reachable in production.** An entity that is not world-registered answers `GetID()`
with `EntityID.INVALID`, and **every unregistered entity shares that one value** — recorded in this tree
at `OVT_InactiveRecruitGroupComponent.c:76-83`, where the same trap cost core's observer map two entities
colliding on one entry. `SpawnFromQueue` reads `group.GetID()` in the **same frame** as the spawn. If that
ever comes back unregistered — under load, on a dedicated server, after an engine change — **every id in
`m_Groups` becomes unresolvable and the very first early-end tick declares the whole siege wiped out**,
jumps to BATTLE against men nobody fought, and hands the resistance the objective for free. §3.9's own
line ("its entity is null / deleted → dead") is what shipped this.

**The fix — `m_bSiegeForceSeenAlive`, an arming latch on the controller.** The early end refuses to
declare anything dead until it has resolved at least one tracked group **alive**:

- armed **only** by an actual successful resolution, never by "we spawned something" — arming it at the
  spawn site would re-open the very hazard, since a batch of unusable ids would arm it and then
  immediately read as a wipe;
- armed from **this tick's own observation, before the test**, so a siege whose groups are all standing
  on its first check can be judged from that same tick;
- `AllNeutralised(0, 0) == false` is **untouched** — the latch is an additional precondition in front of
  it, not a replacement.

⚠ **It does not make the early end unreachable, and that is asserted.** A healthy siege resolves live
groups on its first MUSTER-stage check, arms the latch, and fires normally the moment they are all down —
which is the second half of `…_EarlyEndTreatsAResolvedEmptyGroupAsAlive` (delete the group, drive the
check, require BATTLE). **The one case it gives up** is a force wiped out before any early-end check ever
saw it standing; that siege now waits out its clock instead of ending early. That is D16's own stated
preference — late, never early — and it is pinned deliberately by the third Init case.

**The fixture was wrong too, and both were fixed.** The first cut built its empty group with
`GetGame().SpawnEntity(SCR_AIGroup, …)` and used `GetID()` in the same frame, so it presented an
unresolvable id rather than "a live group with zero agents". It now spawns a **real occupying-faction
group prefab** through the same `SpawnEntityPrefab` call `SpawnFromQueue` uses, under
`SCR_AIGroup.IgnoreSpawning(true)` — the engine's own "create the group, create no members" seam, the
identical pattern `OVT_VirtualizationManagerComponent.SpawnVirtualGroupEntity()` already uses — and takes
**one** unconditional frame hop before touching the id. It also now asserts, explicitly and with a
fixture-flavoured message, that the group **is** world-resolvable and **does** report zero agents, so a
broken fixture can never again be mistaken for a broken product.

**The lesson, stated generally:** in this component a failed `FindEntityByID` is a reading about the
*lookup*, not about the *entity*. Anything that concludes "dead" from it needs evidence that the id was
ever good.

#### The early-end predicate, exactly

Asked on `CheckUpdatePoints`' existing 10 s cadence, **in `MUSTER` only**, over `m_Groups`:

```
per group:  groupResolved = (SCR_AIGroup.Cast(FindEntityByID(id)) != null)
            agentCount    = aigroup.GetAgents(...).Count()
            fitCount      = how many of those pass IsFightingFit(agent.GetControlledEntity())

OVT_QRFSiege.GroupNeutralised(groupResolved, agentCount, fitCount):
   !groupResolved   -> TRUE   (gone from the world, or no longer a group)
   agentCount <= 0  -> FALSE  🔴 ALIVE, DELIBERATELY
   otherwise        -> fitCount <= 0

then, on the controller, in this order:
   if (resolvedNow > 0) m_bSiegeForceSeenAlive = true;   // arm from THIS tick's observation
   if (!m_bSiegeForceSeenAlive) return;                  // 🔴 never seen alive -> never declared dead

OVT_QRFSiege.AllNeutralised(tracked, neutralised):
   tracked <= 0     -> FALSE  (nothing tracked is not everything dead)
   otherwise        -> neutralised >= tracked
```

🔴 **The zero-agent decision, and why.** "Zero agents means the group is dead" is a known-bad prune in
this engine — the AI spawn queue and dormancy both legitimately produce it, and it is unfixed at HEAD.
Siege groups are spawned live and never virtualised, so it should not arise. But a false positive here
does not cost a little accuracy: it ends the muster window, starts scoring against a force that is still
standing, and — because the unmodified scoring loop awards +5 a tick against an empty zone — **hands the
resistance the objective for free, in a way no log would explain.** The failure the bias leaves instead
is the harmless one: a siege that had already been wiped out sits out the rest of its clock.

⚠ **The rule was deliberately lifted out of the controller's loop into `OVT_QRFSiege.GroupNeutralised`,
which is one function more than §3.9's list.** In the loop it could only be commented; as a pure static
it is asserted row by row in the cheapest tier with a recorded can-fail proof (N4). The controller's
loop still carries the reason as a comment at the test site, as T9.8 requires.

#### The three world gates that moved, and the two that deliberately did not

| Gate | Was | Now |
|---|---|---|
| OF economy tick (`OVT_OccupyingFactionManager.c:1622`) | `if(m_CurrentQRF) return;` | `if(IsQRFEngaged()) return;` |
| Deployment evaluation (`OVT_DeploymentManager.c:670`) | `m_CurrentQRF` | `IsQRFEngaged()` |
| The objective town's civilians | `m_OnQRFTownChanged.Invoke` inline in `StartTownQRF` | `STANDARD` keeps the inline invoke; `COUNTER_ATTACK` fires it from the **BATTLE** transition via `OnQRFEngaged()` |
| `OVT_SleepService.c:227` | `m_bQRFActive` | **unchanged, deliberately** — sleeping through an incoming assault is refused from the moment it is incoming |
| `OVT_GMPanelUIComponent.c:546` | `m_bQRFActive` | **unchanged, deliberately** — a Game Master is meant to see the siege forming |

Both non-changes now carry a comment saying so at the site, so a later reader does not "fix" them.

⚠ **The civilian pairing holds BY CONSTRUCTION, and here is the proof.** Scoring — and therefore
`m_OnFinished`, and therefore `OnQRFFinishedTown` — is gated on `m_iTimer <= 0`. In `SILENT_DEPLOY` the
clock is parked at its 120 000 construction default and never moves; in `MUSTER` it counts 1 800 000 down
and **reaching zero IS the BATTLE transition**. So a siege cannot resolve without passing through
`BATTLE`, and the `-1` invoke at the finish always has a matching first invoke. `KillAll()` during
`MUSTER` does not break this either: it wipes the force, the early end fires, and the machine still goes
through `BATTLE`. **Nothing may ever introduce a path that resolves a siege without entering BATTLE.**

#### The three flags, as shipped

| Flag | Lives on | Replicated | STANDARD | COUNTER_ATTACK |
|---|---|---|---|---|
| `m_bQRFActive` | OF manager `:169` | yes (`RpcDo_SetQRFActive/Inactive` + JIP) | true at creation | true at creation, for the whole siege |
| `m_bQRFRevealed` | OF manager `:192` — **new** | yes (`RpcDo_SetQRFRevealed(bool)` + JIP) | true at creation | false until the `MUSTER` transition |
| `IsQRFEngaged()` | OF manager `:1301` — **new**, server-only | no | true at creation | true only in `BATTLE` |

Client consumers of `m_bQRFRevealed`, exactly four: `OVT_EconomyInfo.c:83`, `OVT_MapRestrictedAreas.c:332`,
`OVT_FastTravelService.c:115`, `OVT_RespawnService.c:226`.

⚠ **The one behavioural change to a shipped rule** is fast travel and respawn: during `SILENT_DEPLOY` a
player may still travel to and respawn in the objective, because nobody has told them not to. A refusal
there would be a free reveal delivered by the UI **before** the notification.

#### ⚠ A TRAP FOUND WHILE BUILDING `RevealQRF()`: `m_CurrentQRFBase` AND `m_CurrentQRFTown` ARE NEVER CLEARED

The first cut of `RevealQRF()` decided base-versus-town by asking `if(m_CurrentQRFTown)`. That is wrong,
and it is wrong in a way that would only have surfaced on the *second* battle of a campaign:

**Both finish handlers reset the INDICES (`m_iCurrentQRFBase = -1; m_iCurrentQRFTown = -1;`) and leave
the OBJECT HANDLES (`m_CurrentQRFBase`, `m_CurrentQRFTown`) holding whatever the last battle set.**
Nothing anywhere clears them. So a base siege following any earlier town battle would have found a
truthy `m_CurrentQRFTown` and announced *"Enemy forces have surrounded \<the town from last time\>"* —
a confident lie about where the enemy is, with a real place name in it.

Fixed by gating on the indices, which *are* reset, and reading the handle only behind the index and only
for the base's name. The handles' staleness itself is **left alone**: `OnQRFFinishedTown` reads
`m_CurrentQRFTown` all the way through its own body, so clearing them is a change to the shipped
resolution path and does not belong in this phase. **Anything new that reads either handle must check
the matching index first.**

#### Wire and payload notes

- `RpcDo_SetQRFRevealed(bool)` is a **new pair, not a widened one**, and its arity was diffed by eye
  against `RpcDo_SetQRFTimer` directly above it (**BUG-090** — wrong arity compiles clean and dies
  silently at the wire). One payload argument sent, one parameter received, at three send sites.
- `m_bQRFRevealed` is **appended** to the `RplSave`/`RplLoad` JIP payload, which is positional and has no
  version field. Both halves were edited together.
- ⚠ **`m_iCurrentQRFBase` and `m_iCurrentQRFTown` are ALREADY missing from that payload** and stay
  missing. A client joining mid-battle gets `m_bQRFActive` and `m_vQRFLocation` but neither index, so the
  map's "don't draw the objective base's own restricted circle" rule reads `-1` until an
  `RpcDo_SetQRFBase/Town` that never comes. **Pre-existing defect, not this phase's**, recorded rather
  than fixed — widening a positional payload beyond the one flag it needs is exactly the drive-by that
  makes a wire format unreviewable.

#### 🔴 NO SIEGE STATE IS PERSISTED, AND A MID-SIEGE SAVE ROLLS THE BATTLE BACK

Nothing added in this phase is serialized: not `m_eMode`, not `m_eStage`, not the ring, not the muster
clock, not `m_bQRFRevealed`. That is deliberate and matches §3.8 — **a live battle has always rolled back
on load**, because `m_CurrentQRF` is not persisted either. The consequence, spelled out so a later reader
files no bug against it:

- Save during a 30-minute muster, quit, **Continue** → there is no battle. The occupying faction's
  objective director is restored in `COUNTER_QRF` phase with no live battle, and its **first tick resets
  the objective** (`OVT_ObjectiveDirectorSerializer.c:29` already documents this).
- The ring groups themselves are ordinary AI and follow whatever the world's own group persistence does;
  they are not tracked by anything this feature added.
- `m_bQRFRevealed` therefore never needs a save slot: a loaded campaign has no battle to be revealed
  about, and the flag's default is `false`.

**If a future phase ever wants a siege to survive a save**, it needs a serializer of its own — and the
first thing it will have to decide is what a half-mustered encirclement means when the world's AI has
been rebuilt underneath it.

#### The single-pass spend, and its three bounds

`SendWave()` branches once: `COUNTER_ATTACK` calls `SpendWholeBudgetInOnePass(qrfpos, allocate)` and
skips the follow-up `CallLater`; `STANDARD` runs today's `foreach` verbatim. **The debit is outside the
branch and runs exactly once in both modes** — it is the only place a battle takes resources out of the
war chest, and BUG-027's shape in a new place is what double-debiting or skipping it would be.

The pass cycles the source list until the budget is gone, bounded three ways:

1. **the budget** (`m_iResourcesLeft <= 0`) — the intended exit, and the one that fires in practice;
2. **a zero-progress break** — a whole cycle that allocated nothing can never allocate anything on the
   next one either, so it stops. ⚠ This is reachable today: `baseResourceCost` misauthored to 0 makes the
   per-source slice 0, which makes the inner `while(allocated < allocate)` false on entry, forever;
3. **`MAX_SIEGE_SPEND_PASSES = 64`** — the belt to (2)'s braces, because this runs on the **server
   thread** inside `Start()` and a loop that cannot terminate here does not degrade the game, it hangs it.

#### Orders, and the waypoint that must come off

A siege group gets **one** `Defend` waypoint on its ring slot instead of the Scout/Scout/SaD/SaD ladder,
scheduled with the same 5 s grace the ladder's first entry uses. At the `BATTLE` transition
`IssueAssaultOrders()` **removes every waypoint the group holds** and then adds `SearchAndDestroy`.

⚠ `AddWaypoint` **appends**, and a `Defend` waypoint is never completed — so without the removal the
group would defend its ring slot forever and the assault would simply never happen. The remove-then-add
shape is vanilla's own (`SCR_WaypointGroupCommand.SetWaypointForAIGroup`), and like vanilla it does
**not** delete the waypoint entities: a waypoint may be shared, and today's battles already leave their
completed ones standing.

`GetTargetZone` is called **per group** rather than once for the ring, so twelve groups converging do not
all walk at one blade of grass.

#### Tests added, and the can-fail proofs

- **Logic, 4 new cases in a new `OVT_TEST_Logic_QRFSiege.c`** — the ring (even spacing and no repeats at
  1/2/3/7/12, slot 0 due north, a zero count, a negative count, an out-of-range and a negative index);
  the offset **sign** on all four cardinals as written-out vectors plus two standalone sign claims and
  the "scaled, not normalized" length rows; the publish cadence on both sides of the 120 000 boundary
  including exactly-ten-seconds; the minutes/seconds crossover at exactly 120 000 (exclusive) with the
  round-**up** rows; and `AllNeutralised` + `GroupNeutralised` including the 🔴 zero-agent row.
- **Init, 3 new cases in a new `OVT_TEST_Init_QRFSiege.c`** — engagement and the reveal walked through
  all three stages on a real controller (leading with "a STANDARD battle is engaged at birth", the R18
  guard); the early end refusing to fire on a **live, empty group** and then firing once that group is
  really deleted; and the early end refusing to fire on **an id it never once resolved**, which is the
  case that covers the `m_bSiegeForceSeenAlive` fix (the second case's own fixture arms the latch, so it
  cannot cover it — that limit is written at the case).
- **19 can-fail faults injected and compiled**, every one exiting `tools/compile-check.sh` with **0**:
  3 on the ring bearings, 3 on the offsets, 3 on the publish cadence, 3 on the minutes form, 4 on the
  neutralised pair (including 🔴 N4, the zero-agent prune), and **4 on the controller itself** — deleting
  `IsEngaged()`'s `STANDARD` short-circuit, adding the habitual `if(agents.Count() == 0) dead` prune to
  the loop, reverting the seen-alive guard, and arming the latch unconditionally. Each is written at its
  case with the failure text it produces. **Suites were not run — that is the orchestrator's job**
  (`.claude/test-policy.md`).

⚠ **One recorded fault is honest about its limit and says so at the case**: removing the *first-value
sentinel* from `ShouldPublishTimer` alone changes nothing any row can see, because the clock-jumped test
immediately below it answers `true` for any non-negative time against a negative sentinel. The sentinel
is defence in depth for a future caller that seeds the field differently, not an independently observable
rule, and no row is written to pretend otherwise.

#### Play-test items this phase adds

- 🔴 **Play a normal QRF (F17).** The phase's primary risk and the thing no tier covers. Capture a base
  by hand: notification **immediately**, a 120-second countdown in **seconds**, the first groups landing
  about 17 s in one per second, waves 4-8 minutes apart, scoring the instant the countdown hits zero, and
  the economy/deployments/civilians all suppressed from the start.
- 🔴 **Does a siege actually read as an encirclement?** Reach Phase 3 and watch: no notification, no HUD
  panel, no map circle, the town still full of civilians — then the announcement, and groups standing
  100-150 m out on every side.
- **Is the world really still alive during `SILENT_DEPLOY`?** Deployments should keep being created and
  the faction's six-hourly income should still land while the encirclement forms.
- **Fast travel and respawn during `SILENT_DEPLOY` must still work**, and must start refusing the moment
  the announcement lands.
- **The HUD's minutes form** — "30 min" counting down, crossing to a bare seconds count at two minutes.
- **Wipe the ring before the clock runs out** and confirm the battle resolves to the resistance instead
  of waiting out the window.
- **MP/JIP.** Uncovered. A client joining during `SILENT_DEPLOY` must see **no** panel and **no** circle;
  one joining during `MUSTER` must see both.

#### Owed

- **A localization re-export.** Three new keys were added to `Language/localization_Overthrow.st` only —
  `OVT-BattleStartsInMinutes`, `OVT-Msg-CounterAttackBase`, `OVT-Msg-CounterAttackTown`, each with a
  `Comment`. The `Language/*.conf` exports are Workbench build output and were **not** touched
  (`git diff Language/*.conf` is empty). Until the re-export, the minutes label and both notifications
  render as raw keys.
- **The suites.** Not run here by policy.

### 2026-08-19 — Phase 8 built (the counter-QRF fires, the daylight gate, and the GM wire)

#### T8.1 — THE READ-ONLY SURVEY. C4 STILL HOLDS, AND THE LINE NUMBERS HAVE ALL MOVED.

| # | Claim | Verified state, 2026-08-19 | Verdict |
|---|---|---|---|
| 1 | The landing-zone cache globals are gone | `grep -rn "Goodqrfpos\|Goodqrfbasepos" Scripts/` is **empty** | ✅ holds |
| 2 | The `TracePosition` no-op is fixed | `if (result > 0 && !trace.TraceEnt)` with BUG-031 named in the comment beside it | ✅ holds |
| 3 | The 0°/360° preferred-direction wrap is fixed | `GetRandomDirection()` normalises the SUM and wraps once, with the "normalizing min/max separately inverts the range" comment | ✅ holds |
| 4 | Each wave source resolves its own landing zone | `SendWave()` calls `GetLandingZone` inside the `foreach`, with the no-cache comment on the method | ✅ holds |

⚠ **Every `file:line` in the plan's Phase 8 is stale by ~40 lines** — the file grew an `IsFightingFit()`
helper and a rewritten `CheckUpdatePoints` since the plan was written. The real anchors at HEAD are
`SendTroops` **:245**, its base loop **:255-264**, its empty-list fallback **:266**, `SendWave` **:309**,
its `GetLandingZone` call **:326**, `GetLandingZone` **:505**, `GetRandomDirection`'s convention comment
**:467-470**. Nothing about the design changed; the numbers did.

#### 🔴 THE BEARING SIGN, WRITTEN OUT IN FULL — the phase's most likely defect, and why it points this way

`GetLandingZone` builds its candidate as

```
checkpos = qrfpos + (dir * distance)
```

where `qrfpos` is **the place under attack**. So `dir` is **not** "the direction the attackers travel in".
It is the direction of the **ground they are put down on, measured outward from the objective**. Take
`dir = North` and the landing zone is north of the town; take `dir = South` and it is south of it.

A wave has to land on the side of the objective **its source is on**, so that an attack mounted from a
base to the north visibly arrives out of the north. The bearing therefore has to be the compass bearing of
the **source as seen from the target** — `target → source` — and never `source → target`.

Concretely, in `GetRandomDirection`'s own convention (**0° = North = `-Z`, 90° = East = `+X`**, documented
in file):

- a source **due north** of its target has `dx = 0`, `dz < 0`, so `atan2(dx, -dz) = atan2(0, +) = 0°`;
- `DirectionForDegrees(0)` is `{sin 0, 0, -cos 0}` = `(0, 0, -1)` = **north**;
- `checkpos = target + north * distance` → the landing zone is **north of the target**, i.e. between the
  target and the source. ✅

Invert the subtraction and the same source reads **180°**, the landing zone goes **south**, and every wave
in the battle lands on the **far side** of the objective from the men who sent it — while still being a
perfectly valid, ocean-tested, trace-cleared landing zone. Nothing logs. In play it reads as an AI or
pathing fault, which is why the sign is asserted as a **named Logic case with a due-north source**, and
asserted twice: once as a number (`0`, not `180`) and once as geometry (*the landing zone is closer to the
source than the objective is*), the second form being checkable without knowing the convention at all.

⚠ **`GetRandomDirection()` was deliberately NOT changed.** The authored path (`m_iPreferredDirection`, set
per base/town by a map author) still goes through it byte for byte; the derived path is a separate two-line
expression inside `GetLandingZone`. `OVT_QRFBearing.DirectionForDegrees()` re-states that method's own
`{sin, 0, -cos}` expression, and the Logic case asserts the two agree — if they ever drift, a battle whose
source is known would place waves somewhere different from one whose direction was typed in by hand.

#### `m_Bases` staleness — RE-CONFIRMED AND DELIBERATELY LEFT ALONE

`m_Bases` is filled once in `SendTroops()` and never refreshed. A source base captured by the resistance
mid-battle keeps sending troops, and **so will a forward operating base torn down mid-battle**. This is
pre-existing behaviour, it is on D9's "deliberately excluded" list, and fixing it changes
player-initiated battles too — it belongs to whoever owns the battle layer next, not here. The note is
written **in the file**, at the insertion point, so the next reader does not rediscover it.

#### The two wire bumps, and why an old client needs both

| Constant | 1 → 2 / 2 → 3 | What breaks without it |
|---|---|---|
| `CAMPAIGN_RECORD_COUNT` 2 → 3 | `SendSnapshotEnd` reports `CAMPAIGN_RECORD_COUNT + perEntityRecords` as the total the server sent, and the client stores it as `m_iReportedRecordCount` for exactly the "did this fan lose records" question | Left at 2, every snapshot under-reports by one and a consumer comparing the count against the arrays sees a fan that always looks one short |
| `WIRE_VERSION` 1 → 2 | `RpcDo_SnapshotBegin(seq, wireVersion)` exists so a mismatched build **refuses to stage** rather than half-parsing | Left at 1, a 1-speaking client stages a fan it cannot fully consume and shows a permanently short record count with no diagnosis. The version field's whole job is to make that loud |

⚠ **A NEW PAIR, NOT A WIDENED ONE.** `SendCampaignSchedule` could have grown two arguments for fewer
lines; that would have been an untyped variadic `Rpc()` whose argument count no longer matched its handler
on any build carrying half the change (**BUG-090** — wrong arity compiles clean and dies silently at the
wire). A new pair cannot half-land. **Arity diffed by eye**, as the block comment at the send sites
instructs: `Rpc(RpcDo_CampaignObjective, seq, name, phase)` is three payload arguments and
`RpcDo_CampaignObjective(int, string, int)` takes three. Both send sites carry the
`ShouldRespondLocally(playerId)` listen-server branch in the shape the file already uses.

#### 🔴 THE DAYLIGHT WAIT HOLDS THE PHASE TIMEOUT AND NOTHING ELSE — D17's wording was CORRECTED

⚠ **D17 originally said a gate blocked only by the clock ticks "no starvation or timeout counter". That
wording was wrong and its author corrected it during this build (2026-08-19); `implementation.md` is being
amended to match.** The corrected rule is: *waiting for daylight must not count as the objective FAILING* —
no phase timeout, no re-select, no blacklist. It was never meant to suspend **starvation**.

**The first cut of this phase implemented the original wording literally** — the tick returned before
`AdvanceObjectiveTimers()`, so nothing moved at all — and that is exactly the bug the correction exists to
stop: a player empties the supplying garrison at 22:00, the forward base **should** come down, and instead
it stands frozen for ~2.3 real hours (at 6×) and then launches a counter-attack from a base whose men are
already dead. That contradicts **F7** ("take or empty the base supplying the FOB and it comes down on its
own") outright and punishes a play the player made correctly.

**The distinction, which is the thing to remember:**

| Clock | During the wait | Why |
|---|---|---|
| **Phase timeout** (`phaseTicks`) | **HELD**, and not even tested | It is a clock the director runs against **itself**. Left running, a gate met at 16:00 spends the phase's remaining budget waiting out the dark and the objective is abandoned for being night |
| **Starvation** (`m_FOB.starvationTicks`) | **RUNS**, and may end the objective mid-wait, blacklist and all | It is the **resistance's counterplay** and answers to facts about the WORLD — the supplying base taken or emptied, a strong resistance presence — and those do not stop being true after sunset |
| **Operation cadence** (`nextOpTicks`) | **RUNS**, and the garrison sender fires on it | A frozen cadence either never sends a garrison or sends one every tick, and the garrison is what the starvation rule is measured against |

**How it is built.** `AdvanceObjectiveTimers()` is split into `AdvancePhaseTimeout()` +
`AdvanceOperationCadence()`; `TickFOB()` skips only the first while waiting, and guards the
`phaseTicks == 0` test with the same flag so a timeout that happened to run out at dusk cannot abandon the
objective either. The other two callers (`TickHarassment`, `TickCounterQRF`) still call the combined
method, which is unchanged. There is no reason it could not work — the split is three lines.

⚠ **A case pinned the old behaviour and was changed, not quietly**: the night half of
`OVT_TEST_Init_ObjectiveDirector_GateWaitsForDaylightThenFiresOnce` asserted `opAfterNight == PLANTED` and
`starvationAfterNight == 0`. Both now assert the opposite (`PLANTED - 1` and `1`), and the fixture is cut
off **by construction** — its recorded supplying base IS the resistance-held objective — so the starvation
row is a real claim rather than a reading of zero. A precondition guard rejects a difficulty preset that
would let one round mature it; every shipped preset authors 15 or more. **No pre-existing case could have
pinned this** — the daylight gate is new in this phase.

#### How the battle's end is observed — D8, and why there is nothing to write

`FireCounterAttack()` calls `StartBaseQRF`/`StartTownQRF` directly, exactly as the retired random roll did.
The **end** is observed by the tick's own third early return: while `m_CurrentQRF` is set `DirectorTick()`
returns before the phase machine runs, so the first tick on which it is null again is the first tick that
reaches `TickCounterQRF()` — which resets the objective. **The poll is the freeze.** No second
`m_OnFinished` subscriber exists anywhere in this feature: both of the occupying faction manager's own
finish handlers call `SCR_EntityHelper.DeleteEntityAndChildren(m_CurrentQRF.GetOwner())` from inside the
invoker's own dispatch, so a subscriber ordered after them would run against a deleted entity.

⚠ **`FireCounterAttack()` advances the phase only if a battle actually started.** All three refusals are
real — a battle already running, no base within `BASE_OP_RESOLVE_RADIUS` of the recorded position, a base
marker that no longer resolves, or a base the occupying faction has since retaken. Advancing regardless
would put the machine in COUNTER_QRF with nothing to wait for, and `TickCounterQRF()` would throw the whole
ramp away on the very next tick with a log line saying the counter-attack had "resolved".

⚠ **A win and a loss take the same path**, through the one `ResetObjective()`, and it does **not**
blacklist: a resolved battle is not a failure of the objective, and a place the resistance successfully
defended is very likely worth attacking again.

#### What the `ui-developer` slice (T8.9) has to consume — everything below already exists and compiles

| Symbol | Where | What it is |
|---|---|---|
| `OVT_GMCampaignState.m_sObjectiveName` | `Scripts/Game/GameMode/GM/OVT_GMCampaignState.c` | `string`. Empty means the occupying faction has no target |
| `OVT_GMCampaignState.m_iObjectivePhase` | same | `int`, `OVT_EObjectivePhase`'s ordinal as it crossed the wire |
| `OVT_GMPanelFormat.FormatObjectiveName(string)` | `Scripts/Game/UI/GM/OVT_GMPanelFormat.c` | The name, or `#OVT-GMPanel_ObjectiveNone`. Never empty |
| `OVT_GMPanelFormat.FormatObjectivePhase(int)` | same | One of five phase keys, including `…Unknown` |

Row/widget names to author, following `Row_Threat` verbatim: **`Row_Objective` / `Label_Objective` /
`Value_Objective`** and **`Row_Phase` / `Label_Phase` / `Value_Phase`**, in `CampaignSection` (**not**
`DetailSection` — D13). Two `FindText` calls in `CacheWidgets`, two `SetText` calls in `RenderAll`:

```
m_wValueObjective.SetText(OVT_GMPanelFormat.FormatObjectiveName(state.m_sObjectiveName));
m_wValuePhase.SetText(OVT_GMPanelFormat.FormatObjectivePhase(state.m_iObjectivePhase));
```

⚠ **`SetText`, not `SetTextFormat`.** A `#`-prefixed key is resolved by `SetText`, and a town name is a
proper noun that must never be run through a format string. **All eight localization keys are already
authored** in `Language/localization_Overthrow.st` — the two row labels (`OVT-GMPanel_Objective`,
`OVT-GMPanel_ObjectivePhase`) and the six values — so the slice authors no `.st` at all.
⚠ **No RPC, no `RplProp` and no replication receiver may appear in `OVT_GMPanelUIComponent`**; the grep is
currently empty and must stay so.

#### Design decisions worth not re-deriving

- **The two campaign scalars are read in `SendCampaignSnapshot()`, not in `OVT_GMSnapshotBuilder`.** The
  builder walks per-entity records; threat, both resource pools and the schedule are all read in the fan
  method beside it, and the objective is a campaign scalar of exactly that kind. `git diff` on the builder
  is **empty**. Both calls into the director are pure getters, which is T8.8's actual requirement.
- **`GetObjectiveDisplayName()` is a second getter rather than a reuse of `GetObjectiveName()`.** It
  answers `""` whenever the kind is NONE, so a panel can never label a campaign that has no target even if
  a name were somehow left behind. One line, and it makes the read-only surface's contract explicit.
- **The phase crosses the wire as an INTEGER and is stored as one.** The two ends of the fan can be
  different builds; `FormatObjectivePhase` answers `…Unknown` for a value it does not recognise, which is
  deliberately **not** the same answer as "no objective" — telling a Game Master the campaign has no target
  while a town is being assaulted is a confident lie, and an explicit "Unknown" is a symptom somebody can
  act on.
- **`IsCounterAttackWindow` fails OPEN on a nonsense window** (out-of-range bound, zero width). Failing
  closed would stop the occupying faction counter-attacking at all — the one symptom this feature exists to
  end and the last one anyone would trace to a bounds typo. The shipped bounds cannot reach either branch;
  only a future edit can.
- **An unreadable world clock ALLOWS the battle.** A world with no time-and-weather manager has no night to
  protect anyone from, and refusing would make the feature silently inert in exactly the worlds nobody
  play-tests.
- **`EvaluateCounterAttackGate()` asks the material ramp BEFORE the clock.** An objective whose support has
  not collapsed yet is not "waiting for daylight", it is still being worked on — and reporting it as a
  daylight wait would freeze the whole forward-base phase for a ramp that has not finished.
- **`StartCounterAttackOnBase` refuses a base the occupying faction already holds.** Phase 2 is locked
  against re-selection, so a base retaken by some other route would otherwise draw a battle from its own
  side. It now sits until the phase timeout, loudly, with a warning line per interval.

#### Tests added, and the can-fail proofs

- **Logic, 2 new cases in `OVT_TEST_Logic_ObjectiveAnchorAndBearing.c`** (the file was named for this half
  and left half-written by Phase 3) — `…_PointsFromTheTargetTowardTheSource`: four cardinals, both
  diagonals, a non-origin target, height discarded, a coincident source asserted **for its RANGE rather
  than its value**, and the geometry half over all four cardinals; `…_DirectionAndBearingAreInverses`: the
  four cardinals as vectors, a 24-step round trip every 15°, and four wrap rows including two several
  turns out of range. Bearings compared with a **circular** 1° tolerance, never with `==`.
- **Logic, 1 new case in `OVT_TEST_Logic_ObjectiveScaling.c`** — `…_CounterAttackWindow`: inside, **both
  boundaries of the shipped window** (05:00 in, 15:00 out), outside at both ends of the day, midnight, a
  full **22 → 04 wrapping window on both sides of both its edges**, both "no restriction" rows, and a
  full-day sweep requiring exactly ten open hours.
- **Logic, 1 new case in `OVT_TEST_Logic_GMPanelFormat.c`** — `…_ObjectiveRows`: every shipped phase
  against the **integers that cross the wire**, a future phase and a negative one both reading `Unknown`,
  and an empty name rendering as the None key.
- **Init, 1 new case in `OVT_TEST_Init_GMRequestSeam.c`** — `…_GMCampaignState_CarriesAndClearsEveryScalar`
  walks **every** scalar on the class through `CopyFrom` and then through `Clear()`, written as a checklist
  a reader can diff against the declaration. This is the only mechanical defence against T8.7's
  three-method trap, and it protects the next appended field too if somebody extends it.
- **Init, 1 new case in `OVT_TEST_Init_ObjectiveDirector.c`** —
  `…_GateWaitsForDaylightThenFiresOnce`: plants a night hour and requires the phase timeout **untouched**
  while the operation cadence and the starvation counter have each **served exactly one round** (both
  sides of the corrected D17, on one driven tick), then plants a day hour and requires exactly one battle,
  then drives a third tick and requires the **same controller instance** to still be in the slot. It restores the world clock, the fixture base's faction, the reserve, the battle
  slot and the objective, and it sorts before the forward-base, insertion, operations and sabotage cases.
- **18 can-fail faults recorded** (4 on the bearing, 3 on the direction/wrap, 4 on the window, 3 on the
  formatter, 1 on `Clear()`, **2 on the daylight hold - one each way round**, 1 on the wire). Each is written at its case
  with the exact failure message it produces. **Suites were not run — that is the orchestrator's job**
  (`.claude/test-policy.md`).

⚠ **Two of the recorded faults are honest about their limits and say so at the case**: deleting the
coincident-source guard is not detected on a platform whose `atan2(0, 0)` answers 0 (the row is written as
a RANGE claim for that reason), and the `WIRE_VERSION`/`CAMPAIGN_RECORD_COUNT` pairing has **no automated
check at all** — nothing in the tree can count the `SendCampaign*` calls at runtime. It is a grep in the
acceptance criteria and a comment on both constants, and that is the whole of its protection.

#### Play-test items this phase adds

- 🔴 **Does the counter-attack actually come from the right side?** The headline. Watch a battle whose
  source base is clearly north/south/east of the town and confirm the waves land on **that** side. This is
  the one thing no tier can see.
- 🔴 **Does the forward operating base send waves?** Get one up, let the ramp reach Phase 3, and confirm at
  least one wave's landing zone sits near the forward base rather than near a rear base. The log line
  `"...forward operating base is a wave source: <pos>"` is the marker to look for.
- **Does a player-initiated QRF still behave exactly as before?** Its sources contain no forward base and
  its landing zones fall back to the authored `m_iPreferredDirection`. Capture a base by hand and watch the
  countdown, the waves and the scoring.
- **Does the counter-attack refuse to start at night?** Reach Phase 3 after 15:00 and confirm the objective
  waits, that the log line appears **once**, and that the battle starts after 05:00 — with the objective
  still alive, not blacklisted, and its phase timeout not spent.
- 🔴 **Can the forward base still be killed during a night wait?** The D17 correction, and the half
  no tier fully covers. Reach Phase 3 after 15:00, then take or empty the supplying base — the forward base
  must starve out and the objective must end **during the wait**, not survive until dawn. The dismantle
  action must work in the same window.
- **Does the Game Master panel show the objective?** Only after T8.9 lands. Open Game Master and check the
  two `CampaignSection` rows track the ramp through all three phases and read "None"/"-" when there is no
  objective.
- **MP/JIP.** Uncovered. The GM wire change is the risk: a client on an older build must see the version
  mismatch warning and a blank panel, not a half-parsed one.

#### Owed

- **A localization re-export.** Eight new keys were added to `Language/localization_Overthrow.st` only; the
  `Language/*.conf` exports are Workbench build output and were **not** touched (`git status` on them is
  clean). Until the re-export, the six phase/None values and the two row labels render as raw keys.
- **T8.9**, the `.layout` slice, per the routing table.
- **The suites.** Not run here by policy.

### 2026-08-19 — Phase 7 built (the forward operating base: new world content, a ceiling, and a teardown)

#### 🔴 THE ALL GATE CAME BACK RED AT 2/346 ON THE FIRST CUT OF THIS PHASE. BOTH WERE TEST BUGS, AND NEITHER WAS THE PRODUCT.

| Failing case | Assertion | Root cause |
|---|---|---|
| `Logic_ObjectiveScaling_FOBSiting` | *"a candidate between two exclusions and outside both is clear: got false, expected true"* | **The row's coordinate was arithmetically impossible.** Exclusion A at x=0 r=300 and B at x=1000 r=500 leave a clear gap of only `x ∈ (300, 500)`; the row used **x=700**, which is 300 m from B and squarely INSIDE it. `IsClearOfExclusions` was right and the assertion was false of its own data. |
| `PersistenceRoundTrip_ObjectiveFOB_RelinksItsDeployment` | *"the fixture forward-base deployment is no longer standing after the reload"* | **The case's dirty step destroyed the fixture it depends on, twice over, and both deletions are the product working.** |

**THE PERSISTENCE ONE IS THE INSTRUCTIVE ONE, and the diagnosis order matters: the reload seam can
neither delete a deployment marker nor bring one back (`Instances = {gameMode}` only), so a missing
fixture was deleted by something in the SESSION, not by the restore.** Two independent deleters, both
correct:

1. `CommitObjective()` runs `TearDownFOB()` — added this phase, because a new objective must never leave
   the previous one's forward base standing. Its area sweep deletes any forward-base or garrison
   deployment within `FOB_AREA_RADIUS` of the recorded position.
2. Even without that, `CommitObjective()` enters HARASSMENT, so the fixture's
   `OVT_ObjectiveConditionDeploymentModule` (`m_iRequiredPhase 2`) starts failing and the reinforcement
   module's `m_bDeleteOnConditionFail 1` collects it.

⚠ **SO THE DIRTY STEP OF ANY CASE THAT NEEDS A LIVE OBJECTIVE DEPLOYMENT MAY NOT COMMIT AN OBJECTIVE.**
This one now rewrites the forward-base RECORD instead (position, supplying base, re-link key, spend),
leaving the objective where it is so the fixture keeps qualifying, and it removes the reinforcement
module from the fixture — the rebuy CLEARS the eliminated flags as well as collecting on a failed
condition. The two values that can no longer be dirtied (`IsFOBUp()` and the objective's own position)
are now **labelled preconditions in the case, not round-trip claims**, and case 15 still owns the
objective-position round trip. Nothing was weakened to make it pass: the four fields carrying the claim
are all genuinely dirtied, and the `x=700` row was kept in the Logic case with its **real** answer
(`false`) beside a corrected in-gap row at `x=400`, so the coverage went up rather than down.

**Every other numeric row in the new Logic case was then re-derived against a model of the production
code** — 8 band rows, 6 flatness, 6 elevation, 5 road, 4 sum rows plus the weight ordering, 7 lattice
and 8 lateral — and all of them agree. The exclusion/radius pairing in `CollectFOBExclusions()` was
audited too: four `Insert` pairs, each on adjacent lines inside the same guarded block, so no path can
add a position without its radius.

#### THE SITING ALGORITHM AS BUILT — the numbers, and why each of them

⚠ **THE GENERATED PATH IS THE PRIMARY PATH AND IS TUNED AS IF THE AUTHORED ONE WILL NEVER EXIST (R17).**
No `OVT_FOBPosition` marker is placed in any shipped world; the prefab and the component exist so a map
author *can*, and the whole feature ships standing on the sampler alone. Authored markers win when they
exist, and they are still subject to every hard test.

`OVT_FOBSiting` is pure — band, exclusions, three score terms, and the two lattice helpers. The director
does all the world work and hands it numbers.

| Constant | Value | Why that number |
|---|---|---|
| `FOB_BAND_MIN_FRACTION` | 0.35 | Closer than a third of the way back and the base sits inside the objective's own defended ground, where it is found in the first minute and cannot be supplied |
| `FOB_BAND_MAX_FRACTION` | 0.75 | Further than three quarters and it is a second flag beside the one the faction already has, which is not what a FORWARD base is |
| `FOB_MIN_STANDOFF` | 350 m | Absolute floor. A short supply line would otherwise put the base 80 m outside a town |
| `FOB_MAX_STANDOFF` | 2500 m | Absolute ceiling, and also the radius the teardown and the stranded-marker lookup search |
| `FOB_SITING_STEPS` × `FOB_SITING_LANES` | 8 × 3 | ⚠ **`FOB_SITING_ATTEMPTS` is the PRODUCT, not a retry budget.** The sampler walks a deterministic lattice and 24 is how many points are in it. Each attempt costs an ocean read, a `TraceBox` and five surface samples, on a once-a-minute tick |
| `FOB_LATERAL_SPREAD` | 250 m | Lane 0 is **on the line** (shortest resupply, tried first), then one spread each side — enough to find the field behind the treeline |
| `FOB_FLATNESS_PROBE_RADIUS` / `_TOLERANCE` | 8 m / 2.5 m | ~1-in-6 over a 16 m span. Past it the structure floats at one corner — the one siting failure everybody can see |
| `FOB_ELEVATION_USEFUL_GAIN` | 30 m | Where the height preference saturates |
| `FOB_CLEAR_BOX_HALF` / `_HEIGHT` | 6 m / 8 m | The `TraceBox` clearance volume |
| `FOB_CLEARANCE_BASE` | 500 m | ⚠ **Copied from the placement limit's own figure for a base**, so the OF never plants a flag in ground another system considers spoken for |
| `FOB_CLEARANCE_RESISTANCE_SITE` | 300 m | Resistance FOBs and camps. Player-built and player-owned |
| `FOB_CLEARANCE_TOWN_MARGIN` | 150 m | Added to the town's own `GetTownRange()`. **Resistance-held towns AND villages**; occupying-held towns are deliberately *not* excluded, or a map where the resistance holds everything would have nowhere left |
| Score weights | flatness 3, elevation 2, road 1 | A strict ordering, asserted as an ordering rather than against the numbers. Flatness first because a sloped structure looks broken; road last because the insertion module snaps its own LZ anyway |

**IT IS DETERMINISTIC, WITH NO RANDOMNESS AT ALL** — the same design constraint objective selection
carries. A base that lands somewhere different every time the same campaign reaches the same state is
the unpredictability this feature retires, and determinism also makes a bad placement a reproducible
tuning question.

🔴 **WHAT HAPPENS WHEN IT FINDS NOTHING: the objective is RESET and blacklisted for one round, on the
spot, with a WARNING naming the objective and the attempt count** (T7.4). It is **not** retried next
tick: the band, the exclusions and the terrain do not change from one in-game minute to the next, so an
objective with nowhere to put a base has nowhere to put one for as long as it is the objective. A degenerate
band (`min >= max`, i.e. the supplying base is within ~470 m of the objective) refuses everything, which is
the same outcome.

#### 🔴 HOW `WasRestoredFromSave()` GATES THE RAISE — D11 / R2

The structure is a **persistence-tracked world entity**: vanilla persistence puts it back before any
deployment ticks, and the director's own serializer brings back the record of it. So the raise is gated
by `OVT_FOBRaiseSpawningDeploymentModule.DecideRaise(hasDeployment, alreadyAttempted, restoredFromSave,
eliminated)` — **a pure static, in the shape of the composition module's `DecideBuild()`**, precisely
because the failure it guards only appears on the SECOND load of a campaign, which no automated tier
reaches. Four gates:

1. **no deployment** → refuse, do **not** latch (a config template is later cloned onto a real deployment);
2. **already attempted** → refuse. This is the half that stops a **reinforcement rebuy** — which clears the
   eliminated flags and re-runs the convergence — putting a second structure beside the first;
3. **restored from a save** → refuse. Without it a long campaign grows one more flagpole per load, in a
   slightly different place each time because the site is re-sampled, every one persisted and dismantleable;
4. **eliminated** → refuse, do **not** latch (the rebuy clears the flag and the base is then owed).

⚠ **THE GATE AND THE LATCH ARE BOTH REQUIRED AND NEITHER SUBSTITUTES FOR THE OTHER.** The gate covers the
restored case; the latch covers the live one.

#### ⚠ THE WALKING-ARRIVAL PATH THE PLAN DID NOT DESCRIBE — and why it is not optional

§3.6's pseudo-code hangs the raise on `OnInsertionArrived(lz)`, which is reached **only from
`CompleteInsertion()`** — the truck-arrives path. **Five independent things divert an insertion onto foot**
(hop shorter than the walk threshold, convoy cap spent, no vehicle prefab, truck stalled, truck destroyed)
and **none of them reaches that hook**. Wired literally, the forward base would simply never be raised in
any of those five cases, the phase would sit until its timeout, and the objective would be blacklisted with
a log line blaming the clock.

So the module also polls on `OnUpdate` while its state is not `DRIVING`: any alive registered member within
`m_fRaiseOnFootRadius` (80 m) of the deployment position raises the base. ⚠ Counted through **handles and
the survivor mask**, never agents — a dormant group reports zero agents while being perfectly alive, and a
forward base is supposed to go up unobserved.

⚠ **The structure goes at the DEPLOYMENT POSITION, not at the landing zone**, on both paths. The LZ is
where the truck stopped (up to `m_fLZStandoffDistance` short, snapped to a road); the site is the point the
sampler traced and scored.

#### 🔴 THE CEILING IS A COUNTER, NOT A WALLET — say it out loud, because it reads like a budget

`m_FOB.spent` records what has **already left** the one deployment pool on this forward base and everything
sourced from it. Nothing is reserved, held, moved, refunded or carried over. `CreateObjectiveDeployment()`
now makes **two** tests that ask different questions, and then debits once:

```
pool  >= cost                                  can the faction afford this at all
WithinFOBBudget(cost)                          has this forward base already had its share
  -> IsFOBBudgetActive() == m_FOB.up || m_bFOBDeploymentSent
  -> WithinFOBCeiling(m_FOB.spent + cost, FOBBudgetCeiling(objectiveFOBCost))
ForceCreateDeployment                          refused -> spend nothing
SubtractFactionResources                       <-- THE ONE DEBIT, unchanged
CountFOBSpend(cost)                            a counter recording what already left
```

- **The ceiling is INACTIVE during harassment**, so Phase 1 behaves exactly as it did before this phase.
- **It arms BEFORE the forward base's own create** (`SendFOBOperation()` sets `m_bFOBDeploymentSent` first
  and clears it again if the create is refused), because §3.7 is explicit that the budget covers "the
  structure itself".
- `WithinFOBCeiling`'s **two-argument signature is pinned by Phase 2 logic cases** and was deliberately not
  widened to T7.7's three-argument form; the caller adds the prospective spend, which is what that
  function's own header says it expects.
- `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` is **empty** — the name is not written
  out even in a comment, because a quotation would defeat the acceptance criterion.

#### THE THREE TEARDOWN EXITS, AND THE ONE THAT IS PAID FOR

| Exit | Where | Penalty | Blacklist |
|---|---|---|---|
| **Starvation** | `TickFOBStarvation()` → `ResetObjective(..., true)` | **none** — the supply line failed, nobody took it off them | yes |
| **Player dismantle** | `OnFOBDismantledByPlayer()` → subtract `objectiveFOBCost`, then `ResetObjective(..., true)` | **YES, and only here** | yes |
| **Counter-QRF resolved** | `TickCounterQRF()` → `ResetObjective(..., false)` | none | no |

⚠ **THE TEARDOWN ITSELF IS INSIDE `ResetObjective()`, NOT BESIDE IT.** `TearDownFOB()` is called from the
machine's one reset path (and from `CommitObjective()`, belt and braces), so the phase timeout, a failed
re-link and a re-selection are covered **by construction** rather than by three callers remembering. It:
returns immediately when nothing was ever sent (the common case, no queries at all); sweeps the FOB config
and the garrison config out of `FOB_AREA_RADIUS` (250 m) of the recorded position **plus** a name-scoped
lookup around the objective for a marker the ledger never knew about; removes the structure; drops the
anchor; and zeroes the runtime state.

- **The insertion reservations are released by DELETING the deployments**, not by anything in the teardown:
  `DestroyDeployment()` runs every module's `Cleanup()`, and the insertion module's releases the convoy
  slot, deletes its waypoints and disposes of its truck. A leaked slot is permanent.
- **The structure is removed through `OVT_ResistanceFactionManager.DestroyPlacedItem()`** — Phase 6's
  navmesh-queue-then-delete helper, unchanged and not re-implemented.
- ⚠ **It is found by PREFAB RESOURCE NAME, read off the config**, not by the module's `EntityID`: that link
  is runtime-only, so a campaign that has been loaded since has a structure standing with nothing pointing
  at it. Same join Phase 6 established for structure costs, and for the same reason.

#### The new request's validation list (T7.9)

`OVT_CampaignRequestComponent.DismantleEnemyFOB()` → `RpcAsk_DismantleEnemyFOB()`. **No arguments at all**,
exactly like `StartBaseCapture`, so the payload cannot express a lie (BUG-025). The server checks, in order:
server authority → a real player id → a character that exists → that character is not dead → an objective
director exists → a forward base is actually up → the caller is within `FOB_DISMANTLE_RANGE` (30 m) of the
**director's** recorded position → no living occupying-faction soldier within `FOB_DEFENDER_CLEAR_RADIUS`
(150 m). Every refusal logs through `RejectCampaignRequest`.

🔴 **THE CLIENT MAY NOT ASK THE DIRECTOR WHERE THE FORWARD BASE IS — a defect caught during the build.**
None of the director's state replicates (G12), so on a dedicated server's client `IsFOBUp()` is false and
`GetFOBPosition()` is the zero vector: an action gated on either is invisible or permanently refused for
everyone but the host. The rule was therefore split — `CanDismantleFOBAt(callerPos, fobPos, out refusal)` is
client-safe and is what the action asks **about the flag it is attached to**; `CanDismantleFOB(callerPos,
out refusal)` is the server's overload and adds the `m_FOB.up` test. One body, two entry points, so the
prompt and the server cannot drift.

#### Design decisions worth not re-deriving

- **The garrison config authors NO `DEFEND` patrol module, and T7.6's instruction to add one was refused.**
  `DEFEND` anchors on where a group is **registered**, and an insertion registers its force at the SOURCE —
  so a DEFEND plan would park the forward base's garrison at the base it set out from, permanently. The
  insertion module's own cycling fallback march is what walks them onto the site and holds them there. Same
  refusal, same reason, as Phase 5's for harassment.
- **`objectiveFOBGarrisonMax` caps GARRISON DEPLOYMENTS, not groups within one.** A config cannot read
  difficulty; the director counts live garrison deployments within `FOB_AREA_RADIUS` and stops. Same shape
  as `objectiveHarassmentMaxConcurrent`.
- **Phase 1 operations do NOT continue into Phase 2, and that is a deliberate deferral.** §3.2 has the
  forward base becoming the insertion source for further harassment; all three Phase 1 configs author
  `m_iRequiredPhase 1`, so they are collected the moment the ramp advances, and changing that is a Phase 5
  contract with Init cases pinned to it. What *does* launch from the forward base is its own garrison,
  through `OVT_ObjectiveAnchorSourceProvider` — the seam any later phase needs to make the rest true.
- **A supply party that vanishes before it builds is NOTICED, and that is the phase's one wedge risk
  closed.** A deployment can be collected by its own condition module, wiped out or deleted by a Game
  Master between the send and the raise; with `m_bFOBDeploymentSent` latched and nothing watching, the
  phase would sit doing nothing until its 240-minute timeout with no line in the log. `SendFOBOperation()`
  now distinguishes "sent and still standing" (wait) from "sent and gone" (log, drop the flag, re-site
  next interval). ⚠ The resources already spent stay counted against the ceiling - they really were spent.
- **A restored deployment that can never raise is COLLECTED, not waited on.** A save taken mid-drive comes
  back with a marker whose raise is gated off; `SendFOBOperation()` finds it, deletes it and re-sites next
  interval. One lost interval instead of a permanent wedge.
- **`OVT_ObjectiveAnchorSourceProvider` falls through rather than failing.** No director, no objective, no
  forward base, or one just torn down: all degrade to the nearest controlled base. Answering false would
  strand the force, and the insertion module's whole contract is that it never does.
- **`AbandonRaise()` exists with no production caller, on purpose** — the same decision Phase 6 recorded for
  `AbortMission()`. "This module will not build" and "it built" are different statements, and anything that
  later has to cancel an in-flight raise needs something to reach for that does not put a persisted flagpole
  in the world.
- **`m_rFOBPrefab` has NO defvalue.** The structure prefab is named in exactly one place — the config — so
  the teardown (which reads the authored value back off that config) cannot disagree with the raise.
- **`OVT_FOBPositionComponent` carries ONE attribute.** A per-marker clearance override was drafted and cut:
  an attribute the director does not read is a knob that lies to map authors.
- **The FOB structure carries NO `OVT_PlaceableComponent` or `OVT_BuildableComponent`**, deliberately — it
  must not become a sabotage target or a player-removable item, and both of those components are how a
  thing becomes one.
- 🔴 **NO NOTIFICATION IS SENT WHEN THE BASE GOES UP (D12).** This is an explicit requirement, not an
  oversight. It is the one thing in the ramp the resistance is meant to *find*. The note is written on
  `NotifyDirector()` so the next reader does not "fix" the inconsistency.

#### ⚠ Residual risks and things a Workbench pass should look at

- **The FOB prefab's props were chosen from GUIDs READ out of existing repo files**, never guessed —
  `FlagPole_02_V1` (from `BaseFlag_US.et`), the Soviet camo-net tent, Czech hedgehogs, a barbed-tape coil
  and two crates (all from `AmmoCache_S_US_02.et` / `buildables.conf`). The child offsets are hand-authored
  and have **not** been looked at in Workbench: floating or clipping props are the likely first finding.
  The tent is Soviet-flavoured on both occupier factions, which is the one faction-neutrality compromise.
- **`Duration 15` on the action is authored in the prefab**, so a Workbench re-save of that prefab must keep
  it.
- **No `OVT_FOBPosition` instance exists in any world layer** — by instruction (R17). Placing a handful on
  Eden is a follow-up, not a blocker.

#### Tests added, and the can-fail proofs

- **Logic (1 new case in `OVT_TEST_Logic_ObjectiveScaling.c`)** — `…_FOBSiting`: the band inclusive at both
  ends, one metre either side, and **a collapsed band asserted ON its own bound** (⚠ the only row that can
  catch a deleted degenerate guard — every other distance is refused by the ordinary tests either way);
  exclusions over empty, both-null, one-null, inside, outside, **exactly on the radius**, a **ragged pair**
  and a disabled radius; all three score terms including the **negative-means-no-road sentinel** and the
  below-the-objective elevation floor; the sum bounded at both ends and the three weights strictly ordered;
  and the lattice helpers clamped at both ends with lane 0 on the line.
- **Init (new `OVT_TEST_Init_ObjectiveFOB.c`, 5 cases)** — both configs registered, valid, **scoped to phase
  2**, ordered spawning-before-reinforcement, authoring no patrol module, with a loadable structure prefab
  and groups both factions field; the **raise decision** over all four gates including "eliminated does not
  latch"; the **anchor provider** preferring the forward base, falling through without one, and honouring
  its own distance limit; the **teardown** driven with two REAL inert deployments and asserted to leave
  neither standing; the **dismantle refusal ladder** with its keys, plus the client and server entry points
  agreeing; and **clone fidelity over all twenty-five attributes** with the latch fired first through
  `AbandonRaise()`.
- **Init seam (1 case appended to `OVT_TEST_Init_CampaignRequestSeam.c`)** — the fifth verb is reachable off
  the local controller and refuses cleanly with no forward base standing, leaving the phase and the spend
  counter untouched. ⚠ This is the only mechanical defence against the `Rpc()` arity blind spot for it.
- **Persistence (1 new case + 1 extension)** — `…_ObjectiveFOB_RelinksItsDeployment` saves a forward base
  whose deployment IS standing and proves it is re-adopted; the Phase 2 case gains
  `AssertStrandedObjectiveIsTornDown()`, the other half, proving a payload naming a deployment that does not
  exist ends the objective rather than stranding it.
  ⚠ **BOTH DRIVE EXACTLY `FOB_RELINK_ATTEMPTS` TICKS AND NOT ONE MORE.** The tick that gives up leaves the
  machine IDLE, and the IDLE branch of the next tick selects a real objective and enters its first phase
  with the operation countdown armed to **zero** — so one extra tick buys a real deployment with real
  resources in a real campaign, and nothing refunds a deleted deployment. Both plant a countdown afterwards
  as well.
- **29 faults injected one at a time and compiled** (5 on `OVT_FOBSiting`, 5 config/scoping, 4 on the raise
  decision, 3 on the anchor provider, 3 on the teardown, 5 on the clone, 3 on the dismantle ladder, 1 on the
  re-link radius). **Every one exited `compile-check.sh` 0** — which is the point: a degenerate band accepted, a ragged exclusion pair tolerated,
  a config scoped to the wrong phase, a source provider swapped, a sweep arm deleted and a dropped clone line
  are none of them script errors. All subjects restored and re-compiled clean. **Suites were not run — that
  is the orchestrator's job** (`.claude/test-policy.md`).

#### Play-test items this phase adds (nothing here is covered by the spine)

- 🔴 **Does the campaign grow a second forward base on a save → quit → Continue?** The D11 headline. Get a
  base up, save, quit, Continue, and count the flagpoles. There must be exactly one.
- 🔴 **Does the site look sensible?** The whole generated path is judgement. Watch where three or four bases
  land across a session: level ground, not in a ditch, not on a road, not in somebody's field, not inside a
  village's outskirts, and visibly *between* an occupying base and the objective.
- **Does the truck get there, and does the base go up if it does not?** Both paths must work. Blow the truck
  up mid-drive and confirm the men walk in and the base still goes up.
- **Does the prefab look right?** The child props have never been seen in Workbench. Floating crates, a tent
  clipping the flagpole, or the whole thing sunk into a slope are all live possibilities.
- **Does the garrison come from the FORWARD base once it is up?** Watch the second garrison truck: it should
  set out from the forward base, not from the rear. This is the entire mechanical payoff of the phase.
- **Does starvation fire, and does it recover?** Take the supplying base with the resistance, or stand at the
  forward base for 30 in-game minutes. Both must abandon it, with a log line each way.
- **Does the dismantle work, and only when the site is clear?** Kill the garrison, hold the flag for 15 s.
  With one enemy alive inside 150 m the action must refuse **with a stated reason**.
- **Does the pool fall by exactly `objectiveFOBCost` on a dismantle, and never rise?** Note
  `m_mFactionResources` before and after.
- **Does the ceiling stop the spending?** Let a forward base run for a long time and confirm it stops buying
  garrisons rather than draining the reserve.
- **MP/JIP.** Entirely uncovered. ⚠ **The dismantle action is the first thing in this feature a remote
  client interacts with**, and the client/server split described above has never been exercised on two
  machines.

### 2026-08-19 — Phase 6 built (base sabotage: the feature that destroys player property)

#### 🔴 WHAT THE PLAYER PERMANENTLY LOSES — state this verbatim in the Phase 10 documentation

While a base the resistance holds is the occupying faction's **current objective**, a sabotage team is
sent to it once per operation interval (45 in-game minutes on Normal). If that team reaches the base and
holds it with **no player inside 150 m**, then every `objectiveSabotageHoldSeconds` (120 s on Normal) it
**permanently destroys one player-built structure**, cheapest first, up to
`objectiveSabotageStructuresPerMission` (2 on Normal) per mission.

**Permanently means permanently.** There is no rubble, no repair action, no partial refund, no salvage
and no undo. The money and the XP the player spent are gone. It does **not** come back on the next load
either: removal deletes the entity, and a deleted entity is simply never written to a save.

What is at risk, in the order it goes (the shipped `Configs/Resistance/buildables.conf` prices):

| Order | Structure | Cost |
|---|---|---|
| 1 | Bunkers | 750 |
| 2–3 | Recruitment Tent, Medical Tent | 1 000 |
| 4 | Guard Tower | 1 200 |
| 5–6 | Vehicle Maintenance Ramp, Helipad | 1 500 |
| 7 | Fuel Depot | 2 000 |
| 8 | Garage | 8 000 |

Placeables associated with the base (ammo boxes, cots, furniture) are in the same list at their own
prices. **Only structures associated with THAT base are ever at risk** — a camp's, a forward base's, a
house's and another base's are all excluded, and nothing is touched once the occupying faction holds the
base itself.

**What the player can do about it, and it is all of it:** be there. A single player standing within 150 m
of the base centre stops the clock dead (it pauses, it does not reset). Killing the team ends the
mission. Taking the objective away — by making another place a better target — ends the campaign against
this base. The one notification per mission is the warning, and it fires on the **first** structure, so
there is always at least one interval of warning before the second goes.

#### T6.1 — THE READ-ONLY SURVEY. Nothing was deleted until this table existed.

| # | Subject | Verified state, 2026-08-19 | Verdict |
|---|---|---|---|
| 1 | **The removal path** | `OVT_ResistanceFactionManager.RemovePlacedItem(RplId entityId, int playerId)` is the only removal API for a placed/built structure. Its two load-bearing lines, **in this order**: `OVT_NavmeshRebuild.Queue(entity)` **then** `SCR_EntityHelper.DeleteEntityAndChildren(entity)`. **The order is the whole thing:** `Queue()` measures the entity's bounds at CALL time and issues the merged rebuild 1 000 ms later, so the capture happens while the object still stands and the rebuild happens once it is gone. Reversed, there is nothing left to measure and the carve stays in the navmesh forever — the AI keeps refusing to walk through ground that is now empty, with no symptom anyone would trace to a demolished tent. `OVT_NavmeshRebuild`'s own header states the rule ("CAPTURE BEFORE YOU DELETE"). It sends no notification and performs no untracking; a deleted entity is not saved. | ✅ as the plan described |
| 2 | 🔴 **The blocker** | The owner-or-officer check refuses a server call: `playerUid = GetPersistentIDFromPlayerID(playerId)` (returns `""` for `playerId < 1`), then `if(ownerUid != playerUid && !isOfficer) return;`. A sabotage team has no `playerId`, so it is refused. | **DECIDED: extract a shared helper. NOT a `playerId == -1` bypass.** See below. |
| 3 | **The cost join** | Confirmed absent — `grep` over `m_aPlaceables`/`m_aBuildables` finds only **index-based** lookups (`OVT_ResistanceRequestComponent`, `OVT_ResistanceFactionManager.PlaceItem/BuildItem`, both UI contexts). Nothing joins a live entity to a config entry. 🔴 **AND THE PLAN'S ASSUMED JOIN KEY DOES NOT WORK** — see below. | **New code, one place:** `OVT_ResistanceFactionManager.GetStructureCost(IEntity)` |
| 4 | **The enumerator** | Confirmed absent — there is no registry of placed structures anywhere. `OVT_ItemLimitChecker.CountItemsForLocation(locationId, baseType, searchCenter)` is the exact shape: `QueryEntitiesBySphere` at radius **500 for `EOVTBaseType.BASE`**, `FilterItemCallback` (either `OVT_PlaceableComponent` or `OVT_BuildableComponent` present), `CountItemCallback` (association id **and** type match). | **New code:** the same shape, collecting instead of counting, in the module |

#### 🔴 T6.1 ITEM 3, THE FINDING THAT CHANGED THE DESIGN: the type string does NOT match `m_sName`

The plan says the join is `GetPlaceableType()` / `GetBuildableType()` → `OVT_Placeable.m_sName` /
`OVT_Buildable.m_sName`. **Checked against the shipped data, that join fails for seven of the eight
buildables:**

| Config `m_sName` | Prefab's authored `m_sBuildableType` | Match? |
|---|---|---|
| `Guard Tower` | `GuardTower` | ❌ |
| `Recruitment Tent` | `RecruitmentTent` | ❌ |
| `Medical Tent` | `MedicalTent` | ❌ |
| `Vehicle Maintenance Ramp` | `VehicleMaintenanceRamp` | ❌ |
| `Bunkers` | `Bunker` | ❌ (also singular) |
| `Garage` | `VehicleGarage` | ❌ (a different word) |
| `Fuel Depot` | `FuelDepot` | ❌ |
| `Helipad` | `Helipad` | ✅ (the only one) |

A type-string join would therefore have priced almost every structure in the game at nothing, and
"cheapest first" would have demolished the garage on the first interval — the exact opposite of the
design's one concession to the player.

**THE JOIN IS BY PREFAB RESOURCE NAME.** `OVT_PrefabUtils.GetPrefabName(entity)` matched against the
config entry's `m_aPrefabs`. It is exact, needs no data re-authored (re-authoring seven type strings
would break anything else keyed on them), survives a persistence restore (EPF respawns from the same
prefab), and is what `PlaceItem`/`BuildItem` spawned the object from in the first place. Buildables are
searched before placeables, and a prefab no entry claims answers
`OVT_ResistanceFactionManager.UNKNOWN_STRUCTURE_COST` (**1 000 000, deliberately huge so an unpriced
structure sorts LAST** — sorting first would make anything a mod adds the first casualty).

⚠ **The join's precondition is that no prefab appears in two config entries.** Nothing else in the tree
would notice a duplicate; Init case `…_JCostJoinIsUnambiguous` asserts it across both configs together.

⚠ **The RAW authored `m_iCost`, not the difficulty-multiplied one.** The only consumer orders by price
and the multiplier is uniform, so applying it would add a config dependency and change no ordering.

#### 🔴 T6.1 ITEM 2, THE DECISION: a shared helper, and why NOT the `playerId == -1` bypass

**`OVT_ResistanceFactionManager.DestroyPlacedItem(IEntity entity)`** — the queue-then-delete pair, public,
no permission check, not reachable from any RPC. `RemovePlacedItem` now ends with a call to it and keeps
its owner-or-officer check **exactly where it was**; the sabotage module calls it directly.

The `playerId == -1` bypass was genuinely tempting: this same file already documents that convention
three times (`PlaceItem` and `BuildItem` both say *"-1 = server-initiated, free"*, and
`ChargeForGarrison` says *"A playerId of -1 means a server-initiated (free) garrison"*), and it would not
even have been exploitable, because `OVT_ResistanceRequestComponent.RpcAsk_RemovePlacedItem` already
refuses `playerId <= 0` before calling in. It was still refused, on four counts:

1. **The `RplId` round trip is lossy and silent.** The bypass keeps the `RplId` signature, so the module
   would have to go entity → `Replication.FindId` → `Replication.FindItem` → `rpl.GetEntity()` for an
   entity it is already holding. Any structure whose prefab carries no `RplComponent` is then
   **indestructible, with no log line**. (The shipped bunker survives only because it inherits one from
   `DestructionMultiPhase_Rpl_Base`; that is luck, not a contract, and it certainly is not one for a
   modded placeable.)
2. **It widens the wrong method.** `RemovePlacedItem` exists to answer "is this caller allowed to remove
   this?"; a bypass gives the authorization method a way to skip authorization. The helper separates
   mechanism from authorization, so **there is no bypass to audit at all** and the acceptance criterion
   ("the player's own `RemovePlacedItem` still refuses a non-owner non-officer") is true by construction
   rather than by test. This epic's headline debt is unvalidated capture RPCs (BUG-025) and this phase
   adds nothing to it.
3. **The convention it borrows is about MONEY, not permission.** All three existing `-1` sites are
   "server-initiated, therefore free". Overloading the same sentinel with "therefore also unowned" is the
   kind of drift that costs a security review later.
4. **The pair was already copied FOUR times in this one file** — `RemovePlacedItem`, `RemoveCamp`,
   `CleanupCampObjects` and `CleanupFOBArea`, three of them carrying a comment pointing at the fourth.
   All four now call the helper, so the ordering rule has exactly one home and one doc block. The three
   extra re-points are behaviour-identical (each site already null-checked).

⚠ **One pre-existing delete was deliberately left alone:** `FindAndDeleteOldCamp` deletes a camp entity
with **no** navmesh queue. It predates this feature and is not a placed/built structure; fixing it is a
bug-pass item, not a Phase 6 one.

#### The enumerator as built — shape and radius

`CollectTargets(base, resistance)` on the module: `QueryEntitiesBySphere(base.location,
m_fSearchRadius = 500, CollectTargetCallback, FilterStructureCallback, EQueryEntitiesFlags.ALL)`.

⚠ **500 m IS COPIED FROM `CountItemsForLocation`'s OWN FIGURE FOR `EOVTBaseType.BASE`, AND THE TWO MUST
AGREE.** A structure the placement limit counted towards this base but this module could not find would
be an object the player was charged for and can never lose — an invisible asymmetry between two systems
that both claim to know what belongs to a base.

The filter is the `FilterItemCallback` shape (either ownership component present). The collect callback
reads the association off whichever component is there and calls the pure
`OVT_BaseSabotageBehaviorDeploymentModule.IsSabotageTarget(associatedId, associatedType, targetBaseId,
targetBaseFaction, myFaction)` — three exclusions: the base is ours, it belongs somewhere else (a camp, a
forward base, another base), it belongs nowhere. The manager is **cached in a member for the query** so
the callback pays for one component resolve rather than one per candidate.

⚠ **The candidate list is rebuilt on every interval, never cached.** Two minutes pass between
demolitions; a cached list would hand deleted entities to the removal path and miss everything built
since.

#### Where the cost join lives, and where the pool is still debited

- **Cost join:** `OVT_ResistanceFactionManager.GetStructureCost(IEntity)`, beside `DestroyPlacedItem` and
  the two configs it reads. It is on the manager rather than in the module because the registries are the
  manager's data and the next consumer (a dismantle refund, a Game Master panel, an intel surface) must
  not have to reimplement it.
- **Pool:** unchanged. `OVT_ObjectiveDirectorComponent.CreateObjectiveDeployment()` and nowhere else.
  `SendSabotageOperation()` is a third caller of the same method; there is still exactly one
  `SubtractFactionResources` in the component.

#### The director's base branch, and the Phase 5 trap NOT repeated

- **`SendNextOperation()` now chains three senders**, tower recapture → harassment → sabotage, and still
  sends **at most one operation per interval**. Harassment and sabotage are mutually exclusive by
  objective kind (each refuses on its first line), so the order between those two is arbitrary.
- **`SendSabotageOperation()`** resolves the objective position back to a real base within
  `BASE_OP_RESOLVE_RADIUS` (100 m — selection copies `base.location` verbatim, so this always passes by
  metres in a live campaign) and refuses if the occupying faction already holds it. Concurrency reuses
  `objectiveHarassmentMaxConcurrent`, counted from the LIVE deployment list within `SABOTAGE_OP_RADIUS`
  (300 m), never from the teardown ledger.
- 🔴 **`OnSabotageSuccess()` COUNTS. IT DOES NOT DECIDE.** T6.6 asked for "increments the counter and
  re-checks the Phase 2 gate", exactly as T5.8 did, and it was refused for exactly the reason Phase 5
  recorded. The base gate lives in **`CheckBaseHarassmentGate()`, reached only from `TickHarassment()`**,
  behind `DirectorTick()`'s three early returns.
- **The base Phase-2 gate is `OVT_ObjectivePhaseRules.BasePhase2Gate(successes)` — one completed mission,
  and DELIBERATELY no second conjunct**, unlike the town gate. ⚠ The reason the town gate needs the
  "this ramp did it" conjunct does not exist here: a town can already be under its support threshold when
  it is CHOSEN, so the gate could fire on the phase's entry tick; `sabotageSuccesses` is zeroed by
  `CommitObjective`, nothing in the campaign but a completed mission raises it, and raising it once
  requires a team to have been sent, driven, held the base unopposed and demolished something. The
  counter **is** the world fact here — and it round-trips a save with no session-local state to rebuild.
  The Persistence round-trip fixture saves a **TOWN** objective (its two `OnSabotageSuccess()` calls are
  therefore inert against this gate) and dirties with a **BASE** whose counters `CommitObjective` zeroes,
  so neither half of that case can reach the new transition.
- ⚠ **Phase 5's recorded residual widens slightly and is still accepted:** the persistence fixture's dirty
  phase plants only 4 operation ticks before requesting a reload, and its objective is now a BASE that
  *can* reach a spend path (sabotage as well as recapture). `BASE_OP_RESOLVE_RADIUS` is what keeps it
  harmless — the dirty position `9000 30 9500` is synthetic and has no base within 100 m, so
  `SendSabotageOperation()` refuses before `CreateObjectiveDeployment()` is reached.

#### Design decisions worth not re-deriving

- **The mission latch is on the MISSION, not on a firing.** `EvaluateDemolition` deliberately does not
  latch — it fires once per interval, repeatedly — and `m_bMissionReported` is what ends the mission. A
  latch in the wrong place gives either one structure per mission forever or a team that strips the base
  bare, and neither is a script error.
- **`AbortMission()` exists with no production caller, on purpose.** "The mission is over" and "the
  mission succeeded" are two different statements and only `CompleteMission()` makes the second. Anything
  that later has to cancel an in-flight mission (a teardown, a reset, a Game Master) must have something
  to reach for that does **not** bank a sabotage success for work nobody did. It is also what the Init
  cases use, precisely because `CompleteMission()` would call the live director.
- **"Nothing left to demolish" REPORTS SUCCESS.** A base the resistance built nothing on would otherwise
  send mission after mission for the whole phase and never open the gate — a ramp that cannot finish with
  no log line to explain it. The team still has to hold the base for a full interval first, so G3 holds.
- **One notification per MISSION, on the FIRST structure, broadcast (`playerId -1`).** Preset
  `ObjectiveSabotage`, `{6B8C3F5D00000061}`, appended to `Configs/overthrowBroadcastMessages.conf`, with
  `#OVT-Msg-ObjectiveSabotage` (title) and `#OVT-Msg-ObjectiveSabotageDetail` (body, `%1` = base name).
  It names the base and nothing else — not the attacker, not the objective, and **nothing about the
  forward base, which stays silent by explicit requirement**. There is no faction-scoped send on the
  notification manager, so broadcast is the only option and is fine: a building coming down is public.
- **The sabotage config has NO rungs**, unlike the harassment ladder. A bigger team does not make a
  structure more destroyed; what escalates on a base objective is how many missions the counter-attack
  gate demands, and difficulty already scales that (inverted).
- **The module's search radius, hold radius and max base distance are all authored twice** — on the module
  as fallbacks and in difficulty for the two timing figures. Difficulty wins whenever it is loaded; the
  attributes exist for a world with no campaign, which is every bare `new` subject in a test.

#### Tests added, and the can-fail proofs

- **Logic (1 new case in `OVT_TEST_Logic_ObjectiveScaling.c`)** —
  `…_SabotageTargetsCheapestFirst`: cheapest from front/middle/back, ties by input order, the destroyed
  mask honoured entry by entry, empty/null/ragged all answering `NOTHING_TO_SELECT`, a **negative** cost
  sorting first (the only row that catches a literal-seeded comparison), the **eight shipped buildable
  prices walked end to end** to reproduce the documented bunkers-first/garage-last sequence, and
  `BasePhase2Gate` on both sides of one.
- **Init (new `OVT_TEST_Init_ObjectiveSabotage.c`, 4 cases)** — the config registered, valid, BASE-typed,
  ordered behaviour-before-reinforcement, authoring no patrol module, carrying
  `OVT_BaseControlConditionDeploymentModule` with `m_bRequireControl 0` and the objective condition, and
  fielding a group **both** factions can load; the demolition decision driven through
  empty / defended / held / interrupted / expired with the pause-not-reset claim asserted on the tick
  count, the **fires-again** claim (no per-firing latch) and the reported-mission refusal; the target
  filter over all eight exclusion rows including the accepted one first; the cost join's data
  (every entry priced and prefabbed, **no prefab claimed twice**, the unpriced sentinel greater than every
  authored cost, a null entity unpriced); and clone fidelity over all six attributes with the latch fired
  first, plus difficulty winning over both module fallbacks.
- **28 faults injected one at a time and compiled** (5 Logic, 5 config/ordering, 5 on the demolition
  decision, 4 on the target filter, 4 on the cost join, 5 on the clone and the difficulty precedence).
  **Every one exited `compile-check.sh` 0** — which is the point: a comparison inverted, a guard deleted,
  a config module reordered, two entries claiming one prefab, a dropped clone line and a difficulty
  branch removed are none of them script errors. All subjects restored and re-compiled clean.
  **Suites were not run — that is the orchestrator's job** (`.claude/test-policy.md`).

#### Play-test items this phase adds (nothing here is covered by the spine)

- 🔴 **Does a destroyed structure stay destroyed across a save → quit → Continue?** This is criterion F5
  and the entire reason T6.1 exists. Build a guard tower at a base you hold, let a sabotage mission take
  it, save, quit, Continue. It must not be standing.
- 🔴 **Does the AI path through the space it occupied?** The navmesh carve is captured before the delete
  and rebuilt a second later. Watch an AI group walk across where a bunker was. This is the failure the
  ordering rule exists for and it has no other symptom.
- **Does it take the cheap things first?** Build bunkers AND a garage at the same base and let two
  missions run. The bunkers must go first.
- **Does standing there stop it?** Stand inside 150 m of the base centre through a whole interval. Nothing
  may come down, and the clock must **pause** rather than reset — walk away and the next demolition
  should arrive sooner than a full interval later.
- **Is the notification once per mission?** Two structures per mission on Normal, one message.
- **Does it leave everything else alone?** Put a camp with placeables inside 500 m of the base. Nothing in
  the camp may be touched.
- **Does it stop when the base flips?** Take the base with the occupying faction mid-mission; the team
  must stop demolishing immediately and the deployment must be collected.
- **Does the gate advance?** One completed mission should log "raising a forward operating base" within an
  in-game minute (and then do nothing until Phase 7).
- **Does the pool balance?** Note `m_mFactionResources` for the occupying faction and confirm each
  sabotage operation costs exactly the config's `GetTotalResourceCost()`, once.
- **MP/JIP.** Entirely uncovered by the spine, as for the rest of this feature.

### 2026-08-19 — Phase 5 built (harassment, tower recapture, and the first spending)

#### The two prerequisite registry fixes Phase 4 left behind — both closed

| Fix | Before | After | Why |
|---|---|---|---|
| US `specops_team` | `{D807C7047E818488}…/Group_US_SniperTeam.et` — **the same prefab as `sniper_team`** | `{D0886786634E55AE}Prefabs/Groups/BLUFOR/GreenBerets/Group_US_GreenBeret_Squad.et` | A 2-man sniper/spotter pair cannot hold a radio tower for the 600 s `objectiveTowerRecaptureHoldSeconds` the new module demands of it. The Green Beret squad is a 6-man `GROUPSIZE_LARGE` group (AI budget 6). **USSR's `specops_team` was left on `Group_USSR_ManeuverGroup.et`** — that one is already a proper squad and re-pointing it would have been a tuning change nobody asked for. `m_iCost` stays 40 on both sides so the same role costs the same whichever faction occupies. |
| US `rifle_squad` | **absent** — USSR shipped eight group entries, US seven | `{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et`, appended, `m_iCost 30`, mirroring the USSR entry | Rung 2 of the harassment ladder. Without it a US-occupier campaign had a hole in the ramp. **No degradation was needed** — the prefab exists in the vanilla tree. |

⚠ **Neither GUID was guessed.** The reference tree at `/mnt/n/Projects/Arma 4/ArmaReforger` publishes no
`.meta` files, so both were read out of `Configs/EntityCatalog/US/Groups_EntityCatalog_US.conf`, which
names every US group prefab with its GUID. ⚠ **The Green Beret path is
`Prefabs/Groups/BLUFOR/GreenBerets/…`, not `Prefabs/Groups/BLUFOR/…`** — Phase 4's note omitted the
subdirectory.

#### T5.7 — THE RAMP MECHANISM: four thin registry variants, and why

**DECIDED: variant configs. The plan's preferred option, and the alternative was refused outright.**
The framework has no per-create group override and inventing one would have been a second way of
deciding what a deployment contains, parallel to the config system and invisible to everything that
reads a config — the evaluator's cost model, the Game Master panel, the reinforcement rebuy and the
save all read the CONFIG, not whatever the creator meant.

Each rung is `Deployment_ObjectiveHarassment.conf` inherited in `overthrowDeployments.conf` with a
different name, group type and cost, exactly as `Deployment_TownPatrol` is inherited there today. **The
base config IS rung 0** rather than an unregistered template: an unregistered config cannot be found by
`FindConfigByName`, and a template nobody can create is a file that silently rots.

| Rung | Config name (**the key, matched by string in three places**) | Group | `m_iCostPerGroup` | `m_iBaseCost` | Registry GUID |
|---|---|---|---|---|---|
| 0 | `Objective Harassment (Patrol)` | `light_patrol` | 30 | 20 | `{6B8C3F5D00000041}` (the base config itself) |
| 1 | `Objective Harassment (Fireteam)` | `light_fireteam` | 40 | 25 | `{6B8C3F5D00000042}` |
| 2 | `Objective Harassment (Rifle Squad)` | `rifle_squad` | 55 | 30 | `{6B8C3F5D00000043}` |
| 3 | `Objective Harassment (Heavy)` | `heavy_infantry` | 50 | 35 | `{6B8C3F5D00000044}` |

Plus `Objective Tower Recapture` (`{6B8C3F5D00000045}`, `specops_team`), which has no rungs.

⚠ **THE NAMES ARE A THREE-WAY STRING CONTRACT AND NOTHING ENFORCES IT BUT A TEST.**
`OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER` holds them; `FindConfigByName()` resolves a rung
with one; `GetDeploymentNearPosition()` and the concurrency count match live deployments back with one;
and the teardown ledger stores one. A name changed in the registry and not in the array does not fail to
parse and does not warn — **the ramp simply stops sending anything**, with one WARNING line per in-game
minute as its only symptom. Init case `…_ARampConfigsResolveAndAreOrdered` resolves every entry against
the live registry for exactly that reason.

⚠ **`HarassmentLadderIndex` indexes STRAIGHT INTO that array** and saturates at the top rung.
Reordering the array re-tunes the whole escalation; appending a fifth rung lengthens it for free.

#### T5.2 — the modifier append rule, restated where the next author will hit it

The new entry is **`ObjectiveHarassment`, `{6B8C3F5D00000001}`, at the END of
`Configs/Modifiers/supportModifiers.conf`** — `baseEffect -20`, `timeout 7200`, `stackLimit 4` (one per
ladder rung), `flags 3` (ACTIVE|STACKABLE), title `#OVT-Modifier_ObjectiveHarassment`.

⚠ **`OVT_Modifier.m_iIndex` IS THE POSITIONAL INDEX IN `m_aModifiers`**, assigned in
`OVT_TownModifierSystem.PostInit()`, and that index is what the replicated per-town modifier lists
carry. An entry inserted anywhere but the end shifts every later index by one, so **every town in every
live save comes back carrying different modifiers than it was saved with.** A well-meaning tidy-up that
alphabetised the config would do it and nothing else in the tree would notice — which is why the Init
case asserts the POSITION and not merely the lookup.

`OVT_ObjectiveHarassmentSupportModifier` ships with an empty `OnTick` and says in its header that it
exists to carry an index. It is applied entirely from outside the modifier system, by the harassment
behaviour module through `TryAddSupportModifierByName`.

#### The module order authored in each config, and why (`.conf` files cannot carry comments)

**`Deployment_ObjectiveHarassment.conf`** — insertion (spawning) → **town harassment (behaviour)** →
reinforcement (behaviour, **last**) → objective condition.
**`Deployment_ObjectiveTowerRecapture.conf`** — insertion (spawning) → **tower recapture (behaviour)** →
reinforcement (behaviour, **last**) → radio tower control condition (`m_bRequireControl 0`) → objective
condition.

Update order IS authored order. The mission behaviour must run **before** the reinforcement module for
two separate reasons, and both configs need both:
1. **A completed mission must not be rebought on its way out.** The reinforcement module is what deletes
   a deployment whose conditions failed; asked first, it would decide the deployment was fine and buy the
   force back in the same pass that ended the mission.
2. **For the recapture config it is the closing loop.** The flip is what makes
   `OVT_RadioTowerControlConditionDeploymentModule` (authored with `m_bRequireControl 0`) start answering
   false, and the reinforcement module's `m_bDeleteOnConditionFail 1` is what then collects the
   deployment. Ordered after it, the team would sit at a tower it already owned until the objective ended.

An Init case asserts the ordering in both configs, because the constraint is recorded only in class
headers and enforced by nothing else.

#### 🔴 THE PHASE-1 GATE: THE REGRESSION THIS PHASE CAUSED AND HOW IT IS NOW SHAPED

**The All gate came back red at 2/330 on the first cut of this phase. Both failures were earlier
phases' contracts, both were one mistake, and neither was visible from the code that caused it.**

| Failing case | Assertion | What was really happening |
|---|---|---|
| `Init_ObjectiveDirector_FreezesEveryTimerWhileABattleIsLive` | *"an unfrozen tick must serve exactly one round off the phase timeout: planted 50, read back 240"* | The gate fired on the driven tick, `EnterPhase(FOB)` **legitimately** re-armed the phase timeout, and 240 is a fresh harassment timeout. **The re-arm was never the bug — firing on that tick at all was.** (D4) |
| `PersistenceRoundTrip_ObjectiveDirector_SurvivesSaveAndReapply` | *"the restored objective is in phase 2, not the harassment phase it was saved in"* | The fixture's three `OnHarassmentSuccess()` calls promoted the objective to FOB **before the save**, so FOB is what got saved. Nothing about restore was involved. (G6) |

**THE MISTAKE, once:** `TickHarassment()` gained a Phase-2 gate check and `OnHarassmentSuccess()` gained
one too (T5.8 asked for it), and the gate was §3.2's diagram taken literally — `support < 50 %` and
nothing else.

**Fix 1 — `OnHarassmentSuccess()` COUNTS, IT DOES NOT DECIDE.** T5.8's "increments the counter and
re-checks the Phase 2 gate" is **not implemented and should not be.** A counter increment is not a tick.
The method is public and is called from a deployment update, from a restore and from fixtures arranging
a known state; transitioning from it means all three silently advance the ramp, and a phase entry
re-arms the phase timeout, so the same call also overwrites planted countdowns. Every transition now
happens on `DirectorTick()`, behind its three early returns. Cost: the phase advances up to one in-game
minute later, on a ramp whose cadence is measured in tens of them.

**Fix 2 — the gate needs THE TOWN TO BE CARRYING THIS RAMP'S OWN DEBUFF.** Second conjunct, in the
director (not in `OVT_ObjectivePhaseRules.TownPhase2Gate`, whose signature is pinned by Phase 2 Logic
cases — a Phase 5 mistake does not get to change a Phase 2 contract). Without it the gate fires on the
**first tick of the phase** for any town already under the threshold and the whole harassment phase is
skipped, which breaks **G3** outright: "tens of in-game minutes of visible activity" cannot be true of a
ramp that can advance before it has sent anything. A collapsed town is already rewarded by SELECTION,
which scores low support heavily (§3.4) — the prize is being *chosen*, not skipping the phase.

⚠ **THE SUCCESS COUNTER IS NOT A SUFFICIENT GUARD, AND THAT IS THE NON-OBVIOUS PART.** The natural fix —
"the gate also needs `harassmentSuccesses >= 1`" — repairs the Init case and **NOT** the Persistence one,
because that fixture bumps the counter to three itself. The counter records that operations were
*reported*; the modifier on the town records that one actually *happened there*. Only the second is a
fact about the world, and nothing else in the campaign applies `ObjectiveHarassment`. It is also the
right answer across a restore, with no session-local state to rebuild: **town modifiers are persisted.**
Not a wedge risk either — an expired debuff makes the gate refuse, the ramp sends another operation
which re-applies it, and a ramp that cannot progress at all still hits the phase timeout and blacklists,
loudly.

**Fix 3, found while fixing the other two — EVERY SPEND IS NOW BEHIND THE CADENCE.** The first cut sent
tower-recapture teams *ahead* of the `nextOpTicks` check, on the reasoning that a tower is a discrete
job. That was an **unbounded per-tick spender**: an objective covered by three resistance-held towers
dropped three deployments in one in-game minute — the unpaced lurch this feature exists to replace — and
it made "the director spends once per cadence interval" depend on which kind of operation it happened to
be. `TickHarassment()` now sends **at most one operation per interval** through `SendNextOperation()`,
**tower recapture first** (more urgent, self-deduplicating, bounded), harassment otherwise.

⚠ **A FIXTURE THAT DRIVES `DirectorTick()` MUST PLANT A NON-ZERO OPERATION COUNTDOWN.**
`CommitObjective()` enters a phase, and a phase entry arms `nextOpTicks` to **zero** — so a tick taken
straight afterwards reaches the spend path and puts a real deployment, a real truck and real groups into
the shared world, permanently debiting the occupying faction's pool (**nothing refunds a deleted
deployment — `RecoverResources` no longer exists, only comments referring to it**). All three current
`DirectorTick()` callers plant one. Residual, accepted, not worth contorting production code for: the
persistence case's *dirty* phase plants only 4 ticks before requesting a reload, so a very slow reload
could in principle reach the spend path — its objective is a BASE, which refuses harassment outright, so
the only reachable spend is a recapture team at a resistance-held tower near that base, and the
initialisation worlds start with every tower occupying-held.

#### 🔴 WHERE THE POOL IS DEBITED — the single highest-value line in the feature (G5)

**`OVT_ObjectiveDirectorComponent.CreateObjectiveDeployment()`, and nowhere else.** Both operation
senders (`SendHarassmentOperation`, `SendTowerRecaptureOperations`) go through it; there is no second
spending path in the component.

```
config = deployments.FindConfigByName(configName)        no config  -> WARNING, spend nothing
cost   = config.GetTotalResourceCost()
if (pool < cost) return false                            short      -> retry next tick, spend nothing
created = deployments.ForceCreateDeployment(config, position, faction, cost, 0)
if (!created) return false                               refused    -> SPEND NOTHING
deployments.SubtractFactionResources(faction, cost)      <-- THE LINE
TrackObjectiveDeployment(configName, position)
```

⚠ **`ForceCreateDeployment` DOES NOT DEBIT.** It forwards to `CreateDeployment`, which only *stamps*
`m_iResourcesInvested` so a refund and the Game Master snapshot read a real number. The evaluation path
debits separately, right after its own create. **It was changed this phase to RETURN the component**
(it returned `void` and had zero callers), because "created, now debit" and "refused, debit nothing" are
otherwise the same call — and debiting a refusal burns the reserve on nothing, every tick, silently.

⚠ **THE COUNTDOWN IS ONLY RE-ARMED ON A SUCCESSFUL CREATE.** Every refusal — nothing to recapture, the
cap is full, the pool is short, a config is missing — leaves `nextOpTicks` at zero so the next tick asks
again a minute later. A cadence that punished a temporarily empty pool would stall the ramp for a full
interval over a handful of resources.

#### Two framework seams added, both additive, both because the alternative was worse

- **`OVT_BaseSpawningDeploymentModule.CollectRegisteredHandles(array<int>)`** (overridden in the infantry
  module). ⚠ **HANDLES, NOT ENTITIES.** `GetSpawnedEntities()` answers only about MATERIALISED groups, so
  a behaviour counting through it reads a perfectly alive dormant force as gone — the "0 agents = dead"
  mistake 1.8's spawn queue made fatal and which this framework has already removed from three other
  places. A handle can be asked `GetAliveMemberCount()` (survivor mask) and `GetPosition()` (record)
  whether or not anybody is standing near it.
- **`OVT_BaseBehaviorDeploymentModule`** gained `CountAliveRegisteredMembersWithin`,
  `NearestPlayerDistance` and the deferred-collection pair. ⚠ **THE ONE-FRAME DEFERRAL IS NOT PADDING.**
  `DestroyDeployment()` clears `m_aActiveModules` and then `delete GetOwner()`, while `UpdateDeployment()`
  is mid-`foreach` over a local list of WEAK references into exactly those objects.
  `OVT_ReinforcementBehaviorDeploymentModule` gets away with deleting inline **only because every config
  authors it last**, a rule stated in three headers and enforced by nothing. A mission behaviour is not
  last and cannot be, so it queues the delete with `CallLater(…, 0, false)` and cancels it in
  `OnCleanup()`.

#### Design decisions worth not re-deriving

- **The hold clock PAUSES on interruption, it never resets.** Both modules. A defender walking past once
  every few minutes would otherwise make the mechanic impossible to finish, which reads in play as "the
  occupying faction never does anything" and has no other diagnosis.
- **"Enemy present" is PLAYERS ONLY.** Asking it of every AI agent would cost a sphere query per behaviour
  module per deployment per ten seconds, and a resistance patrol walking past is not what the design means
  by contested.
- **A non-positive authored hold still takes ONE tick.** A misauthored zero would otherwise land the
  debuff, or flip a tower, on the update the men are registered — before they are out of the truck.
- **The recapture module's faction guard is the INVERSE of the capture module's** and does the same job:
  it refuses a tower the faction already holds, so a team that arrived after somebody else flipped it does
  not spend its latch and write a misleading log line.
- **`GetRadioTowersAffecting()` skips SABOTAGED towers**, so a tower off the air is not a recapture target.
  Correct rather than an oversight: it is not helping the resistance hold the objective, and it becomes a
  target again on its own when it recovers.
- **Tower recapture IS on the cadence, and takes priority within it.** One operation per interval, tower
  first. It was briefly off the cadence (a tower being a discrete job) and that made it an unbounded
  per-tick spender — see the regression section above. It still deduplicates against the live deployment
  within 300 m, and a second contested tower is picked up on the next interval; the phase has 240 in-game
  minutes against a 45-minute cadence, so there is room for five operations.
- **Concurrency is counted from the LIVE deployment list, never from `m_aCreatedDeployments`.** That list is
  a teardown ledger that only grows until a reset; an operation that completed, was wiped or was collected
  is still in it and must not hold a slot.
- **The harassment config authors NO patrol behaviour module**, deliberately. A patrol module would answer
  `BuildVirtualPlan()` and pre-empt the insertion module's cycling march onto the deployment position; a
  DEFEND one would park the group on the road 300 m short of the town where the truck dropped it. The Init
  case asserts the absence.
- **The harassment op is created at the TOWN CENTRE** (location type TOWN); the recapture op at the
  **tower** (location type RADIO_TOWER). The insertion module then drives each force from the nearest
  controlled base to a landing zone 300 m short.

#### ⚠ THE THREE NEW MODULES LIVE IN `Objectives/Modules/`, NOT IN `Deployments/Modules/`

The plan's §3.1 lists them under `Scripts/Game/GameMode/Deployments/Modules/`. They are in
`Scripts/Game/GameMode/Objectives/Modules/` instead, and the reason is Phase 4's own recorded invariant:
`grep -rn "OVT_ObjectiveDirector" Scripts/Game/GameMode/Deployments/` is **empty, comments included**, and
all three of these modules necessarily name the director. Keeping the grep true keeps "the deployment
FRAMEWORK does not know the director exists" checkable by one command instead of by reading. A module's
directory has no bearing on how a `.conf` authors it, so nothing else changed.

`grep -rn "OVT_ObjectiveDirector" Scripts/Game/GameMode/Deployments/` is still empty after this phase.

#### Tests added, and the can-fail proofs

**Init (new `OVT_TEST_Init_ObjectiveOperations.c`, 4 cases)** — every ladder rung and the recapture config
registered, valid, location-typed, ordered behaviour-before-reinforcement, authoring a source provider, no
patrol module, a *distinct* group per rung and every group resolving to a loadable prefab **for both
factions**; the recapture config's `m_bRequireControl 0` inversion; both hold decisions driven through
empty / contested / held / interrupted / expired rows with the pause-not-reset claim asserted on the tick
count and the fire-once latch asserted after; clone fidelity over all three new modules with **both latches
fired first** so "a clone must not inherit a fired latch" cannot pass vacuously; and the modifier resolving
by name **at the last index**, stackable, negative and titled.

Plus `…_GateNeedsTheRampsOwnDebuff`, the **regression guard** for the red-gate defect above: counting
three successes moves neither the phase nor the phase timeout, and a driven tick refuses the gate for a
town carrying none of this ramp's debuff while still serving exactly one round off the timeout.

**22 faults injected one at a time and compiled** (5 config/registry, 6 on the two decisions, 4 on the
clones, 4 on the modifier entry, 3 on the regression guards — re-adding the gate call to
`OnHarassmentSuccess`, deleting the debuff conjunct, and putting tower recapture back ahead of the
cadence). **Every one exited `compile-check.sh` 0** — which is the point: a
misspelled config name, a rung that quietly reuses the previous rung's group, a modifier moved one place up
the config, a dropped clone line and a guard deleted from a decision are none of them script errors. All
subjects restored and re-compiled clean. **Suites were not run — that is the orchestrator's job**
(`.claude/test-policy.md`).

#### Play-test items this phase adds (nothing here is covered by the spine)

- **Does the ramp actually run?** With a town objective selected, watch for: a truck leaving a held base
  about a minute in, a group at the town centre, `Sent 'Objective Harassment (Patrol)' …` in the log, the
  support modifier appearing in the town panel a few minutes later, and the NEXT operation arriving as a
  bigger group. Four successes should take the town below 50 % and log "raising a forward operating base"
  (which then does nothing until Phase 7).
- **Does the pool balance?** Note `m_mFactionResources` for the occupying faction, send several
  operations, and confirm the reserve fell by exactly the sum of the configs' `GetTotalResourceCost()`.
  A double debit or a missing one is invisible in the log.
- **Does a recapture team take a tower back?** Flip a tower to the resistance near an objective, then
  leave the area. Ten in-game minutes later it should flip back, with the usual notification — and the
  team's deployment should be collected within a minute of the flip. **Standing at the tower must stop
  the clock without resetting it.**
- **Does the concurrency cap hold?** `objectiveHarassmentMaxConcurrent` operations alive at the objective
  and no more, however long the phase runs.
- **Does a completed operation free its slot?** Watch for the deferred collection: the deployment marker
  should disappear one frame after the hold completes, not linger.
- **US-occupier campaign specifically.** Both prerequisite registry fixes are US-only, so rung 2 and the
  recapture team have never been fielded before on that side.
- **MP/JIP.** Entirely uncovered by the spine, as for the rest of this feature.

### 2026-08-19 — Phase 4 built (the insertion module)

#### What Phase 4 actually changed

**New** — `Scripts/Game/GameMode/Deployments/OVT_InsertionGeometry.c` (pure statics),
`.../Modules/OVT_DeploymentSourceProvider.c` (the seam + `OVT_NearestControlledBaseSourceProvider`),
`.../Modules/OVT_InsertionSpawningDeploymentModule.c` (+ the `OVT_EInsertionState` enum).
**Edited** — `OVT_DeploymentManager.c` (the convoy cap), `OVT_InfantrySpawningDeploymentModule.c`
(**one** new virtual seam, `ResolveRegistrationSpawnDistance()`, returning the literal that used to be
inline — behaviour-identical for every existing subclass), `Configs/Factions/{USSR,US}_OverthrowData.conf`
(two appended group entries each).
**Tests** — new `TestSuites/Logic/OVT_TEST_Logic_ObjectiveInsertion.c` (3 cases) and new
`TestSuites/Init/OVT_TEST_Init_ObjectiveInsertion.c` (4 cases).

#### The lifecycle as built

```
EnsureGroups()          decide  →  transport  →  force        ← the order is load-bearing
  1 DecideInsertion()   resolve the source through m_Source; no source → WARN ONCE and register
                        NOTHING (retried every convergence). Restored-from-save → WALK.
                        Below m_fWalkThresholdDistance → WALK. TryReserveInsertion refused →
                        WALK, or stay undecided if m_bWalkWhenInsertionRefused is false.
                        Otherwise: reserve, compute the LZ, enter DRIVING.
  2 EnsureConvoy()      truck at the source (road-snapped) → crew registered under its OWN owner
                        key at 100 000 m with a NULL plan → one MOVE waypoint to the LZ.
                        No prefab / no crew → WALK.
  3 super.EnsureGroups() the force, registered at the source, beside the truck while DRIVING, at
                        100 000 m while DRIVING, with the behaviour plan or a fallback march.
  4 SeatEveryone()      crew PILOT→TURRET→CARGO, passengers CARGO only.

OnUpdate → TickDrive    truck dead → WALK. crew dead → WALK. HasArrived → CompleteInsertion.
                        IsStuck → WALK. Otherwise re-seat anyone who fell out.
CompleteInsertion()     teleport the force out, drop it to the global ring, RELEASE the slot, call
                        OnInsertionArrived(lz), issue a MOVE home, enter RETURNING.
OnUpdate → TickReturn   home, destroyed, or RETURN_TIMEOUT_TICKS (60) → ReleaseConvoy, FINISHED.
```

⚠ **The five roads to walking, and they all end in the same place:** below the threshold · refused
reservation · missing/failed vehicle prefab · crew refused · stuck or destroyed truck. In every one of
them the force is registered, alive, and already holding a plan that points at the objective — which is
why the plan is decided at REGISTRATION and never re-issued at dismount. `DismountAndWalk()` opens the
doors and nothing else.

#### THE RELEASE-PATH AUDIT (T4.3/T4.10) — every exit that must hand the convoy slot back

`ReleaseReservation()` is idempotent (guarded by `m_bReserved`) and the manager floors its own counter
at zero, so an extra release is free and a missed one is permanent. Every path below was walked:

| # | Exit | Reaches release via | Notes |
|---|---|---|---|
| 1 | Never reserved — below the walk threshold | n/a | `m_bReserved` false; `EnterWalking` only logs |
| 2 | Never reserved — reservation refused | n/a | `TryReserveInsertion` returned false |
| 3 | Never reserved — no source, no deployment manager | n/a | decided before the claim |
| 4 | Vehicle prefab missing / truck failed to spawn | `FallBackToWalking` → `ReleaseConvoy` | ⚠ reserved BEFORE the truck is attempted |
| 5 | Crew type unauthored / registration refused | `FallBackToWalking` → `ReleaseConvoy` | |
| 6 | Truck destroyed mid-drive | `TickDrive` → `DismountAndWalk` → `ReleaseConvoy` | |
| 7 | Crew wiped mid-drive (polled) | `TickDrive` → `DismountAndWalk` → `ReleaseConvoy` | mask-first, never an agent count |
| 8 | Crew wiped mid-drive (event) | `OnVirtualGroupWiped` → `DismountAndWalk` → `ReleaseConvoy` | intercepted BEFORE super, so it never marks the module eliminated |
| 9 | Truck stuck | `TickDrive` → `DismountAndWalk` → `ReleaseConvoy` | |
| 10 | **Arrived** | `CompleteInsertion` → `ReleaseReservation` directly | the slot is about trucks going TOWARDS an objective; the empty one going home does not hold the next up |
| 11 | Force wiped before the convoy set off | `EnsureConvoy` → `ReleaseConvoy` | not a walk: there is nobody to walk |
| 12 | Truck destroyed on the way home | `TickReturn` → `ReleaseConvoy` | already released at 10; idempotent |
| 13 | Truck home | `TickReturn` → `ReleaseConvoy` | ditto |
| 14 | Return timeout (60 ticks) | `TickReturn` → `ReleaseConvoy` | ditto |
| 15 | Crew killed on the way home | `OnVirtualGroupWiped` → `ReleaseConvoy` | ditto |
| 16 | **Deployment torn down** | `OnCleanup` → `ReleaseConvoy` **before** `super.OnCleanup()` | the base class knows nothing about the crew, truck, waypoints or slot |
| 17 | Restore / faction-list teardown | `ApplyPersistedFactionResources` → `ResetInsertionReservations()` | the backstop, beside `ClearAllObjectiveAnchors()` |

`ReleaseConvoy` also, on every one of those paths: deletes this module's owned waypoints (⚠
`AIGroup.AddWaypoint()` does **not** take ownership), unsubscribes every `GetOnAgentAdded` pairing, and
`UnregisterGroup`s the crew — which respects held members, so a crewman still in the seat retires the
group in place rather than being deleted out from under the truck.

#### The one core-record write, and why there is no alternative

`RestoreGlobalSpawnRing()` writes `record.m_iSpawnDistanceOverride = -1` **and** re-stamps
`SetLifecyclePolicy` on the group. Both halves are needed and neither is optional:

- the passengers *must* be registered at 100 000 m to be seatable — a dormant group has nobody to seat,
  so a truck driving through empty country would arrive empty;
- they *must not* stay there once they are down, or one squad per insertion is permanently materialised;
- **core has no setter**, its own `ApplyLifecyclePolicy` is protected, and `api.md` is 🔒 frozen for this
  feature. Re-registering would throw away the survivor mask, which is the point of the registry.

Fixing only the live policy would be the subtler bug: correct this session, and permanently materialised
after the next load, because the *record* is what a save writes. `DESPAWN_HYSTERESIS` (1.15) is a local
const mirroring core's protected default; a server that dialled it elsewhere gets a slightly different
anti-thrash band on that one transient re-stamp.

⚠ **Consider asking core for `SetSpawnDistanceOverride(handle, distance)` if a second consumer ever
appears.** One narrow, documented write is cheaper than an API change; two would not be.

#### Registry entries added (T4.6) — GUID prefix `6B7A2E4C`, grep-verified unused before use

| Faction | Name | Prefab | Provenance |
|---|---|---|---|
| USSR | `specops_team` | `{1A5F0D93609DA5DA}Prefabs/Groups/OPFOR/Group_USSR_ManeuverGroup.et` | **Exactly** the pre-migration `m_aGroupSpecialPrefabSlots[0]`, recovered from `git show a62d7b6f^:Configs/Factions/USSR_OverthrowData.conf`. LAT + designated marksman, 2 men. |
| USSR | `truck_crew` | `{29DFCC25F263026B}Prefabs/Groups/OPFOR/Group_USSR_Transport.et` | Vanilla. 2 × `Campaign_USSR_Player_Driver` (a plain `SCR_ChimeraCharacter` off `Character_USSR_BaseLoadout` with an AKS74U — "Campaign" is a folder name, not a game-mode dependency). AI budget 2, `TRAIT_LOGISTICS`. |
| US | `specops_team` | `{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et` | **Exactly** the pre-migration `m_aGroupSpecialPrefabSlots[0]` — see the finding below. |
| US | `truck_crew` | `{727C134094032B1F}Prefabs/Groups/BLUFOR/Group_US_Transport.et` | Vanilla, the mirror of the USSR entry. 2 × `Campaign_US_Player_Driver`. |

⚠ **FINDING, for Phase 5/6 to decide: `specops_team` and `sniper_team` are the SAME PREFAB for US.**
History is unambiguous — the pre-migration US "special" slot named `Group_US_SniperTeam.et`, and the
migration also mapped that prefab to the new `sniper_team` entry — so the instruction "recover it from
git history rather than guessing" was followed literally and the duplication shipped. It is a tuning
wart rather than a defect (the entry resolves, the Init case passes, and the two names are simply
aliases), but a 2-man sniper/spotter pair is a poor tower-recapture and base-sabotage team, and a name
that silently aliases another is a trap for anyone tuning Phases 5–6. **Two vetted one-line
alternatives**, GUIDs resolved from the reference tree and not guessed:
`{F65B7BB712F46FEE}Prefabs/Groups/BLUFOR/Group_US_ReconTeam.et` (Scout + Scout/RTO — the closest analogue
of USSR's ManeuverGroup) or `{D0886786634E55AE}Prefabs/Groups/BLUFOR/Group_US_GreenBeret_Squad.et`
(with `{4D3BBEC1A955626A}Prefabs/Groups/OPFOR/Spetsnaz/Group_USSR_Spetsnaz_Squad.et` as the USSR mirror,
if a genuinely special-forces pair is wanted on both sides).

⚠ **Second finding, unrelated but adjacent: the US registry has no `rifle_squad`.** USSR ships eight
group entries, US seven. The Phase 5 harassment ladder is documented as
`light_patrol → light_fireteam → rifle_squad → heavy_infantry`, so on a US-occupier campaign that ladder
has a hole in it. Not fixed here — it is Phase 5's authored surface, not Phase 4's.

#### The module is general-purpose and has NO director dependency (D7)

`grep -rn "OVT_ObjectiveDirector" Scripts/Game/GameMode/Deployments/` is **empty**, including comments —
the module's prose says "whatever owns a faction's long-term intent" and never the class name. Everything
place-specific is behind `OVT_DeploymentSourceProvider`; the concurrency cap lives on the deployment
manager, not the director. **A future config can author this module with the shipped
`OVT_NearestControlledBaseSourceProvider` and nothing else** — a supply run, a reinforcement convoy, a
resistance-side insertion — and change no script at all. Phase 7's FOB raiser subclasses it and overrides
the one hook, `OnInsertionArrived(vector lz)` (empty here, called with the point the truck actually
stopped at, **not** the objective).

#### Design decisions worth not re-deriving

- **The crew is registered under a SEPARATE owner key** (`m_sModuleName + "crew"`). The base class
  rebuilds `m_aHandles` from `FindGroupsByOwner` on every convergence, so a crew sharing the key would be
  reclaimed as part of the force, counted towards the force size, and released by the wrong teardown.
  Its wipe is likewise intercepted **before** `super.OnVirtualGroupWiped`, or a dead driver would mark
  the whole module eliminated.
- **The crew is registered with a NULL plan**, never `ResolveVirtualPlan()`. A harassment plan handed to
  a truck crew drives the truck into the town centre and holds it there.
- **The force's plan falls back to a march** when no behaviour module has an opinion: a cycling
  `MOVE(objective) + WAIT(3600 s)`, the shape of the parked-recruit hold loop. Without it a group
  registered at the source with a null plan is a garrison — men standing at the base they set out from
  for the rest of the campaign, which is the exact outcome the walk fallback exists to prevent.
- **A convoy is never resumed across a load** (`WasRestoredFromSave()` → walk). Vehicles are not
  persisted; re-spawning a truck at the source for men who are three kilometres down a road is a second
  insertion, not a restore.
- **Stuck is measured from two origins one tick apart, not from the physics.** A truck spinning its
  wheels against a wall, lying on its roof, or being rocked by the AI all report velocity while covering
  no ground. Two positions cannot be fooled by any of them, it needs no engine call, and it is testable.
  A flipped truck therefore needs no separate test — it fails the progress test on its own.
- **`ResolveRegistrationSpawnDistance()` was added to the base class rather than duplicating
  `RegisterGroups()`.** Three lines, returns the literal that was inline, behaviour-identical for every
  existing subclass — and it keeps the record write one-directional (down only, at dismount).
- **`OVT_OverthrowConfigComponent.SpawnGetInWaypoint(vector pos)` at `:507` loads
  `m_pGetOutWaypointBPrefab`** — the plan's suspected defect, CONFIRMED by reading. Not fixed here (out
  of scope) and not needed: the crew drives on ordinary MOVE waypoints and boarding is done by
  `MoveInVehicle`, not by a waypoint.

#### Tests added, and the can-fail proofs

- **Logic (new `OVT_TEST_Logic_ObjectiveInsertion.c`, 3 cases)** — `ShouldWalk` on both sides of the
  threshold plus the "a zero threshold DISABLES the rule, it does not walk everything" claim; the landing
  zone's **containment** on the closed segment asserted directly (part-distances summing to the whole,
  which also catches a NaN), the over-long standoff, the zero-length line, and an off-axis diagonal so no
  row passes by luck on one axis; and arrival-beats-stuckness with the consecutive-not-cumulative stall
  counter and the zero-elapsed-time speed.
- **Init (new `OVT_TEST_Init_ObjectiveInsertion.c`, 4 cases)** — the convoy cap reserving, releasing,
  refusing at the ceiling, refusing at a cap of zero and refusing to go negative on an over-release, all
  on a **synthetic faction index no faction holds** and with the cap restored; clone fidelity over all
  ten own and all thirteen inherited attributes; both new registry names resolving to prefabs that
  actually `Resource.Load()` for **both** factions (with `light_patrol` spot-checked so a red reads as
  "this entry is wrong" and not "the registry is not loaded"); and the source provider answering the
  nearest controlled base, refusing with a zero vector for a faction that controls nothing and for an
  out-of-range search, with the base-class reference implementation checked too.
- **22 faults injected one at a time and compiled** (12 on `OVT_InsertionGeometry`, 4 on the manager's
  cap, 2 on the source provider, 3 on `CloneModule` including one inherited attribute, 1 config typo).
  **Every one exited `compile-check.sh` 0** — which is the point: a clamp on the wrong side of a bound, a
  release that stopped releasing, a dropped clone line and a misspelled registry name are none of them
  script errors. All subjects restored and re-compiled clean. **Suites were not run — that is the
  orchestrator's job** (`.claude/test-policy.md`).

#### T4.9 — fixture discipline, verdict per `RegisterGroup(` site

`grep -rn "RegisterGroup(" Scripts/Game/Tests/` → **19 lines: 18 call sites + 1 prose mention.** The two
properties that make a fixture safe are unchanged from the 2026-08-17 sweep: (a) it registers a null /
empty / DEFEND-only plan, so virtual movement has nothing to advance; or (b) it registers and
unregisters inside ONE frame.

| Site | Verdict |
|---|---|
| `Init:3420`, `:3442` (unknown composition) | ✅ both registrations are **refused** — nothing is booked |
| `Init:3626` (dormant group) | ✅ (a) + (b) |
| `Init:3783`, `:3785` (GetAllHandles) | ✅ (a) + (b) |
| `Init:4103`, `:4298`, `:4501` (virtual movement) | ✅ movement's own fixtures, torn down in-case |
| `Init:4645` (waypoint ownership) | ✅ real movable plan, but torn down in the same frame — (b) |
| `Init:4790` (deaths flip the mask) | ✅ (a) + (b) |
| `Init:4978` (mask-driven refill) | ✅ (a) + (b) |
| `Init:7820` (deployment reclaim cycle) | ✅ null plan **and** `SPAWN_DISTANCE_NEVER`, asserted to resolve to 0 m |
| `Init:8310` (tower capture garrison) | ✅ null plan + `SPAWN_DISTANCE_NEVER` |
| `Persistence:4520`, `:4619`, `:5022` | ✅ null plan |
| `Persistence:4767` | prose, not a call site |
| `Persistence:5250` (slot-accurate mask) | ✅ the one fixture with a real plan — **DEFEND**, deliberately stationary |
| `Persistence:6855` (`RegisterUnder`) | ✅ null plan + `SPAWN_DISTANCE_NEVER`, asserted to resolve to 0 m |

**Deployment-constructing fixtures — the hazard that actually matters for this module** (a first
`UpdateDeployment` tick would resolve an origin, claim a slot, put a real truck on a real road and
register groups at a 100 000 m ring with the autotest camera inside it):

| Fixture | Verdict |
|---|---|
| `Init` free-seeding case (`SeedFreeDeployments`) | ✅ `Teardown()` marks the deployment **and every spawning module** eliminated, then deletes, all in the creating frame |
| `Init` escalation-ladder case (`CreateDeployment` ×2) | ✅ `MakeInert()` called immediately after each create, and again in `Teardown()` |
| `Persistence` deployment round-trip fixtures (`CreateDeployment` ×2) | ✅ `OVT_TEST_DeploymentRoundTripFixture.MakeInert()` called on the line after the create, with a comment saying why |
| **Phase 4's own Init cases** | ✅ **construct no deployment at all** — every claim is made against a bare `new` module, a loaded faction config, or one integer on the manager. The file header states the rule for whoever needs one later. |

**No fixture anywhere in the tree constructs an `OVT_InsertionSpawningDeploymentModule` inside a
deployment**, and none should until a config authors one.

#### Play-test items this phase adds (nothing here is covered by the spine)

Phase 4 ships the module but **no config uses it**, so none of this is reachable until Phase 5. When it
is:

- **Does a truck actually drive?** Reforger road AI over kilometres is the single biggest risk in the
  feature (R7). Watch one insertion end to end: truck spawns at a held base, crew boards, force boards,
  it drives, it stops within 40 m of the LZ, the force gets out, the truck goes home.
- **Does the walk fallback work, on purpose?** Force the four failure paths: set
  `objectiveMaxConcurrentInsertions` to 0 (every force walks), block a truck with a placeable (stuck),
  destroy a truck mid-drive, and author a `m_sTruckVehicleType` that does not exist. All four must still
  put the men at the objective.
- **Does the convoy slot come back?** Run several insertions through several failure paths and confirm
  the faction is still driving afterwards. A leak presents as "the enemy stopped using trucks" hours
  later, with nothing in the log.
- **Frame cost with two live convoys.** Two trucks, two crews and up to six passenger groups all
  permanently materialised is more live AI than this campaign usually holds.
- **Save → quit → Continue mid-drive.** The convoy must NOT resume: expect the force on foot, no truck,
  and no permanently-materialised squad afterwards.
- **MP/JIP.** Entirely uncovered by the spine, as for the rest of this feature.

### 2026-08-19 — Phase 3 built (the objective anchor in the deployment evaluator)

#### T3.1 — the read-only survey, and the one finding that changed the design

**The count was right and the design was not.** `OVT_CandidatePosition.sortBy` has **exactly one
producer**: the constructor at `OVT_DeploymentManager.c:14` (`sortBy = threat;`), reached from exactly
one construction site in `EvaluateFactionDeployments`. Nothing anywhere in `Scripts/` assigns
`candidate.sortBy`. The only other `sortBy` symbol in the tree is `OVT_BaseData.sortBy` (an `int`,
`OVT_OccupyingFactionManager.c:41`) on an unrelated class. The candidate **score** likewise has one
writer — `finalThreatLevel`, base threat × jitter, immediately above the construction.

⚠ **But the field the constructor writes is DUAL-PURPOSE, and §3.5's illustrative snippet biases the
wrong half of it.** The plan writes:

```
candidatesWithThreat.Insert(new OVT_CandidatePosition(position, final))    // final = biased
```

The constructor copies its argument into **both** `sortBy` **and** `threatLevel`, and `threatLevel` is
then read twice more in the same loop:

| Read | Where it goes | What it decides |
|---|---|---|
| `FindBestDeploymentConfig(..., candidate.threatLevel, ...)` | → `OVT_DeploymentComponent.CheckDeploymentConditions()` → `if (config.m_iMinimumThreatLevel > 0 && threatLevel < config.m_iMinimumThreatLevel) return false;` | **ELIGIBILITY.** A hard gate. |
| `CreateDeployment(..., candidate.threatLevel)` | → `SetThreatLevel()` → persisted by `OVT_DeploymentComponentSerializer` | The saved record, asserted in four Persistence cases. |

And the gate is **live, not theoretical** — three shipped configs author `m_iMinimumThreatLevel` above
zero: `Deployment_BaseATSection.conf` (50), `Deployment_BaseHeavyPatrol.conf` (25),
`Deployment_VehiclePatrol_Heavy.conf` (1200). Following the snippet literally would have let an
objective-adjacent position **buy configs its real threat cannot afford**, and would have written an
inflated threat into the save — a direct violation of **D5's own binding invariant**, which the same
section states three paragraphs later.

**VERDICT: the invariant wins over the snippet.** The bias is applied to **`sortBy` only**, after
construction, through `ApplyObjectiveAnchorBias(candidate, anchor)`. `threatLevel` is never touched.
No second producer of the score exists, so §3.5's *design* is intact; only its example line changed.

#### T3.2/T3.3 — what landed in the evaluator, exactly

`Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`:

- **`OVT_DeploymentObjectiveAnchor : Managed`** (position, radius, weight), declared beside
  `OVT_CandidatePosition`.
- **`protected ref map<int, ref OVT_DeploymentObjectiveAnchor> m_mObjectiveAnchors`**, allocated in
  `OnPostInit` with the rest of the per-faction state.
- **`SetObjectiveAnchor` / `ClearObjectiveAnchor` / `ClearAllObjectiveAnchors` / `GetObjectiveAnchor`**.
  ⚠ `SetObjectiveAnchor` with a **non-positive radius or weight CLEARS rather than storing**, so an
  entry in the store is always a live bias and `if (anchor)` is the whole test at the call site.
- **Two lines in the candidate loop**, and only two: `OVT_DeploymentObjectiveAnchor objectiveAnchor =
  GetObjectiveAnchor(factionIndex);` **before** the loop (one map lookup per faction per pass, not per
  candidate), and `ApplyObjectiveAnchorBias(candidate, objectiveAnchor);` between the construction and
  the `Insert`.
- **The doc block with both invariants** sits on `ApplyObjectiveAnchorBias`, and a shorter restatement
  sits on `OVT_CandidatePosition` itself, because the trap is the class's two fields rather than the
  method.

**How "no anchor is byte-identical" is proven, not asserted:** with no anchor the method returns on
`if (!anchor)` before touching the candidate. No arithmetic runs, nothing is recomputed, and the sort
reads the same float the constructor wrote — there is no floating-point difference to argue about.
`ApplyAnchorBias` itself also returns its argument by identity for a non-positive radius or weight, so
even a caller that reached it would be safe; the guard in the evaluator is the cheaper of the two.

#### T3.2 — the teardown, and what it does NOT cover

`ClearAllObjectiveAnchors()` is called from **`ApplyPersistedFactionResources()`**, which is the only
place in the file that empties and rebuilds the whole per-faction picture — i.e. the faction-list
teardown. Load order is safe: game-mode component serializers run first (the director only sets
`m_bRestorePending`), the deployment manager's restore runs second and clears, and the director
re-pushes on its own first tick. At most one evaluation pass runs unbiased in between.

⚠ **Known limitation, pre-existing and deliberately not fixed here.**
`OVT_DeploymentManagerComponent.s_Instance` is a static that is only ever assigned when null, and the
component has no `OnDelete` to clear it — so a **second campaign in one client session** resolves the
*previous* manager through `OVT_Global.GetDeploymentManager()`. That is a tree-wide condition every
manager shares (the director is the exception; it nulls `s_Instance` in `OnDelete`), it predates this
feature, and fixing it means changing a shared component's lifecycle. The anchor store's own share of
the problem is covered: a new component allocates an empty map, and the restore clears. Worth a
one-line fix in a bug pass, not in this phase.

#### T3.4 — what the director pushes, and the second drop path the plan missed

`Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`:

| Phase | Radius | Why |
|---|---|---|
| `HARASSMENT` | `HARASSMENT_ANCHOR_RADIUS` = **600 m** | Only the objective itself is worth leaning on; harassment is the one phase that may still re-select. |
| `FOB` | `FORWARD_ANCHOR_RADIUS` = **1 200 m** | The objective is locked and the forward base stands *between* the nearest held base and the target, so the band that needs garrisoning is the whole approach. |
| `COUNTER_QRF` | `FORWARD_ANCHOR_RADIUS` = **1 200 m** | The ground being fought over does not shrink when the battle starts. |
| `IDLE` / anything unhandled | **0** | Refused by `SetObjectiveAnchor`, so an unhandled phase fails safe to "no bias". |

The weight is an `[Attribute]`, `m_fObjectiveAnchorWeight`, **default 25** — chosen against the scale
`CalculateThreatLevel` actually produces (a resistance base within 1 km contributes 10, a tower or FOB
5), so the bias is worth "two or three nearby resistance targets" of apparent threat. It deliberately
**cannot** guarantee a win: the global threat term grows and the ±20 % jitter grows with it, so a
genuine hotspot still outsorts the objective late in a campaign. That is D5's bound working, not a
tuning failure.

**Push sites — two, both routed through one method (`PushObjectiveAnchor()`):**
1. `EnterPhase()`, after the phase is set. Committing to an objective enters `HARASSMENT` through
   here, so selection needs no push of its own.
2. `ResolveRestoredObjective()`. A restore writes the phase straight into the record rather than
   entering it, so `EnterPhase` never runs for a loaded campaign.

⚠ **THE DROP IS IN `ClearObjectiveRecord()`, NOT IN `ResetObjective()` — and the plan's marked home was
wrong.** `ResetObjective` is not the only way to end up with no objective: a re-selection that runs
during harassment and finds nothing selectable goes `SelectObjective()` → `EnterIdle()` →
`ClearObjectiveRecord()` and **never touches `ResetObjective`**. A drop placed where T3.4 and the
Phase 2 marker said would have left the occupying faction biased toward a place it had already given
up on, silently, for the rest of the campaign. `ClearObjectiveRecord()` is the single method all three
paths funnel through (`OnPostInit`, `EnterIdle`, `ResetObjective`), so the drop is still in exactly one
place — a stricter one. The marker comment in `ResetObjective` now says so and points at it.

#### The dependency direction, verified rather than asserted

`grep -rn "OVT_ObjectiveDirector" Scripts/Game/GameMode/Deployments/` is **empty**, including comments —
the manager's prose says "whatever owns a faction's long-term intent", never the class name. The
manager references `OVT_ObjectiveSelection` (the pure static holding the arithmetic) and nothing else
from `Objectives/`. `git diff Configs/Deployment/` is empty; the thirteen shipped configs are
byte-identical; `Virtualization/` and `VirtualMovement/` are untouched, with no untracked additions.

#### Tests added, and the can-fail proofs

- **Logic (new `OVT_TEST_Logic_ObjectiveAnchorAndBearing.c`, 3 cases)** — the absent anchor as the
  **identity** (asserted with `==`, not an epsilon, because "sorts on the same float as before" is an
  exact claim); the bias bounded by `weight`, floored at the input, monotonic across the whole band and
  **linear** at the interior points; and the ordering claim as two candidates, with a row one unit
  either side of the bound. The file is named for Phase 8's bearing statics too, so they have a home.
  ⚠ **One boundary is deliberately NOT claimed and the file says so:** `>=` vs `>` on the range test is
  provably unobservable, because at exactly the radius the falloff is an exact zero and `x + 0.0 == x`.
  A case pretending to pin it would be asserting nothing.
- **Init (new `OVT_TEST_Init_ObjectiveAnchor.c`, 3 cases)** — every real faction index in the world
  reads back unbiased (the live form of "no anchor is today's behaviour"), plus a set/replace/refuse/
  clear round-trip **on a synthetic faction index no faction holds**, so the live deployment wave cannot
  see it; the bias writes `sortBy` and leaves `threatLevel` bit-identical, checked through the real
  `array.Sort(true)` on the real class; and the director's push at the occupying index, widening with
  the phase, dropped on **both** ways of reaching "no objective".
  ⚠ **The evaluator is not driven.** `EvaluateFactionDeployments` creates real deployments, debits a real
  pool and is gated on a live player count — driving it to assert one assignment would mutate the shared
  world. `ApplyObjectiveAnchorBias` was made **public** for the case instead, on the precedent
  `FindBestDeploymentConfig` set in the same file for the same reason.
- **Ten faults injected one at a time and compiled.** Seven on `ApplyAnchorBias` (drop the radius guard,
  drop the weight guard, never bias at all, invert the falloff, drop the negative clamp, square the
  falloff, double the term) and three on the new code (store under a fixed index, bias `threatLevel`
  instead of `sortBy`, remove the push from `EnterPhase`). **Every one exited `compile-check.sh` 0** —
  including the `threatLevel` one, which is the whole point: the mistake §3.5's snippet would have
  produced is not a script error. Subjects restored and re-compiled clean. **Suites were not run — that
  is the orchestrator's job** (`.claude/test-policy.md`).
  ⚠ **A useful null result worth not re-deriving:** dropping the radius guard is invisible to every
  obvious row, because the range test already refuses a non-positive radius for any non-negative
  distance. It takes a distance *more negative than the radius* to reach the division. The one row that
  catches it exists for that reason and is commented accordingly.

#### Play-test items this phase adds (nothing here is covered by the spine)

- **Does the ramp visibly pull garrisoning toward the objective?** With an objective in `HARASSMENT`,
  routine deployments within 600 m of it should be bought before equally-threatened places elsewhere —
  and a genuinely hotter place should still win. The weight (25) is the knob.
- **Two campaigns in one client session.** Start a campaign, let an objective be selected, quit to menu,
  start a second. The second must not inherit the first's bias — see the `s_Instance` limitation above,
  which is the one way it could.
- **Save → quit → Continue mid-objective.** The bias must be gone during the load and back within one
  in-game minute of the first tick.

### 2026-08-19 — Phase 2 built (the director: state machine, selection, persistence)

#### What Phase 2 actually changed

**New** — `Scripts/Game/GameMode/Objectives/OVT_ObjectiveSelection.c` (pure statics: scoring, pick,
blacklist, `PositionKey`, `ApplyAnchorBias`), `.../OVT_ObjectiveRecords.c` (two enums + four records),
`.../OVT_ObjectiveDirectorComponent.c` (the brain),
`Scripts/Game/Persistence/Serializers/Components/OVT_ObjectiveDirectorSerializer.c`.
**Extended** — `OVT_ObjectivePhaseRules.c` (six predicates added to Phase 1's two).
**Edited** — `OVT_OccupyingFactionManager.c` (+2 pure reads), `OVT_TownManagerComponent.c` (loop
re-pointed), `OVT_OverthrowGameMode.c` (member + Init + PostGameStart, **last in both chains**),
`OVT_Global.c` (`GetObjectiveDirector()`), `Prefabs/GameMode/OVT_OverthrowGameMode.et` (one component,
GUID `{6B5D1A2C00000010}`), `Configs/Systems/Persistence/Overthrow.conf` (one serializer entry, GUID
`{6B0E7A3042B1C58D}`).

#### T2.1 — the hard rule, and how it was kept

Neither pure-static file contains `OVT_Global`, `GetGame()`, `World` or `Entity` **anywhere, prose
included** — the words *world*, *entity* and *manager* were avoided outright in both, in any case,
because the reviewer grep does not distinguish code from comments and the acceptance grep is
case-sensitive on only two of the four tokens. The two scoring functions take **numbers**, never a
town or base record, so neither class name appears either.

⚠ **The three gates kept the plan's names** — `TownPhase2Gate`, `TownPhase3Gate`, `BasePhase3Gate` —
even though "Phase 2/3" there means the OBJECTIVE's phase and not the build phase. Later phases will
grep the plan for those identifiers, and a better name would have cost more than it bought.

#### T2.6 — the two OF-manager helpers, and the verdict on the duplicated loop

Both added as pure reads beside `GetBasesWithinDistance`:
- `array<OVT_BaseData> GetBasesControlledBy(int factionIndex)` — the enumerator the director needed
  and nothing had: `GetNearestOccupiedBase()` answers about ONE base and hard-codes the faction.
- `array<OVT_RadioTowerData> GetRadioTowersAffecting(vector position)` — range through
  `OVT_InfluenceRules.IsProximitySource` against `radioTowerRange`, and **a sabotaged tower is skipped
  outright** rather than returned for the caller to filter.

**VERDICT: the town manager's inline loop WAS re-pointed** (`OVT_TownManagerComponent.c`, the support
tick). The replacement is behaviour-identical: same range predicate, same `radioTowerRange`, same
disabled skip; only the ORDER of the two tests changed (disabled first), which cannot change the
answer. The loop keeps its own `hasEnemyTower`/`hasFriendlyTower` polarity split because that is the
town system's policy, not a proximity question. Cost: one small array allocated per town per 10-second
support tick, which is the same order as everything else in that loop.

#### T2.4/D3/D4 — the machine as built

- **Three early returns, in order:** not the server → nobody online (parity with the occupying faction
  manager's own tick) → **a battle is live**. Every timer is a tick COUNT and every countdown runs
  through `OVT_ObjectivePhaseRules.TickDown()`, called only from a phase handler — so the freeze is a
  property of the early return rather than a rule applied per timer.
- ⚠ **`AdvanceObjectiveTimers()` is called from the phase handlers, never from the tick body.** That is
  what puts every counter behind all three early returns at once. Moving it up would silently unfreeze
  the machine while a battle ran.
- **Both control-change handlers set `m_bReselectPending` and return** (D3). `Remove` then `Insert` on
  both invokers, guarded by `m_bHooked`, unsubscribed in `OnDelete()` **through cached component
  handles** — resolving a manager again during teardown can hand back a static instance whose entity
  has already gone.
- ⚠ **A re-selection that ran this tick suppresses the IDLE case's own selection.** Both paths decay
  the blacklist by one round, so running both in one tick would let a place meant to sit out one round
  sit out none. `ConsumeReselectRequest()` returns whether it selected, for exactly that reason.
- **`CommitObjective()` clears the pending flag**: committing IS the answer to "re-evaluate the
  target".
- **The reset path is one method, `ResetObjective(reason, blacklist)`**, called by the harassment and
  forward-base timeouts, by the counter-attack's resolution, by the restore re-link failure and by the
  test fixtures. It logs the reason at WARNING every time. ⚠ **Phase 3's `ClearObjectiveAnchor()` goes
  in that method and nowhere else** — there is a comment saying so at the top of it.

#### 🔴 Between this phase and Phase 5, the machine ramps to nothing — on purpose

`TickHarassment()` and `TickFOB()` advance their clocks and check their phase timeout; they create no
deployment, because the modules and configs they would create are Phases 5–7. A play-tested campaign
will therefore log `Objective: town X at score …, ahead of …` about a minute in, sit in harassment for
`m_iPhaseTimeoutTicks` (240 in-game minutes), then log `Objective 'X' ended: harassment ran out of time
without reaching the forward-base gate` and pick again. **That loop is the expected state of the build,
not a defect** — it is also the cheapest possible proof that selection, the timers and the reset path
are all running.

#### T2.9 — the payload, and the load-order rule restated

Version-first, positional, append-only, **pure codec**: `Deserialize` reads and makes exactly one
side-effecting call. Field order is `version, kind, position, phase, phaseTicks, nextOpTicks,
harassmentSuccesses, sabotageSuccesses, blacklistPositions, blacklistRounds, fobUp, fobPosition,
fobSourceBasePosition, fobSpent, fobStarvationTicks, fobDeploymentName` — sixteen, exactly the plan's
list. An absent payload returns early (`if (version < 1) return true;`) and must never clear live state.

⚠ **`ApplyPersistedObjective()` touches no pool and no deployment**, because the deployment manager's
restore clears and refills the faction pool and runs AFTER the game-mode component serializers. Both
kinds of work are deferred to the first tick (`m_bRestorePending`), which is where the forward base is
re-linked by name + position and where a campaign restored mid-counter-attack rolls back. The re-link
gets `FOB_RELINK_ATTEMPTS` (3) ticks of grace because load order between this payload and the
deployment entities' own records is **never assumed**.

⚠ **The objective's display NAME is not persisted.** It is a label resolved from the town and base
registries, re-resolved on the first tick after a load; storing it would let a save disagree with the
world it is restored into. Unknown enum integers from a newer build read as NONE/IDLE rather than as a
phase with no handler.

#### Hardening found while building, worth keeping in mind

- **`OVT_TownManagerComponent.GetTownName()` dereferences the town's map marker without checking it is
  there.** Selection calls it once per candidate per in-game minute, forever, so the director resolves
  names through its own guarded helper (cache first, then `GetNearestTownMarker()`, then the name). A
  town with no marker within five metres would otherwise take the campaign down.
- **`OVT_OccupyingFactionManager.GetBase(entId)` has the same shape of hazard** (its own serializer
  documents avoiding it); the director resolves base controllers itself.
- **The director's records are allocated on clients too.** The getters are public, and a client asking
  a question must get "no objective" rather than a null dereference; behaviour is gated at the entry
  points instead.

#### Tests added, and the can-fail proofs

- **Logic (5 new cases in `OVT_TEST_Logic_ObjectiveScaling.c`)** — selection (highest wins, ties by
  input order, all-blacklisted, empty, ragged), the blacklist (decay floors at zero, an exhausted entry
  stops counting, key stability), every phase gate on both sides of every threshold with each conjunct
  refused independently, starvation on each of its three inputs alone, and the ceiling + `TickDown`.
- **Init (new file `OVT_TEST_Init_ObjectiveDirector.c`, 3 cases)** — the component resolves through
  both accessors as ONE object and starts idle; two selection passes over one unchanged world pick the
  same town (the fixture hands the whole map to the occupying faction and one town back, then restores
  every faction and size it touched); and the freeze, driven by planting a real battle controller on
  `m_CurrentQRF`.
  ⚠ **The tick is DRIVEN, not installed.** `DirectorTick()` is public precisely so a case can take one
  deterministic step; installing the repeating timer in a shared initialisation world would mutate
  campaign state under every case that follows. No polling and no `maxAttempts` anywhere in the file.
  ⚠ **Case names are chosen for alphabetical order** — the "starts idle" case must run before the two
  that drive the director (C < D < F), and both driving cases restore state anyway.
- **Persistence (1 case)** — a whole ramp mid-flight round-trips: kind, place, phase, both counters,
  both tick counters (asserted as the band `(dirty, saved]`, because the director legitimately ticks
  during the asynchronous save), the two-entry blacklist and every forward-base field. Written and read
  entirely through the director's public API; the reload seam was not widened.
- **19 faults injected one at a time and compiled** (M7–M21 on the two pure statics, three on the
  director/prefab, one on the persistence config). **Every one exited `compile-check.sh` 0** — which is
  the point: a gate on the wrong side of its threshold, a missing freeze guard, a component nobody
  declares and a serializer nobody registers are none of them script errors. All subjects restored and
  re-compiled clean. **Suites were not run — that is the orchestrator's job** (`.claude/test-policy.md`).

#### Still owed for this feature (not this phase)

MP/JIP behaviour of the director is uncovered by the whole spine; so is a real save → quit → Continue.
Both stay play-test items.

### 2026-08-19 — Phase 1 built (retirement + the difficulty rewire)

#### T1.1 — the read-only survey, verdicts

Run before any edit. **Every expectation held; nothing was found that required stopping.** Line numbers
had drifted from the plan (the file grew by ~36 lines after the sleep merge), so the real ones are
recorded here for anyone re-reading the plan's `file:line` references.

| Subject | Expected | Found | Verdict |
|---|---|---|---|
| `m_bCounterAttackTimeout` readers | 3 | **4** — declaration `:176`, decrement **two lines** `:1387`+`:1388` (the plan counted the pair as one site), roll condition `:1436`, assignment `:1445` | ✅ same three *sites*, all deleted |
| `counterAttackTimeout` (difficulty field) | 2 | **2** — attribute `OVT_DifficultySettings.c:51`, consumer `OVT_OccupyingFactionManager.c:1444` | ✅ as expected |
| `counterAttackTimeout` authored values | 4 | **4** — `Difficulty_Normal.conf:10`, `Hard.conf:15`, `Extreme.conf:15`, `Insane.conf:14` (Easy and TestWorld never authored it) | ✅ as expected |
| `StartBaseQRF` callers | 2 | **2** — `OVT_CampaignRequestComponent.c:177` (player capture), `OVT_OccupyingFactionManager.c:1443` (the roll) | ✅ no third caller |
| `StartTownQRF` callers | 2 | **2** — `OVT_UprisingRequestComponent.c:92` (uprising), `OVT_OccupyingFactionManager.c:1479` (suppression) | ✅ no third caller |

⚠ One **non-caller** mention survives and is correct to leave: `OVT_CivilianAmbienceManagerComponent.c:593`
names `StartTownQRF` in a comment explaining the `m_CurrentQRF` singleton guard. Any future
"exactly one caller each" grep will show it — it is prose, not a call site.

After the phase: `grep -rn "StartBaseQRF\|StartTownQRF" Scripts/` returns the two player-initiated
callers, the two definitions, and that one comment. **That is the target state.**

#### 🔴 The occupying faction has NO offensive trigger between this phase and Phase 8

Stated explicitly because it will look like a regression to anyone play-testing in the window. Both
legacy triggers are gone and the director that replaces them does not exist yet. The OF still gains
resources, garrisons, patrols, fortifies, reinforces and defends — it simply never attacks, and no
QRF will ever start except one a **player** starts (a base capture or a town uprising).

This is D2, the user's binding retire-first decision: v1.5 is unreleased and temporary OF passivity in
dev play-tests is accepted. Play-tests in this window evaluate defence, deployments and the economy —
**not pressure**, and a report of "the enemy never counter-attacked" is not a defect until Phase 8.

#### What Phase 1 actually changed

- `OVT_OccupyingFactionManager.c` — `m_bCounterAttackTimeout` and its decrement, the hourly roll (with
  the unconditional `RandFloat01()` that fed only it), and the town-suppression loop with its two dead
  locals. ⚠ **The quarter-hour block shell and the threat decay inside it survived** — the decay is the
  only thing left in that block, and its latch comment was re-worded because it used to define itself
  by contrast with the scan that is now gone. Two comments re-worded (T1.6); `UpdateKnownTargets`'
  comment stays. `RplSave`/`RplLoad` and `OVT_OccupyingFactionManagerSerializer.c` untouched.
- `OVT_DifficultySettings.c` — `counterAttackTimeout` out, **twelve** objective fields in.
  ⚠ **The plan says "eleven fields" but its own §3.6 table lists twelve rows and admits so** ("Twelve
  rows; `objectiveFOBBudgetCeiling` is deliberately not a field"). All twelve were authored; the count
  in the prose is the error, not the table. Every field is an `int` — the retired one was a `float`,
  and nothing here is fractional.
- Five shipped presets author all twelve. `Difficulty_TestWorld.conf` authors **none** and inherits
  every default, which is why the Init case excludes it by name rather than by index.
- `Scripts/Game/GameMode/Objectives/OVT_ObjectivePhaseRules.c` — **opened early**, holding only
  `RequiredSabotageMissions` and `HarassmentLadderIndex`. T1.7 requires a Logic case over "the
  difficulty-consumption statics that exist at this point" and none existed; Phase 2 (T2.1) adds the
  remaining predicates to this same class rather than creating it.
- ⚠ **The literal string `counterAttackTimeout` may not reappear anywhere**, including in a comment
  explaining that it was retired — the acceptance grep covers `Scripts/`, `Configs/` and `Prefabs/`
  and does not distinguish code from prose. The header on the new difficulty block says
  "counter-attack cooldown field" for exactly that reason.

#### Can-fail proofs recorded this phase

All six Logic-tier faults (M1–M6) were injected into `OVT_ObjectivePhaseRules.c` one at a time and
compiled: **every one exited `compile-check.sh` 0**, which is the point — a difficulty clamp on the
wrong side of a bound is not a syntax error. The Init-tier fault (Easy's
`objectiveSabotageMissionsRequired` set to 1) also compiled clean. Subjects restored and re-compiled
clean. Suites were **not** run — that is the orchestrator's job (`.claude/test-policy.md`).

### 2026-08-19 — feature started
- Plan amended earlier the same day with the counter-attack QRF mode (§3.9, Phase 9, D14–D17) and the 05:00–15:00 daylight window; `requirements.md` gained a matching Phase 3 addendum.
- `tasks.md` scaffolded from `implementation.md` §4: 106 tasks, 10 phases.
- Build begins at Phase 1.

---

*Update this file as phases complete — `/autorun-feature` writes a session note per phase.*

### 2026-08-19 — the regression gate, and a host-flake verdict worth keeping

The **All** group was run three times around Phases 1–2. The story matters because it will recur:

1. **After Phase 1** — 300/303, with three *timeouts*: `OVT_TEST_Init_Virtualization_AmbientRollCountOverrideIsCalled` (60 s), `..._AmbientSourceRegisters` (500 ms), `OVT_TEST_Init_VirtualMovement_TickAdvancesDormantGroup` (60 s). None of the three is in a domain Phase 1 touches, and all three match failures this project has already diagnosed: `virtualization/integration/context.md:1521` (host main-thread stall, profile dir under OneDrive) and `virtualization/movement/context.md:114` (the vanilla `Setup_Checkpoint` 500 ms I/O flake on the N: mount).
2. **Immediate re-run** — the whole client hung and was killed at the harness's default 300 s. **Exit 124 is "no verdict", not a failure and not a pass.**
3. **After Phase 2, with `--timeout 600`** — **All 313/313 green in 89 s.** The same three cases passed untouched, which retroactively clears Phase 1.

**The operational lesson:** on this host the suites are occasionally starved by I/O, and the failure presents as a *timeout with no assertion text*. Run with `tools/run-tests.sh --timeout 600 "{6A6E2A002F53A581}"` — the default 300 s budget is too tight to survive a stall, and a killed run costs a focus steal and tells you nothing. A timeout with no assertion text is not evidence of a regression; an assertion failure is.

### 2026-08-19 — play-test fix: QRF waypoints were not on the ground (PRE-EXISTING)

**Symptom (play-test):** "the waypoint destinations the QRF groups get are in the terrain and need to
snap to ground."

**The bad Y.** Every QRF waypoint target carried the *objective entity's own* height, never the
terrain's. `GetTargetZone()` rolls a random point in a 50 m radius around `GetOwner().GetOrigin()`
and writes only X and Z; `BuildSiegeRing()` does the same at 100–150 m, where the error is much
larger. On any objective that is not on flat ground the waypoint therefore lands buried in the
hillside or hanging in the air, and a completion radius smaller than that Y error can never be
reached.

**Pre-existing — yes, and it matters for the changelog.** The faulty `GetTargetZone()` predates this
feature (it feeds the standard Scout/Scout/SaD/SaD ladder every battle in the game runs through).
Phase 9's siege ring inherited the same convention rather than introducing it. Changelog this as a
**bug fix to QRF waypoints generally**, not as a counter-attack change.

**Where the clamp went, and why there.** In `OVT_QRFControllerComponent.CreateWaypoint()` — the sole
funnel every QRF waypoint entity is born through. `AddWaypoint()` is its only caller and
`ScheduleWaypoint()` defers into `AddWaypoint()`, so the single clamp covers the standard scheduled
ladder, the siege's `Defend` ring orders and `IssueAssaultOrders()`'s BATTLE-transition
SearchAndDestroy orders at once, and a future caller cannot forget it. Nothing else changed: no
waypoint types, no scheduling, no countdown, budgeting, scoring or resolution.

**The offset decision: none.** `targetPos[1] = world.GetSurfaceY(targetPos[0], targetPos[2])`, bare.
This matches the virtualization core's own waypoint clamp
(`OVT_PatrolBehaviorDeploymentModule.SnapPlanPointsToGround`, which adds nothing).
`OVT_SpawnPointComponent`'s `+ 0.5` exists because it is placing a *physical body* that must not
start intersecting terrain; a waypoint is a navigation target the AI resolves against the navmesh, so
the bare surface height is correct and an offset would only be an unexplained 0.5 m of drift.

**Verdict — the siege ring slots (Phase 9): left alone, correctly.** They do carry the objective's Y,
but a ring slot has exactly one consumer — the `"Defend"` waypoint in `SpawnFromQueue()` — and that
now goes through the clamp. Ring slots are **not** spawn positions (those come from
`GetLandingZone()`), so nothing is ever placed at that Y. The `BuildSiegeRing()` header was rewritten
to state this dependency explicitly: give a slot a second consumer and it must be snapped there too.

**Verdict — `GetLandingZone()`: left alone, correctly.** Its positions are used only as
`spawnParams.Transform[3]` for an `SCR_AIGroup` prefab in `SpawnFromQueue()`. Spawned group members
are navmesh-snapped by the engine, the play-test reported no misplaced *spawns* (only waypoints), and
its own TraceBox clearance check already runs at that height. Clamping it would be a behaviour change
to the one part of this path that is working.

**Tests: not possible at Logic tier, and no vacuous case was written.** The fix is one line whose
entire content is a `BaseWorld.GetSurfaceY()` call — it has no pure-function part to assert, and a
Logic case could only re-assert that `vector[1] = f` assigns, which would pass whether or not the
clamp exists. `CreateWaypoint()` is `protected`, spawns real waypoint entities via config, and needs
a loaded world. Verification is the manual procedure below.

**Manual test procedure.** On a hilly objective (not a flat coastal town): trigger a standard QRF,
`#OVT_Debug` the spawned groups, and confirm the groups walk to and *complete* their Scout waypoint
rather than stalling at the landing zone. Then trigger a counter-attack and confirm the siege ring
groups reach their `Defend` slots on the slope, and that on the BATTLE transition they converge
inward instead of standing still.

**Gate:** `tools/compile-check.sh` exit **0** (6180 files, Game module). Suites not run — orchestrator's.

### 2026-08-19 — play-test fix: the insertion truck now uses authored `OVT_VehiclePatrolSpawn` markers

**Request (play-test, verbatim):** *"one thing about the insertion spawn was it seemed to just pick the
nearest road point, but we have authored `OVT_VehiclePatrolSpawn` entities which should be used as
priority (with fallback to nearest road)"*.

**What changed: where the truck appears, and nothing else.** `SpawnTruck()`'s two inline road-snap
lines moved into `ResolveTruckSpawn()`, which now asks `ResolveAuthoredTruckSpawn()` first and only
then runs the *unchanged* road snap. The convoy lifecycle — the reservation, the release paths, the
walk fallback, the stuck detection — was not touched.

**No second world query, and that was the constraint that shaped the design.**
`OVT_BaseControllerComponent.FindSlots()` already sphere-queries `baseRange` at base init and
`FilterSlotEntities()` caches every marker it finds into `m_aVehiclePatrolSpawns`. The new path reads
*that cache*: source position → `OVT_BaseControllerComponent.FindNearestBaseControllerWithin(m_vSource,
250)` (the static that already exists for the perimeter and road-slot readers; 250 m is the framework's
`BASE_CLASSIFICATION_RADIUS`) → a new `CollectVehiclePatrolSpawns()` accessor. The accessor was needed
because the existing reader, `GetRandomVehiclePatrolSpawn()`, picks at **random** and can therefore
neither be made deterministic nor be asked "which of these is free". It resolves the cached ids
**forward** (so discovery order is preserved for tie-breaking) and prunes stale ones exactly as the
random reader does.

**Selection rule: nearest free marker to the source; ties to the lower index.** Deterministic on
purpose, and deliberately *not* the random reader's rule: a patrol spawns many vehicles and wants them
spread around, an insertion spawns **one** truck and wants the same answer every time, so a play-test
that saw the truck in a bad place can be repeated and a test can assert the choice. Ties are decided
by the base controller's own discovery order (strict `<` in the comparison), which is what a symmetric
authored pair gives you.

**Facing comes from the marker** (`GetAngles()`), not from the road and not zero — that is what the
Workbench arrow on `OVT_VehiclePatrolSpawn` is for, and a truck pointed into a wall is the reason
authored spawns exist at all.

**Occupancy: reused, not invented.** These markers are shared with the vehicle-patrol deployments, so
one can legitimately be occupied. `OVT_VehicleManagerComponent.IsSpotBlockedByVehicle()` (the BUG-129
test) was made **public** and given an optional radius; the insertion module asks it at
`MARKER_CLEARANCE_M = 6` (wider than the car-sized 3 m default, because it is placing a truck). A
blocked marker is skipped and the next nearest is tried.

**Four ways back to the road, all of them the pre-existing behaviour byte-for-byte:** no base within
250 m of the source, a base with no markers authored, every marker occupied, and — degrading the other
way — *no vehicle manager to ask*, which counts every marker as free rather than refusing them all.

**No new attribute, so no `CloneModule` risk.** An opt-out was considered and refused: preferring an
authored spot over a blind road snap is never the wrong answer, and one more authored field is one more
thing `CloneModule`'s hand-copy can silently drop. `CloneModule` is unchanged and still copies all ten
of this module's own fields.

**Tests.** The decidable part was extracted as a pure static —
`OVT_InsertionGeometry.ChooseSpawnMarker(positions, blocked, source)` — and asserted at the Logic tier
by `OVT_TEST_Logic_ObjectiveInsertion_ChoosesNearestFreeVehicleSpawn` (nine rows: nothing authored, one
free, one taken, nearest of three, nearest taken, all taken, the symmetric tie, no occupancy answers at
all, and a short `blocked` array). Three can-fail faults were injected into the subject one at a time
and each compiled clean (exit 0) before the subject was restored: drop the occupancy skip, invert the
distance comparison, and relax the tie-break `>=` to `>`. No `maxAttempts` — the subject has no clock,
no RNG and no world.

**Still manual:** that the truck actually appears *on* the authored marker and *facing the arrow* at a
real base, and that a second insertion from the same base while a patrol vehicle sits on marker 1 picks
marker 2 instead of the road. Both need a live world with authored markers.

**Gate:** `tools/compile-check.sh` exit **0** (6183 files, Game module). Suites not run — orchestrator's.

### 2026-08-19 — `/give-resources` distributes immediately (play-test amendment)

**Reported by the user, mid-play-test:** *"`/give-resources` works but isnt that useful for debugging. it gives them the resources but they arent distributed. it should trigger a distribution tick as if one had happened organically."*

They are right, and the original design note was wrong about what a tester needs. Crediting only `m_iResources` was defensible on accounting grounds — it kept `AllocateDeploymentResources` at its three sanctioned callers — but the **deployment pool is what every visible force spends**, so the command moved no needle until the next in-game minute, with nothing on screen explaining the wait. A debug tool you have to wait on is not a debug tool.

`OnGiveResourcesCommand` now calls `occupying.TransferDefenseShareToPool(amount)` straight after the credit.

**Why this is the organic path and not a shortcut past it:**
- `TransferDefenseShareToPool(int)` is the *same method the live tick and the sleep replay both call* — it is public precisely so the identity can be driven, and `GainAndSpendResources()` is its only other caller.
- It applies the authored defense share, clamps to the reserve (`if(toSpend > m_iResources)`), and moves the money through `AllocateDeploymentResources` — so the conserved-total identity holds exactly as on any other tick.
- ⚠ It therefore adds **no new caller** to `AllocateDeploymentResources`, which is the thing that must never grow a fourth one without a written reason. Q6's `Objectives/` clause is untouched; nothing under `Scripts/Game/GameMode/Objectives/` credits anything.

⚠ **The reported reserve total changed meaning.** It is now read *after* the transfer, so it is the remainder rather than the credited amount — a tester crediting 2000 sees roughly 400 left in reserve and ~1600 in the pool, which is correct and will look wrong to anyone expecting the old number. The `.st` `Comment` records this.

**Owed:** the localization re-export now also covers the amended `OVT-Msg-AdminResourcesAdded` text.
