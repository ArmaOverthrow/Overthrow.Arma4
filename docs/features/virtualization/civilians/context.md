# Virtualization Civilians - Context & Decisions

**Last Updated:** 2026-08-17
**Current Phase:** COMPLETE — all 6 phases built and gated 2026-08-17
**Status:** ✅ Ready for Review (play-test + Workbench passes owed, see "Needs human verification")

**Epic:** `virtualization` (feature #2 of 5). Consumes core's **frozen** `api.md` §4 ambient seam; adds exactly two additive hooks (`IsEntityDead`, `OnEntityPruned`) to `OVT_AmbientSpawnSourceConfig` — nothing else in core may change.

---

## Quick Status

**What's Done:**
- ✅ Planning complete (`implementation.md`, 6 phases; Phase 5 droppable)
- ✅ **Phase 1 built 2026-08-17** — two additive core hooks (`IsEntityDead`, `OnEntityPruned`) + their two call sites in `PruneAmbientEntities`, `OVT_CivilianAmbienceMath` (4 statics), two config-struct fields, 4 Logic cases + 2 Init cases, `api.md` §4 + `core/context.md` additive note. Gates: **compile-check 0** (6120 files, was 6118); core diff is the two methods + call sites only; `Rpc|RplProp|Replication.Bump` grep over `Scripts/Game/GameMode/Civilians/` empty; Logic-tier grep clean; `OVT_TownController.c` untouched; `CONFIG_STREAM_VERSION` still 3.

- ✅ **Phase 2 built 2026-08-17** — the migration to parity. Authored config classes + `Configs/Civilians/CivilianAmbience.conf`, the per-town source config with all seven overrides, `OVT_CivilianAmbienceManagerComponent`, `OVT_Global.GetCivilianAmbience()`, game-mode + prefab wiring, the town-controller rewrite (net **−56** lines), the three retired config attributes, the recruit release, 4 Init cases.

- ✅ **Phase 3 built 2026-08-17** — T3.1–T3.6, all six (T3.5 was NOT dropped). The `m_OnQRFTownChanged` invoker + one manager subscription, the F-C wait-time fix (prefab needed no edit), doorway/POI placement on a lazily-built per-town cache, WANDER + LOITER archetypes authored at 50/30/20, a bounded `m_ConvertedCivilians`, one Logic case and one extended Init case. **Scope amended mid-phase by the user: QRF suppression is now OPT-IN (D13).**

- ✅ **Phase 4 built 2026-08-17** — T4.1–T4.5. Five Overthrow civilian prefab **pairs** (businessman, dockworker, worker, townsfolk, villager), five per-type clothing `.conf` files under `Configs/Civilians/Loadouts/`, the `ApplyCivilianLoadout(character, config)` overload, the per-town `m_aCivilianTypes` attribute authored on **13 of Eden's 20 towns**, six authored civilian types in `CivilianAmbience.conf`, one Logic case and one Init case.

- ✅ **Phase 5 built 2026-08-17** — T5.1–T5.6. The placement spike (decision: **kerb-first with a road-derived fallback**), `OVT_TownVehicleAmbienceConfig` + `OVT_TownVehicleSourceConfig`, a `town_vehicles` source in the same `.conf` (5 civilian car prefabs), the spawn-time `UntrackTransient` (rule 1), the claim path (release → re-track → hand to the vehicle manager), the `TraceBox` obstruction check, and one Init case.

- ✅ **Phase 6 built 2026-08-17** — docs sync. Wiki `overthrow-config` page gained the three new keys (+ `virtualizationSpawnDistance`, previously undocumented); wiki `customizing-overthrow` gained the modder note (post-amendment shape: types are conf + loadout, custom prefab optional); tutorial/Field Manual audit found nothing stale; Field Manual "Welcome to Overthrow" entry gained town-crowd + parked-car paragraphs (3 new localization keys, GUIDs `{6B4E…}` series). Both wiki pages verified by re-fetch.
- ✅ **Amendment built 2026-08-17 (post-Phase-4, user):** the five per-type prefab pairs DELETED (20 files); all types spawn `Group_CIV.et`; type stamp verified explicit (`m_sPendingTypeName`, no reverse lookup); one Init case strengthened to pin every type to the shared prefab.

**What's Next:**
- 👀 User: Workbench + play-test passes (see "Needs human verification"), then commit (nothing is committed — the whole feature sits uncommitted in the working tree)

**Gate results:**
- Phase 6: docs-only — suites deliberately skipped per test policy (no EnforceScript change).
- Amendment (prefab drop): **compile-check 0** (6124 files); **Fast group 0 (179 tests, 58s)** run 2026-08-17. First attempt timed out at 300s (exit 124, NO VERDICT): the client froze mid-scenario-change while the Phase 6 docs agent was concurrently writing `FM_Overthrow.conf`/`localization_Overthrow.st` (files loaded at world start); both files verified byte-balanced vs HEAD before the single re-run. **Lesson: never run the suites while an agent is writing Configs/ or Language/.**
- Phase 5: **compile-check 0** (6124 files, 1 added). Grep gates: `Rpc|RplProp|Replication.Bump|RplSave|RplLoad|PlayerInRange|NearestPlayer|CallLater` over `Scripts/Game/GameMode/Civilians/` **empty**; `git diff Configs/Systems/Persistence/Overthrow.conf` **empty**; core's `Virtualization/` diff still exactly Phase 1's two hooks. **All group 0 (225 tests, 56s)** run 2026-08-17 by the orchestrator.
- Phase 4: **compile-check 0**; **All group 0 (224 tests, 55s)** run 2026-08-17 by the orchestrator.
- Phase 1: compile-check 0; **Fast group 0 (171 tests, 53s)** run 2026-08-17 by the orchestrator (user approved the focus steal mid-play-session).
- Phase 3: **compile-check 0** (6123 files, no new files); **All group 0 (222 tests, 60s)** run 2026-08-17 by the orchestrator. Grep gates all clean: every `AIWaypoint` creation site under `Scripts/Game/GameMode/Civilians/` funnels into `record.m_aWaypoints` and out through `DiscardRecord`; `Rpc|RplProp|Replication.Bump|RplSave|RplLoad|PlayerInRange|NearestPlayer|CallLater` over that directory **empty**; Logic-tier `OVT_Global|GetGameMode` grep **empty**; `OVT_OccupyingFactionManager` diff is **+19 lines, 0 deletions** = one invoker field + two invoke sites (I3).
- Phase 2: **compile-check 0** (6123 files, 3 added); **All group 0 (221 tests, 66s)** run 2026-08-17 by the orchestrator (All ⊃ Fast, one run covers both). Grep gates all clean (see the Phase 2 handoff below). F-A/F-B filed as **BUG-179 / BUG-180**.

**Blockers:**
- None

---

## Key Files

### Created (this feature)
- ~~Per-type prefab pairs (GUIDs `{6B4B2C5F00000020}`…`{…34}`)~~ — **DELETED 2026-08-17 by user amendment** (see "Per-type prefab pairs DROPPED"); all types spawn `Group_CIV.et`, looks come from the loadout confs
- `Configs/Civilians/Loadouts/CivilianClothes_{Businessman,Dockworker,Worker,Townsfolk,Villager}.conf` (+ `.meta`, GUIDs `{6B4B2C5F00000040}`…`{…44}`) ✅ Phase 4
- `Scripts/Game/GameMode/Civilians/OVT_CivilianAmbienceMath.c` — world-free density/filter/weight statics (Logic tier) ✅ Phase 1
- `Scripts/Game/GameMode/Civilians/OVT_CivilianAmbienceConfig.c` — `OVT_ECivilianArchetype`, `OVT_CivilianTypeConfig`, `OVT_CivilianArchetypeConfig`, `OVT_CivilianAmbienceConfig` (the authored template) ✅ Phase 2
- `Scripts/Game/GameMode/Civilians/OVT_TownCivilianSourceConfig.c` — `OVT_AmbientCivilianRecord` + the per-town runtime instance with all seven overrides ✅ Phase 2
- `Scripts/Game/GameMode/Civilians/OVT_CivilianAmbienceManagerComponent.c` — the manager ✅ Phase 2
- `Scripts/Game/GameMode/Civilians/OVT_TownVehicleSourceConfig.c` — `OVT_AmbientVehicleRecord`, the authored `OVT_TownVehicleAmbienceConfig` template and the per-town parked-vehicle source ✅ Phase 5
- `Configs/Civilians/CivilianAmbience.conf` (+ `.meta`, GUID `{6B4B2C5F00000001}`) — the authored ambient registry (modder seam) ✅ Phase 2
- `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_CivilianAmbience.c` — Fast Logic tier ✅ Phase 1

### Modified
- `Scripts/Game/GameMode/Virtualization/` — ONLY the two additive hooks + call sites (I1) ✅ Phase 1; **untouched by Phase 2**
- `Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c` — `civilianDensityMultiplier`, `maxCiviliansPerTown` (no RplSave/RplLoad change, stream version stays 3) ✅ Phase 1; three attributes retired ✅ Phase 2
  ⚠️ the plan says `Scripts/Game/GameMode/OVT_OverthrowConfigComponent.c`; the real path has `Managers/` in it
- `Scripts/Game/Controllers/OVT_TownController.c` — spawner retired, `OnDelete` added ✅ Phase 2 (**+34 / −90**)
- `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` — one release call (T2.9) ✅ Phase 2
- `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` + `Scripts/Game/Global/OVT_Global.c` — field, `EOnInit` block, `PostGameStart` call, accessor ✅ Phase 2
- `Prefabs/GameMode/OVT_OverthrowGameMode.et` — manager component added, `m_AmbientRegistry` wired, `m_pCivilianPrefab` removed ✅ Phase 2
- `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer` — its own `m_pCivilianPrefab` override removed ✅ Phase 2 (**not in the plan; the layer carried a second binding of the retired attribute and would have kept a dangling property**)
- `docs/features/virtualization/core/api.md` §4 + `core/context.md` — additive-change note ✅ Phase 1; §6 prose updated for the retired civilian spawn distance ✅ Phase 2
- `Scripts/Game/Utilities/OVT_LoadoutUtils.c` — `ApplyCivilianLoadout(IEntity, OVT_LoadoutConfig)` overload + an empty-choices guard ✅ Phase 4
- `Configs/Civilians/CivilianClothes.conf` — widened global pool ✅ Phase 4
- `Worlds/MP/OVT_Campaign_Eden_Layers/towns.layer` — `m_aCivilianTypes` on 13 towns (**+39 / −0**) ✅ Phase 4

## Planning defects to fix (not inherit)

- **F-A** Deleting a group entity does NOT delete its members (world-root spawns) — despawn must delete member characters explicitly; corpse-stays-behind is free. ✅ **fixed in Phase 2** (`OVT_TownCivilianSourceConfig.OnEntityDespawning` → `DeleteGroupMembers`).
- **F-B** `DisableCivilianWantedSystem(IEntity)` receives an `AIAgent` and finds no `OVT_PlayerWantedComponent` — the disable has been a no-op; rewrite taking `AIAgent` → `GetControlledEntity()`. ✅ **fixed in Phase 2** (`OVT_TownCivilianSourceConfig.OnCivilianAgentAdded`).
- **F-C** `SpawnWaitWaypoint(pos, time)` drops `time` (missing `SetHoldingTime`); the prefab's `m_TimedWaypointParameters` must also be verified or the setter silently no-ops. ✅ **fixed in Phase 3** (T3.2). **The prefab needed NO edit:** vanilla `Prefabs/AI/Waypoints/AIWaypoint_Wait.et` (GUID `{531EC45063C1F57B}`, the one `m_pWaitWaypointPrefab` is bound to on both the game-mode prefab and the Eden managers layer) already authors `m_TimedWaypointParameters SCR_AITimedWaypointParameters { m_holdingTime 60 }` — verified 2026-08-17 against the 1.8.0.10 reference tree. It is a **vanilla** file and is not in the Overthrow tree at all, so an edit would have meant forking it.
  ⚠ **GLOBAL BEHAVIOUR CHANGE, intended.** The two pre-existing callers stop being no-ops: `OVT_OverthrowConfigComponent.GivePatrolWaypoints` (PERIMETER patrols) asks for `RandFloatXY(45, 75)` and **now actually holds 45–75 s** at each patrol stop instead of the prefab's flat 60, and the deployment waypoint paths get whatever duration they were already passing. Patrolling AI will visibly pause differently from before this phase — that is the fix working, not a regression.

### ⚠️ FILE F-A AND F-B AS BUGS (orchestrator action, T2.11)

Both are **defects in the pre-migration code that shipped to players**, found while planning and fixed
by the migration rather than carried forward. They are worth a tracker entry so the history is not
lost and so anyone reading the old code in a released build knows what it did. **The implementing
agent deliberately did not file them** — that is the orchestrator's call. Everything a report needs:

- **F-A — a despawned town crowd left every civilian standing in the town.**
  `OVT_TownControllerComponent.DespawnCivilians()` (pre-2026-08-17) did
  `SCR_EntityHelper.DeleteEntityAndChildren(aigroup)` and nothing else. Members are spawned as
  **world-root** entities by `SCR_AIGroup.SpawnGroupMember` (`SCR_AIGroup.c:1658-1757` — no `AddChild`,
  no `spawnParams.Parent`), the destructor only destroys *edit-mode* scene instances (`:3391-3409`),
  and vanilla's own `DespawnMembers` deletes controlled entities **explicitly** (`:2897-2906`). So the
  group entity went and every civilian character stayed. Every "player left the 1000 m ring" event
  leaked a town's worth of characters for the session. Severity: performance/AI-count creep in long
  sessions. Fixed by the migration.
- **F-B — town civilians have been running with the wanted system LIVE.**
  `SCR_AIGroup.GetOnAgentAdded()` publishes an **`AIAgent`** (see
  `OVT_LoadoutUtils.RandomizeCivilianClothes(AIAgent)`, and `OVT_BaseUpgradeTowerGuard.c:190` using the
  same signature), but `OVT_TownController.DisableCivilianWantedSystem(IEntity)` called
  `FindComponent(OVT_PlayerWantedComponent)` **on the agent**, where that component does not exist —
  so it silently found nothing and returned. `m_bWantedSystemEnabled` defaults **true**. Severity:
  gameplay — civilians participated in the wanted system they were meant to be exempt from. Fixed by
  the migration, and called out as a deliberate behaviour change (D11).

---

## Important Decisions

### Per-type prefab pairs DROPPED (user amendment, 2026-08-17, post-Phase-4)
The five Character_CIV_<type>/Group_CIV_<type> pairs were deleted after the user observed they were byte-identical deltas whose inherited vanilla look is invisible (each type's loadout config overwrites top/pants/shoes/hat). Every type now spawns `Group_CIV.et`; a type's look comes entirely from its per-type loadout `.conf`. `OVT_CivilianTypeConfig.m_rGroupPrefab` REMAINS as the modder seam — a modder needing more than clothing points their type at a custom prefab. D1's component concern doesn't reappear: `Group_CIV.et` is Overthrow's own, full component set.

### Follow-up idea (user, 2026-08-17 — noted, not scoped): recruit prefab swap
Civilians carry recruit-management actions that only matter post-recruitment (pre-existing on `Character_CIV.et`, not introduced here). User's preferred long-term shape: on recruitment, swap the character for a dedicated "Recruit" prefab copying identity + loadout, so civilians carry only civilian actions. User deemed current behaviour harmless as-is; revisit only if it becomes a real problem (`/plan-feature recruit-prefab-swap`).

### Type-filter semantics re-confirmed by user (2026-08-17, mid-run via Discord)
User flagged that town civilian types must be curatable per-town (e.g. miners in a mining town), not size-derived alone. Confirmed the planned semantics are what they want: **min-town-size filter AND the town's `m_aCivilianTypes` allow-list combined** (T4.3 authors the lists on Eden towns; empty list = whatever size allows). No plan change — Phase 4 must make sure the per-town authoring actually lands on the Eden towns layer so curation is real, not theoretical.


See `implementation.md` §5 (D1–D12) — the authoritative record. Highlights for implementers:
- **D1** Variety = Overthrow-authored prefab pairs (catalog prefabs lack OVT components — cannot recruit/convert/sell).
- **D4** Two additive core hooks, NOT a kill-invoker subscription. Only one is built.
- **D5** Manager + per-town config instance; instance reads template by reference (no field copy).
- **D8** A killed civilian is not replaced until the next approach (cursor, not headcount).
- **D9** Nothing civilian-shaped is ever persisted.

---

## Gotchas & Learnings

- `RandInt` is max-exclusive (memory) — weighted pick rolls use total weight, not total−1.
- Logic-tier grep bans manager/game-mode identifiers **including comments**.
- Test cases need a recorded can-fail proof in a preamble comment; `maxAttempts` banned.
- Concurrent sessions move the tree — re-grep any `file:line` from the plan before editing (R10).

---

## Testing Approach

- Logic tier (Fast): density formula, clamps, multiplier=0, population=0, hard cap, weighted pick, type filter.
- Init tier (Fast): registry resolution, per-town instance, config-struct defaults, `IsEntityDead` virtual dispatch, safe no-op release.
- Persistence tier: **deliberately none** (D9/Q9).
- Play-test list: `implementation.md` §6 Verification Method (13 steps) — user-side.

## Needs human verification (running list)

- **Workbench (Phase 6): re-export localization** — three new keys (`OVT-FieldManual_Welcome_Head3`, `_Text4`, `_Text5`) render as raw keys in the Field Manual until the Workbench localization export is regenerated.
- **Play-test (amendment):** approach a CITY — crowd must show visibly distinct clothing (a suit among work clothes); a VILLAGE must show no suits. If every civilian looks identical, the type stamp is being lost between `RollPrefab` and `OnEntitySpawned` (all types share `Group_CIV.et` now — the loadout confs are the only source of visual difference).

- **Workbench (owed now, Phase 2):** open `Prefabs/GameMode/OVT_OverthrowGameMode.et` and confirm all three text edits round-trip —
  1. `OVT_CivilianAmbienceManagerComponent` `{6B4B2C5F00000003}` is present in the component list,
  2. `OVT_VirtualizationManagerComponent.m_AmbientRegistry` points at `Configs/Civilians/CivilianAmbience.conf` and the source list shows one `town_civilians` entry with one type and one archetype,
  3. `m_pCivilianPrefab` is gone from the config component's attribute list (it was also removed from `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer`, which carried its own override).
  Also open `Configs/Civilians/CivilianAmbience.conf` itself once — an authored `.conf` for a scripted
  subclass array is the one thing in this phase the compiler cannot check (risk R11); the Init case
  `OVT_TEST_Init_Civilians_AmbienceRegistryResolves` is the automated half of the same proof.
- **Workbench (owed now, Phase 5): open `Configs/Civilians/CivilianAmbience.conf` once more.** It now
  carries a SECOND source, `town_vehicles`, authored as an `OVT_TownVehicleAmbienceConfig`
  (`{6B4B2C5F00000060}`). Three things only a human can confirm, none of which the compiler sees:
  1. the registry shows **two** sources, and the second one's class is the vehicle subclass (a base-class
     entry resolves and then casts to null, and the manager is deliberately SILENT about a missing
     vehicle source, so this failure is invisible in the log);
  2. its `m_aPrefabs` list shows **five** vehicles that all resolve (S105 randomized, S1203 transport
     randomized, UAZ452 CIV randomized, UAZ469 uncovered blue, UAZ469 covered teal) — a bad GUID here is
     a car that never spawns;
  3. the six size-scaled count fields and the three placement fields are present and hold the authored
     values (0/1, 2/4, 4/8, 35 m, 120 m, 4.5 m).
  `OVT_TEST_Init_Civilians_VehicleSourceResolvesAndRolls` is the automated half of the same proof.
- **Workbench (owed now, Phase 4): open each new prefab + play-test F9.** Five character prefabs
  (`Prefabs/Characters/Factions/CIV/Character_CIV_{Businessman,Dockworker,Worker,Townsfolk,Villager}.et`),
  five group prefabs (`Prefabs/Groups/INDFOR/Group_CIV_*.et`) and the five per-type loadout files under
  `Configs/Civilians/Loadouts/`. Three things only a human can confirm:
  1. each character prefab opens clean and shows **all six** user actions (sell-drugs, recruit,
     convert-supporter, set-inactive, set-active, swap-loadout) plus `OVT_PlayerOwnerComponent` and
     `OVT_PlayerWantedComponent`, and inherits `RplComponent` + `SCR_CharacterFactionAffiliationComponent`
     from the vanilla chain (they are NOT re-declared - see the diff method in the Phase 4 handoff);
  2. each group prefab's `m_aUnitPrefabSlots` holds exactly its own character and nothing else;
  3. `Configs/Civilians/CivilianAmbience.conf` shows six civilian types, and each new type's `m_Loadout`
     shows its per-type slots - that field is authored as an **inline object inheriting from a `.conf`
     file** (the `m_CivilianLoadout` idiom from the game-mode prefab), which is the one piece of this
     phase the compiler cannot check. `OVT_TEST_Init_Civilians_TypeCurationBySize` is the automated half.
  Then F9 into a city and a village and confirm the crowds look different from each other.
- Play-test §6 steps 1–13 (spawn feel, leaks, interactions, QRF locality, density knobs). **Steps 1–8 are live as of Phase 2**; step 9 (QRF locality) is Phase 3, steps 11–12 (variety, real pauses) are Phase 3/4, step 13 is Phase 5.
  ⚠️ **Step 9's expected result is now PERMANENTLY different from the plan text** (D13): with the shipped
  default (`despawnCiviliansDuringQRF false`), town **B keeps its crowd and town A ALSO keeps its own** —
  civilians no longer vanish when a QRF starts, because players recruit them to fight it. To exercise the
  suppression path, set `despawnCiviliansDuringQRF true` in `$profile:Overthrow_Config.json` and restart:
  then A loses its crowd for the battle, B is untouched, and approaching A after the QRF ends produces a
  **fresh** crowd (re-rolled against the town's post-battle population, not the crowd that was there before).
  Step 12 (loiterers standing still) is live as of Phase 3, and worth a stopwatch — the pause is 90–240 s.
- **Play-test §6 step 13 is live as of Phase 5, and it is THREE separate checks** (the third is the one
  that catches the phase's real risk):
  1. **Parked-car placement quality — the spike's decider.** Walk the streets of a city, a town and a
     village. Expect cars standing *parallel* to kerbs and roads, on the verge or against the pavement,
     none in a driving lane, none inside a building, none in the sea, and roughly 4–8 in a city / 2–4 in
     a town / 0–1 in a village. **If they are in the lane, raise `m_fRoadLateralOffset` in
     `Configs/Civilians/CivilianAmbience.conf`; if they are in hedges, lower it.** No script change
     either way.
  2. **Take a car and keep it.** Drive an ambient car out of town, leave the area entirely (past the
     despawn ring), come back: it is where you left it. Then save → quit → **Continue**: it is still
     there. Both halves matter — the first proves the release, the second proves the re-track.
  3. **A save near an UNTOUCHED crowd of cars has no vehicle records for them.** Park yourself next to
     several ambient cars you have not entered, save, and confirm the save carries no records for them
     (they should simply be re-rolled on the next approach). This is the T5.3 assertion and the one that
     protects the save from doubling every session.

---

## Phase 1 → Phase 2 handoff (from the implementing agent)

**The core seam is live and is exactly what §3.4 specified.** Both hooks are plain virtual methods on
`OVT_AmbientSpawnSourceConfig`, both are called from `PruneAmbientEntities`, and the prune walk now
reads its config once at the top (`OVT_AmbientSpawnSourceConfig config = instance.m_Config;`).

- `IsEntityDead(IEntity entity)` — **not** `notnull`, deliberately: `OnEntitySpawned` /
  `OnEntityDespawning` are not either, and a `notnull` on a method every consumer overrides is a
  footgun. The base therefore carries a `if (!entity) return false;` guard the manager's old inline
  copy did not need. Core never passes null (the prune loop drops nulls one branch earlier), so this
  is unreachable from core and existing sources are unaffected.
- `OnEntityPruned` fires after **both** removals, and the loop clamps its own cursor
  (`if (i > instance.m_aEntities.Count()) i = instance.m_aEntities.Count();`) so a hook that shrinks
  the source's list mid-walk cannot read past the end. An entity displaced that way is re-examined on
  the next pass, not lost.
- The manager's `IsAmbientEntityDead` is **kept** as the null-config fallback, as T1.1 required —
  don't delete it in a later phase thinking it is dead code.
- **Nothing observes the hooks from inside core's tick yet.** The Init case asserts virtual dispatch
  through a base-typed reference (the manager's own reference type) and the stock default's
  semantics; it deliberately does not wait for an activation, exactly as the existing `RollCount`
  case does not. The live proof of the prune path is a Phase 2 play-test (§6 step 6).

**`OVT_CivilianAmbienceMath` ships four statics, not three.** `SumWeights(array<int>)` was added
alongside `PickWeightedIndex`, because a caller cannot roll in range without it and a caller that
rolls against anything else silently stops honouring the weights. `PickWeightedIndex` takes the roll
as an argument on purpose (deterministic, tier-A assertable); Phase 2's caller does
`PickWeightedIndex(weights, s_AIRandomGenerator.RandInt(0, SumWeights(weights)))` — `RandInt` is
max-exclusive, so the total is the right bound, not the total − 1.

**Decisions baked into the density formula, in order** (all asserted): population 0 beats `minCount`;
`rate * multiplier <= 0` beats `minCount`; the raw figure is **rounded**, not truncated (this is what
makes 38 → 4 instead of 3); the ceiling is applied after the floor, so a mis-authored `max < min`
resolves to `max`; the hard cap is applied **last**, so `maxCiviliansPerTown = 1` beats an authored
floor of 2. `hardCap <= 0` is uncapped — turning civilians off is `civilianDensityMultiplier`'s job,
and the two keys are separate on purpose.

**`TypeAllowed` takes plain ints, not `OVT_TownSize`,** to keep the file free of any type declared in
a manager file; the enum members convert implicitly at every call site. Name matching is **exact and
case-sensitive** (the same rule `OVT_AmbientSpawnSourceRegistry.FindByName` uses) and an **empty**
allow-list means "no restriction", never "nothing allowed" — every un-authored town carries one.

**Test-tier notes.** The Logic file avoids the literal retry-attribute identifier and the two banned
accessor identifiers entirely (they are described in prose instead), so the directory-wide greps stay
clean. Both suites are already in the Fast and All group configs, so the 6 new cases need no config
change — they are discovered from their `[Test(suite: ...)]` attributes.

---

## Phase 2 → Phase 3 handoff (from the implementing agent)

### The one engine finding that decides the whole lifetime model

**A vanilla `SCR_AIGroup` DELETES ITSELF ~1 ms after its last member dies.** `m_bDeleteWhenEmpty`
defaults to **1** (`SCR_AIGroup.c:96-97`, an authored attribute nothing in the `Group_CIV.et` →
`Group_FIA_Base.et` → `Group_Base.et` chain overrides), and `OnEmpty()` (`:2441-2455`) schedules
`SCR_EntityHelper.DeleteEntityAndChildren(this)` on a 1 ms deferred call unless the group is dormant.

Left alone that makes the whole prune design impossible: the group entity would be gone long before
the 2 s ambient tick, **core would see a plain `null`**, drop it one branch *before* it consults the
config, and `OnEntityPruned` would never run — so the dead civilian's five waypoint entities would
leak for the session, which is exactly what the retired spawner did. `OnEntitySpawned` therefore
calls **`group.SetDeleteWhenEmpty(false)`** and this feature owns the husk from then on. Anybody
touching the spawn hook must keep that line.

The mirror image is the **release** path: `ReleaseCivilian()` re-arms `SetDeleteWhenEmpty(true)`
rather than deleting the husk, because at release time the recruit's agent is **still in that group**
(the release deliberately runs before the reparent), and deleting a group out from under a live agent
is how a player's recruit gets orphaned. The engine then deletes the emptied husk one call later, for
free, and a transfer that somehow fails leaves the recruit with a harmless group of its own.

### Two more engine facts worth having before Phase 3

- **`SCR_EAIGroupLifecyclePolicy` defaults to `Manual`, not `ProximityDriven`** (`SCR_AIGroup.c:114`,
  no `[Attribute]`, so it cannot be authored on a prefab). An ambient civilian group therefore does
  **not** self-despawn at vanilla's 600/800 m rings — which is what makes the dead-check safe. If it
  were `ProximityDriven`, every walk past the 800 m band would empty a group, the predicate would read
  that as death, and the crowd would erode between the group ring (800 m) and the ambient despawn ring
  (~2012 m). It also means an ambient group costs no per-frame lifecycle tick.
- **An AI-budget eviction still reads as death** (D8/R2, accepted). That is now the *only* path into
  the predicate other than a real kill, because routine proximity dormancy is off the table per the
  point above. If it ever proves ugly in play, the one-line fix is a `GetOnMembersDespawning`
  subscription that suppresses the predicate for one pass — do **not** reach for `IsDormant()`, whose
  counts core deliberately overwrites for registered groups.

### Where the crowd's bookkeeping actually lives

`OVT_TownCivilianSourceConfig` keeps `map<EntityID, ref OVT_AmbientCivilianRecord>` and **every**
path that ends a civilian funnels through one method, `DiscardRecord()` — despawn, prune, release and
the orphan sweep. That is deliberate: Q4's "every waypoint creation site is paired with a deletion
site" is one function to audit rather than four.

**`SweepOrphanedRecords()` is an addition beyond the literal task text**, called from `RollCount()`.
Reason: core cannot hook the case where the group entity is destroyed by something else entirely (a
GM deleting it, a world edit) — the reference simply becomes null and the prune drops it before any
config hook runs, so nothing would ever delete that civilian's waypoints. `RollCount()` is the one
moment the source is *provably* owed nothing (a despawn closed every record, a prune closed every
record it saw, a release closed its own), so anything still in the map there is by definition an
orphan. It deletes the waypoints always and the surviving husk only when that husk has no agents.

**`m_sPendingTypeName` couples `RollPrefab()` to the `OnEntitySpawned()` that follows it.** That is
only safe because core's `SpawnAmbientEntity()` does roll → position → spawn → hook synchronously,
one entity at a time. If core ever batches, `RollPrefab` has to start returning the type instead.

### Two deliberate safety additions the task text did not ask for

- **`DeleteGroupMembers()` refuses to delete a character somebody OWNS.** `RecruitCivilian()` stamps
  `OVT_PlayerOwnerComponent` *before* it asks for the release, so an owned character still sitting in
  an ambient group can only mean the release did not take (an agent that failed to resolve for a
  frame, an absent manager). Without the guard that one-in-a-thousand case would delete a player's
  freshly bought recruit at the next town despawn. The guard leaves a group husk behind instead,
  which the orphan sweep collects. It cannot misfire on a real ambient civilian: every
  `SetPlayerOwner()` call site in the tree is a recruit, a vehicle or a resistance placeable.
- **The manager's `OnDelete` destroys NOTHING** — it drops its two maps and `s_Instance` and stops
  there, mirroring core's `ClearAmbientState()` decision verbatim ("deleting entities from inside a
  teardown is how a shutdown turns into an error cascade"). It cannot leak, because core's own
  `OnDelete` clears its ambient source map in the same teardown and that releases every instance
  registered from here. The plan's literal text said "unregister every source"; the destructive path
  is `DeactivateTown()` / `DeactivateAllTowns()`, which is what a *live* campaign uses — including
  Phase 3's QRF suppression.

### Things a Phase 3 author will trip over

- ~~**QRF suppression is GONE for this phase, and that is a known, temporary regression.**~~ **RESOLVED IN
  PHASE 3, but not the way this paragraph expected — read D13.** The map-wide despawn is gone for good; the
  town-local mechanism exists and is **off by default** on the user's instruction. The retired
  `CheckSpawnCivilian` suppressed *every* town's crowd whenever a QRF ran anywhere
  (`&& !OVT_Global.GetOccupyingFaction().m_CurrentQRF`). Nothing replaces it in Phase 2 — town
  civilians now stay put during a QRF, in **every** town including the one under attack. T3.1 is what
  makes suppression town-local; until it lands, §6 play-test step 9's expected result is "both towns
  keep their crowds", not "A loses its own". This was the plan's intent (suppression is Phase 3's
  task) and is recorded here so nobody reports it as a Phase 2 bug.
- **The archetype is already rolled** (`RollArchetype()`, weighted, from the template) and its wait
  bounds are already used. Phase 3's T3.4 only has to branch on `m_eArchetype` inside `BuildRoute()`;
  the enum, the config class, the weights and the `.conf` entry all exist. WANDER and LOITER currently
  fall through to the PINGPONG route.
- **`RollPosition()` is already the fallback path T3.3 needs.** It is today's exact placement
  (`GetRandomNonOceanPositionNear` → `FindNearestRoad`); `m_fBuildingPlacementChance` is authored at
  `0` and `m_aPlacementCache` is allocated and empty, so T3.3 is "fill the cache lazily and roll
  against the chance", not a rewrite.
- **`ResolveAllowedTypes(template, townSize, allowedNames)` on the manager already takes the
  allow-list.** Phase 2 passes `null` ("no restriction"). Phase 4's T4.3 only has to add the
  `m_aCivilianTypes` attribute to the town controller and pass it through `BuildTownSource`.
- **The town controller's `CheckUpdateFlag` repeating 10 s deferred call is still not removed** (D12),
  and the new `OnDelete` says so in a comment so the next reader does not think it was missed.

### Residuals recorded honestly

- **Waypoint entities are still persistence-TRACKED.** `Scripts/Game/Modded/SCR_AIGroup.c` untracks
  group entities, members and *prefab-authored* waypoints (`AddWaypointsDynamic`), but waypoints
  spawned through `OVT_OverthrowConfigComponent.SpawnWaypoint()` are not covered. This is **unchanged
  from the retired spawner** and identical to how core treats its own registered-group waypoints
  (tracked, `SelfSpawn 0`), and it is strictly better than before in one respect: these waypoints are
  now *deleted* with their civilian, and deleting a tracked entity drops its record with it. Only
  waypoints alive at save time write a record, and `SelfSpawn 0` means they are not resurrected. Left
  alone deliberately; noted in case Phase 5's save checks see waypoint records near a live crowd.
- **D7's spawn-ring consequence is live and unmeasured.** The source ships
  `m_iSpawnDistanceOverride -1`, so town crowds now materialise at `virtualizationSpawnDistance`
  (**1750 m**) where the retired path used 1000 m. More towns are populated at once than before.
  §6 step 2 of the play-test should **count the live AI** before Phase 3 adds variety — the escape
  hatches are `maxCiviliansPerTown`, `civilianDensityMultiplier` and putting a number in the `.conf`'s
  override field.
- **Civilians spawn in the autotest world again.** They were silent while the epic kill switch
  suppressed the legacy spawner; the migrated path is deliberately **not** behind that switch, so the
  test world's one town (population ~50 → ~5 civilians) now builds a small crowd during Campaign and
  Persistence runs. Nothing in the suites counts world AI or non-static entities (checked), but this
  is the most likely source of a surprise in the first post-Phase-2 run.

---

### D13 — QRF suppression is OPT-IN, and civilians stay by default *(user amendment, 2026-08-17, mid-Phase-3)*

The plan (G7/D6) said "QRF suppression becomes town-local". It is — the mechanism is built in full — but the
**default flipped** on the user's instruction, taken mid-phase:

> Civilians must stay spawned during a town QRF by default. The old always-despawn was a perf shortcut from
> when Reforger AI was expensive; players actively recruit civilians to fight the QRF and hate them vanishing.
> Heavy servers may still want the despawn, so it becomes a server-config option.

So the shipped behaviour is:

| `despawnCiviliansDuringQRF` | Town A (under QRF) | Town B (elsewhere) |
|---|---|---|
| **false (default)** | keeps its crowd | keeps its crowd |
| true | crowd despawned, re-rolled fresh after the battle | keeps its crowd |

Either way the **map-wide** despawn of the retired spawner is gone for good — that is the part of D6 that was
never in question. Base QRFs still do not publish a town id at all (D6 unchanged).

Consequences to carry forward:
- **Integration criterion I4 now reads "exactly THREE fields"** on `OVT_OverthrowConfigStruct`
  (`civilianDensityMultiplier`, `maxCiviliansPerTown`, `despawnCiviliansDuringQRF`). `RplSave`/`RplLoad` and
  `CONFIG_STREAM_VERSION` (3) are still untouched — the new field is server-only like the other two.
- **§6 play-test step 9's expected result changes permanently** (see the play-test note below).
- **Phase 6 (T6.1/T6.2) has a third knob to document**, and the "civilians are always around towns" tutorial
  wording is now *more* true than it was, not less.

---

## Phase 3 → Phase 4 handoff (from the implementing agent)

### The QRF wiring, exactly as built

`OVT_OccupyingFactionManager` gained **one field and two invoke sites, and nothing else** (I3 — the file's
diff is +19 / −0):

- `ref ScriptInvoker<int> m_OnQRFTownChanged`, declared beside `m_OnAIKilled` / `m_OnBaseControlChanged` /
  `m_OnPlayerLoot`;
- invoked with `m_iCurrentQRFTown` at the end of `StartTownQRF()`, after the field and both RPCs;
- invoked with `m_iCurrentQRFTown` (which is `-1` by then) at the end of `OnQRFFinishedTown()`.

`m_bQRFActive`, `m_iCurrentQRFTown`, `m_vQRFLocation` and every RPC are untouched — the invoker is a pure
addition on the server side of a method that was already server-only.

The civilians manager holds **one** subscription (inserted in `PostGameStart()`, removed in `OnDelete()`) and
**one** piece of state, `m_iSuppressedTownId`. Two things about it are load-bearing:

- **The end-of-battle restore does NOT consult the config flag.** It restores whatever *this manager actually
  suspended*. An operator can edit `Overthrow_Config.json` between a QRF starting and finishing; reading the
  flag at both ends would leave a town permanently empty (flag turned off mid-battle) or double-register one
  (flag turned on mid-battle).
- **Suspension is unregister/re-register, not a flag inside the roll path.** A source that is not registered
  cannot be ticked, so suppression can never be half-applied, and the despawn goes through the ordinary
  `OnEntityDespawning` hook — members, waypoints and husks — with no second teardown path to keep in step.
  `ActivateTown()` gained a guard for the in-between state: a town present in `m_mSources` but absent from
  `m_mHandles` is *suspended*, and re-activating it must not build a second source behind the suppression.

`OVT_TownCivilianSourceConfig` gained `SetTownLocation()`/`GetTownLocation()` so the resume can re-register at
the same place without re-resolving a town record.

### Placement: what the cache actually contains

`m_aPlacementCache` is filled by **one** `QueryEntitiesBySphere(origin, radius, null, CollectPlacementCandidate,
EQueryEntitiesFlags.STATIC)`, run **lazily on the first spawn that wants a candidate** — and only when
`m_fBuildingPlacementChance > 0`, so a server that authors 0 never pays for the query at all. Two kinds of
candidate, in preference order per entity:

1. **an authored `OVT_SpawnPointComponent` point** (`GetSpawnPoint()`) — hand-placed at doorways, the best
   civilian positions in the world and free to find;
2. **a building doorstep** — for `SCR_DestructibleBuildingEntity` only, derived from `GetWorldBounds()`: a
   point `max(halfX, halfZ) + 1.5 m` from the origin at an angle **rolled once and then frozen in the cache**.
   The origin itself is never used; it is the middle of the building, i.e. inside a wall.

Both paths run `OVT_WorldUtils.IsOceanAtPosition` at **build** time, so a cached candidate is dry by
construction and `RollPosition` needs no second check. The list is capped at `MAX_PLACEMENT_CANDIDATES` (256).

⚠ **`m_bPlacementCacheBuilt` is set BEFORE the query, not after.** A town with no authored spawn points and no
ownable buildings legitimately produces an empty list, and "empty" must never be read as "not built yet" — or
that town re-runs an 800 m sphere query for every civilian it ever spawns.

### Archetypes: three behaviours, one deletion contract

`BuildRoute()` branches on the rolled archetype and every builder inserts each waypoint into the record **at
the moment of creation**, before wiring anything up. That is the Q4 contract: a waypoint that never reaches a
record is a waypoint nothing will ever delete. `DiscardRecord()` is still the single deletion site.

- **PINGPONG** — unchanged parity route (far point, back to spawn).
- **WANDER** — `m_iPointCount` scattered points, **clamped to 2..6**. Not trusted: 0/1 gives a cycle a civilian
  completes instantly and stands in forever; 50 gives one civilian 100 waypoint entities.
- **LOITER** — a `SpawnMoveWaypoint` (not patrol — patrol makes an idle civilian shuffle) plus a long wait,
  cycled. The spot is **where the civilian already is**, snapped to a cached candidate within 25 m. Marching a
  loiterer across town to a "better" spot the instant it spawns is precisely what a loiterer must not do; and
  `RollPosition` has *already* decided (via `m_fBuildingPlacementChance`) whether this one belongs at a doorway.

Authored mix in `CivilianAmbience.conf`: `street_pingpong` 50 / `wanderer` 30 / `loiterer` 20, summing to 100 so
each weight reads as a percentage. Waits: ping-pong 15–50 s, wander 8–25 s, loiter **90–240 s** — the loiter
numbers only mean anything because of the F-C fix, and they are the most visible thing in this phase.

### T3.5 was done, not dropped

`OVT_TownManagerComponent.m_ConvertedCivilians` prunes ids that no longer resolve
(`RplComponent.Cast(Replication.FindItem(id))`) once the set reaches 512 entries — only on the insert that
crosses the threshold, so the normal path is still a single set lookup. Not persisted, not load-bearing, and
the sweep is conservative: an id that still resolves is always kept.

### Things a Phase 4 author will trip over

- **The `.conf` now has three archetype entries and a non-zero placement chance.** Phase 4 adds *types*, not
  archetypes — leave `m_aArchetypes` alone unless a new civilian type wants a behaviour that does not exist.
- **`m_iPointCount` on a PINGPONG or LOITER entry is ignored** (both build fixed-shape routes). It is authored
  anyway for readability; do not "fix" a PINGPONG entry's 2 into something else expecting an effect.
- **Per-type clothing (T4.2) is still unimplemented and `m_Loadout` is still unread.** `OnEntitySpawned` inserts
  the global `OVT_LoadoutUtils.RandomizeCivilianClothes` and nothing else; the record already carries
  `m_sTypeName`, which is the hook Phase 4 needs.
- **Everything in the Phase 2 handoff below still applies** — `SetDeleteWhenEmpty(false)` at spawn,
  `m_sPendingTypeName`'s coupling to a synchronous core, `SweepOrphanedRecords()`, and the owned-character
  guard in `DeleteGroupMembers()`.

---

## Phase 4 → Phase 5 handoff (from the implementing agent)

### The five prefab pairs, and how completeness was actually verified

> ⚠️ **HISTORICAL — the prefab pairs below were DELETED 2026-08-17 by user amendment** (byte-identical deltas made invisible by the per-type loadout overwrite). Every type now spawns `Group_CIV.et` `{1AF5B9AE5CFD4434}`; the per-type clothing confs (last column) are what makes types look different, and the type is carried by the explicit `m_sPendingTypeName` stamp in `OVT_TownCivilianSourceConfig` (never a prefab reverse-lookup). Kept for the verification method, which future prefab work should copy.

| type | vanilla look it deltas | character GUID (deleted) | group GUID (deleted) | per-type clothing |
|---|---|---|---|---|
| `businessman` | `{E024A74F8A4BC644}` Character_CIV_Businessman_1 | `{6B4B2C5F00000020}` | `{6B4B2C5F00000030}` | `{6B4B2C5F00000040}` |
| `dockworker` | `{C6FAF52907A544AC}` Character_CIV_Dockworker_1 | `{6B4B2C5F00000021}` | `{6B4B2C5F00000031}` | `{6B4B2C5F00000041}` |
| `worker` | `{6F5A71376479B353}` Character_CIV_ConstructionWorker_1 | `{6B4B2C5F00000022}` | `{6B4B2C5F00000032}` | `{6B4B2C5F00000042}` |
| `townsfolk` | `{8C7093AF368F496A}` Character_CIV_CottonShirt_1 | `{6B4B2C5F00000023}` | `{6B4B2C5F00000033}` | `{6B4B2C5F00000043}` |
| `villager` | `{11EB9A0D2A5899EA}` Character_CIV_DenimJacket_1 | `{6B4B2C5F00000024}` | `{6B4B2C5F00000034}` | `{6B4B2C5F00000044}` |

**The component check was mechanical, not eyeballed** (the memory says Overthrow has shipped prefabs
missing vanilla components **twice** - BUG-088, BUG-132 - so "it looks right" was not good enough):

1. **The declared delta is byte-identical.** `diff <(tail -n +2 Character_CIV.et) <(tail -n +2
   Character_CIV_<type>.et)` is **empty but for the trailing newline** on all five. Same three component
   blocks, same component IDs, same six actions, same UIInfo strings.
2. **The inherited set was resolved by walking the parent chain** in both trees (a script followed each
   header's `{GUID}path` up to the root and unioned the component class names declared at each level).
   `Character_CIV.et` resolves to `Character_CIV.et → Character_CIV_base.et → Character_Base.et`; every
   new prefab resolves to `<look>_1 → <look>_Base → Character_CIV_baseLoadout.et → Character_CIV_base.et
   → Character_Base.et`. **The set difference against `Character_CIV.et` is empty in the "missing"
   direction for all five**; the only extras are the three clothing/inventory components the vanilla
   loadout layer adds (`BaseLoadoutManagerComponent`, `SCR_CharacterInventoryStorageComponent`,
   `SCR_InventoryStorageManagerComponent`).
3. **The two components the memory is about are in the SHARED ancestors**, so they cannot be missing
   from one chain and present in the other: `RplComponent` at `Character_Base.et:1255`,
   `SCR_CharacterFactionAffiliationComponent "CIV"` at `Character_CIV_base.et:9`. Neither is
   re-declared in an Overthrow file, and neither should be.

⚠ **The component IDs are REUSED VERBATIM from `Character_CIV.et`, deliberately.**
`ActionsManagerComponent "{520EA1D2F659CE02}"` and its `UserActionContext "{520EA1D2F659CE17}"` are the
**inherited** vanilla ids (`Character_Base.et:566,568`) - using a fresh id there would add a SECOND
actions manager instead of overriding the one the character already has. The Overthrow-authored ids
(`OVT_PlayerOwnerComponent`, the six actions) are reused for the same reason a vanilla prefab reuses
`ID "520EC961A090B1EE"`: object ids are scoped per resource, and these five prefabs are siblings of
`Character_CIV.et`, never descendants of it.

### Clothing: the honest numbers

`CivilianClothes.conf` (the global pool) went **pants 7 → 10** (the three `Pants_Fisherman_01`),
**shoes 4 → 5** (`TankerBoots_US_01`) and **hats 21 → 26** (three `Hat_ZmijovkaCap_01`, two
`Hat_Ushanka_01`). **Tops did not move, and cannot: they were already 27 of 27.** Every civilian-
appropriate jacket/shirt Reforger 1.8.0.10 ships (3 denim, 3 raincoat, 3 suit, 12 cotton shirt, 6
turtleneck) was already authored. The plan's "~94 uniform prefabs" figure counts the military uniform
sets; putting a Soviet field jacket on a shopkeeper is not a widening, it is a bug. **The variety in
this phase comes from the per-type loadouts, not from a bigger global pool** - which is exactly what
D1/F-G predicted.

Two things a later author needs to know about the loadout mechanism:

- **A slot a per-type file does not author is LEFT ALONE**, so the prefab's own clothing survives there.
  That is why `CivilianClothes_Businessman.conf` omits Shoes (vanilla ships no dress shoes; the
  `Character_CIV_baseLoadout.et` boots stand) and why the worker's hard hat is authored explicitly
  rather than left to the prefab (it would otherwise be the same yellow one on every construction
  worker in the world).
- **The GUID pool for vanilla clothing is fully enumerable and now proven complete.** The extracted
  vanilla tree ships no `.meta` files, so a prefab's GUID is only knowable from somewhere that
  references it. A sweep of both trees found a GUID for **every** non-`_base` prefab under
  `Prefabs/Characters/{Uniforms,HeadGear,Footwear}` except six military ones. Nothing civilian was left
  out for want of a GUID.

### `m_Loadout` is authored as an inline object INHERITING from a `.conf` file

The attribute is `ref OVT_LoadoutConfig` (an inline object), and the task asked for shipped *files*.
Both are satisfied with **no script change**, using the tree's own idiom from the game-mode prefab
(`m_CivilianLoadout OVT_LoadoutConfig "{...}" : "{...}Configs/Civilians/CivilianClothes.conf" {}`):

```
m_Loadout OVT_LoadoutConfig "{6B4B2C5F00000054}" : "{6B4B2C5F00000040}Configs/Civilians/Loadouts/CivilianClothes_Businessman.conf" {
}
```

A modder overrides one slot for one type by putting it inside those braces; a modder replacing the
whole set edits the file. **This is authored text the compiler never sees** (risk R11), which is why
the Init case asserts that at least one shipped type resolves a loadout with slots in it, and why the
Workbench check is owed.

### Dressing moved INSIDE `OnCivilianAgentAdded`

`OnEntitySpawned` used to insert **two** callbacks into `GetOnAgentAdded` - the global
`OVT_LoadoutUtils.RandomizeCivilianClothes` and the feature's own. It now inserts **one**: the clothes a
civilian gets depend on the type it was rolled from, and only the record knows which that was.
`ResolveTypeLoadout(record.m_sTypeName)` looks the type up **by name** in the town's allowed set rather
than caching an object pointer on the record - a handful of entries, once per civilian, and a name
cannot dangle if a template is re-authored.

⚠ The order inside that callback is now **dress → disable-wanted → mark `m_bSeenAgent`**. The mark stays
last on purpose: it is the dead-check's precondition, and a civilian that were marked before it was
dressed could in principle be pruned between the two.

### Per-town curation: which Eden towns were actually curated, and why

13 of 20. Every list carries `generic`, so no authored town can ever curate itself empty.

| town | size | list | reason |
|---|---|---|---|
| St. Phillipe | CITY | generic, townsfolk, dockworker, businessman, worker | the northern port city - the one place all five fit |
| Montignac | CITY | generic, townsfolk, businessman, worker | inland administrative centre, no docks |
| St. Pierre | CITY | generic, townsfolk, dockworker, businessman | southern coastal city |
| Meaux | TOWN | generic, townsfolk, villager, worker | inland |
| Morton | TOWN | generic, townsfolk, dockworker, worker | harbour |
| Lamentin | TOWN | generic, townsfolk, dockworker, villager | harbour |
| Levie | TOWN | generic, townsfolk, villager | inland hill town |
| Regina | VILLAGE | generic, worker, villager | the sawmill village (vanilla even ships a `Hat_TruckerCap_01_sawmillregina`) |
| Erquy | VILLAGE | generic, townsfolk, villager | coastal, but see the note below |
| Tyrone, Gravette, Chotain, Laruns | VILLAGE | generic, villager | rural hamlets |

The other seven towns (Villeneuve, Le Moule, Figari, Provins, Entre-Deux, Durras, Vernon) are left
**un-authored on purpose**: an empty list is the "whatever this size allows" default, and leaving some
towns on it keeps that path exercised in the shipped world rather than only in a test.

⚠ **`dockworker` is authored `m_eMinTownSize TOWN`, so a coastal VILLAGE cannot have one** even if its
list names it. Erquy, Vernon and Durras are coastal villages that therefore get no dockworkers. That is
a deliberate trade: dropping the minimum to VILLAGE would put dockworkers in every *inland* hamlet that
never authored a list, which is the worse of the two wrongs. If a later author wants coastal villages
to have dockworkers, the fix is to promote those towns to TOWN in `towns.layer`, **not** to lower the
type's minimum size.

### Things a Phase 5 author will trip over

- **The type table is now six entries, and `RollPrefab` weights them 100/70/70/40/40/30.** A town does
  not roll a fixed mix - it rolls per civilian out of whatever its size and list allow, so a village
  crowd is generic/townsfolk/villager/worker and a curated city crowd is visibly different.
- **`m_iSpawnDistanceOverride` and the density numbers were NOT touched** - a city now has more *kinds*
  of civilian, not more civilians. D7's unmeasured spawn-ring consequence is still unmeasured.
- **Nothing in this phase replicates or persists**, and the forbidden greps over
  `Scripts/Game/GameMode/Civilians/` are still empty. The prefabs carry `RplComponent` from the vanilla
  chain, which is the *character's* replication and has nothing to do with the ambient bookkeeping.
- **Everything in the Phase 3 and Phase 2 handoffs below still applies.**

---

## T5.1 — the placement spike, and the decision it produced (2026-08-17)

**The honest headline: no live counts were obtainable.** The spike was to answer "how many kerb spots
and how many road-derived spots exist inside 3 towns of different sizes", and that question can only be
answered by running the world. The Eden terrain is not readable from the repository (no world entity
file in the tree; the vanilla 1.8.0.10 reference tree ships assets and prefabs, not the placed world),
and the phase's rule was **do not gold-plate the spike**. So the decision below is made on the
code-level argument, and the placement-quality play-test (§6 step 13) is named as the decider.

### What IS knowable statically, and it is more than it looks

| Question | Answer, with the evidence |
|---|---|
| Does the kerb matcher actually match Eden's kerbs? | **Yes, and both hand-placed and generated ones.** `OVT_VehicleManagerComponent.FilterKerbAddToArray` matches `ClassName() == "StaticModelEntity"` with `Pavement_` or `Kerb_` in the mesh resource name. Vanilla ships `ConcreteKerb_01` (4 variants), `ConcreteKerb_01_USSR` (4), `AsphaltPavement_01` (24) and `TilePavement_01`, all `StaticModelEntity : Pavements_Base.et` with meshes named `…ConcreteKerb_01_4m_v2.xob` / `…AsphaltPavement_01_Wide_8m.xob` — both substrings hit. The `WEGenerators/Pavements/*` wall generators emit those same prefabs (`WG_ConcreteKerb_01.et` names `ConcreteKerb_01_2m_v2.et` as its wall asset), so a procedurally kerbed street is matched exactly like a hand-placed one. |
| Is the kerb path production-exercised? | **Yes, three call sites.** `SpawnStartingCar` (20 m), `SpawnVehicleNearestParking` (30 m) and the persistence vehicle rebuild (30 m). It is the fallback the whole vehicle economy leans on when a house has no `OVT_ParkingComponent`. |
| Does the kerb path give a usable heading? | **Yes, and a better one than the road path.** It steps 3 m off the kerb piece along the piece's own forward axis and yaws −90°, i.e. the car ends up **parallel to the kerb**, which is what parked cars look like. |
| Is the road path production-exercised? | **Yes.** `OVT_WorldUtils.FindNearestRoadSpawn` has three callers in `OVT_TravelRequestComponent`, and it already returns road-aligned angles from the nearest polyline segment. (The memory that `GetClosestRoad` has no vanilla call site is true and irrelevant here — Overthrow is the call site, and it fails closed on every branch.) |
| Does a lateral offset from the centreline land on the shoulder or in the lane? | **UNKNOWN — this is the one thing only a play-test can answer.** `FindNearestRoadSpawn` projects onto the road POLYLINE, which is the centreline, so an un-offset car straddles the white line. Eden's two-lane roads are ~6 m wide, so ~3 m reaches the lane edge and ~4.5 m should reach the verge. **4.5 m is authored** (`m_fRoadLateralOffset`) precisely so it can be re-tuned without a script change. |
| Can a town-wide spot census be run at all? | **Not affordably, and it is not needed.** A kerb census over a city's 800 m range would be one `QueryEntitiesBySphere` over the whole town per activation. The shipped design never does that: it rolls a scatter point first and then runs ONE 35 m kerb query around it, which is the same cost the vehicle shop already pays per purchase, ≤6 times per car and ≤8 cars per city. |

### The decision

**Kerb-first, with a road-derived fallback.** Not road-only (a kerb spot is strictly better wherever one
exists, and the code is already written and proven), not the authored-pool bail-out (nothing about the
two existing helpers disappoints on the evidence above — the only open question is one offset number,
which is authored). The bail-out stays available at zero cost: set `m_fKerbSearchRange 0` to get
road-only, or re-author `m_fRoadLateralOffset` to fix a bad verge.

**The play-test is the decider** (§6 step 13, expanded below). If cars come out in the driving lane,
raise `m_fRoadLateralOffset`; if they come out in hedges, lower it; if kerbside cars look wrong in a
particular town, that is a kerb-geometry finding worth its own note.

---

## Phase 5 → Phase 6 handoff (from the implementing agent)

### The one line the phase exists for

`OVT_TownVehicleSourceConfig.OnEntitySpawned` calls
`OVT_PersistenceManagerComponent.UntrackTransient(entity)` on the line after the spawn. Vehicles carry
NATIVE persistence components — unlike a civilian, which the modded `SCR_AIGroup` chokepoint untracks
for free — so without it every parked car writes a save record and comes back **duplicated** on load,
one copy from the record and one from the next ambient roll (risk R6, the BUG-118 shape). Anybody
touching that hook must keep the line, and it must stay **before** anything that can early-return.

### T5.4's mechanism is NOT a compartment subscription, and could not be

**Reforger 1.8.0.10 ships no vehicle-side compartment-entry signal.** `SCR_BaseCompartmentManagerComponent`
publishes exactly one invoker, `GetOnDoneSpawningDefaultOccupants`; the engine
`BaseCompartmentManagerComponent` publishes none; `GetOnCompartmentEntered()` lives on the ENTERING
CHARACTER's `SCR_CompartmentAccessComponent`, and `FactionAffiliationComponent.OnCompartmentEntered` is a
protected engine event that would have to be modded globally. Subscribing per character would mean a
global per-player subscription with respawn and join-in-progress bookkeeping — precisely the dangling
handler risk the task wanted avoided — to learn something **core's own prune pass already asks this
source, every tick, for every car it owns**.

So the trigger is core's tick:

- `IsEntityDead(vehicle)` answers **true** when the car is claimed (an `OVT_PlayerOwnerComponent` uid —
  stamped server-side by Overthrow's existing driver-seat claim path — **or** a player-controlled
  occupant in any compartment, which covers the passenger seat the claim path never fires for) or when
  the base class's damage-state check says it is a wreck. The record's `m_bClaimed` bit is what tells
  the two apart afterwards.
- `OnEntityPruned(vehicle)` then either **adopts** it (`CancelUntrackTransient` + `Track` — the
  recruit-body precedent BUG-131 verbatim — and a remove-then-insert into
  `OVT_VehicleManagerComponent.m_aVehicles`) or leaves the wreck alone to rust and die with the session.
- `OnEntityDespawning(vehicle)` is the belt-and-braces hatch the task asked for: **anybody** inside
  (player or a player's recruit) and the car claims itself out of the despawn instead of being deleted.
  This is also what covers a player driving an ambient car out past the despawn ring faster than the
  prune could see them.

**Consequences to carry forward:** latency is one ambient tick (~2 s) rather than instant — nothing can
go wrong in that window, because the only race (a despawn) is covered by the hatch — and **there is no
subscription anywhere in this phase, so there is nothing to unsubscribe**. If a later author wants
instant adoption, the honest hook is Overthrow's own `OVT_VehicleRequestComponent.RpcAsk_ClaimUnownedVehicle`
(server-side, already resolves the vehicle), not a compartment invoker.

### Placement, exactly as built

`RollPosition` rolls up to `m_iPlacementAttempts` (6) scatter points and for each tries **kerb first**
(`FindNearestKerbParking`, 35 m) then **road** (`FindNearestRoadSpawn`, 120 m, plus a ±4.5 m lateral step
off the centreline, side rolled). Every candidate is ocean-tested and then trace-tested with the
vehicle box `Mins "-1.5 0 -3"` / `Maxs "1.5 2.5 3"`, **rejecting on `TracePosition() < 0`** — the
`OVT_ParkingComponent` idiom, spelled out in a comment because BUG-031 was the same test inverted.
`FindSafeSpawnPosition` (2 m probe) and `OVT_EntitySpawningAPI.ValidateSpawnPosition` (sticky state) are
deliberately not used.

⚠ **Two things about that shape a later author will trip over:**

- **`RollPosition` cannot refuse.** Core calls roll → position → spawn → hook, and the position hook must
  return something. So a total placement failure is carried forward on `m_bPendingPlacementFailed`, and
  `OnEntitySpawned` **releases and deletes** the car. That costs one wasted spawn in the rare case and is
  the only shape that keeps the obstruction test genuinely *before* the car is allowed to stay.
- **Core spawns by POSITION, not by matrix**, so a car arrives facing world north whatever the road does.
  `OnEntitySpawned` applies the heading with `SetTransform` plus the tree's own settle nudge
  (`GetPhysics().ApplyImpulse("0 -1 0")`, the `OVT_FlipVehicleAction` idiom). This is safe **only**
  because the car is one frame old and empty — the "SetOrigin on a vehicle occupant throws them off the
  map" trap needs an occupant, and an occupied ambient car is never repositioned by this feature.

### The count model is size-scaled, not population-scaled

Three authored pairs on the template (`m_iVillageMin/Max` 0–1, `m_iTownMin/Max` 2–4, `m_iCityMin/Max`
4–8, CAPITAL reads the city pair), rolled through `OVT_VirtualizationMath.RollCountSafe` so an authored
0/0 village cannot hit `RandInt(n, n)`. Population is deliberately NOT the input: a fishing village has
people but no traffic. **The base class's `m_iMinCount`/`m_iMaxCount` are therefore unused by this
source** — they are authored 0/8 in the `.conf` for readability only; re-authoring them changes nothing.

### The manager now holds four maps, not two

`m_mVehicleSources` / `m_mVehicleHandles` are exact parallels of the civilian pair, with the same
"present in sources but absent from handles = suspended" convention. Both sources are registered in the
same `ActivateTown()`, torn down in the same `DeactivateTown()`, and suppressed/restored together by the
QRF path (D13 — still opt-in, still `despawnCiviliansDuringQRF`). The vehicle half is registered
**first and is not gated on the civilian half**: a registry that authors one but not the other must
still deliver what it did author, and a registry with no `town_vehicles` entry is **silent**, because
this phase is droppable and an old registry must not warn on every town.

### Residuals recorded honestly

- **A wreck left by the prune path is untracked**, so a burnt-out ambient car disappears on reload. That
  is the intended reading of "nothing ambient is ever persisted"; it is called out because a player who
  burns a car, saves and reloads will see it gone.
- **The claim path can adopt a car with no owner uid** (a player riding as a passenger while an AI or
  nobody drives). It is then tracked and in `m_aVehicles` but unowned — which is exactly what an unowned
  vehicle bought from a shop and abandoned looks like, so nothing downstream is surprised.
- **`m_fRoadLateralOffset` is a guess until somebody looks at it.** See the spike record above.
- **No Logic-tier case was added.** The count model is three authored pairs fed to an existing, already
  asserted core static (`RollCountSafe`); a Logic case would be asserting the core static a second time.
  The authoring itself is asserted by the new Init case.

---

## Session Notes

### 2026-08-17 — Phase 5 built (`component-developer`)
- T5.1–T5.6 all landed. **compile-check exit 0** (6124 files, 1 script added). Suite runs deferred to the orchestrator per the test policy; **All** is owed (persistence tracking is touched).
- Files: **1 created** (`Scripts/Game/GameMode/Civilians/OVT_TownVehicleSourceConfig.c`), **3 modified** (`OVT_CivilianAmbienceManagerComponent.c`, `Configs/Civilians/CivilianAmbience.conf`, the Init suite), **2 docs**.
- Grep gates: forbidden-identifier sweep over `Scripts/Game/GameMode/Civilians/` **empty**; `git diff Configs/Systems/Persistence/Overthrow.conf` **empty**; core's `Virtualization/` diff unchanged from Phase 1. Every vehicle-record creation site (`OnEntitySpawned`) is paired with the one deletion site (`DiscardRecord`), reached from despawn, prune, claim and the orphan sweep.
- Deviations from the literal task text, all argued in the handoff above: **T5.1 produced no live numbers** (the world is not readable statically) so the decision rests on the code-level argument and the play-test is the decider; **T5.4 uses no compartment subscription** because 1.8.0.10 ships no vehicle-side compartment-entry signal — core's prune tick plus the despawn occupancy hatch are the trigger, which also means there is no handler to leak; a **failed placement releases and deletes** the car in `OnEntitySpawned` because `RollPosition` has no way to refuse; the source's count comes from **three size-scaled authored pairs**, leaving the base class's min/max unused.
- Behaviour change to expect in play: towns now have parked civilian cars (S105, S1203, UAZ452, two UAZ469s) along kerbs and roadsides — free to take, and yours (and in the save) once you get in.

### 2026-08-17 — Phase 4 built (`component-developer`)
- T4.1–T4.5 all landed. **compile-check exit 0** (6123 files, no scripts added - everything new this phase is prefab/config text). Suite runs deferred to the orchestrator per the test policy; **Fast and All** are both owed (the town controller and the Eden towns layer are touched).
- Files: **30 created** (5 character prefabs, 5 group prefabs, 5 per-type loadout `.conf`, each with its `.meta`), **7 modified** (`CivilianClothes.conf`, `CivilianAmbience.conf`, `OVT_LoadoutUtils.c`, `OVT_TownController.c`, `OVT_CivilianAmbienceManagerComponent.c`, `OVT_TownCivilianSourceConfig.c`, `towns.layer`), **2 test files**, **2 docs**.
- Grep gates: `Rpc|RplProp|Replication.Bump|RplSave|RplLoad|PlayerInRange|NearestPlayer|CallLater` over `Scripts/Game/GameMode/Civilians/` **empty**; Logic-tier `OVT_Global|GetGameMode` over the new case **empty**; `towns.layer` diff **+39 / −0**.
- Deviations from the literal task text, all argued in the handoff above: **5 pairs, not 4–6 with per-type files only for businessman and construction worker** (all five got one, because a variant without its own clothing is invisible); the **tops pool could not be widened** (27/27 already authored); an **empty-choices guard** added to `SpawnDefaultCharacterItem` (a modder's empty slot would otherwise hit `RandInt(0, 0)`); a **warning log** when a town curates itself to zero types.
- Behaviour change to expect in play: town crowds now contain up to six visibly different kinds of person, and 13 Eden towns have hand-authored casts.

### 2026-08-17
- Feature started via /autorun-feature (Discord). Scaffolding created from implementation.md; Phase 1 next.

### 2026-08-17 — Phase 1 built (`component-developer-advanced`)
- T1.1–T1.8 all landed; compile-check exit 0 (6120 files, 2 added). Suite run deferred to the orchestrator per the test policy.
- Files: 2 created (`OVT_CivilianAmbienceMath.c`, `OVT_TEST_Logic_CivilianAmbience.c`), 4 modified (2 in core's Virtualization dir, the config component, the Init suite), 2 docs modified (`core/api.md`, `core/context.md`).
- Deviation worth knowing: a fourth static (`SumWeights`) and the prune-loop cursor clamp were added beyond the literal task text — both are argued in the handoff above.

### 2026-08-17 — Phase 3 built (`component-developer`)
- T3.1–T3.6 all landed, **including the optional T3.5** (it was cheap; the phase did not run long). **compile-check exit 0** (6123 files, no files added). Suite runs deferred to the orchestrator per the test policy — **Fast and All** are both owed (T3.5 touches the town manager, so All is required).
- Files: **0 created**, **7 modified** — `OVT_OccupyingFactionManager.c` (+19/−0), `OVT_OverthrowConfigComponent.c` (the F-C fix + the new config field), `OVT_TownManagerComponent.c` (T3.5), `OVT_CivilianAmbienceManagerComponent.c`, `OVT_TownCivilianSourceConfig.c`, `OVT_CivilianAmbienceConfig.c` (doc/attribute text now that the archetypes are real), `Configs/Civilians/CivilianAmbience.conf`; plus 2 test files and these 2 docs.
- **Prefab check result (T3.2): NO EDIT NEEDED.** `AIWaypoint_Wait.et` is a vanilla file and already authors `m_TimedWaypointParameters`. The fix is the single `SetHoldingTime(time)` call.
- **Mid-phase user amendment (D13):** QRF suppression became opt-in (`despawnCiviliansDuringQRF`, default false). Integration criterion I4 goes from two config-struct fields to three — sanctioned, recorded here so review does not flag it.
- Tests added: 1 Logic case (`OVT_TEST_Logic_CivilianAmbience_ArchetypeWeights`) and 1 extended Init case (`OVT_TEST_Init_Civilians_AmbienceConfigDefaults` now also asserts the new field's false default). Both carry can-fail proofs; no `maxAttempts`.
- Behaviour changes to expect in play beyond civilians: **every existing wait waypoint in Overthrow now honours its requested duration** (PERIMETER patrols pause 45–75 s instead of the prefab's flat 60).

### 2026-08-17 — Phase 2 built (`component-developer-advanced`)
- T2.1–T2.11 all landed; **compile-check exit 0** (6123 files, 3 added). Suite run deferred to the orchestrator per the test policy — **Fast and All** are both owed.
- Files: **5 created** (3 scripts under `Scripts/Game/GameMode/Civilians/`, `Configs/Civilians/CivilianAmbience.conf` + `.meta`), **8 modified** (town controller, config component, recruit manager, game mode, `OVT_Global`, the game-mode prefab, the Eden managers layer, the Init suite), **2 docs** (`core/api.md` §6, this file).
- Grep gates: retired-symbol sweep over `Scripts/ Prefabs/` **empty**; `OVT-VIRT-PLAYTEST-ONLY` **10 → 9** with the removed one being exactly the `SpawnCivilians` guard; replication and proximity/tick greps over `Scripts/Game/GameMode/Civilians/` **empty**; `Configs/Systems/Persistence/Overthrow.conf` diff **empty**; core's `Virtualization/` diff unchanged from Phase 1; `OVT_TownController.c` **+34 / −90**.
- Deviations from the literal task text, all argued in the handoff above: `SetDeleteWhenEmpty(false)` at spawn and `(true)` at release (without them the design cannot work), `SweepOrphanedRecords()` (the one leak path core cannot hook), and the extra `m_pCivilianPrefab` removal from `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer` (a second binding of the retired attribute that the plan's grep did not cover).
- Behaviour change to expect in play: **town civilians are no longer suppressed during a QRF** until T3.1 lands, and (D11) they finally stop participating in the wanted system.

### 2026-08-17 (final — autorun complete)
- All 6 phases + 2 user amendments built and gated in one autonomous run (Discord `/autorun-feature`).
- Suite verdicts: Fast 171 → All 221 → All 222 → All 224 → All 225 → Fast 179, all green; one 300s timeout (no verdict) traced to concurrent docs-agent writes, single re-run green.
- BUG-179/BUG-180 filed against the pre-migration code (historical, already fixed here).
- Nothing committed; user owns git. Next: user Workbench/play-test passes per the verification list.

### 2026-08-17 (post-play-test fix)
- User play-test verdict: **no regressions; dressing, curation and parked cars all confirmed working.** Localization re-export done by the user.
- One issue found: NPE spam from `OVT_VehicleManagerComponent.FilterKerbAddToArray` when the ambient vehicle path is the session's FIRST parking search — pre-existing latent bug (`FindNearestKerbParking` only Cleared an array that `GetNearestParkingSpot` allocates; kerb failures fell through to the road path, so placement still looked right). Fixed by allocating in `FindNearestKerbParking`; filed + closed as **BUG-181**. Gates: compile 0, Fast 179/179.

### 2026-08-17 (post-play-test fix 2 — barefoot civilians)
- User reported some civilians spawning without shoes. Cause: the Businessman/Townsfolk/Villager per-type loadout confs shipped **without a Shoes slot** (Dockworker/Worker had one), and a per-type loadout replaces the global conf entirely — no slot ⇒ nothing on the feet.
- Fixed: added a Shoes slot to all three (slot GUIDs `{6B4B2C5F000000A3/D3/E3}`, verified repo-unique; braces/quotes balanced). Note: **vanilla ships no civilian footwear** — 5 boot prefabs only, and vanilla's own civilian base loadout wears `CombatBoots_Soviet_01` — so the pools are curated boot mixes (businessman = clean tanker/US, villager = dirty variants).
- Gate: conf-only change, suites deliberately skipped (policy) — also deferred because the user's Workbench session was live. The Init loadout-deserialization case covers it on the next run.
- **Rule for future types: every per-type loadout conf MUST author all four slots (Pants/Top/Shoes/Hat)** — a missing slot is silently barefoot/topless, not a fallback to the global conf.
