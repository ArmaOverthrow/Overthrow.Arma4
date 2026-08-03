# Test Coverage — Empirical Findings (Phase 1 spike)

**Date:** 2026-08-02
**Reforger build:** game version **1.7.0.54** built 2026-06-14 12:57:43 UTC, engine version **190965**
(from `console.log`: `INIT : Creating game instance(ArmaReforgerScripted), version 1.7.0.54 built 2026-06-14 12:57:43 UTC.` /
`ENGINE : Initializing engine, version 190965`).
**Validity:** these findings are valid ONLY for this build. Re-run the experiment set after any Reforger update.
**Launcher:** every run used `tools/run-tests.sh [--keep-artifacts] <target>` (no `.exe` was ever invoked directly, no tool
was modified). Default addon set: EDF + EPF (packed workshop) + Overthrow source; profile `OverthrowCI`.
**Steam:** running and logged in throughout.
**Tree state:** feature #2's four test files plus throwaway probe files under `Scripts/Game/Tests/TestSuites/Probe/`,
a throwaway `Configs/Tests/OVT_TestGroup_Probe.conf(+.meta)`, and temporary probe steps in `OVT_TEST_SuiteBase.c` /
`OVT_TEST_SmokeSuite.c`. **All of it deleted in task 1.12**; `tools/compile-check.sh` exits 0 with it gone.

---

## Experiment table

| # | Command | Observed outcome | Wall time (tool time) | Log dir | Notes |
|---|---|---|---|---|---|
| 1.2 | `run-tests.sh OVT_TEST_ProbeWorldFreeSuite` | **exit 0.** `GetWorldFile()` -> `ResourceName.Empty` compiles AND runs. **Zero** `Requesting scenario change:` lines. The harness starts **once** (one `CLI autotest suite:` line, one `Creating: SCR_TestRunner`) instead of twice. `GetGame().GetGameMode()` is `false` — the case runs in the main-menu world. | **11 s** (8 s) | `logs_2026-08-02_00-26-15` | Fastest possible shape. Tier A is genuinely world-free. |
| 1.5 / 1.6 / 1.8 / 1.11-B | `run-tests.sh OVT_TEST_ProbeInitSuite` | **exit 0.** Tier-B shape: world loaded, managers present, campaign **not** started (`HasGameStarted=false IsInitialized=false`). Base-class and derived-suite Setup steps both ran, after the world transition. Filesystem prefixes resolved. | **18 s** (17 s) | `logs_2026-08-02_00-30-41` | 2x `Requesting scenario change:` (normal; feature #2's "harness runs twice"). |
| 1.3 / 1.4 / 1.7 / 1.11-C | `run-tests.sh OVT_TEST_ProbeCampaignSuite` | **exit 0.** Campaign started from a case Main step; `HasGameStarted` and `IsInitialized` both `true` **in the same frame**. 16 s post-start observation window. `SaveGame()` silently no-ops. **62 VM exceptions** fired at t~6.5 s (see Bugs #1). | **33 s** (30 s) | `logs_2026-08-02_00-31-51` | The only run long enough to reach the 5 s and 10 s deferrals. |
| 1.10a | `run-tests.sh "{6A6E04103558938B}"` — group config, suite class **without** `[BaseContainerProps()]` | **exit 2 (indeterminate).** GUID resolved fine (`CLI autotest config: SCR_AutotestGroup<0x...>`), but `DEFAULT (E): Unknown class 'OVT_TEST_SmokeSuite' at offset 76(0x4c)` — the container could not instantiate the suite. Harness listing shows every suite `: 0`. `junit.xml` = empty `<testsuites>`. | **10 s** | `logs_2026-08-02_00-54-01` | The decisive negative result for Decision 4 step 5. |
| 1.10b | same target, `[BaseContainerProps()]` added to `OVT_TEST_SmokeSuite` | **exit 0, 1 test.** Harness listing shows `OVT_TEST_SmokeSuite: 1`, everything else `0`. | **18 s** (16 s) | `logs_2026-08-02_00-54-49` | 2x `Requesting scenario change:`. |
| 1.10c | same target, **two** suites in `m_aSuites` | **exit 0, 2 tests**, both in `junit.xml` as separate `<testsuite>` elements. 3x `Requesting scenario change:`. | **19 s** (17 s) | `logs_2026-08-02_00-55-22` | +1 transition, +1 s wall, +1.2 s junit `time=` per extra suite. |
| 1.5b | two-suite group with a `Sample()` in the `OVT_TEST_SuiteBase` Setup step | **exit 0, 2 tests.** Second suite's sample shows a **different** game mode and a **different** town manager than the first suite's. | **19 s** (16 s) | `logs_2026-08-02_00-56-26` | Established the objects change; did not yet discriminate "nulled" from "stale". |
| 1.5c | same, plus explicit null-discrimination of the weak statics | **exit 0, 2 tests.** `prevGameModeRefIsNULLnow=true`, `prevGlobalTownsRefIsNULLnow=true`, `globalEqualsLive=true`. **Statics were nulled by the engine, not left dangling.** | **20 s** (17 s) | `logs_2026-08-02_00-57-25` | The definitive 1.5 result. |
| 1.3b | `run-tests.sh OVT_TEST_ProbeCampaignNoCloseSuite` (campaign start **without** `CloseLayout()`) | **exit 0.** `HasGameStarted=true IsInitialized=true` same frame, still true 6005 ms later. 62 VM exceptions again (deterministic). | **23 s** (21 s) | `logs_2026-08-02_00-58-03` | `CloseLayout()` is **not required**, but see 1.3 notes — it is still worth keeping. |
| 1.3d | `run-tests.sh OVT_TEST_ProbeCampaignSetupSuite` (campaign start from a **suite `[Step(EStage.Setup)]` bool step**, difficulty chosen **by name**) | **exit 0.** `difficulty selected by name = 'Test World'`; `suite Setup same-frame started=true initialized=true`; `suite Setup done after 1 polls, 71 ms`; the case then sees `started=true initialized=true` on its first tick. **0 VM exceptions** (run ended before the 5 s deferral). | **18 s** (16 s) | `logs_2026-08-02_00-59-33` | This is Phase 3 task 3.1's exact mechanism, proven end to end. |

`tools/compile-check.sh` was run before **every** launch and exited 0 each time (one intermediate failure, fixed
immediately: `PrintFormat` on `SCR_AutotestCaseBase` accepts only **three** string params — the 4th positional is
`LogLevel`).

---

## 1.2 — World-free tier: **FEASIBLE, no caveats**

```c
class OVT_TEST_ProbeWorldFreeSuite : OVT_TEST_SuiteBase
{
	override ResourceName GetWorldFile()
	{
		return ResourceName.Empty;
	}
}
```

- **Compiles.** `compile-check: OK (5975 files, Game module, 6s)`. The cross-script-module sealing hazard
  (`SCR_Hack_AutotestSuiteBase`, `#ifdef MODULE_AUTOTEST`) does **not** block the override — R8 did not materialise.
- **Runs, and skips the world.** `grep -c "Requesting scenario change" console.log` -> **0**.
- **Side effect not in the plan: the harness runs ONCE, not twice.** The double harness startup documented in feature #2
  is caused by the world transition reloading scripts. With no transition there is one `CLI autotest suite:` line and one
  `Creating: SCR_TestRunner`.
- Verbatim (run `logs_2026-08-02_00-26-15`):

```
00:26:20.629 SCRIPT       : CLI autotest suite: OVT_TEST_ProbeWorldFreeSuite
00:26:20.630 SCRIPT       : Creating: SCR_TestRunner
00:26:22.297 SCRIPT       : 	PROBE-1.2: pure arithmetic 7+3 = 10
00:26:22.297 SCRIPT       : 	PROBE-1.2: GetGame().GetGameMode() non-null = false
00:26:22.297 SCRIPT       : SCR_TestRunner has finished running
```

```xml
<testsuites time="1.667488" timestamp="2026-08-01T14:26:20.629Z">
	<testsuite name="OVT_TEST_ProbeWorldFreeSuite" tests="1" time="0.000342" timestamp="2026-08-01T14:26:22.297Z">
		<testcase classname="OVT_TEST_ProbeWorldFreeSuite" name="OVT_TEST_Probe_WorldFree" time="0.000081" />
	</testsuite>
</testsuites>
```

**Consequence for Phase 5:** `OVT_TEST_LogicSuite` overrides `GetWorldFile()` -> `ResourceName.Empty`. Cost of a Tier A run
drops from ~17 s to **8 s**. Nothing world-, manager- or game-mode-dependent may appear in it — `GetGame().GetGameMode()`
is null there, so a stray manager call is a null dereference, not a silent pass.

---

## 1.3 — Campaign start: **WORKS, synchronously**

Both placements work: from a case Main step (run 1.3), and from a **suite `[Step(EStage.Setup)]` bool step** (run 1.3d,
the Phase 3 design).

Verbatim, case-step variant (`logs_2026-08-02_00-31-51`):

```
00:32:04.473 	PROBE-1.3: pre-start HasGameStarted=false IsInitialized=false
00:32:04.473 	PROBE-1.3: difficulty before=Normal presetCount=5
00:32:04.473 	PROBE-1.3: difficulty set to preset[0]='Easy' startingCash=500 baseThreat=0
00:32:04.473 	PROBE-1.3: GetStartGameContext() non-null = true
00:32:04.473 	PROBE-1.3: called GetStartGameContext().CloseLayout()
00:32:04.473 	PROBE-1.3: calling DoStartNewGame()
00:32:04.473 	PROBE-1.3: DoStartNewGame() returned after 0 ms
00:32:04.473 	PROBE-1.3: calling DoStartGame()
00:32:04.521 	PROBE-1.3: DoStartGame() returned after 49 ms
00:32:04.521 	PROBE-1.3: post-start (same frame) HasGameStarted=true IsInitialized=true
```

Verbatim, suite-Setup variant (`logs_2026-08-02_00-59-33`) — **this is the shape Phase 3 must implement**:

```
00:59:47 PROBE-1.3d: difficulty selected by name = 'Test World'
00:59:47 PROBE-1.3d: suite Setup calling DoStartNewGame() + DoStartGame()
00:59:47 PROBE-1.3d: suite Setup same-frame started=true initialized=true
00:59:47 PROBE-1.3d: suite Setup done after 1 polls, 71 ms
00:59:48 	OVT_TEST_Probe_CampaignAlreadyStarted: SUCCESS
```

Findings:

- **`DoStartNewGame()` costs 0 ms; `DoStartGame()` costs 49 ms.** `HasGameStarted()` and `IsInitialized()` are both
  `true` on return, **in the same frame**. The polling loop the plan describes terminates on its **first** poll (71 ms
  wall, one iteration). Polling is still correct to keep — it costs nothing and survives a future async change.
- **The start menu is not open when the suite Setup step runs.** In the final (third) world instance,
  `[Overthrow] Showing start menu for single player` fires ~4 ms *after* the suite Setup steps
  (`logs_2026-08-02_00-30-41`: probe steps at `00:30:54.888`, menu at `00:30:54.892`). So `m_Difficulty` is still the
  **prefab default `'Normal'`** at that moment — the plan's reasoning about the difficulty mismatch is correct, for a
  different reason than stated (the menu has not run *yet*, rather than having run with a different preset).
- **`CloseLayout()` is not required** (run 1.3b started the campaign without it and reached
  `IsInitialized=true`), but it **should still be called**: it is a no-op when the layout is not shown
  (`OVT_UIContext.CloseLayout()` begins `if(!m_wRoot) return;`), and any Setup step that polls for a few frames will
  race the menu handler that opens the layout milliseconds later.
- No errors are attributable to the start sequence itself. The errors that do appear are listed under
  "Bugs found (log only)".

### 1.3c — the difficulty preset list is NOT what the plan assumed

Verbatim (`logs_2026-08-02_00-55-22`):

```
PROBE-1.3c presets: count=5 | [0]='Easy' cash=500 res=200 | [1]='Normal' cash=100 res=500 | [2]='Hard' cash=100 res=1000
                    | [3]='Extreme' cash=0 res=1500 | [4]='Test World' cash=100000 res=200 || m_Difficulty='Normal'
```

`Prefabs/GameMode/OVT_OverthrowGameMode.et` declares 4 presets (Easy/Normal/Hard/Extreme). The test world layer
(`Worlds/MP/OVT_Campaign_Test_Layers/default.layer:33-42`) declares an `m_aDifficultyPresets` override containing
`Configs/Difficulty/Difficulty_TestWorld.conf`. **The layer's array APPENDS, it does not replace.** At runtime there are
**5** presets and `Difficulty_TestWorld.conf` ("Test World", cash 100000, resources 200) is at index **4**, not 0.

> **`m_Difficulty = m_aDifficultyPresets[0]` selects `'Easy'` (cash 500, resources 200, baseThreat 0)** — NOT the
> TestWorld preset. Phase 3 must select **by name** (`preset.name == "Test World"`), as run 1.3d does, or the campaign
> tiers will silently run on Easy and every economy anchor in the plan (cash 100000 / resources 200) will be wrong.

Observable consequence: with Easy, `OVT_OccupyingFactionManager.m_iResources` settles at 500; with Test World it is
1000. That difference is visible in the 1.4 samples below.

---

## 1.4 — Post-start settling budget

Measured from the frame in which `DoStartGame()` returned (`logs_2026-08-02_00-31-51`).

| Event | Observable | When | Evidence |
|---|---|---|---|
| `HasGameStarted()` / `IsInitialized()` | game mode flags | **same frame (0 ms)** | `post-start (same frame) HasGameStarted=true IsInitialized=true` |
| Base controllers initialised (`CallLater(InitBaseControllers, 0)`) | `GetBaseByIndex(0) != null` | first frame after start | `00:32:05.066 [Overthrow] Initialized base 0 at <55.331, 1, 128.241> with faction 3` / `00:32:05.067 [Overthrow] InitBaseControllers complete` |
| Shop inventory initialised (`CallLater(InitShopInventory, 0)`) | sum of `OVT_ShopComponent.m_aInventory.Count()` | **> 0 ms, <= 604 ms**; still **0** at t=0 | 1.3d sample: `t=0ms ... shops=5 shopStockEntries=0`; 1.3 sample: `t=604ms ... shops=5 shopStockEntries=286` |
| Occupying-faction initial resource distribution (`CallLater(..., 5000)`) | console marker | **~6 540 ms** after `DoStartGame()` returned | `00:32:11.062 [Overthrow.OccupyingFactionManager] Distributing 200 resources to Base` (start returned `00:32:04.521`) |
| Deployment evaluation (`CallLater(EvaluateDeployments, 10000)`) | a deployment is created | **~11 960 ms** | `00:32:16.476 [Overthrow] Creating deployment 'Town Patrol' for faction 3` / `00:32:16.477 [Overthrow] Created deployment 'Town Patrol' for faction 3 near Town` |

Two corrections to the plan's expectations:

1. **The 5 s and 10 s deferrals land ~1.5-2 s late in wall-clock terms** (6.5 s and 12.0 s), because the frame budget is
   still recovering from the world load — the first observable Main tick after `DoStartGame()` is **604 ms** later.
   Budget from the *observed* numbers, not from the `CallLater` constants.
2. **`EvaluateDeployments` is NOT blocked by "no players".** `OVT_DeploymentManager.EvaluateDeployments()` returns early
   when `GetPlayerManager().GetPlayerCount() == 0`, and the plan assumed that is the case in the autotest client. It is
   not: the client spawns a real local player (`[Overthrow] OVT_SpawnLogic.DoSpawn_S called for playerId: 1`), so
   deployments really are created ~12 s in. This is also what makes Bug #1 fire.

Steady-state sample at t=16 s (Easy difficulty):

```
PROBE-1.4 t=16001ms started=true initialized=true towns=1 shops=5 shopStockEntries=286 bases=1
          garrisonPrefabs=0 baseCtrl0=true ofResources=500 ofThreat=0
```

`garrisonPrefabs` stays **0** for the whole 16 s window — `OVT_BaseData.garrison` is not populated by the start sequence
in this world. **Do not assert on garrisons** in the campaign tier.

### `timeoutS` recommendations for Phases 4-5

| Assertion depends on | Poll budget | `timeoutS` |
|---|---|---|
| `IsInitialized()`, towns, bases, base controllers | <= 1 s | `30` |
| Shop inventory / economy post-start state | <= 2 s | `30` |
| Occupying-faction initial resource distribution | ~7 s | `45` |
| Deployments | ~12 s | `60` (out of scope for this feature) |

A campaign suite's Setup step needs no extra budget of its own: it completes in 71 ms.

---

## 1.5 — Stale singletons: **do NOT manifest on this build**

The experiment: a two-suite group run. Suite 1 (`OVT_TEST_ProbeInitSuite`) resolves `OVT_Global.GetTowns()` — which
populates `OVT_TownManagerComponent.s_Instance` — and stashes both the game mode and the manager in weak statics of the
probe class. Suite 2 (`OVT_TEST_SmokeSuite`) then runs its own `Setup_OpenWorld`, which **re-requests the same world**
(third `Requesting scenario change:` in the run), destroying and recreating the game mode inside the *same script VM*.
Sample #7 is taken in suite 2's Setup, after that transition.

Verbatim (`logs_2026-08-02_00-57-25`, `autotest.log`):

```
00:57:38 TestSuite #OVT_TEST_ProbeInitSuite started
00:57:40 PROBE-1.5 [#1 base-suite-Setup] gameMode=true gameModeSameAsPrevSample=false prevGameModeRefIsNULLnow=true liveTowns=true globalTowns=true globalEqualsLive=true globalSameAsPrevSample=false prevGlobalTownsRefIsNULLnow=true liveTownCount=1 globalTownCount=1
00:57:40 PROBE-1.5 [#2 derived-suite-Setup] gameMode=true gameModeSameAsPrevSample=true prevGameModeRefIsNULLnow=false liveTowns=true globalTowns=true globalEqualsLive=true globalSameAsPrevSample=true prevGlobalTownsRefIsNULLnow=false liveTownCount=1 globalTownCount=1
00:57:40 	PROBE-1.5 [#6 case-TearDown] gameMode=true gameModeSameAsPrevSample=true prevGameModeRefIsNULLnow=false liveTowns=true globalTowns=true globalEqualsLive=true globalSameAsPrevSample=true prevGlobalTownsRefIsNULLnow=false liveTownCount=1 globalTownCount=1
00:57:40 TestSuite #OVT_TEST_SmokeSuite started
00:57:41 PROBE-1.5 [#7 base-suite-Setup] gameMode=true gameModeSameAsPrevSample=false prevGameModeRefIsNULLnow=true liveTowns=true globalTowns=true globalEqualsLive=true globalSameAsPrevSample=false prevGlobalTownsRefIsNULLnow=true liveTownCount=1 globalTownCount=1
```

Reading of sample #7 (across the suite boundary and its world reload):

- `prevGameModeRefIsNULLnow=true` and `prevGlobalTownsRefIsNULLnow=true` — the weak statics that pointed at the previous
  world's objects **were nulled by the engine when those objects were destroyed**. They were not left dangling.
- `globalEqualsLive=true` — `OVT_Global.GetTowns()` therefore re-ran its `if (!s_Instance)` branch and re-resolved
  against the **live** game mode. `OVT_Global` and `FindComponent` agree.
- `globalEqualsLive=true` in **every** sample of every run (#1-#7, across four launches).

**Conclusion: the R3 / Decision 6 premise is wrong on 1.7.0.54.** `static OVT_TownManagerComponent s_Instance;` is a weak
reference; Enfusion nulls it on component destruction, so the "never invalidated singleton" self-heals and
`GetInstance()` re-resolves. A manager-resolution helper is **defensive, not mandatory** — worth keeping (it costs one
method and documents intent, and the pattern would break if a manager ever held a `ref`), but suites may use
`OVT_Global` directly without a flake risk, and no test should be written to *depend* on the stale case.

Honest limits of this result: it covers destruction-by-world-reload of a game-mode component, which is the only
mechanism in play in an autotest run. It says nothing about a manager caching a *different* live object.

---

## 1.6 — Base-class Setup step ordering: **exactly as the plan hoped**

Observed order in one suite's Setup stage (`logs_2026-08-02_00-30-41`, corroborated in every later run):

| Order | Step | Evidence |
|---|---|---|
| 1 | `SCR_AutotestSuiteBase.Setup_PrintPrelude` | `TestSuite #OVT_TEST_ProbeInitSuite started` |
| 2 | `SCR_AutotestSuiteBase.Setup_OpenWorld` | `Requesting scenario change: {D87EF7EED4210569}Worlds/MP/OVT_Campaign_Test.ent` |
| 3 | `SCR_AutotestSuiteBase.Setup_AwaitWorld` | (implicit — see 4) |
| 4 | `SCR_AutotestSuiteBase.Setup_CloseMenus` | (private, not logged) |
| 5 | **`OVT_TEST_SuiteBase` Setup step** | `PROBE-1.6 ORDER: [2] ... transitionInProgress=false gameMode=true` |
| 6 | **derived suite Setup step** | `PROBE-1.6 ORDER: [3] DERIVED suite Setup step` |
| 7 | case Setup step | `PROBE-1.6 ORDER: [4] CASE Setup step` |
| 8 | case Main step | `PROBE-1.6 ORDER: [5] CASE Main step first tick` |

Verbatim:

```
00:30:53 TestSuite #OVT_TEST_ProbeInitSuite started
00:30:53 Requesting scenario change:
	{D87EF7EED4210569}Worlds/MP/OVT_Campaign_Test.ent
	{1C60D2EDA2B468B8}Configs/Systems/BaseGameModeSystems.conf
00:30:54 PROBE-1.6 ORDER: [2] OVT_TEST_SuiteBase Setup step (PROBE_Setup_Base)
00:30:54 PROBE-1.6 ORDER: [2] transitionInProgress=false gameMode=true
00:30:54 PROBE-1.6 ORDER: [3] DERIVED suite Setup step (OVT_TEST_ProbeInitSuite.PROBE_Setup_Derived)
```

`transitionInProgress=false` and `gameMode=true` at step 5 prove the world is fully loaded before any Overthrow-owned
Setup step runs. Base-class steps run **before** derived-suite steps (methods execute in definition order, base first),
and both run **after** all inherited BI steps. **Phase 3 can put the campaign-start step directly on
`OVT_TEST_SuiteBase` with no ordering workaround** — proven end to end by run 1.3d.

A failing Setup step also behaves as documented: `SetResult(SCR_AutotestResult.AsFailure(...))` in a Setup step
terminates the suite and skips TearDown (framework doc `TestingFramework.c` section "Failure unwind"; not separately
exercised here, so Phase 3's S7 check should still be run).

---

## 1.7 — Persistence reality check: **ladder rung L3, worse than the plan assumed**

Verbatim (`logs_2026-08-02_00-31-51`), campaign started, `IsInitialized()` true:

```
00:32:05.077 SCRIPT       : 	PROBE-1.7: GetPersistence() non-null = true
00:32:05.077 SCRIPT       : 	PROBE-1.7: HasSaveGame() before SaveGame() = false
00:32:05.077 SCRIPT       : 	PROBE-1.7: calling SaveGame() ...
00:32:05.077 SCRIPT       : 	PROBE-1.7: SaveGame() returned
00:32:05.077 SCRIPT       : 	PROBE-1.7: HasSaveGame() after SaveGame() = false
00:32:05.077 SCRIPT       : 	PROBE-1.7: calling AutoSave() ...
00:32:05.077 SCRIPT       : 	PROBE-1.7: AutoSave() returned
```

- **`SaveGame()` and `AutoSave()` return completely silently.** Not even the `TODO(vanilla-persistence)` WARNING prints.
  The reason is one level deeper than the plan recorded: both methods guard their warning with `if (m_PersistenceSystem)`,
  and `m_PersistenceSystem` is **null**, because `OnPostInit` failed:

```
00:32:04.065 SCRIPT    (E): [Overthrow] Failed to get SCR_PersistenceSystem instance!
```

  followed a fraction of a second later by the reassuring-but-meaningless
  `00:32:04.189 SCRIPT : [Overthrow] Initializing Persistence`.
- **`HasSaveGame()` is `false` before and after** (it is a hardcoded `return false`).
- **Nothing is written to disk anywhere under the profile.** After the campaign run the entire `OverthrowCI` profile
  tree contains no `.db`, no `Overthrow` save directory and no new file at all:

```
OverthrowCI/addons/saves            (empty, unchanged, created at profile creation)
OverthrowCI/profile/.save/...       (engine + game settings only)
OverthrowCI/profile/DbgUIState.bin
OverthrowCI/profile/Overthrow_Config.json
OverthrowCI/profile/resourceDatabase.rdb
```

- **EPF is loaded but never initialises.** `console.log` shows EPF's addon being mounted
  (`FileSystem: Adding package '...EnfusionPersistenceFramework_5D6EBC81EB1842EF/'`, its `resourceDatabase.rdb` cached)
  and then **zero** EPF script activity: no `EPF_PersistenceManager`, no DB connection, no SETUP state, no autosave tick,
  no world-load restore. `grep -i "EPF\|persistence" console.log` returns only addon-mount lines, Overthrow's own two
  messages, and the error above.

> **Ladder rung: L3.** There is no save path in either system. `OVT_TEST_PersistenceSuite` ships green with
> same-session write->read-back coverage through the public manager API; `OVT_TEST_PersistenceRoundTripSuite` ships
> quarantined and red, and its flip to exit 0 is `vanilla-persistence`'s acceptance criterion. Phase 4's task 4.5
> (diagnostic failure) is **more important than the plan assumed**: because the trigger is *silent*, a naive round-trip
> test would fail with a confusing assertion mismatch rather than "saving is not implemented". The suite must assert the
> capability explicitly (e.g. `HasSaveGame()` after `SaveGame()`) and name it in the failure text.

## 1.9 — In-session reload survivability: **NOT APPLICABLE**

Task 1.9 is gated on 1.7 finding a save path. It did not, so no reload experiment was run and L1-vs-L2 is moot.

One relevant observation collected for free: **the script VM does survive an in-session world transition.** In a
group run, suite 2's `Setup_OpenWorld` re-requests the world; the harness instance, its statics and the running suite
all continue afterwards (1.5's sample #7 is emitted by the same VM that emitted samples #1-#6). So if a save path ever
appears, **L1 is the likely rung** — but that must be re-tested from inside a Main step before it is relied upon,
because the surviving transitions observed here were all initiated by the framework's own Setup step.

---

## 1.8 — Save-directory determination: **the plan's expected path is WRONG**

Determined by execution, not inference: the probe wrote a marker file through each engine filesystem prefix and the
files were then located on disk.

```
00:30:54.890 SCRIPT : 	PROBE-1.8 profile=WROTE(exists=true) saves=OPEN_FAILED logs=WROTE(exists=true) mkdir($profile:.dbprobe)=true
```

On disk afterwards:

```
/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/OverthrowCI/profile/ovt_probe_marker.txt
/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/OverthrowCI/profile/.dbprobe/
/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/OverthrowCI/logs/logs_2026-08-02_00-30-41/ovt_probe_marker.txt
```

| Prefix | Resolves to (for `-profile OverthrowCI`) |
|---|---|
| `$profile:` | `<My Games>/OverthrowCI/`**`profile`**`/` |
| `$logs:` | `<My Games>/OverthrowCI/logs/logs_<timestamp>/` (the current run's dir) |
| `$saves:` | **not writable from script** — `FileIO.OpenFile` returned null. (`<My Games>/<profile>/addons/saves` exists in every profile root and is empty everywhere, including the user's retail `ArmaReforger` profile.) |

**EPF (`EDF_FileDbDriverBase.DB_BASE_DIR = "$profile:/.db"`) would therefore put the `OverthrowCI` save DB at:**

```
/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/OverthrowCI/profile/.db/Overthrow
```

Windows form: `C:\Users\Aaron Static\OneDrive\Documents\My Games\OverthrowCI\profile\.db\Overthrow`

**This is one directory level deeper than `implementation.md` task 1.8 predicted** (`<My Games>/OverthrowCI/.db/Overthrow`).
`ovt_profile_dir <name>` returns the profile **root** (`<My Games>/<name>`); `$profile:` is `<root>/profile`. The shape
matches `.scripts/reset_save.sh`'s existing Workbench default exactly:

```
# .scripts/reset_save.sh:5 (existing, unchanged)
SAVE_PATH="${OVERTHROW_SAVE_DIR:-/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/ArmaReforgerWorkbench/profile/.db/Overthrow}"
```

so Phase 2's `--profile <name>` must resolve **`$(ovt_profile_dir <name>)/profile/.db/Overthrow`**, not
`$(ovt_profile_dir <name>)/.db/Overthrow`.

Confirmed by inspection (read-only, nothing was modified or deleted): the user's real save DB exists at
`<My Games>/ArmaReforgerWorkbench/profile/.db/Overthrow`, and a second one at
`<My Games>/ArmaReforgerWorkbench/profile_old/.db`. **The `OverthrowCI` profile has no `.db` directory at all** — the
automated runs have never produced a save, consistent with 1.7.

**What vanilla persistence would use instead.** Vanilla's `SaveGameManager`
(`scripts/Game/generated/Plugins/Persistence/SaveGame/SaveGameManager.c`) is an engine-side `sealed class` whose storage
location is not script-visible: it exposes `RequestSavePoint` / `GetSaves` / `Load` / `Delete` / `Purge` and named,
numbered save points, but no path. It is referenced **nowhere** in Overthrow. The only script-visible candidate location
is `$saves:` (`<My Games>/<profile>/addons/saves`), which `FileIO` documents as a deletable/copyable location but which
is **not openable for writing from script** and is empty on every profile on this machine — including the user's retail
profile, which has real playthroughs. Conclusion for the migration record: **when `vanilla-persistence` lands, the
`.scripts/*` tools' `.db/Overthrow` assumption will be obsolete, and the new location must be determined empirically at
that time** (it is very likely engine-managed and not a `$profile:/.db`-shaped directory at all).

---

## 1.10 — Group config: **works, and requires `[BaseContainerProps()]` on every member suite**

Files hand-authored per feature #2's Decision 4 (throwaway; deleted in 1.12):

`Configs/Tests/OVT_TestGroup_Probe.conf`
```
SCR_AutotestGroup {
 m_aSuites {
  OVT_TEST_SmokeSuite "{6A6E04112852459C}" {
  }
  OVT_TEST_ProbeInitSuite "{6A6E041217F96379}" {
  }
 }
}
```

`Configs/Tests/OVT_TestGroup_Probe.conf.meta` — copied from `Configs/Deployment/overthrowDeployments.conf.meta`
(all six `CONFResourceClass` blocks incl. `PS5`), with
`Name "{6A6E04103558938B}Configs/Tests/OVT_TestGroup_Probe.conf"`.

**GUID `6A6E04103558938B`** (16 uppercase hex, timestamp-prefixed). Collision-checked and **unused** in:
this repo, `/mnt/n/Projects/Arma 4/ArmaReforger`, `/mnt/n/Projects/Arma 4/EnfusionPersistenceFramework`,
`/mnt/n/Projects/Arma 4/EnfusionDatabaseFramework`, `<My Games>/ArmaReforgerWorkbench/addons`, and the 56 GB game
install `.../Arma Reforger/addons` (`rg -l --binary` -> exit 1, no match). The two per-entry instance GUIDs
(`6A6E04112852459C`, `6A6E041217F96379`) were generated the same way. Phase 6 must generate **new** GUIDs for the real
group configs and repeat this check.

### The mechanism, in order

1. **Hand-authored `.conf` + `.meta` register correctly with no Workbench round-trip.** The engine resolved the GUID
   on the first attempt: `00:54:06.115 SCRIPT : CLI autotest config: SCR_AutotestGroup<0x000002A215765270>`, and
   **never** `Invalid resource path for autotest config`. Decision 4 steps 1-4 are proven; step 6 (ask the user to use
   the GUI) was never needed.
2. **Without `[BaseContainerProps()]` on the concrete suite class the group loads but instantiates nothing:**

```
00:54:06.115   DEFAULT   (E): Unknown class 'OVT_TEST_SmokeSuite' at offset 76(0x4c)
00:54:06.115 SCRIPT       : CLI autotest config: SCR_AutotestGroup<0x000002A215765270>
00:54:06.115 SCRIPT    (D): (SCR_AutotestHarness) Tests to run:
00:54:06.115 SCRIPT    (D): 	OVT_TEST_SmokeSuite: 0
```

   `junit.xml` is an empty `<testsuites>` element and `run-tests.sh` correctly reports **exit 2**
   (`INDETERMINATE: junit.xml contains zero <testcase> elements`). **Inheriting `[BaseContainerProps(category: "Autotest")]`
   from `SCR_AutotestSuiteBase` is NOT sufficient** — Decision 4 step 5 is mandatory, not a contingency.
3. **With `[BaseContainerProps()]` added to the concrete suite class it works immediately** — `OVT_TEST_SmokeSuite: 1`
   in the listing, exit 0, one `<testcase>`.
4. **Two suites both run:**

```xml
<testsuites time="5.274337" timestamp="2026-08-01T14:55:32.516Z">
	<testsuite name="OVT_TEST_ProbeInitSuite" tests="1" time="2.316106" timestamp="2026-08-01T14:55:34.115Z">
		<testcase classname="OVT_TEST_ProbeInitSuite" name="OVT_TEST_Probe_Init" time="0.850189" />
	</testsuite>
	<testsuite name="OVT_TEST_SmokeSuite" tests="1" time="1.357941" timestamp="2026-08-01T14:55:36.431Z">
		<testcase classname="OVT_TEST_SmokeSuite" name="OVT_TEST_Smoke_HarnessRuns" time="0.123406" />
	</testsuite>
</testsuites>
```

### Group-run mechanics worth knowing before Phase 6

- **Execution order is NOT the config's `m_aSuites` order.** `OVT_TEST_SmokeSuite` is listed first in the `.conf`, but
  `OVT_TEST_ProbeInitSuite` ran first. Order follows the harness's own suite registration (the `Tests to run:` listing
  order, which is alphabetical by class name). **No group may depend on suite order.**
- **Every enabled suite pays its own world transition.** 1 suite -> 2 `Requesting scenario change:`;
  2 suites -> 3. `Setup_OpenWorld` does not detect "already in the target world" (feature #2 finding, re-confirmed).
- **Measured cost of one extra suite in a group: ~ +1 s wall, +1.2 s of `junit.xml` `time=`** (16 s -> 17 s tool time;
  `<testsuites time=>` 4.048 -> 5.274). Cheap. The world-transition cost is much lower for a *second* load of an
  already-resident world (`LoadEntities` ~150 ms) than for the first.
- **Non-member suites are properly disabled** — the harness listing showed `OVT_TEST_MetaSuite: 0` and every probe suite
  `: 0` in every group run, and none of their cases appeared in `junit.xml`. Feature #2's no-leak result holds for a
  populated group, so Phase 6's Q8 quarantine requirement is satisfiable.
- `OVT_TEST_SuiteBase` and `SCR_AutotestSuiteBase` continue to appear in the listing as disabled, empty suites.

---

## 1.11 — Timing baseline

| Shape | Target used | Tool time (`run-tests: OK (..., Ns)`) | Wall time | World transitions |
|---|---|---|---|---|
| **Tier A** (world-free suite) | `OVT_TEST_ProbeWorldFreeSuite` | **8 s** | 11 s | **0** |
| **Tier B** (world, managers, no campaign) | `OVT_TEST_ProbeInitSuite` | **17 s** | 18 s | 2 |
| **Tier C** (campaign started in suite Setup, short) | `OVT_TEST_ProbeCampaignSetupSuite` | **16 s** | 18 s | 2 |
| **Tier C/D** (campaign started + 16 s observation) | `OVT_TEST_ProbeCampaignSuite` | **30 s** | 33 s | 2 |
| **Group, 1 suite** | `{6A6E04103558938B}` | **16 s** | 18 s | 2 |
| **Group, 2 suites** | `{6A6E04103558938B}` | **16-17 s** | 19-20 s | 3 |
| Inherited reference (feature #2, smoke) | `OVT_TEST_SmokeSuite` | 14-22 s | — | 2 |

Readings for Phase 6 and feature #4:

- **Client boot + first world load dominates** (~14 s); everything after is cheap. A group of 4 suites should land around
  **19-21 s**, comfortably inside `run-tests.sh`'s 300 s default. `OVERTHROW_TEST_TIMEOUT` guidance is not urgent.
- **Campaign-tier cost is driven by what you wait for, not by starting the campaign.** Starting it costs 71 ms; waiting
  16 s for deployments costs 16 s. Keep campaign assertions on the <=2 s side of the settling table wherever possible.
- **The Fast group (Logic + Init) should cost ~17-18 s**; the All group ~20-22 s. The fast/slow split buys ~4 s, not a
  category change — its real value is scope (fewer things that can be red on a push), not wall time.
- Determinism across the four repeat group runs: identical exit codes, identical case counts, wall times within 2 s.

---

## Differs from assumptions

Contradictions and corrections to `implementation.md`'s ground truth, Decisions and Risks. Ordered by how much
downstream work they change.

1. **`m_aDifficultyPresets[0]` is `'Easy'`, not the TestWorld preset.** (Ground truth row 3, Phase 1 task 1.3, Phase 3
   task 3.1.) The world layer's array override **appends**: 5 presets at runtime, `'Test World'` at index **4**.
   Selecting index 0 gives cash 500 / resources 200 / baseThreat 0 and `m_iResources` 500; selecting `'Test World'`
   by name gives cash 100000 / resources 200 and `m_iResources` 1000. **Phase 3's Setup step must select by name.**
2. **The `OverthrowCI` save-DB path is `<My Games>/OverthrowCI/profile/.db/Overthrow`**, one level deeper than
   task 1.8 predicted. `$profile:` = `<profile root>/profile`. Phase 2 task 2.4 must append `/profile/.db/Overthrow`
   to `ovt_profile_dir`'s output.
3. **Stale singletons do not manifest.** (Ground truth row 5, Decision 6, R3.) The `static s_Instance` weak references
   *are* nulled by the engine when the component is destroyed, so `GetInstance()` re-resolves after a world reload.
   `OVT_Global` agreed with `FindComponent` in 100 % of samples, including across a suite boundary with a world reload
   between them. The resolution helper is worth keeping as documentation of intent, but it is **not** load-bearing and
   R3 should be downgraded.
4. **`SaveGame()` does not print a warning — it does nothing at all, silently.** (Ground truth row 8.) `m_PersistenceSystem`
   is null, so even the `TODO(vanilla-persistence)` WARNING is skipped. This makes Phase 4's diagnostic-failure task
   (4.5) load-bearing rather than a nicety.
5. **`[BaseContainerProps()]` on each concrete suite class is mandatory for group membership**, not a contingency.
   (Decision 4 step 5, R5.) Without it the group loads and runs nothing, and the symptom is `Unknown class '<Suite>'`
   plus a `run-tests.sh` exit 2 — which is easy to misread as a broken GUID.
6. **A world-free suite also halves the harness startup**, not just the world load: with no scenario change the harness
   runs **once**, not twice. Tier A costs 8 s, not the ~15 s the plan budgeted.
7. **The 5 s / 10 s post-start deferrals land at ~6.5 s / ~12.0 s** in wall-clock terms, and the first observable Main
   tick after `DoStartGame()` is **604 ms** later. Budget from observation, not from the `CallLater` constants.
8. **`EvaluateDeployments` is not blocked by an empty player list** — the autotest client spawns a real local player
   (`playerId: 1`), so deployments are created ~12 s after start. The plan assumed no player exists.
9. **`GetStartGameContext().CloseLayout()` is not required** for the start to succeed, but the start menu opens a few
   milliseconds *after* the suite Setup steps run, so any multi-frame Setup step will race it. Keep the call.
10. **`$saves:` is not writable from script** and is empty on every profile on this machine, so it is not a usable
    fixture location; and vanilla `SaveGameManager` exposes no path at all. The plan's R12 ("vanilla stores saves
    somewhere else entirely") is confirmed as an open question that only the migration can answer.
11. Everything else held: `GetWorldFile()` overriding is not sealed (R8 did not materialise); the harness runs twice and
    loads the world three times for world-bearing suites; `<testsuite>` still has no `failures=` attribute; hand-authored
    `.conf`/`.meta` GUIDs register with no GUI round-trip; the test world has exactly 1 town and 1 base;
    `DoStartNewGame()`/`DoStartGame()` are unguarded public methods callable with no player and no RPC.

---

## Bugs found (log only)

Per Decision 10 these are **recorded, not fixed**. Line references are to this repo on `vanilla-persistence`.

1. **`OVT_BaseUpgradeComposition.FillAmmoboxes` null-dereferences the inventory manager — 62 VM exceptions per campaign
   start.** Fires deterministically ~6.5 s after the campaign starts, when `DistributeInitialResources` spends the
   occupying faction's starting resources. Reproduced identically in two independent runs
   (`logs_2026-08-02_00-31-51`, `logs_2026-08-02_00-58-03`), 62 exceptions each. Verbatim:

```
00:32:11.062 SCRIPT       : [Overthrow.OccupyingFactionManager] Distributing 200 resources to Base
00:32:11.083 SCRIPT    (E): Virtual Machine Exception

Reason: NULL pointer to instance

Class:      'SCR_InventoryStorageManagerComponent'
Function: 'OnItemAdded'
Stack trace:
Scripts/Game/Inventory/SCR_InventoryStorageManagerComponent.c:539 Function OnItemAdded
Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/OVT_BaseUpgradeComposition.c:73 Function FillAmmoboxes
Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/OVT_BaseUpgradeComposition.c:43 Function Spend
Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c:299 Function SpendResources
Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c:500 Function DistributeInitialResources
Scripts/Game/game.c:872 Function OnUpdate
```

   Impact on this feature: none on the verdict (VM exceptions do not fail a test), but a campaign-tier suite that lives
   past ~7 s will emit 62 script errors into `console.log`. Feature #4 must not treat `console.log` error counts as a
   CI signal until this is fixed.

2. **`SaveGame()` / `AutoSave()` fail completely silently.** `m_PersistenceSystem` is null (see 1.7), so the guarded
   `TODO(vanilla-persistence)` WARNING never prints and the caller gets no indication of failure whatsoever. Combined
   with `OVT_MainMenuContext` showing `#OVT-Saved` unconditionally
   (`OVT_MainMenuContext.c:262-271`), **a player pressing Save today is told the game saved, and nothing anywhere logs
   that it did not.** This is the highest-severity item in this list.

3. **`[Overthrow] Failed to get SCR_PersistenceSystem instance!`** fires once per test-world load during
   `OVT_PersistenceManagerComponent.OnPostInit`, and is immediately followed by
   `[Overthrow] Initializing Persistence` — a success message printed on the failure path's sibling. Known from
   feature #2; the follow-up message is newly noted here as actively misleading.

4. **`RandomGenerator.RandInt: invalid parameters min = 0 max = 0`** — one `SCRIPT (E)` per campaign start, emitted
   3 ms after `[Overthrow] Starting Deployment` (`00:32:04.524`, immediately after `DoStartGame()`'s deployment block).

5. **`[Overthrow] Matching item price config not found for {6113990D163E5249}.../BallisticTable_US.et`** and the `_USSR`
   equivalent — two per world load, in every run.

6. **Base-game prefab config error surfaced by Overthrow's base upgrades:**
   `ENTITY (E): Destructible SCR_DestructibleEntityClass doesn't have a initial and final phase!` and
   `DEFAULT (E): SCR_DestructionMultiPhaseComponent can't be attached to a destructible entity of type SCR_DestructibleEntityClass`
   for `{BE7B5BAFE1A16A50}Prefabs/Props/Military/Fortification/BarbedTape_Coil.et`, spawned during
   `DistributeInitialResources`.

7. **The test world layer's `m_aDifficultyPresets` override appends instead of replacing** (see 1.3c). Whether this is
   an Enfusion semantic or an authoring mistake, the effect is that the "test world" difficulty is *not* the one a
   naive `[0]` selection picks. Recorded as a content trap rather than a code bug.

8. **Navmesh does not load in the test world** (`PATHFINDING(E): Failed to load Navmesh from file!`) — inherited from
   feature #2, unchanged.

---

## Probe artefacts (all deleted in task 1.12)

| Artefact | Purpose |
|---|---|
| `Scripts/Game/Tests/TestSuites/Probe/OVT_TEST_ProbeDiag.c` | string-building diagnostics (samples, filesystem prefixes, preset list, settling state) |
| `Scripts/Game/Tests/TestSuites/Probe/OVT_TEST_ProbeWorldFreeSuite.c` | 1.2 |
| `Scripts/Game/Tests/TestSuites/Probe/OVT_TEST_ProbeInitSuite.c` | 1.5, 1.6, 1.8, 1.11-B |
| `Scripts/Game/Tests/TestSuites/Probe/OVT_TEST_ProbeCampaignSuite.c` | 1.3, 1.4, 1.7, 1.11-C |
| `Scripts/Game/Tests/TestSuites/Probe/OVT_TEST_ProbeCampaignNoCloseSuite.c` | 1.3 control (no `CloseLayout()`) |
| `Scripts/Game/Tests/TestSuites/Probe/OVT_TEST_ProbeCampaignSetupSuite.c` | 1.3d — Phase 3's exact mechanism |
| `Configs/Tests/OVT_TestGroup_Probe.conf(+.meta)` | 1.10 |
| temporary `[Step(EStage.Setup)]` in `OVT_TEST_SuiteBase.c` | 1.6, 1.5 |
| temporary `[BaseContainerProps()]` on `OVT_TEST_SmokeSuite` | 1.10 |
| `<My Games>/OverthrowCI/profile/ovt_probe_marker.txt`, `.dbprobe/` | 1.8 (deleted after inspection) |

Nothing under `tools/`, `.scripts/`, `Scripts/Game/` outside `Tests/`, or the Workbench profile was modified at any
point. `.scripts/reset_save.sh`, `backup_save.sh` and `activate_save.sh` were **never executed** in this phase — task
1.8 was directory inspection only.

## Run index (log directories under `My Games/OverthrowCI/logs/`)

| Experiment | Log dir |
|---|---|
| 1.2 world-free | `logs_2026-08-02_00-26-15` |
| 1.5 / 1.6 / 1.8 / 1.11-B | `logs_2026-08-02_00-30-41` |
| 1.3 / 1.4 / 1.7 / 1.11-C | `logs_2026-08-02_00-31-51` |
| 1.10a group, no `[BaseContainerProps()]` (exit 2) | `logs_2026-08-02_00-54-01` |
| 1.10b group, 1 suite | `logs_2026-08-02_00-54-49` |
| 1.10c group, 2 suites | `logs_2026-08-02_00-55-22` |
| 1.5b group, 2 suites + base sample | `logs_2026-08-02_00-56-26` |
| 1.5c group, 2 suites + null discrimination | `logs_2026-08-02_00-57-25` |
| 1.3b campaign without `CloseLayout()` | `logs_2026-08-02_00-58-03` |
| 1.3d campaign from suite Setup step | `logs_2026-08-02_00-59-33` |

*(Interleaved `logs_2026-08-02_00-54-35`, `_00-56-19`, `_00-57-18`, `_00-59-19` are `tools/compile-check.sh` runs — the
Workbench shares the `OverthrowCI` profile root.)*

---

# Phase 2: save-script verification matrix

**Date:** 2026-08-02
**Scope:** `.scripts/reset_save.sh`, `.scripts/backup_save.sh`, `.scripts/activate_save.sh` (tasks 2.1-2.7). Bash only —
no game launch, no compile.
**Safety:** every destructive scenario ran against a throwaway directory (`/tmp/ovt-save-probe/.db/Overthrow`) or a fake
`My Games` (`/tmp/ovt-fake-mygames`, via `OVERTHROW_MYGAMES_DIR`). The user's real save
(`<My Games>/ArmaReforgerWorkbench/profile/.db/Overthrow`) was **never a target** and is untouched (its `Overthrow/`
directory still carries its 2025-11-26 mtime). The real `OverthrowCI` profile was only ever **printed**, never deleted
into — see P2. Three `ovtprobe_*` archives were created in `.saves/` during the run and deleted afterwards; the six
`testworld_*` archives are byte-identical before and after (`ls -1 .saves/` diff is empty).

## W1-W6 matrix (all run, all observed)

| # | Scenario | Command | Exit | Observed |
|---|---|---|---|---|
| W1 | reset on **missing** dir | `OVERTHROW_SAVE_DIR=$PROBE reset_save.sh` | **0** | `Resolved save directory: …` / `Save directory not found: …` / `Nothing to delete.` / `Done.` |
| W2 | reset on **present** dir | `OVERTHROW_SAVE_DIR=$PROBE reset_save.sh` | **0** | resolved path printed, then `Deleting save directory: …` / `Save data deleted successfully!`; `Overthrow/` gone, parent `.db/` left in place |
| W3a | reset, **empty** `OVERTHROW_SAVE_DIR` | `OVERTHROW_SAVE_DIR= reset_save.sh` | **1** | `REFUSING to delete: the save path is empty or '/'` / `Nothing was deleted.` — probe still present afterwards |
| W3b | reset, path `/` | `OVERTHROW_SAVE_DIR=/ reset_save.sh` | **1** | same refusal, nothing deleted |
| W3c | reset, non-save-shaped path | `OVERTHROW_SAVE_DIR=/tmp/definitely-not-a-save reset_save.sh` | **1** | `REFUSING to delete: '/tmp/definitely-not-a-save'` / `That is not a save DB path (expected an absolute path ending in .db/Overthrow).` |
| W3d | reset, **relative** `.db/Overthrow` path | `OVERTHROW_SAVE_DIR=tmp/ovt/.db/Overthrow reset_save.sh` | **1** | refused — the guard requires an **absolute** path, so a `cd` in the wrong place cannot make a relative target resolve |
| W3e | reset, trailing slash | `OVERTHROW_SAVE_DIR=$PROBE/ reset_save.sh` | **0** | accepted; trailing slashes are stripped before the shape test, then deleted normally |
| W4a | backup **with** argument | `OVERTHROW_SAVE_DIR=$PROBE backup_save.sh ovtprobe_matrix_SP </dev/null` | **0** | no prompt, no suggestion line; `.saves/ovtprobe_matrix_SP_<TS>.tar.gz` created. stdin was `/dev/null`, which proves nothing is read |
| W4b | backup **without** argument | `echo "ovtprobe interactive SP" \| … backup_save.sh` | **0** | suggestion line + prompt exactly as before; name sanitised to `ovtprobe_interactive_SP` (spaces → `_`) |
| W4c | backup, missing save dir | `OVERTHROW_SAVE_DIR=/tmp/ovt-nope/… backup_save.sh ovtprobe_never` | **1** | `Save directory not found: …` (unchanged message) |
| W4d | backup, empty `OVERTHROW_SAVE_DIR` | `OVERTHROW_SAVE_DIR= backup_save.sh ovtprobe_never` | **1** | `OVERTHROW_SAVE_DIR is set but empty - refusing to guess a save directory` |
| W5a | activate **by name** (substring, newest) | `… activate_save.sh ovtprobe_matrix_SP` | **0** | selected `ovtprobe_matrix_SP_<TS>.tar.gz`, reset ran, archive extracted; nested files restored |
| W5b | activate by exact **filename** | `… activate_save.sh ovtprobe_matrix_SP_<TS>.tar.gz` | **0** | same, resolved inside `.saves/` |
| W5c | activate by exact **path** | `… activate_save.sh /…/.saves/ovtprobe_matrix_SP_<TS>.tar.gz` | **0** | same, absolute path accepted |
| W5d | activate, **unmatched** name | `… activate_save.sh no_such_save_xyz` | **1** | `No backup matching 'no_such_save_xyz' found in …` then the full numbered `Available save backups:` list; the save dir was **not** reset |
| W5e | activate, no argument, `1` typed | `echo 1 \| … activate_save.sh` | **0** | numbered menu, selection, reset, extract — byte-identical to the original except reset's new resolved-path line |
| W5f | activate, no argument, `q` typed | `echo q \| … activate_save.sh` | **0** | `Cancelled` |
| W5g | activate, no argument, `999` / `abc` typed | `echo 999 \| … activate_save.sh` | **1** | `Invalid selection` |
| W6 | every case above | — | — | ran against `/tmp/ovt-save-probe/.db/Overthrow` or `/tmp/ovt-fake-mygames`, never the user's save |

## `--profile` resolution (task 2.4)

| # | Scenario | Command | Exit | Observed |
|---|---|---|---|---|
| P1 | reset `--profile` against a **fake** `My Games` | `OVERTHROW_MYGAMES_DIR=/tmp/ovt-fake-mygames reset_save.sh --profile FakeCI` | **0** | `Resolved save directory: /tmp/ovt-fake-mygames/FakeCI/`**`profile`**`/.db/Overthrow` — deleted; the `profile/` level of finding 1.8 is reproduced exactly |
| P1b | backup `--profile` | `… backup_save.sh --profile FakeCI ovtprobe_fakeci_SP` | **0** | archived the fake profile's save |
| P1c | activate `--profile` | `… activate_save.sh --profile FakeCI ovtprobe_fakeci_SP` | **0** | the child `reset_save.sh` received the resolved path (`Resolved save directory: /tmp/ovt-fake-mygames/FakeCI/profile/.db/Overthrow`), then extracted there |
| P2 | **real** profile, print-only | `backup_save.sh --profile OverthrowCI ovtprobe_never` | **1** | `Save directory not found: /mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/OverthrowCI/profile/.db/Overthrow` — the exact path from finding 1.8. `backup_save.sh` is read-only, so this observes the real resolution without any risk |
| P3 | `OVERTHROW_SAVE_DIR` beats `--profile` | `OVERTHROW_SAVE_DIR=$PROBE OVERTHROW_MYGAMES_DIR=… reset_save.sh --profile FakeCI` | **0** | `OVERTHROW_SAVE_DIR is set - ignoring --profile FakeCI`; the probe was deleted and the FakeCI save was left intact |
| P4 | unresolvable `My Games` | `OVERTHROW_MYGAMES_DIR=/nonexistent/mg reset_save.sh --profile FakeCI` | **2** | `ovt: ERROR: OVERTHROW_MYGAMES_DIR is set but not a directory: '/nonexistent/mg'` (message from `common.sh`, exit code passed through) |
| P5 | `--profile` with no name | `reset_save.sh --profile` | **1** | `--profile requires a profile name` + usage |
| U1 | unknown argument | `reset_save.sh --wat` | **1** | `Unknown argument: --wat` + usage |
| U2-U4 | `-h` on all three | — | **0** | one-line usage |
| U5 | too many positionals | `backup_save.sh a b` | **1** | `Too many arguments: b` + usage |

## Interactive parity (task 2.7)

Method: the pre-change scripts and the post-change scripts were placed in two identical fake repos (`/tmp/ovt-parity/old`
and `/new`, each with its own `.saves/` holding the same two archives), fed **identical stdin**, and their output
diffed with timestamps and repo paths normalised.

| Interactive invocation | Result |
|---|---|
| `reset_save.sh` (no args) | **one added line**: `Resolved save directory: <path>` — required by task 2.3. Everything else identical |
| `backup_save.sh` (no args, name typed) | **byte-identical** |
| `activate_save.sh` (no args, `1` typed) | identical apart from the same one added line from the nested `reset_save.sh` |
| `activate_save.sh` (`q`) | **byte-identical** (`Cancelled`, exit 0) |
| `activate_save.sh` (`999`, `abc`) | **byte-identical** (`Invalid selection`, exit 1) |

The two `read -p` prompt strings are unchanged character-for-character (they only gained leading indentation from moving
inside an `if` branch).

## Bug found and fixed during Phase 2 (in the save scripts, not gameplay)

**`activate_save.sh` was broken for any absolute invocation path containing a space.** The original
`saves=($(find "$BACKUP_DIR" -name "*.tar.gz" -type f | sort -r))` word-splits on spaces, and `BACKUP_DIR` is
`$(dirname "$0")/../.saves`. Invoked the way the user does — `.scripts/activate_save.sh` from the repo root, or from
inside `.scripts/` — `$0` is relative and the bug is invisible. Invoked by **absolute** path, as automation and CI must
(`/mnt/n/Projects/Arma 4/Overthrow.Arma4/.scripts/activate_save.sh`), the array becomes
`["/mnt/n/Projects/Arma", "4/Overthrow.Arma4/.scripts/../.saves/…"]`, the menu prints garbage entries, and
`du -h "/mnt/n/Projects/Arma"` walks the entire project tree — the first matrix run hung for over two minutes on it
before being killed. Fixed by reading `find` output line by line into the array. Discovered by execution; it would have
bitten feature #4 on the first CI call.

## Notes for the migration record

- `<My Games>/OverthrowCI/profile/.save/` exists (created by the autotest runs) but contains only
  `settings/ReforgerEngineSettings.conf` / `ReforgerGameSettings.conf` under an
  `app1874880_user<steamid>` directory — engine **settings**, not save games. It is not the vanilla-persistence storage
  location, so finding 1.8's conclusion stands: the new location must be determined empirically after the migration.
- `du -h` on a `drvfs` mount reports `0` for archives under ~4 KB, so `backup_save.sh`'s `Size:` line can read `Size: 0`
  for a tiny save. Cosmetic, pre-existing, unchanged.

---

# Phase 3: suite base extension + Tier B Init suite

**Date:** 2026-08-02
**Scope:** tasks 3.1-3.10. `Scripts/Game/Tests/TestFramework/OVT_TEST_SuiteBase.c` (extended) and
`Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c` (new). Nothing else under `Scripts/` was touched
(`git diff --stat -- Scripts/ ':!Scripts/Game/Tests/'` is empty), and `tools/`, `Configs/` and `.scripts/` were not
modified at all.
**Build:** unchanged from Phase 1 — Reforger 1.7.0.54, engine 190965. `tools/compile-check.sh` exited 0 before every
launch (one intermediate failure, fixed immediately: `PrintFormat` on `SCR_AutotestCaseBase` accepts **three** string
params — the 4th positional is `LogLevel`, same trap Phase 1 hit).

## Run log

| # | Command | Exit | Observed |
|---|---|---|---|
| R1 | `run-tests.sh` (default = Smoke), immediately after the 3.1 base-class change | **0** | `run-tests: OK (1 tests, 14s)` — task 3.2, first half |
| R2 | `run-tests.sh OVT_TEST_MetaSuite`, same tree | **1** | `run-tests: FAILED (1 of 1) in 13s` — task 3.2, second half |
| R3 | `run-tests.sh OVT_TEST_InitSuite` (first run of the new suite) | **0** | `run-tests: OK (4 tests, 14s)`; all four cases `SUCCESS` |
| R4 | `.scripts/reset_save.sh --profile OverthrowCI` | **0** | see 3.8 below |
| R5 | `run-tests.sh OVT_TEST_InitSuite` immediately after R4 | **0** | `run-tests: OK (4 tests, 14s)` — verdict unchanged by the reset |
| R6-R9 | four can-fail perturbation runs | **1** ×4 | see the can-fail table |
| R10 | Init suite with a **temporary** `RequiresStartedCampaign() = true` override | **0** | exercises the base class's campaign-start branch end to end — see below |
| R11-R13 | `run-tests.sh OVT_TEST_InitSuite` ×3, final tree | **0, 0, 0** | 4 testcases / 0 failures every time — task 3.10 |
| R14 | `run-tests.sh` (Smoke), final tree | **0** | 3.2 re-confirmed after the Init suite existed |
| R15 | `run-tests.sh OVT_TEST_MetaSuite`, final tree | **1** | 3.2 re-confirmed |

Green Tier-B wall time is **14 s tool time** for four cases — the same as feature #2's one-case smoke suite, i.e. case
count is free and only suite count costs (finding 1.10). junit shape:

```xml
<testsuite name="OVT_TEST_InitSuite" tests="4" time="1.437111" timestamp="2026-08-01T15:38:41.762Z">
	<testcase classname="OVT_TEST_InitSuite" name="OVT_TEST_Init_Controllers_AreRegistered" time="0.000118" />
	<testcase classname="OVT_TEST_InitSuite" name="OVT_TEST_Init_Economy_PriceAndDemandSeams" time="0.000061" />
	<testcase classname="OVT_TEST_InitSuite" name="OVT_TEST_Init_Globals_ManagersResolve" time="0.000071" />
	<testcase classname="OVT_TEST_InitSuite" name="OVT_TEST_Init_Towns_ArePopulated" time="0.000053" />
</testsuite>
```

**Case execution order is alphabetical by case class name**, not source order — the same rule finding 1.10 recorded for
suites inside a group. No case may depend on another having run.

## Can-fail proofs

Method for every row: perturb the covered thing in the case's own source, `compile-check` (0), `run-tests.sh
OVT_TEST_InitSuite`, observe exit **1** and the failure text, revert, observe the case green again in the **next**
perturbation run (and all four together in R11-R13). Failure text is quoted verbatim from `.tmp/run-tests/junit.xml`;
`autotest_failed.log` named exactly the perturbed case each time and nothing else.

| Case | Perturbation | Exit | Observed failure text |
|---|---|---|---|
| `OVT_TEST_Init_Globals_ManagersResolve` | Added `if (!OVT_Global.GetController()) return "GetController()";` as the first entry of the sweep — `GetController()` is one of the four deliberately excluded player-dependent getters and returns null with no local player, so this drives the real detection path rather than inverting a comparison | 1 | `OVT_Global getter returned null: GetController() - the game mode is missing that manager component` |
| `OVT_TEST_Init_Towns_ArePopulated` | `if (town.population <= 0)` → `<= 100` (test world town has population 50) | 1 | `Town 0 has no population: 50` |
| `OVT_TEST_Init_Controllers_AreRegistered` | `if (townId < 0)` → `<= 0` (the single town resolves to id 0) | 1 | `The town at the town controller's position is not in m_Towns (GetTownID returned 0)` |
| `OVT_TEST_Init_Economy_PriceAndDemandSeams` | expected buy price computed with `m_fShopProfitMargin + 0.1` instead of the configured margin | 1 | `GetBuyPrice() returned 1250, expected 1350 (base 1000 plus m_fShopProfitMargin)` |

Each perturbation left the other three cases green in the same run, which doubles as the revert proof for the previous
row. No probe code remains: `grep -n "CAN-FAIL PROBE\|TEMP VERIFICATION"` over the test tree returns nothing and
`compile-check` exits 0.

## 3.1 — what the suite base gained, and what was verified

- `bool RequiresStartedCampaign()` — virtual, default `false`. Smoke, Meta and Init are all unaffected (R1/R2/R14/R15).
- `[Step(EStage.Setup)] bool Setup_StartCampaign()` — returns `true` immediately when the virtual is false, so it is a
  single no-op frame for every non-campaign suite. When true: preset selected **by name**, `DoStartNewGame()` +
  `DoStartGame()` guarded by `!HasGameStarted()`, `CloseLayout()` on the start-game context (called on **every** poll,
  not just once — it is a no-op when the layout is not shown and the menu opens ~4 ms after Setup), then `IsInitialized()`
  as the completion condition, with a 600-poll diagnostic backstop so a future async change fails with a message instead
  of hanging to the harness timeout.
- `OVT_DifficultySettings FindDifficultyPreset(config, name)` and `Managed ResolveManager(typename)` — the latter is
  **defensive documentation only** per finding 1.5, and its header says so.

**The campaign-start branch was exercised for real (R10)**, by temporarily overriding `RequiresStartedCampaign()` to
`true` on the Init suite and reverting afterwards. Verbatim:

```
01:42:53.595 SCRIPT : [OVT_TEST] Campaign start: difficulty preset selected by name = 'Test World'
01:42:53.595 SCRIPT : [Overthrow] Starting New Occupying Faction
01:42:53.595 SCRIPT : [Overthrow] NewGameStart: Setting 1 bases to occupying faction index 3
01:42:53.642 SCRIPT : [Overthrow] Overthrow Starting - setting m_bGameInitialized = true
01:42:53.642 SCRIPT : [OVT_TEST] Campaign start: initialized after 0 poll(s)
```

`initialized after 0 poll(s)` reproduces finding 1.3 exactly (same frame, no polling needed), and all four Init cases
stayed green with the campaign running. **Phases 4 and 5 inherit a proven mechanism, not an untested one.** The run
ended before the ~6.5 s resource distribution, so Bug #1's 62 VM exceptions did not fire; total `SCRIPT (E)` count for
that run was 13, all of them the pre-existing noise catalogued in Phase 1.

## 3.8 — fresh-campaign precondition

```
$ .scripts/reset_save.sh --profile OverthrowCI            # exit 0
Resetting Overthrow save data...
Resolved save directory: /mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/OverthrowCI/profile/.db/Overthrow
Save directory not found: /mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/OverthrowCI/profile/.db/Overthrow
Nothing to delete.
Done.
```

The `OverthrowCI` profile still has no `.db` directory at all, consistent with findings 1.7/1.8 — the automated runs have
never produced a save. `run-tests.sh OVT_TEST_InitSuite` exits **0 both with and without** the preceding reset (R3 vs
R5), which is the expected result today: `HasSaveGame()` is a hardcoded `return false`, so nothing about world init
depends on the save DB. **The wiring is proven before it becomes load-bearing** — the moment persistence works, this
same command is what makes "fresh campaign" reproducible. The user's Workbench save was never a target: the resolved
path printed above is the CI profile, and `--profile` was the only form used.

## Observations worth carrying forward

1. **`PrintFormat` on a suite/case takes at most THREE string params.** The 4th positional is `LogLevel`, so a 4-param
   call fails compilation with `Cannot convert 'string' to 'int' for argument '4'`. Split the message instead.
2. **Diagnostic `Print`/`PrintFormat` output lands in `console.log` but not in `autotest.log`** — `autotest.log` keeps
   the suite prelude/epilogue and the per-case verdict lines. When checking that an assertion was actually reached,
   grep `console.log`.
3. **Observed values in the test world (Tier B, no campaign):** 1 town, id 0, population 50 at `<208.237, 1, 102.173>`,
   controller name `Town`; 1 base; all 18 non-player `OVT_Global` getters non-null; `m_fShopProfitMargin` 0.25, so
   `GetBuyPrice` on a base price of 1000 is 1250.
4. **Town and base controller registration happens at Init, not at campaign start** (`FilterTownControllerEntities` and
   `InitializeBases` both run from the managers' `Init`), which is what makes the controllers case legitimately Tier B.
5. **Economy price/demand maps accept arbitrary int keys.** Real IDs are indices into `m_aResources` (`GetInventoryId`),
   so the case uses synthetic IDs far outside that range and disturbs no real item's price for the rest of the session.
   The documented fallbacks for an unknown key (price **500**, demand **5**) are pinned by the same case.

No new bugs were found in this phase; nothing was pinned under Decision 10 and the "Bugs found (log only)" list above is
unchanged.

---

# Phase 4: Persistence — Tier D green + Tier D' quarantined gate

**Date:** 2026-08-02
**Scope:** tasks 4.1-4.9. Three new files under `Scripts/Game/Tests/TestSuites/Persistence/`
(`OVT_TEST_PersistenceSuite.c`, `OVT_TEST_PersistenceRoundTripSuite.c`, `OVT_TEST_PersistenceSubject.c`) and one new
subsection in `tools/README.md`. Task **4.10 (L4 `main`-worktree validation) was NOT started** — it is optional and
requires explicit user approval.
**Build:** unchanged — Reforger 1.7.0.54, engine 190965. `tools/compile-check.sh` exited 0 before every launch.
**Scope discipline:** `tools/run-tests.sh` is byte-identical (`md5 c1b0cbb55fecf93c70b0fda77fe3c7e4` before and after);
`git diff --stat -- tools/` shows only `tools/README.md`; nothing under `Scripts/Game/` outside `Tests/`, nothing in
`Configs/`, `.scripts/` or the Smoke/Meta/Init suites was touched.

## What shipped

| Suite | Cases | Verdict | In a group? |
|---|---|---|---|
| `OVT_TEST_PersistenceSuite` (Tier D) | **8** — player money, player skills+XP, real estate ownership, recruits, town control, town population, town stability, town support | **exit 0**, 13-15 s | Yes (Phase 6 adds it to All) |
| `OVT_TEST_PersistenceRoundTripSuite` (Tier D') | **9** — the 8 state kinds + the capability gate | **exit 1 by design** | **NEVER** |

junit shape of the green suite (execution order is alphabetical by case class name, as in Phase 3):

```xml
<testsuite name="OVT_TEST_PersistenceSuite" tests="8" time="1.463472" timestamp="2026-08-01T16:09:17.326Z">
	<testcase classname="OVT_TEST_PersistenceSuite" name="OVT_TEST_Persistence_PlayerMoney_RoundTrips" time="0.000091" />
	<testcase classname="OVT_TEST_PersistenceSuite" name="OVT_TEST_Persistence_PlayerSkills_RoundTrip" time="0.000147" />
	<testcase classname="OVT_TEST_PersistenceSuite" name="OVT_TEST_Persistence_RealEstateOwnership_RoundTrips" time="0.001087" />
	<testcase classname="OVT_TEST_PersistenceSuite" name="OVT_TEST_Persistence_Recruits_RoundTrip" time="0.000142" />
	<testcase classname="OVT_TEST_PersistenceSuite" name="OVT_TEST_Persistence_TownControl_RoundTrips" time="0.000524" />
	<testcase classname="OVT_TEST_PersistenceSuite" name="OVT_TEST_Persistence_TownPopulation_RoundTrips" time="0.000074" />
	<testcase classname="OVT_TEST_PersistenceSuite" name="OVT_TEST_Persistence_TownStability_RoundTrips" time="0.000113" />
	<testcase classname="OVT_TEST_PersistenceSuite" name="OVT_TEST_Persistence_TownSupport_RoundTrips" time="0.000040" />
</testsuite>
```

Observed values in the started test-world campaign (Test World difficulty), stable across every run:
starting money **100000**, town 0 population **50**, support **0**, stability **100**, faction **3** (occupying),
player persistent ID `d13d3018-3b6a-461e-af2d-9e8ee5c4fddf`, first configured skill key `Trade`.

## Run log

| # | Command | Exit | Observed |
|---|---|---|---|
| R1 | `.scripts/reset_save.sh --profile OverthrowCI` | **0** | `Resolved save directory: …/OverthrowCI/profile/.db/Overthrow` / `Nothing to delete.` — the CI profile still has no `.db` |
| R2 | `run-tests.sh OVT_TEST_PersistenceSuite` (first run, with the throwaway modifier probe) | **0** | `run-tests: OK (8 tests, 14s)`; probe output below |
| R3 | `run-tests.sh OVT_TEST_PersistenceSuite` (stability case upgraded, probe deleted) | **0** | `OK (8 tests, 13s)`; `Town stability round-trip on town 0: 100 -> 90 -> 100` |
| R4 | `run-tests.sh OVT_TEST_PersistenceRoundTripSuite` | **1** | `run-tests: FAILED (9 of 9) in 14s`, all nine `<failure>` elements identical — quoted verbatim below |
| R5-R12 | eight can-fail perturbation runs | **1** ×8 | see the can-fail table |
| R13-R15 | `run-tests.sh OVT_TEST_PersistenceSuite` ×3, final tree | **0, 0, 0** | 8 testcases / 0 failures each; 15 s / 13 s / 13 s |
| R16 | `run-tests.sh OVT_TEST_PersistenceRoundTripSuite`, final tree | **1** | same nine identical failures |

## The quarantined suite's red output, verbatim (task 4.9)

`.tmp/run-tests/junit.xml`, every one of the nine cases:

```xml
<testcase classname="OVT_TEST_PersistenceRoundTripSuite" name="OVT_TEST_PersistenceRoundTrip_Capability_SaveGameProducesASave" time="0.000111">
	<failure type="Result">Persistence capability absent: SaveGame() produced no save (HasSaveGame() still false). The vanilla-persistence migration is not complete.</failure>
</testcase>
```

stdout / `autotest_failed.log`:

```
OVT_TEST_PersistenceRoundTrip_Capability_SaveGameProducesASave
OVT_TEST_PersistenceRoundTrip_PlayerMoney_SurvivesSaveAndReload
OVT_TEST_PersistenceRoundTrip_PlayerSkills_SurvivesSaveAndReload
OVT_TEST_PersistenceRoundTrip_RealEstateOwnership_SurvivesSaveAndReload
OVT_TEST_PersistenceRoundTrip_Recruits_SurvivesSaveAndReload
OVT_TEST_PersistenceRoundTrip_TownControl_SurvivesSaveAndReload
OVT_TEST_PersistenceRoundTrip_TownPopulation_SurvivesSaveAndReload
OVT_TEST_PersistenceRoundTrip_TownStability_SurvivesSaveAndReload
OVT_TEST_PersistenceRoundTrip_TownSupport_SurvivesSaveAndReload
run-tests: FAILED (9 of 9) in 14s
```

A reviewer opening `junit.xml` learns the reason without opening a source file — which was the whole point of task 4.5,
because `SaveGame()` is silent (finding 1.7) and a naive round-trip test would have failed with a value mismatch far
downstream instead.

## Can-fail proofs (task 4.8)

Method for every row, identical to Phase 3: perturb the covered thing in the case's own source (never the comparison
where a real seam could be driven instead), `compile-check` (0), `run-tests.sh OVT_TEST_PersistenceSuite`, observe exit
**1**, revert. Failure text is quoted verbatim from `.tmp/run-tests/junit.xml`. **In every run,
`autotest_failed.log` named exactly the perturbed case and nothing else** — the other seven stayed green, which doubles
as the isolation proof.

| Case | Perturbation | Exit | Observed failure text |
|---|---|---|---|
| `OVT_TEST_Persistence_PlayerMoney_RoundTrips` | `AddPlayerMoney(playerId, …)` → `AddPlayerMoney(playerId + 1, …)` — writes to a player ID that does not exist, so the manager's own "no such record" path runs | 1 | `AddPlayerMoney(12345) did not stick: money was 100000, is now 100000` |
| `OVT_TEST_Persistence_PlayerSkills_RoundTrip` | `skills.GiveXP(…)` → `skills.TakeXP(…)` — the opposite seam, which clamps at 0 | 1 | `GiveXP(400) did not stick: xp was 0, is now 0` |
| `OVT_TEST_Persistence_RealEstateOwnership_RoundTrips` | `SetOwnerPersistentId(persId, …)` → `SetOwnerPersistentId(persId + "-wrong", …)` — ownership really is recorded, under the wrong key | 1 | `SetOwnerPersistentId() did not stick: GetOwnerID() returned 'd13d3018-…-wrong', expected 'd13d3018-…'` |
| `OVT_TEST_Persistence_Recruits_RoundTrip` | `AddRecruitXP(recruitId, …)` → `AddRecruitXP(recruitId + "x", …)` — a recruit ID that resolves to nothing | 1 | `AddRecruitXP(400) did not stick: xp was 0, is now 0` |
| `OVT_TEST_Persistence_TownControl_RoundTrips` | `ChangeTownControl(town, playerFaction)` → `ChangeTownControl(town, occupyingFaction)` | 1 | `ChangeTownControl(player) did not stick: town faction is 3, expected 1` |
| `OVT_TEST_Persistence_TownPopulation_RoundTrips` | `TakeSupportersFromNearestTown(pos, DELTA)` → `(pos, DELTA + 1)` — one more supporter than the town has, so the manager's own refusal path runs | 1 | `TakeSupportersFromNearestTown(5) did not move population: was 50, is now 50` |
| `OVT_TEST_Persistence_TownStability_RoundTrips` | `RemoveStabilityModifier(townId, modifierIndex)` → `(townId, modifierIndex + 1)` — removes a modifier the town does not have | 1 | `Removing the modifier did not stick: town has 1 stability modifiers, expected 0` |
| `OVT_TEST_Persistence_TownSupport_RoundTrips` | `AddSupport(town.location, DELTA)` → `AddSupport(town.location, 0)` | 1 | `AddSupport(7) did not stick: support was 0, is now 0` |

No probe code remains: `grep -rn "PROBE\|TEMP" Scripts/Game/Tests/TestSuites/Persistence/` returns nothing (the `PROBE_*` hits elsewhere in the tree are Phase 3's synthetic economy IDs, not probe code), and `compile-check` exits 0.

## The anti-vacuous-pass design (task 4.6), and why each hole is closed

The requirement is that `OVT_TEST_PersistenceRoundTripSuite` be **unable** to go green without persistence actually
working. Written adversarially — for each stub that could fake a pass, the closure that catches it:

| # | Stub that could fake a pass | Closure |
|---|---|---|
| 1 | `HasSaveGame()` hardcoded **true** (a layer that lies about having saved) | The capability case asserts the whole **transition**: false before the session's first save, true after. Constant-true fails the "before" half; constant-false (today) fails the "after" half. This is why the run recipe requires `reset_save.sh`. |
| 2 | A reload that is a **no-op** | Every state-kind case **dirties** the value after saving and before reloading, then asserts the **saved** value came back — not that the value never changed. A no-op reload leaves the dirty value. This closure does not use `HasSaveGame()` at all, so it survives closure 1 being defeated. |
| 3 | A reload that resets to **campaign-start defaults** | The saved value is deliberately not a value the campaign start produces (money 424242, stability 37, a town handed to the player faction), and the assertion is equality with the saved value. |
| 4 | A case that silently **skips** its assertions (null manager, unresolved subject) | Every resolution failure is an explicit `SetResultFailure` with a named reason. No path reaches `SetResultSuccess` without asserting. |
| 5 | Reliance on **case order** (the capability case happening to run first) | `Capability` does sort first alphabetically, but no case depends on that: every case calls the capability gate itself before doing anything. |

Deliberate design point, recorded because it looks like a hole and is not: the state-kind cases require only "a save
exists after saving", **not** the fresh-session transition — because once persistence works, the first case to run
creates a save and the others would then legitimately see one already present. The fresh-session half is asserted once,
by the capability case, and the suite is the unit of the gate.

## Deferred state kinds (task 4.3), with cause

Recorded here and in the Tier D suite header. These are the growth path; each is deferred for a reason visible in the
source, not for convenience:

| State kind | Why deferred |
|---|---|
| **Vehicles**, **placed structures** | Persist through per-entity components on spawned prefabs rather than through a manager. A behaviour-level round-trip would have to spawn and despawn entities in a world with no navmesh. |
| **Container inventories** | Not saved at all: the inventory manager's save layer has empty read/apply stubs on this branch. There is nothing to round-trip, and a case would be the "test that cannot fail" the requirements forbid. |
| **Character held items** | Deliberately switched off in the character controller's save layer pending an upstream bug. Covering them would pin a disabled feature. |
| **Loadouts** | A separate scripted-state path whose repository methods are unimplemented stubs, so the manager has nothing to hand back. |
| **Garrisons** | Never populated in the test world (finding 1.4). Red on arrival, and would say nothing about Overthrow. |

## Closest-public-seam choices, documented (task 4.2)

Two state kinds have no direct public mutator; the plan permits the closest public seam, documented. Both are recorded
in the case headers as well:

- **Town population.** There is no public "set population" mutator at all — growth is a protected, RNG-driven routine on
  the modifier tick. The only public method that moves population is `TakeSupportersFromNearestTown()`, which removes
  supporters *and* population together and refuses to act unless the town has at least that many supporters. The case
  therefore raises support with `AddSupport()` first, then takes supporters. **Consequence: the case can lower
  population and cannot raise it**, so it restores support and leaves population 5 lower than it found it. Nothing else
  in the suite reads population and the session ends seconds later.
- **Town stability.** Originally planned as a record-field write, because every stability path goes
  modifier → `RecalculateStability` → broadcast and the middle two are protected. A throwaway probe (below) proved the
  modifier seam is in fact synchronous server-side, so the case was **upgraded to the real seam**:
  `TryAddStabilityModifier()` → `RemoveStabilityModifier()`, with the expected value **derived** from the modifier
  system's own recalculation of the stored modifier list rather than hardcoded. Observed: 100 → 90 → 100.

## New empirical finding — `Rpc()` self-delivery on the server (settled by throwaway probe)

The probe (deleted after one run, `logs_2026-08-02_02-09`) called `TryAddStabilityModifier()` from a case and printed
what happened:

```
02:09:18.789 	PROBE-4.2 stability: TryAddStabilityModifier returned true, modifiers 0 -> 1
02:09:18.789 	PROBE-4.2 stability: RplSession.Mode()=0, IsServer=true, stability now 90
```

Reading, and it matters well beyond this phase:

- `RplSession.Mode()` is **0 (`RplMode.None`)** in the autotest client, and `Replication.IsServer()` is **true**.
- **`Rpc(RpcAsk_X, …)` (an `RplRcver.Server` RPC) called on the server executes locally and SYNCHRONOUSLY** — the
  modifier was inserted and stability recalculated before the very next statement ran.
- **`Rpc(RpcDo_X, …)` (an `RplRcver.Broadcast` RPC) does NOT execute locally** — the modifier count went 0 → 1, not
  0 → 2, even though `RpcAsk_AddStabilityModifier` both inserts and then broadcasts `RpcDo_AddStabilityModifier`.
- That is exactly the model Overthrow's own code assumes: broadcast handlers are called directly *and* Rpc'd
  (`AddSupport`, `TakeSupportersFromNearestTown`), while `RplRcver.Server` handlers are only Rpc'd.

**Consequence for future test authoring:** an `Rpc(RpcAsk_…)`-mediated public mutator IS observable from a test in the
same frame and does not need polling. A `Rpc(RpcDo_…)`-only path is not.

## Bugs found (log only) — continuing the Phase 1 list

Per Decision 10 these are **recorded, not fixed**. Both were found by reading the source while choosing seams, and both
were deliberately routed around rather than asserted, so neither affects a verdict.

9. **`OVT_RecruitManagerComponent.RemoveRecruit()` leaves the entity→recruit mappings behind.** It removes the recruit
   from `m_mRecruits` and from `m_mRecruitsByOwner`, but never from `m_mEntityToRecruit` (or `m_mRplIdToRecruit`), so
   `GetRecruitFromEntity()` keeps resolving a removed recruit's ID for as long as the entity lives, and
   `GetRecruit()` then returns null for it. A slow leak plus a stale lookup.

10. **`OVT_OwnerManagerComponent.DoSetOwnerPersistentId()` does not clear the previous owner.** It overwrites
    `m_mOwners[pos]` but only *inserts* into the new owner's `m_mOwned` list, leaving the old owner's list still
    containing that position. After a transfer, `GetOwnerID(building)` reports the new owner while
    `IsOwner(oldOwner, id)` still returns **true**, and `IsOwned()` scans the stale list. The Tier D case avoids this by
    only ever taking ownership of a building that is currently unowned — which is also why it resolves its subject
    through `GetRandomUnownedHouse()` rather than "the nearest building".

## Observations worth carrying forward

1. **The local player is registered BEFORE the suite's campaign-start Setup step**, by ~18 ms
   (`Setting up player: <uid> with playerId: 1` at `01:42:53.577`, campaign start at `01:42:53.595` in the Phase 3
   log). Player-scoped persistence cases therefore need no polling for the persistent ID; they resolve it directly and
   fail with a named diagnostic if it is ever missing.
2. **Campaign-tier cases must finish inside ~10 s of the campaign start.** `MODIFIER_FREQUENCY` is 10 000 ms and the
   modifier tick recalculates support, stability and population growth. Every mutator this phase uses is synchronous, so
   the whole suite finishes ~1.5 s after the start and never meets that tick — but a future case that polled for seconds
   would race it, and that would look like flakiness rather than a design error.
3. **Two identifier spaces, one record.** `AddPlayerMoney()` takes a *runtime* player ID and `GetPlayerMoney()` takes a
   *persistent* ID; `SetOwner()` takes a runtime ID and `SetOwnerPersistentId()`/`IsOwner()` take a persistent one.
   Writing through one and reading through the other is deliberate in these cases — it is the seam most likely to break
   in a migration that changes how player records are keyed.
4. **`AddRecruit()` needs an `IEntity` but not a character.** It only reads `GetOrigin()`, `GetID()` and
   `FindComponent()` off it. The registered town controller entity is used as the subject: a test must not spawn AI (no
   navmesh in the test world) and must not hijack the local player's character. The manager logs a warning about the
   entity lacking a persistence component — expected gameplay-code output, not a failure.
5. **Subject resolution is shared, in `OVT_TEST_PersistenceSubject.c`**, so a case body reads as
   "mutate → read back → assert" and every unresolvable subject produces a named diagnostic instead of a null
   dereference that would appear in `junit.xml` as nothing at all.
6. **The assertion rule's own tokens cannot appear in the test tree — including in comments.** The first draft quoted
   Decision 4 verbatim in both suite headers and thereby tripped the very grep it was describing (4 hits, all comments).
   The headers now quote the rule with the three type-name tokens replaced by descriptions and say explicitly that this
   is deliberate, pointing at `implementation.md` for the verbatim text. The DoD grep now returns **zero** lines
   ("at most one" satisfied).
7. **`PrintFormat` on a case still takes at most three string params** (Phase 3 observation, hit twice more here).

---

# Phase 5: Campaign logic — Tier A pure + Tier C started campaign

**Date:** 2026-08-02
**Scope:** tasks 5.1-5.8. Five new files — `Scripts/Game/Tests/TestSuites/Logic/` (`OVT_TEST_LogicSuite.c`,
`OVT_TEST_Logic_Town.c`, `OVT_TEST_Logic_Jobs.c`, `OVT_TEST_Logic_Skills.c`) and
`Scripts/Game/Tests/TestSuites/Campaign/` (`OVT_TEST_CampaignSuite.c`, `OVT_TEST_Campaign_Economy.c`).
**Build:** unchanged — Reforger 1.7.0.54, engine 190965. `tools/compile-check.sh` exited 0 before every launch.
**Scope discipline:** nothing outside `Scripts/Game/Tests/TestSuites/{Logic,Campaign}/` was modified. `tools/`,
`Configs/`, `.scripts/`, `OVT_TEST_SuiteBase.c` and the Smoke / Meta / Init / Persistence suites were not touched at
all (`git status` shows only the two new directories plus this feature's docs).

## What shipped

| Suite | Cases | World | Campaign | Verdict | Tool time |
|---|---|---|---|---|---|
| `OVT_TEST_LogicSuite` (Tier A) | **14** | **none** (`GetWorldFile()` → `ResourceName.Empty`) | no | **exit 0** | **6-9 s** |
| `OVT_TEST_CampaignSuite` (Tier C) | **4** | test world | started in Setup | **exit 0** | **13-15 s** |

Tier A case list (alphabetical = execution order): `Jobs_DealerCondition_PinsAxisOnlyCheckBug`,
`Jobs_DealerCondition_SetAndUnset`, `Jobs_RandomCondition_DeterministicEdges`,
`Jobs_TownSupportCondition_MinMaxAndUnset`, `Player_LevelAccessors`,
`Player_LevelProgress_IsFractionalWithinTheLevel`, `Skills_EffectsWriteOnlyTheirOwnField`,
`Skills_GivePermissionIsIdempotent`, `Town_AreaHeat_ClampsNegativeToZero`,
`Town_CopyFrom_CopiesRecordButNotLocation`, `Town_IsWithinTownBounds_FixedRadius`,
`Town_Modifiers_RecalculateSumsAndClamps`, `Town_SupportModifiers_DeterministicBranches`,
`Town_SupportPercentage_Boundaries` (all prefixed `OVT_TEST_Logic_`).

Tier C case list: `OVT_TEST_Campaign_Economy_IncomeMatchesTownState`, `OVT_TEST_Campaign_Economy_ShopsInitialise`,
`OVT_TEST_Campaign_GameMode_IsStartedAndInitialized`, `OVT_TEST_Campaign_Towns_AreActivated`.

**Tier A confirms finding 1.2 in production shape:** `grep -c "Requesting scenario change" console.log` → **0**, the
harness starts once, and 14 cases cost the same 6-9 s that one case would. Case count really is free; only suite count
costs.

## Run log

| # | Command | Exit | Observed |
|---|---|---|---|
| R1 | `run-tests.sh OVT_TEST_LogicSuite` (first run, 15 cases) | **1** | 2 of 15 failed — **both of them the cases the plan told this phase to write as bug pins.** See "Differs from assumptions" below; this is the phase's headline result |
| R2 | `run-tests.sh OVT_TEST_LogicSuite` (with a temporary division-semantics probe) | **0** | 14 cases; probe output quoted below, then deleted |
| R3 | `run-tests.sh OVT_TEST_CampaignSuite` (first run) | **0** | 4 cases green first try, including the town-activation and shop-stocking polls |
| R4-R17 | 14 Tier A can-fail perturbation runs | **1** ×14 | each named **exactly** the perturbed case |
| R18-R21 | 4 Tier C can-fail perturbation runs | **1** ×4 | three named exactly one case; the fourth (campaign start disabled) named three, by design |
| R22 | `run-tests.sh OVT_TEST_CampaignSuite` with the lowered-stability block added | **0** | `tax at 90 stability is 1125, down from 1250 at full stability` |
| R23 | 5th Tier C can-fail run (stability block) | **1** | `A negative stability modifier did not lower stability: it was 100 and is 100` |
| R24-R26 | `run-tests.sh OVT_TEST_LogicSuite` ×3, final tree | **0, 0, 0** | 14 testcases / 0 failures each — 8 s / 6 s / 7 s |
| R27-R29 | `run-tests.sh OVT_TEST_CampaignSuite` ×3, final tree | **0, 0, 0** | 4 testcases / 0 failures each — 14 s / 13 s / 14 s |
| R30 | `run-tests.sh` (Smoke) and `run-tests.sh OVT_TEST_MetaSuite`, final tree | **0** / **1** | feature #2's standing regression pair still holds |
| R31 | `run-tests.sh OVT_TEST_InitSuite` and `OVT_TEST_PersistenceSuite`, final tree | **0** / **0** | Phases 3 and 4 unaffected by the two new suites |

## THE HEADLINE: EnforceScript does not do C-style integer division

The plan's risk R7 and Decision 10 both rest on the premise that several `int / int` expressions in Overthrow
truncate, and Phase 5 was budgeted to ship pinning cases for them (`OVT_TownData.SupportPercentage`,
`OVT_EconomyManagerComponent.GetTaxIncome`, `GetSellPrice`'s stock term, `OVT_PlayerData.GetLevelProgress`).

**Two of those pinning cases failed on their first run — because the behaviour they pinned does not exist.** Verbatim,
run R1:

```
<failure type="Result">PINNED BUG CHANGED: SupportPercentage() with support 25 of population 50 returned 50,
	which the pin recorded as 0 (intended: 50). ...</failure>
<failure type="Result">PINNED BUG CHANGED: GetLevelProgress() at 50 of 100 xp returned 0.5,
	which the pin recorded as 0 (intended: 0.5). ...</failure>
```

A throwaway probe (added in R2, deleted immediately afterwards) settled the language rule:

```
PROBE-5 division: int q = 0, float q = 0.5, float scaled = 50
PROBE-5 division: int scaled = 0, int 7/2 = 3, float 7/2 = 3.5
```

for the source

```c
int probeA = 25;
int probeB = 50;
int probeIntQuotient   = probeA / probeB;          // 0
float probeFloatQuotient = probeA / probeB;        // 0.5
float probeScaled      = (probeA / probeB) * 100;  // 50
int probeIntScaled     = (probeA / probeB) * 100;  // 0
```

**The rule: an arithmetic expression's evaluation mode is chosen from the type it is being converted TO, not from the
types of its operands.** The same source expression `(a / b) * 100` is 50 when the result is consumed as a float and 0
when it is consumed as an int. Truncation happens at the conversion, not at the division.

Consequences, in order of how much they change:

1. **`OVT_TownData.SupportPercentage()` is correct.** `Math.Round(...)` takes a float, so the division inside it is a
   float division: 25 of 50 reads as 50, 49 of 50 reads as 98. **Not a bug.** The plan's boundary table shipped as a
   plain correctness case.
2. **`OVT_PlayerData.GetLevelProgress()` is correct.** `current / total` is returned as a float, so it is a float
   division: 0.5 at 50 xp of 100. **Not a bug.**
3. **`OVT_EconomyManagerComponent.GetTaxIncome()` is correct — MEASURED, not inferred.** At 100 stability an integer
   and a fractional stability factor give the same answer, so the campaign case deliberately lowers stability through
   the real modifier seam and re-measures: `tax at 90 stability is 1125, down from 1250 at full stability`
   (25 taxIncome × 50 civilians × 0.9). A truncated factor would have produced 0.
4. **`GetSellPrice`'s stock term and `GetNextLevelXP`** sit in the same float-consuming context (`Math.Round(...)`), so
   the same rule applies. Not directly measured — `GetSellPrice`'s town-stock branch needs a shop-bearing position and
   is Tier B/C work for a later phase, and it is recorded here as *reassessed*, not as *proven*.
5. **The remaining `int / int` in `OVT_TownSupportModifierSystem.Recalculate` (`int supportPerc = Math.Round((newsupport
   / max) * 100)`) is also fine** — `Math.Round` again — and in any case only feeds the RNG-gated branch this phase
   deliberately does not test.

**Nothing in Phase 5 pins an integer-division bug, and the plan's R7 should be considered closed.** Any future
EnforceScript review should apply the rule above rather than C intuition — and should be suspicious of the *opposite*
mistake, `int x = a / b;`, which really does truncate.

## Can-fail proofs (task 5.7)

Method for every row, identical to Phases 3 and 4: perturb the covered thing in the case's own source (never the
comparison, where a real seam could be driven instead), `compile-check` (0), run the suite, observe exit **1**, revert.
Failure text is quoted verbatim from `.tmp/run-tests/junit.xml`. **In every single-case row, the run named exactly the
perturbed case and nothing else** — the other cases stayed green, which doubles as the isolation proof. Driven by a
scripted loop so all 19 perturbations used the identical procedure.

### Tier A — `OVT_TEST_LogicSuite` (14 rows)

| Case | Perturbation | Exit | Observed failure text |
|---|---|---|---|
| `Town_SupportPercentage_Boundaries` | town built with 26 supporters of 50 instead of 25 — a different town state, not an inverted comparison | 1 | `SupportPercentage() with support 25 of population 50 returned 52, expected 50` |
| `Town_Modifiers_RecalculateSumsAndClamps` | modifier id 0's configured `baseEffect` changed from -5 to -6 — the real input `Recalculate()` sums | 1 | `Recalculate() with one -5 modifier returned 94, expected 95` |
| `Town_SupportModifiers_DeterministicBranches` | `max` lowered from 50 to 25 so the gained supporter is clamped away — drives the real clamp path | 1 | `Support Recalculate() with a +80 effect on a base of 25 returned 25, expected 26 (one supporter gained)` |
| `Town_IsWithinTownBounds_FixedRadius` | town moved 200 m, so the 499 m sample really is outside the radius | 1 | `IsWithinTownBounds() rejected a position 499 m away, inside the 500 m radius` |
| `Town_AreaHeat_ClampsNegativeToZero` | positive heat fed to the negative-clamp assertion — the clamp really is not applied | 1 | `SetAreaHeat(-5) left the area heat at 5, expected it clamped to 0` |
| `Town_CopyFrom_CopiesRecordButNotLocation` | source stability changed to 43 — `CopyFrom()` really copies a different value | 1 | `CopyFrom() left stability at 43, expected 42` |
| `Jobs_TownSupportCondition_MinMaxAndUnset` | minimum raised above the fully-supportive town's 100 — the condition's own refusal path runs | 1 | `m_iMinSupport 50 refused a town at 100 support` |
| `Jobs_DealerCondition_SetAndUnset` | dealer moved onto the X = 0 plane, which the condition really does read as absent | 1 | `OVT_TownHasDealerJobCondition reported no dealer in a town whose gunDealerPosition is <0, 0, 250>` |
| `Jobs_DealerCondition_PinsAxisOnlyCheckBug` | dealer moved off the X = 0 plane, so the pinned bug no longer manifests | 1 | `PINNED BUG CHANGED: OVT_TownHasDealerJobCondition now reports a dealer at <2, 0, 500>. The pin recorded that a position with X exactly 0 reads as 'no dealer'. ...` |
| `Jobs_RandomCondition_DeterministicEdges` | the "never" condition given a certain chance — the real roll now succeeds | 1 | `OVT_RandomJobCondition with m_fChance 0 started a job - the roll is never negative, so this can never be beaten` |
| `Skills_EffectsWriteOnlyTheirOwnField` | the effect's configured discount changed from 0.25 to 0.3 — the real value it writes | 1 | `A 0.25 trade discount left priceMultiplier at 0.7, expected 0.75` |
| `Skills_GivePermissionIsIdempotent` | the "different" permission made a duplicate, so the idempotency guard itself suppresses it | 1 | `Granting a second, different permission left 1 permissions, expected 2` |
| `Player_LevelAccessors` | xp lowered below the level 2 threshold — the real curve returns a different level | 1 | `GetLevel() with 150 xp returned 1, expected 2` |
| `Player_LevelProgress_IsFractionalWithinTheLevel` | xp moved to 60 % through level 1 — the derived expectation follows, the 0.5 claim does not | 1 | `The level curve moved: 50 xp is now 0.6 of the way through level 1, not 0.5` |

### Tier C — `OVT_TEST_CampaignSuite` (5 rows, 4 cases)

| Case | Perturbation | Exit | Observed failure text |
|---|---|---|---|
| `Campaign_Economy_IncomeMatchesTownState` | the town handed back to the occupying faction instead of being liberated — the tax-exemption path really runs | 1 | `Liberating a town of 50 civilians at 100 stability produced no tax income at all` |
| `Campaign_Economy_IncomeMatchesTownState` (second, for the lowered-stability block) | the modifier finder made to pick a POSITIVE effect, which clamps at the stability maximum instead of lowering it | 1 | `A negative stability modifier did not lower stability: it was 100 and is 100` |
| `Campaign_Economy_ShopsInitialise` | requires more stocked shops than the test world has (5), so the poll really exhausts | 1 | `No shop was stocked within 601 polls: 5 shop id(s) registered, 5 resolved to a component, none holds a single inventory entry` |
| `Campaign_Towns_AreActivated` | requires more activated towns than the test world has (1), so the poll really exhausts | 1 | `No town was activated within 601 polls: 1 town(s) and 1 town controller(s) are registered, but no town has a gun dealer position recorded` |
| `Campaign_GameMode_IsStartedAndInitialized` | the suite's `RequiresStartedCampaign()` opt-in turned off — **no campaign is started at all** | 1 | `HasGameStarted() is false after the suite's campaign-start Setup step` |

The last row is the strongest available proof for a campaign-tier suite: it removes the campaign rather than editing an
expectation. It also reds `ShopsInitialise` and `Towns_AreActivated` (both correctly report their real observation),
which is the point — those two cases genuinely depend on the campaign start.

**It did NOT red `Economy_IncomeMatchesTownState`, and that is worth recording:** the income calculators read town
records that already exist at world load, so that case passes with no campaign. It stays in Tier C because the plan
places it there and because a started campaign is the state a real income tick runs in — but it is a Tier B case by
dependency, and a future rebalance of the tiers could move it down at zero cost.

## Bugs found (log only) — continuing the Phase 1 / Phase 4 list

11. **`OVT_TownHasDealerJobCondition` only inspects the X axis of the dealer position.** The condition is
    `if (town.gunDealerPosition && town.gunDealerPosition[0] != 0)`, so a gun dealer standing anywhere on the X = 0
    plane reads as "this town has no dealer" no matter how far along Y or Z they are. The evident intent is "is the
    position set", which the zero-vector check alone does not express. Harmless on Everon and in the test world (whose
    town sits at X = 208), and invisible until a map places a town near the western edge of the world. **Pinned** by
    `OVT_TEST_Logic_Jobs_DealerCondition_PinsAxisOnlyCheckBug`, which asserts the current behaviour and will go red
    when it is fixed. **FIXED 2026-08-03 (BUG-005):** the condition is now the zero-vector check
    (`gunDealerPosition != vector.Zero`); the pin case was rewritten as regression case
    `OVT_TEST_Logic_Jobs_DealerCondition_XZeroPlaneIsStillADealer` (can-fail re-proven: old check temporarily
    restored → exit 1 with the case's BUG-005 diagnostic; restored → Fast group green).

### Suspected bugs from the plan that turned out NOT to be bugs

Recorded because "we looked and there is nothing there" is as useful to the backlog as a defect, and because the reason
is a language semantic several people will otherwise re-derive:

| Plan's suspicion (R7 / Decision 10) | Verdict | Evidence |
|---|---|---|
| `OVT_TownData.SupportPercentage()` truncates to a step function | **Not a bug** | 25 of 50 → 50, 49 of 50 → 98, 150 of 50 → 300 (`Town_SupportPercentage_Boundaries`) |
| `OVT_PlayerData.GetLevelProgress()` always returns 0 | **Not a bug** | 0.5 at 50 xp, and derived values at 350 / 399 xp (`Player_LevelProgress_IsFractionalWithinTheLevel`) |
| `OVT_EconomyManagerComponent.GetTaxIncome()` truncates its stability factor | **Not a bug, measured below full stability** | 1250 at 100 stability, 1125 at 90 (`Campaign_Economy_IncomeMatchesTownState`) |
| `GetSellPrice()`'s stock term, `GetNextLevelXP()` | **Reassessed as not bugs**, not directly measured | Same float-consuming context; see the division rule above |
| `OVT_MainMenuContext` reports a successful save unconditionally | **Still a bug** — unchanged from Phase 1 item 2, and untouched by this phase (it is UI, and out of tier) | — |

## Two behaviours pinned as "current", without calling them bugs

- **`OVT_TownData.IsWithinTownBounds()` uses a HARDCODED 500 m radius**, not the town controller's `m_iTownRange`
  (80 m in the test world). Anything deciding "is the player in this town" through this method uses a radius no map can
  configure. Several callers depend on the generous radius, so it is pinned as documented behaviour rather than logged
  as a defect.
- **`OVT_TownData.CopyFrom()` copies campaign state but NOT `location` or `size`, and shares the modifier arrays by
  reference** rather than deep-copying them. Both are correct for its single caller (the persistence restore path,
  where the live world's town placement is authoritative and the source record is discarded immediately), and both are
  asserted so that a change to either is deliberate.
- **`SetAreaHeat()` clamps negatives to zero and has NO upper bound.** Pinned so that adding a ceiling is a visible
  change.

## Deliberately not covered, with cause

| Subject | Why |
|---|---|
| `OVT_TownSupportModifierSystem.Recalculate()`'s RNG-gated branches (summed effect strictly between -75 and 75) | `s_AIRandomGenerator` has no seam to seed or inject, so the branch cannot be made deterministic from test code. The four deterministic branches (> 75, < -75, == 0, `max == 0`) plus both clamps ARE covered. The framework's per-case retry attribute is banned by this feature's quality bar and appears nowhere in the test tree. |
| `OVT_StaminaSkillEffect` | Intentionally inert — its `OnPlayerSpawn()` resolves a stamina component and does nothing, with a source comment saying the engine exposes no stamina parameters. A case could only assert that nothing happens, which would pass for the wrong reason the day it is implemented, and it needs a spawned character Tier A does not have. |
| The "stability above 75 doubles donations" branch | Dropping below 75 stability needs several stability modifiers whose effects come from a content config, which would turn a behaviour assertion into a content assertion. The branch IS honoured by the case's derived expectation, which reads the town's live stability. |
| Garrisons, deployments, occupying-faction resource distribution | Out of scope per the plan; garrisons never populate in this world (finding 1.4). |
| Job conditions that resolve a player entity or ask a manager for the nearest town | Not pure, so not Tier A; no campaign-tier need for them yet. |

## Observed values (Tier A is world-free; these are Tier C, test world, 'Test World' difficulty)

```
Campaign started: HasGameStarted and IsInitialized true, difficulty 'Test World', starting cash 100000
Towns activated: 1 of 1 have a gun dealer recorded after 0 poll(s)
Shops initialised after 1 poll(s): 5 of 5 registered shops hold stock
Total stock entries across all shops: 286
Income: occupied tax 0, liberated tax 1250, donations with 16 supporters
Income: donations 0 -> 160 -> 320, increment 160 per step
Income: tax at 90 stability is 1125, down from 1250 at full stability
```

Difficulty coefficients in force (read from the config at runtime, never hardcoded in a case): `taxIncome` 25,
`donationIncome` 10, `startingCash` 100000. `Difficulty_TestWorld.conf` overrides only `startingCash`,
`startingResources`, `baseRange`, `baseCloseRange` and `minFastTravelDistance`; the income coefficients come from
`OVT_DifficultySettings`' own attribute defaults.

## Observations worth carrying forward

1. **`new` does NOT apply `[Attribute()]` defvalues.** Attribute defaults are applied by the container / config
   loader; a hand-built object starts with every field zeroed or empty. This is the single biggest trap in Tier A, and
   it bites hardest where the declared default is *not* zero: `OVT_TownSupportJobCondition`'s "unset" sentinel is -1
   (a hand-built condition has 0, which is a real constraint), and `OVT_RandomJobCondition`'s three chance factors
   default to 1 (a hand-built condition has 0, which silently zeroes the whole chance). Every Tier A factory sets every
   field explicitly for this reason.
2. **A world-free suite really is free per case.** 14 Tier A cases run in 6-9 s — the same as feature #2's one-case
   smoke suite in Tier B shape. Zero `Requesting scenario change:` lines, one harness start.
3. **Gameplay code that prints an ERROR is not a test failure.** The `max == 0` guard in
   `OVT_TownSupportModifierSystem.Recalculate()` prints `SCRIPT (E)` plus a diagnostic dump; a green Tier A run
   therefore contains 3 `SCRIPT (E)` lines. Verdicts come from `junit.xml`, never from console error counts — the same
   caution feature #4 already needs for the campaign start's 62 VM exceptions.
4. **Perturb the world, not the assertion.** Every can-fail row in this phase changes an INPUT (a town's supporters, a
   modifier's configured effect, a dealer's position, an xp value, the campaign opt-in itself) rather than inverting a
   comparison. Two of the campaign rows require *more* than the test world contains, which drives the real poll-
   exhaustion path and proves the backstop message as a side effect.
5. **A bounded poll backstop is worth writing even when the observable is same-frame.** Both polling Tier C cases
   report what they last saw (`5 shop id(s) registered, 5 resolved to a component, none holds a single inventory
   entry`) instead of dying on the harness timeout with no explanation. The measured settling stayed well inside it:
   town activation on poll 0, shop stock on poll 1.
6. **Restore campaign state on the failure path too.** `Campaign_Economy_IncomeMatchesTownState` runs its checks in a
   helper that RETURNS a problem string, so the caller can put the town's faction, support and stability back before
   reporting. Otherwise a red run cascades into whatever runs next — cases execute alphabetically and this one sorts
   first.
7. **The Tier A purity grep covers comments.** No file under `TestSuites/Logic/` may contain the name of the static
   manager accessor or the engine's game-mode getter, in code OR in prose (DoD Q5) — the same trap Phase 4 hit with the
   persistence assertion rule. The suite header says so explicitly, and is itself worded around it.
8. **`PrintFormat` on a case still takes at most three string params** (Phase 3 observation, unchanged).

---

# Phase 6: Group configs — the fast/slow contract

**Scope:** tasks 6.1-6.7. Two new hand-authored config pairs under `Configs/Tests/` and two new rows + one subsection
in `tools/README.md`. **No script, suite or gameplay file was touched** — `tools/run-tests.sh` is byte-identical
(`md5 c1b0cbb55fecf93c70b0fda77fe3c7e4`, the same hash Phase 4 recorded).
**Build:** Reforger 1.7.0.54 / engine 190965. `tools/compile-check.sh` → **0** (5984 files, 9 s) with both configs present.

## What shipped

| Target | GUID | Config | Suites in `m_aSuites` | Cases |
|---|---|---|---|---|
| **Fast** | `6A6E29FF47ECB840` | `Configs/Tests/OVT_TestGroup_Fast.conf` | `OVT_TEST_LogicSuite`, `OVT_TEST_InitSuite` | **18** (Logic 14 + Init 4) |
| **All** | `6A6E2A002F53A581` | `Configs/Tests/OVT_TestGroup_All.conf` | + `OVT_TEST_CampaignSuite`, `OVT_TEST_PersistenceSuite` | **30** (+ Campaign 4 + Persistence 8) |

Both counts match Phase 5's prediction exactly (Fast 18, All 30). Deliberately in neither group:
`OVT_TEST_MetaSuite`, `OVT_TEST_PersistenceRoundTripSuite`, `OVT_TEST_SmokeSuite`.

## 6.1 / 6.2 — GUIDs and the collision check

Eight GUIDs were generated (two config GUIDs for the `.meta` `Name` fields, six per-entry instance GUIDs for the
`m_aSuites` rows), all 16 uppercase hex in Enfusion's timestamp-prefixed style (`6A6E29FF`/`6A6E2A0x` = 2026-08-02,
generated as `printf '%X' $(date +%s)` + 4 bytes of `/dev/urandom`). **None reuses the Phase 1 probe's
`6A6E04103558938B`.**

| GUID | Role |
|---|---|
| `6A6E29FF47ECB840` | **Fast group config** (`.meta` `Name`) |
| `6A6E2A002F53A581` | **All group config** (`.meta` `Name`) |
| `6A6E2A0155D8103B` | Fast → `OVT_TEST_LogicSuite` entry |
| `6A6E2A021D02A654` | Fast → `OVT_TEST_InitSuite` entry |
| `6A6E2A03B617D5BC` | All → `OVT_TEST_LogicSuite` entry |
| `6A6E2A0450A8E3B1` | All → `OVT_TEST_InitSuite` entry |
| `6A6E2A05444C30CC` | All → `OVT_TEST_CampaignSuite` entry |
| `6A6E2A063CA42167` | All → `OVT_TEST_PersistenceSuite` entry |

**Collision check — all eight, one `rg` alternation, content (`--binary`, so packed/binary assets are searched) plus a
filename-level `find` on the shared prefixes. Every root returned NOTHING for all eight:**

| Root | Content | Filenames |
|---|---|---|
| `/mnt/n/Projects/Arma 4/Overthrow.Arma4` (this repo) | none | none |
| `/mnt/n/Projects/Arma 4/ArmaReforger` (reference tree) | none | none |
| `/mnt/n/Projects/Arma 4/EnfusionPersistenceFramework` | none | none |
| `/mnt/n/Projects/Arma 4/EnfusionDatabaseFramework` | none | none |
| `<My Games>/ArmaReforgerWorkbench/addons` (packed workshop) | none | none |
| `<Steam>/steamapps/common/Arma Reforger/addons` (56 GB game install) | none | none |

The bare form was searched, which subsumes the braced `{GUID}` form. **Positive control for the game-install scan**
(the one root where a silent skip would be invisible): the same `rg` invocation for the known base-game addon GUID
`58D0FB3206B6F859` returned `addons/data/ArmaReforger.gproj` and
`addons/data/data007/scripts/Game/Tests/TestFramework/SCR_AutotestFramework.c` — the scan is real, not an empty walk.
The game install was located from `tools/README.md`'s `OVERTHROW_GAME_EXE` default; `<My Games>` from
`tools/lib/common.sh`'s `ovt_mygames_dir` (resolves to the OneDrive-redirected Documents path on this machine).

### File shapes — byte-identical in form to the proven Phase 1 probe

`Configs/Tests/OVT_TestGroup_Fast.conf`:

```
SCR_AutotestGroup {
 m_aSuites {
  OVT_TEST_LogicSuite "{6A6E2A0155D8103B}" {
  }
  OVT_TEST_InitSuite "{6A6E2A021D02A654}" {
  }
 }
}
```

`Configs/Tests/OVT_TestGroup_Fast.conf.meta` — the six `CONFResourceClass` blocks copied verbatim from
`Configs/Deployment/overthrowDeployments.conf.meta`, with
`Name "{6A6E29FF47ECB840}Configs/Tests/OVT_TestGroup_Fast.conf"`. `OVT_TestGroup_All` is the same shape with four
suite entries. LF endings and **no trailing newline** on any of the four files, matching every existing `.conf` /
`.conf.meta` in the repo (verified by `xxd`).

**Registered first try, both configs, no Workbench round-trip** — exactly as finding 1.10 predicted:

```
03:32:40.142 SCRIPT       : CLI autotest config: SCR_AutotestGroup<0x000001FBAF3458C8>
```

No `Invalid resource path for autotest config`, and **no `Unknown class 'OVT_TEST_…'`** — the known failure signature
(missing `[BaseContainerProps()]` → empty `<testsuites>` → exit 2) did not occur. All four member suite classes were
verified to carry `[BaseContainerProps()]` **before** the first run: `OVT_TEST_LogicSuite.c:56`,
`OVT_TEST_InitSuite.c:22`, `OVT_TEST_CampaignSuite.c:48`, `OVT_TEST_PersistenceSuite.c:82`. No suite class was edited.

*(The console does contain unrelated `Unknown class 'SCR_WidgetExportRuleRoot'` and `Unknown class
'OVT_HireGunnerAction'` lines from the GUI and WORLD subsystems. They are pre-existing, present in single-suite runs
too, and are not the group-config failure signature — that one is `SCRIPT`-adjacent and names a `OVT_TEST_*` class.)*

## 6.3 / 6.5 — Run log (verification + determinism, three consecutive runs each)

`.scripts/reset_save.sh --profile OverthrowCI` was run before the first Fast run and before every All run
(`Nothing to delete` every time — the CI profile still has no `.db`, consistent with Phase 3's R-series).

| # | Target | Exit | `run-tests:` summary | Wall | junit `<testcase>` | `<failure>`/`<error>` |
|---|---|---|---|---|---|---|
| F1 | `"{6A6E29FF47ECB840}"` | **0** | `OK (18 tests, 15s)` | **16 s** | **18** | 0 |
| F2 | `"{6A6E29FF47ECB840}"` | **0** | `OK (18 tests, 16s)` | **17 s** | **18** | 0 |
| F3 | `"{6A6E29FF47ECB840}"` | **0** | `OK (18 tests, 13s)` | **15 s** | **18** | 0 |
| A1 | `"{6A6E2A002F53A581}"` | **0** | `OK (30 tests, 19s)` | **20 s** | **30** | 0 |
| A2 | `"{6A6E2A002F53A581}"` | **0** | `OK (30 tests, 16s)` | **18 s** | **30** | 0 |
| A3 | `"{6A6E2A002F53A581}"` | **0** | `OK (30 tests, 16s)` | **18 s** | **30** | 0 |

**Determinism: identical exit codes (0/0/0 each), identical case counts (18 / 30), identical summary shape.** Only the
duration digit varies (13-16 s Fast, 16-19 s All — a ±3 s spread that is client boot, not test work).

Per-suite `tests=` in the All run's `junit.xml`, in the emitted (alphabetical) order:

```xml
<testsuites time="5.530114" timestamp="2026-08-01T17:35:08.788Z">
	<testsuite name="OVT_TEST_CampaignSuite"    tests="4"  time="1.650818" …
	<testsuite name="OVT_TEST_InitSuite"        tests="4"  time="1.168136" …
	<testsuite name="OVT_TEST_LogicSuite"       tests="14" time="0.000731" …
	<testsuite name="OVT_TEST_PersistenceSuite" tests="8"  time="1.184073" …
```

**Alphabetical execution order re-confirmed at scale** (finding 1.10): `OVT_TEST_LogicSuite` is first in the `.conf`
and third in the output. Nothing in either group depends on order.

## 6.4 — Leak check (verified, not assumed)

The All run's harness listing, verbatim, first pass of run A3:

```
03:35:04.443 SCRIPT    (D): (SCR_AutotestHarness) Tests to run:
03:35:04.443 SCRIPT    (D): 	OVT_TEST_CampaignSuite: 1
03:35:04.443 SCRIPT    (D): 	OVT_TEST_InitSuite: 1
03:35:04.443 SCRIPT    (D): 	OVT_TEST_LogicSuite: 1
03:35:04.443 SCRIPT    (D): 	OVT_TEST_MetaSuite: 0
03:35:04.443 SCRIPT    (D): 	OVT_TEST_PersistenceRoundTripSuite: 0
03:35:04.443 SCRIPT    (D): 	OVT_TEST_PersistenceSuite: 1
03:35:04.443 SCRIPT    (D): 	OVT_TEST_SmokeSuite: 0
03:35:04.443 SCRIPT    (D): 	OVT_TEST_SuiteBase: 0
```

`OVT_TEST_MetaSuite: 0` and `OVT_TEST_PersistenceRoundTripSuite: 0` — the quarantined and meta suites are enumerated
and **disabled**, not merely absent. `grep -c 'OVT_TEST_Meta\|OVT_TEST_PersistenceRoundTrip\|OVT_TEST_Smoke'
.tmp/run-tests/junit.xml` → **0** on all three All runs. `4 + 4 + 14 + 8 = 30` accounts for every `<testcase>`, so
there is no room for a leaked one.

The Fast run's listing is the same shape with `OVT_TEST_CampaignSuite: 0` and `OVT_TEST_PersistenceSuite: 0` added to
the disabled set. Feature #2's no-leak result therefore holds for a **populated** group at four members, which is what
the plan's Q8 quarantine requirement needed.

## 6.7 — Measured cost of group membership

Group wall time vs. the sum of the same suites run individually (individual figures from Phases 3-5, tool time):

| | Suites | Individual runs (tool time, summed) | Group run (tool time) | Saving |
|---|---|---|---|---|
| **Fast** | Logic (6-9 s) + Init (14 s) | **~21 s over 2 launches** | **13-16 s, 1 launch** | ~5-8 s |
| **All** | + Campaign (13-14 s) + Persistence (13-15 s) | **~49 s over 4 launches** | **16-19 s, 1 launch** | **~30-33 s** |

**Marginal cost of one extra suite inside a group: ~+1.5 s wall** (Fast's 2 suites → All's 4 suites is +3 s), which
confirms and slightly refines finding 1.10's `+1 s per extra suite` measured at 1→2 suites. The reason the saving is so
large is that **client boot (~13 s) is paid once per launch, not once per suite** — the suite itself is nearly free.

World transitions (`Requesting scenario change:`): **Fast 2, All 4**, split 1+1 and 1+3 across the harness's two
passes. In the second (real) pass the count equals the number of enabled suites that need a world — Init, Campaign,
Persistence — because `OVT_TEST_LogicSuite` is world-free (`GetWorldFile()` → `ResourceName.Empty`) and **contributes
no transition at all**. A world-free suite is therefore free to add to any group; a world-requiring one costs ~1.5 s.

**Reading for a future merge-the-suites decision:** merging the four tiers into one suite would save ~4.5 s per All run
and would cost the fast/slow split, the per-tier Setup and the ability to run one tier while debugging. Not worth it at
these numbers. The opposite move — splitting into ~8 subject suites, as Decision 1 rejected — would add ~6 s to All,
which is real but small; Decision 1's argument stands on comprehension and tier-mapping rather than on wall time.

## 6.6 — Where the targets are documented

`tools/README.md` → `## tools/run-tests.sh` → **`### Group targets (the fast/slow contract)`** (new subsection, in the
same style as Phase 4's Persistence-acceptance subsection), plus the `{GUID}` row of the **Accepted `<target>` forms**
table now pointing at the Fast GUID instead of BI's example. It carries: the two-row target table (GUID, config path,
member suites, case count, typical time), the copy-pasteable commands, the never-in-a-group list, the recommended CI
usage (reset via `.scripts/reset_save.sh --profile OverthrowCI`; Fast on every push, All nightly/pre-merge), an explicit
"no `OVERTHROW_TEST_TIMEOUT` needed" note, and the add-a-suite procedure with the `[BaseContainerProps()]` failure
signature. One stale sentence was corrected in place: the target-forms paragraph said authoring a group config "makes
it feature #3's problem" — it now points at the two shipped groups.

**The `workbench-workflow` skill was deliberately NOT touched.** Its "Running the Autotests" section is a bash code
block, not a targets table, and four separate statements in it are stale in ways only Phase 7 task 7.4 is scoped to fix
(the "single smoke test" claim, "there is no run-everything form … until someone authors an `SCR_AutotestGroup`
config", and two more in the Development/Testing Cycles). Editing one line inside a section that Phase 7 rewrites
wholesale would create a merge hazard for no benefit. Phase 7 owns it.

## Differs from assumptions (Phase 6)

1. **Both groups were faster than Phase 1 predicted.** Finding 1.11 estimated Fast at 17-18 s and All at 20-22 s;
   measured 13-16 s and 16-19 s. The estimate treated the world-free Logic suite as paying a transition, which it does
   not.
2. **The fast/slow split buys ~3 s, not the ~4 s finding 1.11 estimated — and that is still not the point.** Its value
   is scope: a push-gate that cannot be reddened by campaign or persistence state. The plan already said this; the
   measurement makes it concrete enough to quote when someone asks why CI runs the subset.
3. **Nothing about the hand-authoring procedure needed diagnosis.** Both configs registered on the first launch. The
   entire `Unknown class` contingency path in the plan went unused, because Phases 3-5 had already added
   `[BaseContainerProps()]` as instructed.

---

# Cross-phase review fixes — persistence suites

**Date:** 2026-08-02
**Scope:** four code-review findings against Phase 4's two files
(`Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceSuite.c`, `OVT_TEST_PersistenceRoundTripSuite.c`).
Nothing else was touched — no gameplay code, no `tools/`, no `Configs/`, no other suite. Numbering above is unchanged;
this is an appended record, not a rewrite.

## M1 (major) — the stability round-trip case could have kept the acceptance gate shut forever

`OVT_TEST_PersistenceRoundTrip_TownStability_SurvivesSaveAndReload` wrote `town.stability = 37` straight onto the town
record, under a header sentence claiming "no synchronous public stability mutator exists". **That claim was false** —
Phase 4's own probe had already proven the modifier seam is synchronous, which is why the *green* Tier D case was
upgraded to use it (`TryAddStabilityModifier` / `RemoveStabilityModifier`, observed 100 → 90 → 100).

Why it mattered, and why it is filed as major rather than as tidiness: Overthrow derives stability from the modifier
list via `RecalculateStability`. A correctly migrated persistence layer stores the **modifier list** and recomputes
stability on load, so it would restore the recomputed value and never a raw field write. The case would then have
failed with a value mismatch **no matter how complete the migration was** — a permanent false negative in the one suite
whose exit code *is* `vanilla-persistence`'s acceptance criterion. A gate that cannot open measures nothing, and this
one would have inverted its own purpose.

**The new mechanics**, mirroring the green suite's seam exactly:

| Step | Old | New |
|---|---|---|
| Establish the saved value | `town.stability = 37` (raw field) | `TryAddStabilityModifier(townId, <first index with a negative base effect>)` |
| Expected value | hardcoded constant `37` | **derived**: `system.Recalculate(GetTown(townId).stabilityModifiers)`, asserted equal to the stored value before the save |
| Anti-vacuous (closure 3) | "37 is not a campaign-start value" | the negative modifier puts stability **below the configured maximum**, which the campaign start never produces — plus an explicit assertion that it actually went down |
| DIRTY step (closure 2) | `town.stability = 91` | `RemoveStabilityModifier(townId, index)` → stability recalculates back up; asserted to differ from the saved value |
| Assertion after reload | `stability == 37` | `stability == m_iSavedStability` (the modifier-derived value), dirty value quoted in the failure text |

Capability-gate ordering is unchanged: `RequireSaveCapability()` is still the first thing the case does, so **today it
still fails with the standard rung-L3 sentence**, not a new one. Verified verbatim in `.tmp/run-tests/junit.xml`:

```xml
<testcase classname="OVT_TEST_PersistenceRoundTripSuite" name="OVT_TEST_PersistenceRoundTrip_TownStability_SurvivesSaveAndReload" time="0.000052">
	<failure type="Result">Persistence capability absent: SaveGame() produced no save (HasSaveGame() still false). The vanilla-persistence migration is not complete.</failure>
</testcase>
```

### Can-fail proof of the reworked case (throwaway experiment, deleted)

A perturbation of the new logic is normally unfalsifiable here: every path is behind the capability gate, so any change
produces the same "capability absent" text. So the gate was **temporarily bypassed in that one case only** (the two
`RequireSaveCapability()` calls replaced by `""`, and the reload request replaced by `SetResultSuccess()`), which made
the new mutate/derive/dirty block reachable. Two runs, then the file was restored byte-for-byte
(`md5 2c68424a02d39e233d1f0c1fee51163c` before and after; `grep -rn "EXPERIMENT" Scripts/Game/Tests/` → 0 lines):

| Run | State | Exit | Observed |
|---|---|---|---|
| E1 | seam reachable, unperturbed | 1 (**8** of 9 — the stability case **passed**) | `EXPERIMENT stability: saved 90, dirty 100, modifier index 0` — the same 100 → 90 → 100 the green suite measures |
| E2 | `RemoveStabilityModifier(townId, index)` → `(townId, index + 1)`, i.e. remove a modifier the town does not have | 1 (9 of 9) | `Removing the modifier left stability at the saved value 90 - the reload would have nothing to prove` |

E2 is the meaningful one: it proves the DIRTY step is a real assertion and that closure 2 cannot be satisfied
vacuously. The save/reload half remains unreachable at rung L3 and is still covered only by the gate's red.

## m1 (minor) — unchecked manager-accessor dereferences

Five sites dereferenced a manager accessor without a null check while the *identical* call was guarded, with a named
diagnostic, elsewhere in the same case. Each now has the same guard: `OVT_TEST_PersistenceSuite.c` — `GetPlayer(persId)`
before/after the skill mutation and after `TakeXP()`, and `GetRecruit(recruitId)` after `AddRecruitXP()`; the round-trip
suite's two equivalents — `GetTown(townId)` in the population and support cases' mutate phase, which was guarded in
those same cases' assert phase but not before the save. No assertion changed; the failure mode changes from a silent
null deref (which reaches `junit.xml` as nothing at all) to a sentence.

## m2 (minor) — cross-accessor comment overstated what it proves

The green stability case compares `GetTowns()[townId].stability` against `GetTown(townId).stability` and claimed this
would catch "a persistence layer that wrote through one and read through the other". Both return the **same object**
(`m_Towns[townId]`), and the manager exposes no second, independent stability accessor to compare against — checked
before rewriting rather than inventing one. The **check is unchanged** (it still pins the indexing contract: a town's ID
is its index into `GetTowns()`, and the list is long enough to contain it); only the comment was downgraded to say
exactly that, plus a pointer that this is the line to strengthen if a migration ever adds an independent accessor.

## m3 (minor) — anti-vacuous closure 5 named the wrong dependency

Closure 5 said "no case DEPENDS on that [alphabetical order]". One does: the capability case's `RequireFreshSession()`
requires `HasSaveGame()` to be false, which holds **only** because `..._Capability_...` sorts first and therefore runs
before any other case has called `RequireSaveCapability()`. The header now states the dependency, why it holds, and the
condition that would break it: *a case added to this suite whose class name sorts before the capability case AND that
triggers a save* — which would turn the fresh-session check into a "precondition violated" failure. State-kind cases
remain order-independent, as before.

## Verification of the fixed tree

| Check | Result |
|---|---|
| `tools/compile-check.sh` | **0** (`OK (5984 files, Game module, 8s)`) |
| `tools/run-tests.sh OVT_TEST_PersistenceSuite` ×2 consecutive | **0, 0** — 8 testcases / 0 failures each, 19 s / 18 s |
| `tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite` | **1** — 9 of 9, all nine still the identical capability sentence |
| `grep -rn "EPF_\|SCR_Persistence\|SaveData" Scripts/Game/Tests/` | **0 lines** (DoD Q4 still satisfied) |
| `grep -rn "maxAttempts" Scripts/Game/Tests/` | **0 lines** (DoD Q3) |
| ternaries in the two files | none |
