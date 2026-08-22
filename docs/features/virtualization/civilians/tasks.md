# Virtualization Civilians - Task Checklist

**Last Updated:** 2026-08-17
**Progress:** 39/39 tasks complete (100%) — all 6 phases built and gated 2026-08-17 (incl. two user amendments: QRF opt-in despawn, per-type prefabs dropped)

**Agent routing:** Phases 1–2 **ADVANCED** (`component-developer-advanced`); Phases 3–5 `component-developer`; Phase 6 `help-docs-sync`. Phase 5 is **droppable**.

---

## Phase 1: Core seam, declarative model, density math (8/8 complete) — ADVANCED ✅

- [x] **T1.1 Add `IsEntityDead` hook to core config**
  - Description: `bool IsEntityDead(IEntity)` on `OVT_AmbientSpawnSourceConfig`, body = manager's current damage-state check; `PruneAmbientEntities` calls it. Byte-for-byte behaviour for existing sources.
  - File(s): `Scripts/Game/GameMode/Virtualization/`
  - Estimate: 🟡

- [x] **T1.2 Add `OnEntityPruned` hook to core**
  - Description: default no-op, called AFTER list + reverse-map removal; order stated in header.
  - File(s): `Scripts/Game/GameMode/Virtualization/`
  - Estimate: 🟢

- [x] **T1.3 Document the additive change**
  - Description: `api.md` §4 (hooks + "leave the body, delete the companions" rule); dated additive note in `core/context.md`.
  - File(s): `docs/features/virtualization/core/api.md`, `core/context.md`
  - Estimate: 🟢

- [x] **T1.4 `OVT_CivilianAmbienceMath` statics**
  - Description: `ResolveTownCivilianCount`, `TypeAllowed`, `PickWeightedIndex` — world-free, no manager/game-mode identifiers.
  - File(s): `Scripts/Game/GameMode/Civilians/OVT_CivilianAmbienceMath.c`
  - Estimate: 🟡

- [x] **T1.5 Config-struct fields**
  - Description: `civilianDensityMultiplier` (1.0), `maxCiviliansPerTown` (30) in `OVT_OverthrowConfigStruct` + `SetDefaults()`; NOT in RplSave/RplLoad; `CONFIG_STREAM_VERSION` stays 3.
  - File(s): `Scripts/Game/GameMode/OVT_OverthrowConfigComponent.c`
  - Estimate: 🟢

- [x] **T1.6 Logic-tier test file**
  - Description: parity (283→28, 38→4), clamps, multiplier=0, population=0, hard cap, hardCap<=0 uncapped, weighted pick edges, type filter. Can-fail proofs.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_CivilianAmbience.c`
  - Estimate: 🟡

- [x] **T1.7 Init case: `IsEntityDead` virtual dispatch**
  - Description: a config subclass's override is the one core calls.
  - File(s): `Scripts/Game/Tests/TestSuites/OVT_TEST_InitSuite.c`
  - Estimate: 🟢

- [x] **T1.8 Init case: config-struct defaults**
  - Description: both new fields read back defaults through `OVT_Global.GetConfig().m_ConfigFile`.
  - File(s): `Scripts/Game/Tests/TestSuites/OVT_TEST_InitSuite.c`
  - Estimate: 🟢

---

## Phase 2: The migration (parity) (11/11 complete) — ADVANCED ✅

- [x] **T2.1 Authored config classes**
  - Description: `OVT_CivilianTypeConfig`, `OVT_ECivilianArchetype`, `OVT_CivilianArchetypeConfig`, `OVT_CivilianAmbienceConfig`.
  - File(s): `Scripts/Game/GameMode/Civilians/`
  - Estimate: 🟡

- [x] **T2.2 Per-town source config + record (the lifecycle core)**
  - Description: `OVT_TownCivilianSourceConfig` + `OVT_AmbientCivilianRecord`, all seven overrides; parity content (one type, PINGPONG); F-A explicit member deletion; F-B agent-based wanted disable; prune leaves the body.
  - File(s): `Scripts/Game/GameMode/Civilians/`
  - Estimate: 🔴

- [x] **T2.3 Author `CivilianAmbience.conf`**
  - Description: registry root + `town_civilians` at parity (rate 0.1, min 2, max 40, distance override -1); fresh GUID; deployments-conf shape.
  - File(s): `Configs/Civilians/CivilianAmbience.conf`
  - Estimate: 🟢

- [x] **T2.4 Wire `m_AmbientRegistry` on the game-mode prefab**
  - Description: text-wire to the new .conf; flag Workbench verification as user task.
  - File(s): `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 🟢

- [x] **T2.5 `OVT_CivilianAmbienceManagerComponent`**
  - Description: manager pair, `s_Instance`, server guard, `ActivateTown`/`DeactivateTown`/`ReleaseRecruitedCivilian`/`OnDelete`.
  - File(s): `Scripts/Game/GameMode/Civilians/OVT_CivilianAmbienceManagerComponent.c`
  - Estimate: 🟡

- [x] **T2.6 Game-mode + `OVT_Global` wiring**
  - Description: `GetCivilianAmbience()`; EOnInit + PostGameStart blocks after Virtualization's; component text-wired onto prefab.
  - File(s): `Scripts/Game/GameMode/OVT_OverthrowGameMode.c`, `OVT_Global.c`, prefab
  - Estimate: 🟢

- [x] **T2.7 Retire the town-controller spawner**
  - Description: `ActivateTown()` keeps gun dealer + gains manager call; add `OnDelete`; delete `m_aCivilians`, `m_bCiviliansSpawned`, `CheckSpawnCivilian`, `SpawnCivilians`, `SpawnCivilian`, `DespawnCivilians`, `GetGroup`, `DisableCivilianWantedSystem` (+ its OVT-VIRT-PLAYTEST-ONLY guard line).
  - File(s): `Scripts/Game/Controllers/OVT_TownController.c`
  - Estimate: 🟡

- [x] **T2.8 Retire the three config attributes**
  - Description: `m_pCivilianPrefab` (+ prefab binding), `m_fCivilianSpawnRate`, `m_iCivilianSpawnDistance`; grep-prove zero readers first; update core `api.md` §6 prose.
  - File(s): `OVT_OverthrowConfigComponent.c`, `Prefabs/GameMode/OVT_OverthrowGameMode.et`, core `api.md`
  - Estimate: 🟢

- [x] **T2.9 Recruit release hook**
  - Description: in `RecruitCivilian()` after `AddRecruit`/`SetRecruitFaction`, before `AddRecruitToPlayerGroup()`; null-safe; reasoning in a code comment.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c`
  - Estimate: 🟡

- [x] **T2.10 Init-tier cases**
  - Description: registry resolves `town_civilians` as `OVT_CivilianAmbienceConfig`; per-town instance exposes template + radius; manager via `OVT_Global`; release no-ops safe.
  - File(s): `Scripts/Game/Tests/TestSuites/OVT_TEST_InitSuite.c`
  - Estimate: 🟡

- [x] **T2.11 Seed context + file F-A/F-B bugs**
  - Description: context.md play-test list; file F-A and F-B as bugs against pre-migration code for history.
  - File(s): `docs/features/virtualization/civilians/context.md`, bug tracker
  - Estimate: 🟢

---

## Phase 3: QRF locality, real pauses, placement, archetypes (6/6 complete) ✅

- [x] **T3.1 `m_OnQRFTownChanged` invoker + town-local suppression** *(scope amended by the user mid-phase — D13)*
  - Description: invoker on `OVT_OccupyingFactionManager` (town id at `StartTownQRF`, -1 at `OnQRFFinishedTown`); civilians manager subscribes once and suspends/resumes that town's source. **Suppression is now OPT-IN**: new config-struct field `despawnCiviliansDuringQRF`, default **false**, so civilians stay during a QRF unless a heavy server asks otherwise.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` (real path has `Factions/`), civilians manager, `OVT_OverthrowConfigComponent.c`
  - Estimate: 🟡

- [x] **T3.2 Fix F-C wait-time bug** — prefab needed NO edit (`AIWaypoint_Wait.et` already authors `m_TimedWaypointParameters { m_holdingTime 60 }`)
  - Description: `SpawnWaitWaypoint` calls `SetHoldingTime(time)`; VERIFY `AIWaypoint_Wait.et` authors `m_TimedWaypointParameters`; note the two existing callers start pausing 45–75 s.
  - File(s): `OVT_OverthrowConfigComponent.c`, `Prefabs/AI/Waypoints/AIWaypoint_Wait.et`
  - Estimate: 🟢

- [x] **T3.3 Doorway/POI placement with lazy cache**
  - Description: `RollPosition` doorway/POI with `m_fBuildingPlacementChance`, else road point; per-town candidate list built lazily, cached; never ocean.
  - File(s): `Scripts/Game/GameMode/Civilians/`
  - Estimate: 🟡

- [x] **T3.4 WANDER + LOITER archetypes**
  - Description: per-archetype waypoint construction in `OnEntitySpawned`; every waypoint recorded; weights authored in .conf.
  - File(s): `Scripts/Game/GameMode/Civilians/`, `Configs/Civilians/CivilianAmbience.conf`
  - Estimate: 🟡

- [x] **T3.5 (optional) Bound `m_ConvertedCivilians`** — DONE, not dropped
  - Description: prune unresolvable RplIds past a threshold. Drop if phase runs long.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c`
  - Estimate: 🟢

- [x] **T3.6 Logic cases + play-test steps**
  - Description: archetype weight resolution cases; QRF-locality/archetype play-test steps in §6.
  - File(s): `TestSuites/Logic/OVT_TEST_Logic_CivilianAmbience.c`, docs
  - Estimate: 🟢

---

## Phase 4: Variety — prefabs, clothing, per-town filtering (5/5 complete) ✅

- [x] **T4.1 Author 4–6 civilian prefab pairs** — ⚠️ superseded 2026-08-17: pairs DELETED by user amendment (redundant; looks come from per-type loadout confs; all types spawn Group_CIV.et) — **5 pairs** (businessman, dockworker, worker, townsfolk, villager)
  - Description: `Character_CIV_<type>.et` deltas over vanilla looks carrying ALL Overthrow components (diff against `Character_CIV.et`!); `Group_CIV_<type>.et` deltas overriding `m_aUnitPrefabSlots`. Component blocks are **byte-identical** to `Character_CIV.et`'s; both chains share `Character_CIV_base.et` (faction affiliation) and `Character_Base.et` (`RplComponent`).
  - File(s): `Prefabs/Characters/Factions/CIV/`, `Prefabs/Groups/INDFOR/`
  - Estimate: 🔴

- [x] **T4.2 Widen clothing + per-type loadouts** — **5 per-type files**, one per new pair
  - Description: widened `CivilianClothes.conf` (pants 7→10, shoes 4→5, hats 21→26; **tops were already exhausted at 27/27** — see context.md); `ApplyCivilianLoadout(character, config)` overload with the one-arg form delegating; per-type files under `Configs/Civilians/Loadouts/`, wired through `OnCivilianAgentAdded` (null loadout = the global conf).
  - File(s): `Configs/Civilians/`, `OVT_LoadoutUtils.c`
  - Estimate: 🟡

- [x] **T4.3 Per-town `m_aCivilianTypes` attribute** — authored on **13 of Eden's 20 towns**
  - Description: attribute on town controller (empty = size default), passed through `BuildTownSource` → `ResolveAllowedTypes` → `TypeAllowed` (both filters combined); authored in `towns.layer` (+39/−0 lines). Every authored list carries `generic`, so curation can never empty a settlement.
  - File(s): `OVT_TownController.c`, `Worlds/MP/OVT_Campaign_Eden_Layers/towns.layer`
  - Estimate: 🟡

- [x] **T4.4 Author type entries in the .conf** — 6 types total
  - Description: `generic`/`townsfolk`/`villager`/`worker` VILLAGE, `dockworker` TOWN, `businessman` CITY; weights 100/70/70/40/40/30; each new type binds its group prefab and its per-type loadout file.
  - File(s): `Configs/Civilians/CivilianAmbience.conf`
  - Estimate: 🟢

- [x] **T4.5 Filter tests**
  - Description: Logic case `OVT_TEST_Logic_CivilianAmbience_ShippedTypeCuration` (the shipped six-type table by size and by allow-list); Init case `OVT_TEST_Init_Civilians_TypeCurationBySize` (CITY vs VILLAGE sets differ, an allow-list narrows, an empty list does not, per-type loadouts deserialised). Can-fail proofs recorded; no `maxAttempts`.
  - File(s): test suites
  - Estimate: 🟢

---

## Phase 5: Ambient parked civilian vehicles (6/6 complete) — DROPPABLE, BUILT ✅

- [x] **T5.1 Placement spike (time-boxed ~2 h, FIRST)**
  - Description: kerb vs road-derived spot counts for 3 towns; decide kerb-first/road-only/bail-out from numbers.
  - **Done:** no live counts were obtainable statically (the Eden world is not readable from the repo); the decision was made on the code-level argument recorded in `context.md` — **kerb-first with a road-derived fallback**, per-spawn queries, no town-wide scan. Placement quality is flagged as the play-test decider.
  - File(s): scratch + `context.md`
  - Estimate: 🟡

- [x] **T5.2 `OVT_TownVehicleSourceConfig` + `town_vehicles` source**
  - Description: count by town size (villages 0–1, cities 4–8); pool via base-class `m_aPrefabs`.
  - File(s): `Scripts/Game/GameMode/Civilians/`, `Configs/Civilians/CivilianAmbience.conf`
  - Estimate: 🟡

- [x] **T5.3 Untrack at spawn**
  - Description: `UntrackTransient(vehicle)` immediately after spawn (gun-dealer idiom) — BUG-118 shape.
  - File(s): `Scripts/Game/GameMode/Civilians/`
  - Estimate: 🟢

- [x] **T5.4 Release on first player entry**
  - Description: compartment-entry → `ReleaseAmbientEntity` → re-track (`CancelUntrackTransient` + `Track`) → register with `OVT_VehicleManagerComponent`.
  - **Deviation:** Reforger 1.8.0.10 ships NO vehicle-side compartment-entry invoker, so the trigger is core's own prune tick (`IsEntityDead` → `OnEntityPruned`) plus the occupancy check in `OnEntityDespawning`. No subscription exists, so none can dangle. See `context.md`.
  - File(s): `Scripts/Game/GameMode/Civilians/`
  - Estimate: 🟡

- [x] **T5.5 Obstruction check**
  - Description: `TraceBox` with the vehicle box, reject on `TracePosition() < 0`; NOT `FindSafeSpawnPosition`/`ValidateSpawnPosition`.
  - File(s): `Scripts/Game/GameMode/Civilians/`
  - Estimate: 🟢

- [x] **T5.6 Vehicle-source tests + play-test steps**
  - Description: Init case (source resolves, rolls a prefab); §6 step 13 play-test steps.
  - File(s): test suites, docs
  - Estimate: 🟢

---

## Phase 6: Help & documentation sync (3/3 complete) ✅

- [x] **T6.1 Wiki server-configuration page**
  - Description: both knobs, defaults, "0 = no civilians", spawn distance rides `virtualizationSpawnDistance`.
  - File(s): wiki
  - Estimate: 🟢

- [x] **T6.2 Fact-check tutorials/Field Manual civilian mentions**
  - Description: cite a file:line or cut the sentence; QRF-locality nuance.
  - File(s): `Configs/Tutorials/`, `Configs/FieldManual/`
  - Estimate: 🟡

- [x] **T6.3 Modder-facing wiki note**
  - Description: adding a civilian type / ambient source via `CivilianAmbience.conf` + per-town override.
  - File(s): wiki
  - Estimate: 🟢

---

## Bugs & Issues

**Active Bugs:**
- (none yet)

**To file (orchestrator action, carried over from T2.11):** F-A (group deletion leaves members) and F-B (wanted-disable no-op) as bugs against the pre-migration code. Full write-ups with file:line evidence are in `context.md` under "FILE F-A AND F-B AS BUGS" — the implementing agent deliberately did not file them.

---

## Technical Debt

- [ ] 💳 **`CheckUpdateFlag` 10 s `CallLater` never removed** — Priority: Low
  - Description: out of scope (D12); known leftover on the town controller.

---

## Progress Tracking

### Discovered New Tasks
- **~~Phase 3 (T3.1) closes a temporary regression~~ — resolved differently.** Phase 2 removed the retired spawner's global QRF suppression. T3.1 restored the *mechanism* town-locally but, per the user's mid-phase amendment (D13), the *default* is now "civilians stay" — so §6 play-test step 9's expected result is permanently "both towns keep their crowds" unless `despawnCiviliansDuringQRF` is on.
- **Phase 3 added a THIRD config-struct field** (`despawnCiviliansDuringQRF`) beyond integration criterion I4's "exactly two" — sanctioned by the user amendment (D13) and recorded in `context.md` so review does not flag it.
- **Workbench verification of the game-mode prefab + the new `.conf` is owed now** (T2.4/T2.6 text-wiring), not at the end of the feature — see `context.md` "Needs human verification".
- **Phase 4 added a second Workbench debt:** the 5 new prefab pairs, the 5 per-type loadout `.conf` files and the inline `m_Loadout` inheritance in `CivilianAmbience.conf` are all authored text no compiler sees. Open each and play-test F9 — see `context.md` "Needs human verification".
- **The vanilla civilian TOP pool is exhausted** (all 27 civilian jackets/shirts vanilla ships were already in `CivilianClothes.conf`). Widening the *global* pool further would mean putting military uniforms on civilians; per-type loadouts are what actually buys the variety. Recorded so nobody re-opens T4.2 expecting a bigger number.
