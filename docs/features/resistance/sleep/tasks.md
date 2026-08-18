# Sleep - Task Checklist

**Last Updated:** 2026-08-19 (post-review)
**Progress:** 32/33 tasks complete (97%) — T6.3 blocked, see the Phase 6 session note. **Phase 7 (review fixes) is complete.**

> **Phase 1 is ADVANCED** (`component-developer-advanced`) — it cuts into two shipped `CheckUpdate` bodies and
> fixes a shipped money exploit (BUG-179). Phases 2-5 are `component-developer`, Phase 6 is `help-docs-sync`.
> Task numbering matches `implementation.md` §4. **GUID series reserved: `{6B5D0000000000XX}`.**
>
> **Test-run policy:** `tools/compile-check.sh` runs freely; `tools/run-tests.sh` is run by the orchestrator
> only, once, after a phase completes. See `.claude/test-policy.md`.

---

## Phase 1: Accounting catch-up, and BUG-179 (8/8) ✅ — `component-developer-advanced`

- [x] **T1.1 `OVT_SleepSchedule` pure arithmetic class**
  - Description: §3.3/§3.4 functions (`CountIntervalCrossings`, `CountHourEntries`, `CountStepCrossings`, `IsIntervalBoundary`, `LandingHour`, `AbsoluteGameHours`, `CooldownRemainingHours`, `FormatRemaining`) + constants. No `OVT_Global`/`GetGame`/`IEntity`/`World`, ever. Every floor written as `(int)Math.Floor((float)a / b)`.
  - File(s): `Scripts/Game/Services/OVT_SleepSchedule.c` (new)
  - Estimate: 🟡

- [x] **T1.2 `AssertHourLatches(int hour)`**
  - Description: protected; sets `m_iHourPaidIncome`/`m_iHourPaidStock`/`m_iHourPaidRent` all to `hour`, with the doc comment explaining why one value is correct for all three.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c`
  - Estimate: 🟢

- [x] **T1.3 BUG-179 — call `AssertHourLatches` from three places**
  - Description: one-shot at the top of `CheckUpdate()` behind `m_bLatchesAsserted`; `Init()` before the `CallLater`; `ApplyPersistedEconomy()`. Defensive `m_Time` resolution as `CheckUpdate` does.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c`
  - Estimate: 🟡

- [x] **T1.4 `OVT_EconomyManagerComponent.HandleTimeSkip(int hours)`**
  - Description: public, server-guarded, five steps of §3.3 (flush `CheckUpdate()`, income+shops per 6h crossing, restock if hour 7 entered, rent if hour 0 entered, `AssertHourLatches(landingHour)`). Must not inherit the player-count-0 early return.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c`
  - Estimate: 🟡

- [x] **T1.5 Extraction 1: `SpendResourcesOnBases(int newResources)`**
  - Description: pure move of the inline block at `:1188-1229`. Named `SpendResourcesOnBases` to avoid colliding with `OVT_BaseControllerComponent.SpendResources`. `CheckUpdate` calls it at the same gate.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 🟡

- [x] **T1.6 Extraction 2: `DecayThreatStep()`**
  - Description: pure move of the four lines at `:1255-1259` only. The town-uprising scan stays in `CheckUpdate`.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 🟢

- [x] **T1.7 `OVT_OccupyingFactionManager.HandleTimeSkip(int hours)`**
  - Description: public, server-guarded, chronological 15-minute loop of §3.3; counter-attack roll and town-uprising scan deliberately excluded, documented in the doc comment.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 🟡

- [x] **T1.8 Logic suite for the schedule arithmetic**
  - Description: §8 case list — crossings, restock/rent entries, quarter-hour steps, boundary predicate, landing hour. Each case with a recorded can-fail proof in its preamble. No `maxAttempts`.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_SleepSchedule.c` (new)
  - Estimate: 🟡

---

## Phase 2: Sleep service — clock advance and cooldown persistence (6/6) ✅ — `component-developer`

- [x] **T2.1 `m_fLastSleepGameHours` on `OVT_PlayerData`**
  - Description: `float m_fLastSleepGameHours = -1;` in the persisted block; server-only, never replicated (not in the player manager's `RplSave`/`RplLoad`).
  - File(s): `Scripts/Game/Data/OVT_PlayerData.c`
  - Estimate: 🟢

- [x] **T2.2 Player manager serializer version 5**
  - Description: `float lastSleepGameHours;` appended **last** on `OVT_PersistedPlayer`, written in `Serialize`, adopted in `ApplyPersistedPlayers`, `if (version < 5) ClearSleepCooldown(records);` beside the existing clears. VERSION HISTORY extended.
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c`
  - Estimate: 🟡

- [x] **T2.3 `OVT_SleepService` — gates, orchestration, clock advance**
  - Description: static service; `SKIP_HOURS = 8`, `COOLDOWN_HOURS = 12`; `IsSleepLocation`, `CanSleep(user, out reasonKey)`, `GetCooldownRemainingHours`, `PerformSleep` (no fade yet), `AdvanceClock` with the `SetDate` validation ladder. Four location constants re-declared and attributed to `OVT_ItemLimitChecker`.
  - File(s): `Scripts/Game/Services/OVT_SleepService.c` (new)
  - Estimate: 🔴

- [x] **T2.4 Cooldown stamp written pre-skip**
  - Description: stamp `m_fLastSleepGameHours` from the pre-skip calendar via `OVT_SleepSchedule.AbsoluteGameHours`, **before** `AdvanceClock`.
  - File(s): `Scripts/Game/Services/OVT_SleepService.c`
  - Estimate: 🟢

- [x] **T2.5 Logic cases for cooldown + formatting**
  - Description: `AbsoluteGameHours`, `CooldownRemainingHours` (incl. the D17 fail-open on negative elapsed), `FormatRemaining`.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_SleepSchedule.c`
  - Estimate: 🟡

- [x] **T2.6 Persistence round-trip case for the cooldown stamp**
  - Description: modelled on `…_PlayerLastKnownPosition_SurvivesSaveAndReload` (`:2963`) — set, save, dirty, re-apply, assert.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 🟡

---

## Phase 3: The user action and the bed overrides (3/3) ✅ — `component-developer`

- [x] **T3.1 `OVT_SleepAction`**
  - Description: §3.6 — SP + location gate in `CanBeShownScript` with the 1 s cache shape of `OVT_RearmVehicleAction`; QRF/wanted/cooldown as disabled-with-reason; countdown label by key-plus-literal concatenation; `PerformAction` → `OVT_SleepService.PerformSleep`. Actor via `SCR_PlayerController.GetLocalControlledEntity()`, **not** `RplComponent.IsOwner()`.
  - File(s): `Scripts/Game/UserActions/OVT_SleepAction.c` (new)
  - Estimate: 🟡

- [x] **T3.2 Seven bed override prefabs + `.meta`**
  - Description: `Bed_01`, `Bed_02`, `BedDouble_01`, `BedDouble_02`, `BunkBed_01`, `BunkBed_01_Double`, `BunkBed_02/BunkBed_02` under `Prefabs/Props/Furniture/`. Parent header + `ID` copied verbatim from each vanilla file; `.meta` `Name` GUID recovered from a recorded inbound reference (citation goes in the Phase 3 session note below).
  - File(s): `Prefabs/Props/Furniture/*.et` + `.et.meta` (new)
  - Estimate: 🔴

- [x] **T3.3 Localization for the action keys**
  - Description: `OVT-Sleep`, `OVT-SleepCooldown`, `OVT-SleepQRF`, `OVT-SleepWanted`, `OVT-SleptWell` — `.st` items with reserved GUIDs **and** mirrored into all seven runtime `.conf` exports (`Ids` alphabetical + `Texts` at the same index), as commit `6085521a` did. Verify `.st` brace balance before and after.
  - File(s): `Language/localization_Overthrow.st`, `Language/localization_Overthrow.*.conf`
  - Estimate: 🟡

---

## Phase 4: The cot placeable (4/4) ✅ — `component-developer`

- [x] **T4.1 `OVT_Cot_Placed.et` (+ `.meta`)**
  - Description: inherits `{C9CFDED29542A968}…/CotMilitary_US_01.et`; adds `OVT_PlaceableComponent { m_sPlaceableType "Cot" }`, `OVT_PlayerOwnerComponent`, `RplComponent`, `SCR_EditableEntityComponent` (PLACEABLE), `ActionsManagerComponent` with the Sleep action. Component set copied from `OVT_CabinetMetal_01_grey_V1.et`.
  - File(s): `Prefabs/Props/Military/Furniture/OVT_Cot_Placed.et` (+ `.meta`) (new)
  - Estimate: 🟡

- [x] **T4.2 Placeable config entry**
  - Description: one `OVT_Placeable` "Cot" in the minimal Ammobox shape (`:44-56`), cost ~30, vanilla cot editor preview. Do not touch the existing "Furniture" entry.
  - File(s): `Configs/Resistance/placeables.conf`
  - Estimate: 🟢

- [x] **T4.3 Localization for the placeable keys**
  - Description: `OVT-Place_Cot`, `OVT-Place_Cot_Description` — same `.st` + seven-export procedure as T3.3.
  - File(s): `Language/localization_Overthrow.st`, `Language/localization_Overthrow.*.conf`
  - Estimate: 🟢

- [x] **T4.4 Confirm no persistence config change is needed (D9)**
  - Description: verify the `ComponentClass "OVT_PlaceableComponent"` / `SelfSpawn 1` rule at `Configs/Systems/Persistence/Overthrow.conf:152-171` covers the cot and that `PlaceItem` tracks it (`:773`). Record the check here; change nothing.
  - File(s): (verification only)
  - Estimate: 🟢

---

## Phase 5: Fade, polish and the pad pass (6/6) ✅ — `component-developer`

- [x] **T5.1 Screen fade in `OVT_SleepService`**
  - Description: `SCR_ScreenEffectsManager` → `SCR_FadeInOutEffect`, following `SCR_FastTravelComponent.c:264-296`. `FADE_DURATION = 1.0`, `BLACK_DURATION = 0.5`. Every step null-guarded; the `CallLater` that does the work fires regardless.
  - File(s): `Scripts/Game/Services/OVT_SleepService.c`
  - Estimate: 🟡

- [x] **T5.2 Confirm the fade effect is registered — STATIC HALF ONLY**
  - Description: verified by reading that `OVT_PlayerController.et` adds only components and never declares `SCR_HUDManagerComponent`, so vanilla's `SCR_ScreenEffectsManager` tree (incl. `SCR_FadeInOutEffect`) is inherited intact. Citations in the Phase 5 session note. **The runtime confirmation is play-test item 3 in `context.md`'s "Needs human verification"** and is NOT claimed here; fallback if it is absent is registering the effect on `OVT_PlayerController.et`.
  - File(s): (verification; fallback `Prefabs/Characters/Core/OVT_PlayerController.et`)
  - Estimate: 🟢

- [x] **T5.3 Wake hint**
  - Description: `SCR_HintManagerComponent.ShowCustomHint("#OVT-SleptWell", "", 4)` at the end of `PerformSleepNow()`, after the clock advance and before the fade back in. The payout notifications were left alone (R9).
  - File(s): `Scripts/Game/Services/OVT_SleepService.c`
  - Estimate: 🟢

- [x] **T5.4 Gamepad pass — STATIC HALF ONLY**
  - Description: the action adds **no** keybinding — it is a `ScriptedUserAction` on a prefab `UserActionContext` and rides the standard interaction bind; `git status` shows no input `.conf` touched by this feature. `check-input-conflicts.py` exits 0 with 0 errors/0 warnings and its three combo notes are all pre-existing menu binds, none related to sleep. **The in-game pad pass is play-test item 11** and is NOT claimed here.
  - File(s): (static check + play-test)
  - Estimate: 🟢

- [x] **T5.5 Final localization sweep**
  - Description: all seven `#OVT-` keys this feature references audited key by key in the `.st` and in all seven runtime exports, with the structural entry parser (never line offsets). Result table in the Phase 5 session note.
  - File(s): `Language/*`
  - Estimate: 🟢

- [x] **T5.6 Assemble the feature's play-test checklist**
  - Description: merge `implementation.md` §6's Verification Method with the manual items Phases 3, 4 and 5 recorded (bed offsets/radii, the cot's build/place/persist run, the fade's runtime presence, the pad pass, BUG-179, the MP F11 check) into ONE numbered list under a "Needs human verification" heading in `context.md`.
  - File(s): `docs/features/resistance/sleep/context.md`
  - Estimate: 🟢

---

## Phase 6: Help and documentation sync (2/3) — `help-docs-sync`

- [x] **T6.1 Field Manual entry**
  - Description: what sleeping does — 8 hours, the accounting, the 12-hour cooldown, where it works, single-player only. Every sentence cites a `file:line` or is cut.
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`, `Language/localization_Overthrow.st` + all seven exports
  - Estimate: 🟡

- [x] **T6.2 Cot in the placeable listing**
  - Description: add the cot wherever the manual lists placeables.
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`, `Language/localization_Overthrow.st` + all seven exports
  - Estimate: 🟢

- [ ] **T6.3 Public wiki sync** — ⏸️ **BLOCKED, NOT ATTEMPTED**
  - Description: keep the wiki page in step with the manual (verify pages by content, not by search pageId).
  - Blocker: the `mcp__wikijs__*` tools were **not present in the Phase 6 agent's tool set at all** — not a search miss, not an auth failure, no server to reach. Nothing was written, nothing was faked. Re-run this one task from a session that has the wikijs MCP server attached; the source text to mirror is the eleven English strings listed in the session note below.
  - File(s): (wikijs)
  - Estimate: 🟡

---

## Phase 7: Review fixes — post-build (3/3) ✅ — `component-developer-advanced`

> Added 2026-08-19 after the cross-phase review and one user decision. **Not** a re-plan: two defects and one
> behaviour change, each scoped in `implementation.md` §4 Phase 7 and §5 D16a / D18 / D18a.

- [x] **T7.1 Occupying-faction tick latches, start flush and landing assertion (D18/D18a)**
  - Description: new `m_iHourGainedResources` and `m_iMinuteDecayedThreat` (both `-1` armed) mirroring
    `m_iHourPaidIncome`'s shape exactly, including its lack of an else-reset; both live gates gain **one
    condition and one assignment** and nothing else; the four-line boundary payload becomes
    `GainAndSpendResources()` (a third pure move, so the flush is not a fourth copy); `HandleTimeSkip` gains a
    start-boundary flush for both grids and asserts both latches at the landing instant. New
    `OVT_SleepSchedule.IsStepBoundary(absoluteMinute, stepMinutes)` so no `% 15` lands in a `CheckUpdate` seam.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`,
    `Scripts/Game/Services/OVT_SleepSchedule.c`
  - Estimate: 🟡

- [x] **T7.2 Cooldown runs from waking, not from lying down (D16a)**
  - Description: `OVT_SleepService.StampCooldown` writes `ReadAbsoluteGameHours(time) + SKIP_HOURS` — the wake
    instant — off the same pre-skip clock read the catch-up used; the sequence in `PerformSleepNow` is
    unchanged and the post-skip clock is not read. Every "about four hours" claim chased: three doc comments,
    the serializer's field and version-history notes, the Field Manual string `OVT-FieldManual_Sleep_Text4`
    (text **and** its `FACT-CHECKED` ledger) in the `.st` and all seven runtime exports, and five places in
    `implementation.md`.
  - File(s): `Scripts/Game/Services/OVT_SleepService.c`, `Scripts/Game/Data/OVT_PlayerData.c`,
    `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c`,
    `Language/localization_Overthrow.st` + 7 `.conf` exports, `docs/features/resistance/sleep/implementation.md`
  - Estimate: 🟡

- [x] **T7.3 Logic tier: one case changed, two added**
  - Description: `..._CooldownRemaining_ClampsAndFailsOpen` now asserts **12 h** remaining at the wake instant
    (and 8 h twelve hours after lying down, 0 at twenty — the door-to-door row);
    `..._IsStepBoundary_ExactMinutesOnly` pins the new predicate; `..._WindowEdges_FlushPlusReplayCovers`
    `EachBoundaryOnce` states both edges as one property over all 1440 start minutes, with the 10:00 and 12:00
    starts named explicitly and an **independent enumeration** as the oracle. Four fault injections recorded
    (M1, M8, M14, M15), each compiled at exit 0. No `maxAttempts`.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_SleepSchedule.c`
  - Estimate: 🟡

---

## Session notes

_Per-phase notes land here as phases complete — bed `.meta` GUID citations (T3.2), the D9 persistence check (T4.4), and anything a later reader would otherwise have to re-derive._

### Phase 1 — 2026-08-18 (complete, `tools/compile-check.sh` exit 0, 6060 files)

**New public/protected surface**

| Symbol | File |
|---|---|
| `class OVT_SleepSchedule` (all-static) | `Scripts/Game/Services/OVT_SleepSchedule.c` |
| `void OVT_EconomyManagerComponent.HandleTimeSkip(int hours)` | economy manager |
| `protected void OVT_EconomyManagerComponent.AssertHourLatches(int hour)` | economy manager |
| `protected bool OVT_EconomyManagerComponent.AssertHourLatchesFromClock()` | economy manager |
| `protected TimeContainer OVT_EconomyManagerComponent.ResolveGameTime()` | economy manager |
| `protected void OVT_OccupyingFactionManager.SpendResourcesOnBases(int newResources)` | occupying manager |
| `protected void OVT_OccupyingFactionManager.DecayThreatStep()` | occupying manager |
| `void OVT_OccupyingFactionManager.HandleTimeSkip(int hours)` | occupying manager |

**Two deliberate departures from the plan, both load-bearing for Phase 2.**

1. **`AbsoluteGameHours` measures from `EPOCH_YEAR = 1989`, not from year 0.** §3.4's literal formula
   `(year * 366 + dayInYear) * 24 + …` is arithmetically right and numerically unusable: an
   EnforceScript `float` is IEEE binary32, the engine's default campaign year is 1989
   (`ArmaReforger/scripts/Game/Components/Environment/SCR_TimeAndWeatherHandlerComponent.c:79`), and
   the term therefore lands at ~1.75e7 where the float increment is **2 hours**. Measured on a
   binary32 model: at that magnitude a twelve-minute difference between two stamps comes out as
   **exactly 0.0**, i.e. the countdown would never move and the cooldown would snap to multiples of
   two hours. Subtracting the epoch puts a default campaign at ~4.8e3, where the increment is under
   0.02 s. The signature, the monotonicity contract and every §8 cooldown case are unchanged — only
   the origin moved. Phase 2 needs no adjustment; `m_fLastSleepGameHours` is still one float.
2. **One function beyond T1.1's list: `static int StepMinuteAt(int startHour, int startMinute,
   int stepMinutes, int index)`.** §3.3's chronological loop needs the absolute minute of each step
   to feed `IsIntervalBoundary`, and the listed eight cannot produce it — writing `% 15` in the
   manager body instead would put untestable timing arithmetic back in a `CheckUpdate` seam, which
   §7 forbids in as many words. It is pure, world-free, and covered by the replay-walk case.

**Two smaller judgement calls.**

- The `Print("… Reserve Resources: …")` line is duplicated into the occupying replay loop (it is
  diagnostics, not a payout, and DoD **F7** asks the play-test to count one `Gaining Resources` /
  `Reserve Resources` pair per boundary). `UpdateSpecops()` stayed OUT of `SpendResourcesOnBases`,
  matching §3.3's loop, so the extraction is lines `:1188-1228` and not `:1188-1229`.
- `OVT_OccupyingFactionManager.HandleTimeSkip` does **not** re-check `m_CurrentQRF`, even though the
  live `CheckUpdate` returns early on it. Phase 2's `CanSleep` is the QRF gate (**F9**) and
  `PerformSleep` re-validates before calling anything; a second silent guard here would make the
  occupying replay a no-op while the economy replay still ran, which is a worse failure than either.
  **Phase 2 must keep that re-validation** — it is what makes this safe.

**Verification done, and not done.** Both extractions were diffed against `HEAD` and are
whitespace-normalised byte-for-byte identical moves (33 and 4 lines); `CheckUpdate`'s diff is exactly
two call-site replacements and nothing else (**Q9**/**R6**). Eight faults were injected into
`OVT_SleepSchedule.c` one at a time and each compiled clean at exit 0 — the point being that none of
them is a syntax error — and the value each produces was computed on a binary32 model, so the failure
text recorded in every case preamble is the real one. **The in-harness red/green pass is owed to the
orchestrator's suite run.** BUG-179's play-test checks (Verification Method steps 12-14) are
untouched and its status is deliberately still `open`.

### Phase 2 — 2026-08-18 (complete, `tools/compile-check.sh` exit 0, 6061 files)

**New public/protected surface** (all on `Scripts/Game/Services/OVT_SleepService.c`, all static unless noted)

| Symbol | Notes |
|---|---|
| `static bool OVT_SleepService.IsSleepLocation(vector pos, string playerPersistentId)` | owned house → own camp → FOB → captured base, in that order |
| `static bool OVT_SleepService.CanSleep(IEntity user, out string reasonKey)` | SP mode, resolvable record, QRF, wanted, cooldown |
| `static float OVT_SleepService.GetCooldownRemainingHours(string playerPersistentId)` | 0 = ready; fails open on anything unresolvable |
| `static void OVT_SleepService.PerformSleep(IEntity user)` | re-validates, then schedules |
| `protected void OVT_SleepService.ScheduleSleep(string playerPersistentId)` | instance — `CallLater` binds a method on an object |
| `protected void OVT_SleepService.PerformSleepNow()` | instance — the deferred skip; **AdvanceClock is last** |
| `protected static void OVT_SleepService.StampCooldown(string playerPersistentId)` | pre-skip calendar |
| `protected static void OVT_SleepService.AdvanceClock(int hours)` | + `AdvanceDate`, `ReadAbsoluteGameHours`, `ResolveTimeManager`, `ResolvePersistentId` |
| `float OVT_PlayerData.m_fLastSleepGameHours` | persisted, server-only, never replicated |
| `float OVT_PersistedPlayer.lastSleepGameHours` | serializer **version 5**, appended last |
| `protected void OVT_PlayerManagerSerializer.ClearSleepCooldown(...)` | the `version < 5` reset to `-1` |

**Constants:** `SKIP_HOURS = 8`, `COOLDOWN_HOURS = 12`, `WORK_DELAY_MS = 1500`, `MAX_HOUSE_PLACE_DIS = 30`,
`MAX_CAMP_PLACE_DIS = 75`, `MAX_FOB_PLACE_DIS = 100` (base range comes from `m_Difficulty.baseRange`, as in
`OVT_ItemLimitChecker`), `REASON_QRF/WANTED/COOLDOWN`.

**The four location distances were re-verified against `Scripts/Game/Utilities/OVT_ItemLimitChecker.c:13-15`
on 2026-08-18 and still match** (30 / 75 / 100, base via `OVT_Global.GetConfig().m_Difficulty.baseRange`).
Phase 3's re-check task can cite this.

**Judgement calls.**

1. **`OVT_SleepService` has one lazily-created instance**, holding only the pending sleeper's id and a
   re-entry flag. T2.3 says "static, no instance state" and this is the minimum deviation that makes the
   deferred structure possible at all: `CallLater` binds a method on an OBJECT, not a static function
   (`OVT_NavmeshRebuild.c:53` pays the same toll). No API is instance-facing — every entry point is static.
2. **`WORK_DELAY_MS = 1500`, not the plan's 1.2 s.** §3.2's sketch says +1.2 s; Phase 5's T5.1 says the work
   runs at `(FADE_DURATION 1.0 + BLACK_DURATION 0.5) * 1000`. Those disagree, so the Phase 5 number was used
   and the constant's doc comment says why. Phase 5 only has to start the fade before `ScheduleSleep` and fade
   back in at the end of `PerformSleepNow`.
3. **The location gate is NOT part of `CanSleep`**, exactly as T2.3 lists it — it is the action's
   *visibility* question. `PerformSleep` therefore re-validates QRF/wanted/cooldown but not location.
4. **`ApplyPersistedPlayers` adopts the stamp unconditionally** (the money rule), not the "only when unset"
   body-id rule: it is campaign state, a re-apply is a rollback, and the round-trip case depends on it.
5. **The wake hint is not here.** §3.2 step (e) lists it, but T5.3 assigns it to Phase 5; Phase 2 stops at the
   clock advance.

**Can-fail proofs.** Five more faults (M9-M13) were injected into `OVT_SleepSchedule.c` one at a time and each
compiled at exit 0 — the point being that none is a syntax error — and the resulting values were computed on a
binary32 model, so the failure text recorded in each new case preamble is the real one. The in-harness
red/green pass is owed to the orchestrator's suite run, as is the persistence case's can-fail (its recipe is
in its preamble). `tools/run-tests.sh` was not run.

### T3.2 — bed resource GUIDs, recovered up front (orchestrator, 2026-08-18)

Recovered from inbound references in the vanilla tree (`/mnt/n/Projects/Arma 4/ArmaReforger`) before Phase 3
started, so **R4 is retired**: every GUID below is unique and came from a real reference, not a table.

| Prefab (`Prefabs/Props/Furniture/…`) | `.et.meta` `Name` GUID | Inbound citation |
|---|---|---|
| `Bed_01.et` | `{0164749D6EAA4518}` | `Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/Furniture/PubVillage_E_2L01_furniture_03.et:4` |
| `Bed_02.et` | `{990827BD66BE9397}` | `Prefabs/Structures/Houses/Prefabricated/ApartmentBuilding_5I01/Furniture/ApartmentBuilding_USSR_5I01_furniture_apartments_01.et:455` |
| `BedDouble_01.et` | `{8A6DD31A4FF94E48}` | `Prefabs/Structures/Houses/Prefabricated/ApartmentBuilding_5I01/Furniture/ApartmentBuilding_USSR_5I01_furniture_apartments_02.et:306` |
| `BedDouble_02.et` | `{E639E7FAF33DC166}` | `Prefabs/Structures/Houses/Town/House_Town_E_2I01/Furniture/House_Town_E_2I01_furniture_01.et:1029` |
| `BunkBed_01.et` | `{457331BE1519B200}` | `Prefabs/Structures/Houses/Villa/Villa_E_2I01/Furniture/Villa_E_2I01_furniture_02.et:239` |
| `BunkBed_01_Double.et` | `{AC49D9D23A2BD1D8}` | `Prefabs/Structures/Airport/ControlTower_01/Furniture/ControlTower_01_furniture_01.et:776` |
| `BunkBed_02/BunkBed_02.et` | `{8BE469B1C9144671}` | `Prefabs/Structures/Houses/Prefabricated/ApartmentPrefab_5I01/Furniture/ApartmentPrefab_USSR_5I01_furniture_01_flats_left.et:362` |

**Line 1 / line 2 to copy verbatim** (read from the vanilla files directly):

- Six of the seven: `GenericEntity : "{4CC2C69DE1AEF103}Prefabs/Props/Core/DestructibleMultiPhase_Props_Base.et" {`
- `BunkBed_02/BunkBed_02.et`: `GenericEntity : "{A43461050704B41B}Prefabs/Props/Furniture/BunkBed_02/BunkBed_02_base.et" {`
- `ID` lines: `Bed_01` `54DA27348BDA77B2` · `Bed_02` `51E0B8FBFAF08522` · `BedDouble_01` `51E2DC2DD52F3EAB` ·
  `BedDouble_02` `F0DBA538AC2A0552` · `BunkBed_01` `F0DBA538AC2A0552` · `BunkBed_01_Double` `5111E74D4E7A8DB6` ·
  `BunkBed_02` `F0DBA538AC2A0552`

⚠️ **The duplicate `ID` is real, not a transcription error.** `BedDouble_02`, `BunkBed_01` and `BunkBed_02` all
carry `ID "F0DBA538AC2A0552"` **in vanilla**. The plan's note (T3.2) guessed this was a research slip; it is
not. Copy each one verbatim anyway — the `ID` is the root entity's id *within* its own prefab file, and vanilla
itself reuses it across files.

**R5 is also retired:** the inbound references above are `GenericEntity : "{GUID}…Bed_XX.et"` entries inside
furniture composition prefabs — i.e. the beds in houses are **prefab instances**, so a same-path override does
reach them.

### Phase 3 — 2026-08-18 (complete, `tools/compile-check.sh` exit 0, 6062 files)

**New surface**

| Symbol / file | Notes |
|---|---|
| `class OVT_SleepAction : ScriptedUserAction` | `Scripts/Game/UserActions/OVT_SleepAction.c` (new) |
| `Prefabs/Props/Furniture/{Bed_01,Bed_02,BedDouble_01,BedDouble_02,BunkBed_01,BunkBed_01_Double}.et` (+ `.et.meta`) | same-path deltas |
| `Prefabs/Props/Furniture/BunkBed_02/BunkBed_02.et` (+ `.et.meta`) | same-path delta |
| `#OVT-Sleep`, `#OVT-SleepCooldown`, `#OVT-SleepQRF`, `#OVT-SleepWanted`, `#OVT-SleptWell` | `.st` + all seven runtime exports |

**GUIDs consumed from the reserved `{6B5D0000000000XX}` series**

| Range | Consumer |
|---|---|
| `01`-`05` | the five `CustomStringTableItem`s (Sleep, SleepCooldown, SleepQRF, SleepWanted, SleptWell, in that order) |
| `10`-`14` | `Bed_01.et` (ActionsManagerComponent, UserActionContext, PointInfo, OVT_SleepAction, UIInfo — always in that order) |
| `20`-`24` | `Bed_02.et` |
| `30`-`34` | `BedDouble_01.et` |
| `40`-`44` | `BedDouble_02.et` |
| `50`-`54` | `BunkBed_01.et` |
| `60`-`64` | `BunkBed_01_Double.et` |
| `70`-`74` | `BunkBed_02/BunkBed_02.et` |

**`80` upward is free for Phase 4** (the cot prefab and its placeable entry).

**Per-bed action context tuning (judgement call).** No vanilla bed carries a `UserActionContext`, so there was
no offset to copy and no dimension file to read — the `.xob` is binary and the `.et` records no extents. The
values below are reasoned from the model each prefab references and from vanilla's own furniture contexts
(`ArmaReforger/Prefabs/Props/Furniture/Toilet_01.et:24-30` uses `Radius 0.7` on a fixture the player stands
directly over; `BarTap_01.et` puts its point at tap height, i.e. Y is up in `Offset`).

| Prefab | `Offset` | `Radius` | Why |
|---|---|---|---|
| `Bed_01`, `Bed_02` | `0 0.6 0` | 1.2 | single beds ~0.9 × 2.0 m; the point sits at mattress height in the middle, so a player standing at either long side is ~1.1 m away |
| `BedDouble_01`, `BedDouble_02` | `0 0.6 0` | 1.5 | ~1.5-1.6 m wide, so the side-standing distance is ~0.3 m greater than a single |
| `BunkBed_01`, `BunkBed_02` | `0 0.9 0` | 1.3 | two tiers: the point is midway between the lower (~0.5 m) and upper (~1.4 m) mattress so neither is out of reach; the extra 0.1 m radius pays for the vertical offset |
| `BunkBed_01_Double` | `0 0.9 0` | 1.7 | two bunks side by side (~2 m wide) — the widest of the seven |

These are approach ranges, not hitboxes: too small means a player standing beside the bed sees nothing, too
large means the action follows them around the room. **They are a play-test dial**, and step 1 of the
Verification Method (a bed inside an owned house) is where a wrong one shows up.

**Judgement calls.**

1. **The gates resolve the actor as `SCR_PlayerController.GetLocalControlledEntity()`, not the `user` argument.**
   The action is `HasLocalEffectOnlyScript`, single player only and its owner prefabs have no `RplComponent`, so
   the local controlled entity is both the honest answer and the only one available in `GetActionNameScript`,
   which gets no `user` at all. Using two different sources for "who is asking" between the three overrides
   would be the real hazard.
2. **An EMPTY reason key from `CanSleep` refuses WITHOUT calling `SetCannotPerformReason`.** Phase 2 made the
   empty key mean "not single player / no record / no clock" — states `CanBeShownScript` already hides — so
   inventing a sixth localization key for them was explicitly out of scope.
3. **`GetActionNameScript` is not cached.** It re-reads the cooldown every frame (one map lookup plus a clock
   read), because the label is a live countdown and a 1 s cache would buy nothing visible. The expensive gate —
   the four-collection location walk — is the one behind `CHECK_TTL_MS`.
4. **`Radius` is authored, `Omnidirectional` is not.** Vanilla's furniture contexts set `Omnidirectional 0`
   (approach from one side only), which is right for a tap or a toilet and wrong for a bed you can walk around.
   Leaving the inherited default keeps both sides usable.
5. **The `.et.meta` files list six platform configurations** (PC, XBOX_ONE, XBOX_SERIES, PS4, PS5, HEADLESS),
   copying `Prefabs/Props/Military/Furniture/FurnitureMilitary_base.et.meta` — the repo's other vanilla-GUID
   furniture delta — rather than `SignBusStop_01.et.meta`, which predates PS5 and omits it.

**The `.st` brace balance was 1556/1556 before the edit and 1566/1566 after** (each `CustomStringTableItem`
contributes two pairs: the GUID literal and the block). The file's missing trailing newline was preserved.

**The seven `.conf` exports were edited structurally, not by line offset.** Each file's `Ids` and `Texts`
blocks were parsed into parallel entry lists — `Texts` entries can span several physical lines, joined by a
trailing `\`, and contain `\"` escapes, so a naive line-index insert would have silently desynchronised every
key after the insertion point. Both halves were 777 entries before and 782 after in all seven files, and the
alignment was re-verified afterwards by looking up unrelated keys (`OVT-SoldierCost`, `OVT-Skill_Trade`,
`OVT-StartUprising`) and confirming their translated text still came back.

⚠️ **The plan's literal acceptance grep is unsatisfiable as written.** `grep -c "OVT-Sleep"
Language/localization_Overthrow.en-us.conf` returns **4** (`OVT-SleptWell` does not contain the substring
`OVT-Sleep`) and the `Texts` half returns **0**, because the texts are English prose, not keys — there is
nothing in a `Texts` block for that pattern to ever match. The equivalent structural check is the
entry-count-plus-alignment one above, and it passed.

### Phase 4 — 2026-08-18 (complete, `tools/compile-check.sh` exit 0, 6062 files)

**New surface**

| File | Notes |
|---|---|
| `Prefabs/Props/Military/Furniture/OVT_Cot_Placed.et` (+ `.et.meta`) | new mod-owned prefab over the vanilla US cot |
| `Configs/Resistance/placeables.conf` | one `OVT_Placeable` "Cot", inserted between `Furniture` and `PirateRadio` |
| `#OVT-Place_Cot`, `#OVT-Place_Cot_Description` | `.st` + all seven runtime exports |

**GUIDs consumed from the reserved `{6B5D0000000000XX}` series**

| GUID | Consumer |
|---|---|
| `80` | the cot prefab's resource GUID (`.et.meta` `Name`) |
| `81` | `OVT_PlayerOwnerComponent` |
| `82` / `83` | `SCR_EditableEntityComponent` / its `SCR_EditableEntityUIInfo` |
| `84` / `85` / `86` | `ActionsManagerComponent` / `UserActionContext` / `PointInfo` |
| `87` / `88` | `OVT_SleepAction` / its `UIInfo` |
| `90` | the `OVT_Placeable` "Cot" config entry |
| `91` / `92` | the two `CustomStringTableItem`s (`OVT-Place_Cot`, `OVT-Place_Cot_Description`) |

**`93` upward (and `06`-`09`, `89`, `8A`-`8F`) remain free.**

**Verified inbound facts**

- Vanilla cot GUID/path `{C9CFDED29542A968}Prefabs/Props/Military/Furniture/CotMilitary_US_01.et` — confirmed
  against the reference tree by inbound references *and* against this repo's own existing uses
  (`Configs/Resistance/placeables.conf:138` in the `Furniture` entry, `Prefabs/Structures/Military/FOB/OVT_RecruitmentTent.et:109`).
- Preview texture `{6E215B4D7224319B}UI/Textures/EditorPreviews/Auto/Props/Military/Furniture/E_CotMilitary_US_01.edds`
  — GUID recovered from an inbound reference in the vanilla tree; the file exists at that path.

**T4.4 — persistence verification (D9). Nothing changed; `git status` shows `Configs/Systems/Persistence/Overthrow.conf` clean.**

| Claim | Where it actually is | Verdict |
|---|---|---|
| A rule matches on the component, not on a prefab | `Configs/Systems/Persistence/Overthrow.conf:152-170` — `EntityPersistenceConfig "{6B0E7A215A7FD39C}"` with `Rule ComponentClassPersistenceConfigRule "{6B0E7A226B80E4AD}" { ComponentClass "OVT_PlaceableComponent" }` at `:153-155` | ✅ the cot is covered the moment it carries the component |
| `SelfSpawn 1` | `:158` (with `Priority 35000` `:157`, `ParentHandling "Ignore always"` `:159`) | ✅ the entity re-spawns itself from the save, so no placement-manager replay is needed |
| The component serializers cover what the cot carries | `:160-169` — `GenericEntitySerializer`, `OVT_PlaceableComponentSerializer` `:163`, `OVT_PlayerOwnerComponentSerializer` `:165`, `BaseInventoryStorageComponentSerializer` `:167` | ✅ the cot carries `OVT_PlaceableComponent` and `OVT_PlayerOwnerComponent`; it has no storage, so the third serializer simply never matches |
| `PlaceItem` tracks the placed entity | `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c:773` — `OVT_PersistenceTracking.Track(entity);`, last, after the handler-reject delete | ✅ exactly the line the plan cited |
| …and stamps ownership | same file `:707-716` (`OVT_PlaceableComponent` lookup + `SetOwnerPersistentId`) and `:761-767` (`OVT_PlayerOwnerComponent.SetPlayerOwner` + `SetLocked(false)`) | ✅ both components the cot carries are populated at placement |

**Drift from the plan:** the config block is `:152-170`, not `:152-171` — `:171` is already the next
`EntityPersistenceConfig` (`OVT_BuildableComponent`). `PlaceItem`'s `Track` call is at `:773` exactly, as written.

**Judgement calls.**

1. 🔴 **The `OVT_PlaceableComponent` on the cot reuses the INHERITED component GUID `{65CE9A2ECEC30E10}`, not a
   new one from the reserved series.** The plan's §3.7 says "new GUIDs from the reserved series", and that is
   right for every component the cot *adds* — but `OVT_PlaceableComponent` is not added, it is **inherited**:
   `CotMilitary_US_01.et` inherits `FurnitureMilitary_base.et`, which this repo already overrides at the same
   path (`Prefabs/Props/Military/Furniture/FurnitureMilitary_base.et:4`) to add
   `OVT_PlaceableComponent "{65CE9A2ECEC30E10}" { m_sPlaceableType "Furniture" }`. Declaring a *fresh* GUID
   would have left the cot with **two** `OVT_PlaceableComponent`s and made `FindComponent` — which
   `PlaceItem:707` and every type-matching job condition call — return whichever one the engine ordered first.
   Reusing the inherited GUID is the delta form: one component, type `"Cot"`. `tools/check-placeables.py`
   confirms the resolution, reporting `OK Cot type 'Cot' across 1 prefab(s)` — it walks the chain, so it would
   have reported `Furniture` had the override not landed.
2. **`RplComponent "{5624A88DC2D9928D}"` for the same reason.** `DestructibleMultiPhase_Props_Base.et:16` already
   declares an `RplComponent` under that GUID (from a `.ct` base, so it is off by default); the `{ Enabled 1 }`
   block turns it on. This is precisely what `OVT_CabinetMetal_01_grey_V1.et:9-11` and
   `OVT_PirateRadio.et` both do, and it is the D10 replication requirement satisfied.
3. **`ID "F0DBA538AC2A0552"`** — the repo convention for a new mod-owned prefab (`OVT_PirateRadio.et:2`,
   `OVT_CabinetMetal_01_grey_V1.et:2`, `FurnitureMilitary_base.et:2`). Vanilla itself reuses this root id
   across unrelated files, and a derived prefab's root id need not match its parent's (vanilla
   `CotMilitary_US_01.et` is `5272E314F01B2DBE` over a parent that is `F0DBA538AC2A0552`).
4. **Action context: `Offset 0 0.5 0`, `Radius 1.2`.** A cot's mattress sits ~0.45 m up and the frame is
   narrower than a civilian single bed, so the point is 0.1 m lower than the beds' `0 0.6 0` while the radius
   matches the singles — a player standing at either long side is ~1.1 m from the centre. Same play-test dial
   as the seven beds; `Omnidirectional` is likewise left inherited so both sides work.
5. **`m_iCost 30`, no `m_iRewardXP`, no flags.** The minimal five-field Ammobox shape as instructed; 30 sits
   just above `Furniture`'s 25 (a cot that does something). No `m_bIllegal` — sleeping gear is not propaganda —
   and no `m_bAssociateWithNearest 0`, so the default association applies exactly as it does for the ammobox.
6. **Entry position: between `Furniture` and `PirateRadio`.** The list has no ordering contract in code (it is
   indexed by position, and the build menu renders in file order), so the cot was appended after the last
   "prop" entry rather than at the end, to keep the propaganda/illegal items last. The `Furniture` entry was
   not touched, as instructed — it still randomises across the two raw vanilla cots.

**Localization.** `.st` brace balance **1566/1566 before, 1570/1570 after** (+2 per item: the GUID literal and
the block); the file's missing trailing newline was preserved. The seven `.conf` exports were edited with the
same structural parser Phase 3 used — `Ids` split one-per-line, `Texts` grouped into entries by trailing-`\`
continuation, insert by entry index, never by line offset. All seven were **782 entries per half before and 784
after**, and alignment was re-verified afterwards by looking up unrelated neighbours (`OVT-Place_Camp`,
`OVT-Place_EquipmentBox`, `OVT-SoldierCost`, `OVT-Skill_Trade`, `OVT-StartUprising`) and confirming their
translated text still came back, plus Phase 3's own `OVT-Sleep`/`OVT-SleptWell`. Non-English exports carry the
English text as the placeholder, per `6085521a`. A Workbench re-export is still owed at the end of the feature.

**Not verified here (play-test items).** That the cot actually appears in the build menu, places, carries a
working Sleep action and survives a save/load — DoD **F12**/**I1**. `tools/check-placeables.py` exits 0 and is
the only static gate that exists for this.

### Phase 5 — 2026-08-19 (complete, `tools/compile-check.sh` exit 0, 6062 files)

**New surface** (all on `Scripts/Game/Services/OVT_SleepService.c` — no new file, no new class)

| Symbol | Notes |
|---|---|
| `static const float OVT_SleepService.FADE_DURATION = 1.0` | seconds, out and back in |
| `static const float OVT_SleepService.BLACK_DURATION = 0.5` | seconds held black |
| `static const string OVT_SleepService.HINT_SLEPT_WELL = "#OVT-SleptWell"` | the wake cue |
| `static const int OVT_SleepService.HINT_DURATION = 4` | seconds, matching `OVT_SetHomeAction.c:12` |
| `protected static void OVT_SleepService.SetFade(bool toBlack)` | the whole fade, null-guarded end to end |

Three call sites, and that is the entire diff: `SetFade(true)` in `ScheduleSleep()` on the line **before** the
`CallLater`; `ShowCustomHint(...)` then `SetFade(false)` as steps 5 and 6 at the end of `PerformSleepNow()`.
`WORK_DELAY_MS` was **not** changed — Phase 2 already set it to 1500 = `(1.0 + 0.5) * 1000`; only its doc
comment was rewritten now that the two durations it was derived from actually exist. The lazily-created
instance and the `ScheduleSleep`/`PerformSleepNow` split are untouched (`CallLater` cannot schedule a static
function).

**Q4 — the guarantee is STRUCTURAL, not merely likely.** `SetFade` returns `void`, is called for its effect
only, and swallows both null cases itself; `ScheduleSleep` then calls `CallLater(PerformSleepNow, ...)` on the
next statement with **no branch, no early return and no value from the fade between the two**. There is no
control-flow path in which a missing `SCR_ScreenEffectsManager` or a missing `SCR_FadeInOutEffect` reaches the
schedule. Proven as instructed by temporarily replacing the lookup with `SCR_ScreenEffectsManager manager =
null;` — compile-check stayed exit 0 (6062 files) and the skip path is unchanged by inspection — and the edit
was reverted; `grep -n GetScreenEffectsDisplay` confirms the real lookup is back at `:396`.

**T5.2 — static half. Findings, with citations.**

| Claim | Evidence | Verdict |
|---|---|---|
| Overthrow's controller is a delta that only ADDS | `Prefabs/Characters/Core/OVT_PlayerController.et:1-9` — parent `{225E51284CC95CFA}…/DefaultPlayerControllerMP.et`, a `components { }` block containing exactly `OVT_PlayerStartMenuHandlerComponent "{66C6016D1E335793}"` (`:4`) and `OVT_RespawnScreenHandlerComponent "{6A83D5A0000000D0}"` (`:6`) | ✅ nine lines total; no `SCR_HUDManagerComponent`, no `InfoDisplays`, nothing removed |
| The MP parent keeps the effects tree | `ArmaReforger/Prefabs/Characters/Core/DefaultPlayerControllerMP.et:12-31` re-declares `SCR_HUDManagerComponent "{2FDC275D9EBCDB8B}"` and `SCR_ScreenEffectsManager "{5AF2CE6B10209B7D}"` under **the same GUIDs** as its own parent, adding only a `SCR_BleedingScreenEffect` override | ✅ same-GUID = delta, not replacement, so the sibling effects survive |
| The fade effect itself | `ArmaReforger/Prefabs/Characters/Core/DefaultPlayerController.et:49-53` — `SCR_FadeInOutEffect "{5AF2CE6898574BE6}" { m_eLayer BACKGROUND; m_bAdaptiveOpacity 0; m_eShow 47 }`, a sibling of the six other screen effects at `:24-48` | ✅ exactly where the plan said, inherited two levels down |

That the effect is *registered at runtime* still needs the play-test — it is item 3 of the checklist in
`context.md`. Nothing was changed on `OVT_PlayerController.et`.

**T5.4 — static half.** The action adds no keybinding: it is a `ScriptedUserAction` instanced inside a
`UserActionContext` on each bed prefab and on the cot, so it rides the standard user-action interaction bind
like every other Overthrow action. `git status` confirms no input `.conf` is touched anywhere in this feature.
`python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` → exit 0,
`0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged`; all three notes are the shipped
`OverthrowMainMenu`/`OverthrowVehicleMenu` combos and none involves sleep. ⚠️ Per the known blind spot, that
script cannot see inline `ActionContext` actions — but this feature declares none, so the blind spot does not
apply here.

**T5.5 — the audit, key by key.** Enumerated from the source rather than from the plan: the five keys
`grep -oh '#OVT-[A-Za-z0-9_]*'` finds across `OVT_SleepAction.c` / `OVT_SleepService.c` /
`OVT_SleepSchedule.c`, plus the two the cot's `placeables.conf:150-151` entry and `OVT_Cot_Placed.et:14-15`
reference. Parsed with the structural entry parser (Ids one-per-line; Texts grouped into entries by
trailing-`\` continuation), never by line offset.

| Key | Referenced from | `.st` | 7 exports | English text |
|---|---|---|---|---|
| `OVT-Sleep` | `OVT_SleepAction.c:99`; `UIInfo Name` on all 7 beds + the cot (`:35`) | 1 item ✅ | 7/7 ✅ | "Sleep" |
| `OVT-SleepCooldown` | `OVT_SleepService.REASON_COOLDOWN` | 1 ✅ | 7/7 ✅ | "You need to stay awake longer before sleeping again" |
| `OVT-SleepQRF` | `OVT_SleepService.REASON_QRF` | 1 ✅ | 7/7 ✅ | "You cannot sleep while an attack is underway" |
| `OVT-SleepWanted` | `OVT_SleepService.REASON_WANTED` | 1 ✅ | 7/7 ✅ | "You cannot sleep while you are wanted" |
| `OVT-SleptWell` | `OVT_SleepService.HINT_SLEPT_WELL` (new this phase) | 1 ✅ | 7/7 ✅ | "You slept for 8 hours" |
| `OVT-Place_Cot` | `placeables.conf:150`, `OVT_Cot_Placed.et:14` | 1 ✅ | 7/7 ✅ | "Cot" |
| `OVT-Place_Cot_Description` | `placeables.conf:151`, `OVT_Cot_Placed.et:15` | 1 ✅ | 7/7 ✅ | "A military cot you can sleep on" |

All seven exports: **784 `Ids` entries and 784 `Texts` entries, aligned**, with the four unrelated probes
(`OVT-SoldierCost`, `OVT-Skill_Trade`, `OVT-StartUprising`, `OVT-Place_Camp`) still returning their own text.
`.st` brace balance **1570/1570**, unchanged — Phase 5 added no localization key, it only consumed one Phase 3
already shipped. A Workbench re-export is still owed at the end of the feature (the exports are hand-mirrored).

**No raw `#OVT-` key can reach the screen from anything this feature added.** The three refusal reasons go
through `SetCannotPerformReason` only when non-empty (`OVT_SleepAction.c:80`), the label is the
key-plus-literal form the engine translates (`:99`, `"#OVT-Sleep (" + FormatRemaining(h) + ")"`), the hint is a
bare key, and all seven keys resolve in every export above.

**Judgement calls.**

1. **The fade back in is at the END of `PerformSleepNow()`, not on a second `CallLater(+0.5 s)`.** §3.2's
   sketch (step f) nests another delay; T5.1 specifies the `SCR_FastTravelComponent` shape, where `Teleport()`
   does the work and fades back in in the same frame (`:279-296`). The FastTravel shape was used: it is the one
   path in the base game known to work from script, and a second timer would be a second thing to get wrong for
   half a second of black.
2. **`SetFade(true)` lives inside `ScheduleSleep`, after the re-entry guard, not in `PerformSleep`.** A second
   perform while a skip is in flight returns before scheduling; putting the fade earlier would let it re-fade to
   black without a matching fade-in. One fade out per scheduled skip, always paired.
3. **The hint fires before the fade back in**, so it is already on screen as the world reappears; the fade takes
   1.0 s and the hint lasts 4.
4. **`WORK_DELAY_MS` stayed a literal `1500` rather than becoming `(int)((FADE_DURATION + BLACK_DURATION) * 1000)`.**
   The plan contradicts itself on this delay (§3.2 says 1.2 s, T5.1 says 1.5 s) and Phase 2 already settled on
   1.5 s; a derived expression buys nothing at a call queue that takes an int, and the doc comment now names both
   constants so a change to either is visible.

### Phase 6 — 2026-08-19 (T6.1/T6.2 complete, T6.3 blocked; `tools/compile-check.sh` exit 0, 6062 files)

**Surfaces audited.** Three, as the agent's brief requires.

| Surface | Finding before this phase | Action |
|---|---|---|
| `Configs/Tutorials/*.conf` (16 entries) | no mention of sleeping, beds or the cot | **nothing added, deliberately** — see the gap note below |
| `Configs/FieldManual/Categories/FM_Overthrow.conf` (the only category file) | no mention of sleeping; no enumerated placeable list anywhere, the nearest thing being the "Where Placing Works" paragraph of *Camps and Placing* | one new entry + one new paragraph |
| `https://wiki.armaoverthrow.com` | **not audited** — no wikijs tooling in this session | T6.3 left open |

**T6.1 — one new Field Manual entry, "Sleeping", last in the "The Resistance" category**
(`FM_Overthrow.conf`, after *Base Capture*). Nine content pieces: an untitled opening paragraph
then four header/paragraph pairs — *Where a Bed Works*, *The Hours Still Count*, *Twelve Hours
Between Sleeps*, *When It Will Not Perform*.

**T6.2 — one new paragraph in *Camps and Placing***, appended under the existing "Where Placing
Works" header, naming the cot, its cost and the fact that it carries the same action. Written as its
**own string item rather than appended to `OVT-FieldManual_Camps_Text2`**, so that paragraph's
existing German and Ukrainian translations stay valid instead of silently going one sentence short.

**GUIDs consumed from the reserved `{6B5D0000000000XX}` series**

| Range | Consumer |
|---|---|
| `93` | the cot `SCR_FieldManualPiece_Text` inside the *Camps and Placing* entry |
| `94` | the `SCR_FieldManualConfigEntry_Standard` for *Sleeping* |
| `95`-`9D` | its nine content pieces, in document order (Text, Head, Text2, Head2, Text3, Head3, Text4, Head4, Text5) |
| `A0`-`AA` | the eleven `CustomStringTableItem`s |

**`9E`-`9F`, `AB` upward — and `06`-`09`, `89`, `8A`-`8F` — remain free.** Verified: a repo-wide grep
for `6B5D0000000000[93-9D,A0-AA]` returns exactly 11 hits in the conf and 11 in the `.st`, nothing else.

**The citation ledger is in the strings themselves.** Every one of the eleven items carries a
`Comment` in the established Field Manual form (`OVT-FieldManual_Camps_Text2`'s is the model): what
the string is, how many sentences, whether it carries rich-text markup, then `FACT-CHECKED
2026-08-19 against …` with a `file:line` for every factual clause. That is where a later reader
should look, not at this note.

**One claim was cut for lack of a citation.** A closing sentence for *When It Will Not Perform*
reading "it becomes available again once the attack is over or the heat has died down" was drafted
and removed: the QRF half is citable (`OVT_SleepService.c:223-228` reads a live flag) but the wanted
half asserts that a wanted level decays on its own, and this feature's code only ever *reads*
`GetWantedLevel()` (`:236`). Rather than go and verify a neighbouring system's decay to justify half a
sentence, the sentence was replaced with the hidden-versus-disabled contrast, which is fully cited.

**Numbers deliberately not quoted.** The four location distances (30 / 75 / 100 / `baseRange`) are
in the code and would have been citable, but `OVT-FieldManual_Camps_Text2` already established that
this manual names the four *places* and not their radii, and one of the four is difficulty-derived.
The eight hours, the twelve hours, the six-hour mark, seven in the morning, midnight, the quarter
hour and the cot's cost of thirty are all literals in the source and are quoted.

**Localization.** Eleven new keys, all `#OVT-FieldManual_*`:
`OVT-FieldManual_Placing_Cot_Text`, `OVT-FieldManual_Sleep_Title`, `_Text`, `_Head`, `_Text2`,
`_Head2`, `_Text3`, `_Head3`, `_Text4`, `_Head4`, `_Text5`.
`.st` brace balance **1570/1570 before, 1592/1592 after** (+2 per item). Items inserted in
case-insensitive sorted position, matching the file's existing order. The seven runtime `.conf`
exports were edited with the same **structural entry parser** Phases 3-5 used — `Ids` one per line,
`Texts` grouped into entries by trailing-`\` continuation, insert by entry index, **never by line
offset** — and the `Ids`/`Texts` block bounds were located by the exact one-space-indented `}` rather
than by counting braces, because a text entry may legally contain one. All seven went **784 → 795
entries per half**, and alignment was re-verified after the insert by confirming five unrelated
probes (`OVT-SoldierCost`, `OVT-Place_Camp`, `OVT-StartUprising`, `OVT-Sleep`, `OVT-SleptWell`) still
resolve to their own text. Non-English exports carry the English text as the placeholder, per
`6085521a`. **A Workbench re-export is owed** — until it happens these eleven keys render as raw keys.

**Gaps left on purpose.**

1. **No tutorial popup was added.** The tutorial requirements say to keep volume restrained and to
   prefer an existing entry over a new popup, and there is no trigger event that means "a Sleep
   action just became available to you": `OVT_TutorialTrigger.c:12-45` enumerates fourteen events and
   the closest, `PLAYER_PLACE` (`:21`, filterable by placeable name, so `"Cot"` would match), would
   fire only for players who build a cot and never for the bed path, which is the common one. Adding
   that event belongs to the tutorial-system feature, not here.
2. **The entry uses the shared `default_ui.edds` tile** (`{CF6B203430123E78}`) because there is no
   `sleeping_ui` art. Every other Field Manual entry has its own tile; a dedicated one is owed.
3. **T6.3, the public wiki, was not touched.** See the task above.

### Phase 7 — 2026-08-19 (complete, `tools/compile-check.sh` exit 0, 6062 files)

**Two changes. One is a user decision, one is a defect the cross-phase review found; neither was visible to
any assertion in the tree before this phase.**

#### T7.1 — the occupying faction's two window edges (D18 / D18a)

**The defect.** `OVT_EconomyManagerComponent` gets both edge defences — a flush `CheckUpdate()` at `:253` for
the open start and `AssertHourLatches(landingHour)` at `:279` for the closed end.
`OVT_OccupyingFactionManager.HandleTimeSkip` had **neither**, and its live gates had **no latch to assert**.
The two consequences:

| Start | What happened | DoD |
|---|---|---|
| 10:00, lands exactly on 18:00 | replay paid 12:00 **and** 18:00; the tick that resumes ~a second later still reads `m_iMinutes == 0` at 18:00 and paid a **third** `Gaining Resources`/`Reserve Resources` pair | **Q1**, **F7** |
| exactly 12:00 | the half-open replay excludes 12:00 and the live tick had most likely not fired it — the payday was **lost** | **Q2** |
| any hour, start minute in {0,15,30,45} (1/15 of sleeps) | the replay's last step lands ON the landing instant and the resumed tick decays again — **33** decay steps; and at the front the 12:00-style start **loses** one | **F7** |

**The fix, and its exact diff shape.** Two new members initialised to `-1` (the armed state, matching the
economy latches' convention), and the two live gates each gain **exactly one condition and one assignment**:

```
	 && time.m_iMinutes == 0                          <- unchanged
	 && m_iHourGainedResources != time.m_iHours)      <- ADDED
	{
		m_iHourGainedResources = time.m_iHours;       <- ADDED
		GainAndSpendResources();                      <- the four lines that were here, extracted
	}
```

and, inside the quarter-hour branch whose own `if` expression is byte-identical to before:

```
		if(m_iMinuteDecayedThreat != time.m_iMinutes) <- ADDED
		{
			m_iMinuteDecayedThreat = time.m_iMinutes; <- ADDED
			DecayThreatStep();                        <- unchanged call
		}
```

`HandleTimeSkip` then flushes the start (each grid behind its own latch) and asserts both latches at the
landing instant: `m_iHourGainedResources = LandingHour(startHour, hours)` and
`m_iMinuteDecayedThreat = startMinute` (the landing minute-of-hour, because the skip is whole hours and
`AdvanceClock` preserves minutes and seconds).

**Why normal play is provably unchanged (Q9 / R6).** The gate is only reachable when the minute is exactly on
the boundary. The latch starts at `-1`, which no boundary value can equal, so the first boundary a fresh or
freshly loaded campaign reaches always fires. Consecutive boundaries are always *different* numbers
(0→6→12→18, and 0→15→30→45), so a latch holding the last one can never suppress the next — the same argument
that has always made `m_iHourPaidIncome` safe without an else-reset, which is why this mirrors it including
the missing reset. The only thing the latch can therefore suppress is a **second run inside the same in-game
minute**, which is a strict de-duplication and, today, a real (if rare) double-pay whenever two ticks land in
one in-game minute. **This applies on dedicated servers too**, not only on the SP sleep path, because the
latch is on the live gate; that is stated in the code and is the correct trade.

**The 33rd decay step: FIXED, not accepted.** The review called it cosmetic and left the choice open. It was
fixed because the error is not only an extra step at the end — a start on a quarter hour also **loses** one at
the front, which is Q2's class of defect, and the fix is the identical five-line idiom. `Reduced Threat to:`
now prints exactly **32** times for a start off the quarter-hour grid and **33** for a start on it, the extra
line being the flush paying a boundary the live tick owed and had not run. **The town-uprising scan in the
same `if` block was deliberately left unlatched** — it is a `PlayerInRange`-gated world scan, not a payload
owed once per boundary, and latching it would change behaviour with no accounting argument behind it.

**Judgement call: a third extraction.** The four-line boundary payload (`GainResources`,
`SpendResourcesOnBases`, `UpdateSpecops`, `Print`) already existed twice — Phase 1 copied it into the replay
loop. The flush would have made a third copy, and §7 says in as many words that a second implementation of a
payout is a defect by construction. `GainAndSpendResources()` is therefore a pure move in the same spirit as
T1.5/T1.6, and D1 is not violated: no event bus, no restructuring, the gate expressions untouched.

**Judgement call: a new pure predicate.** `OVT_SleepSchedule.IsStepBoundary(absoluteMinute, stepMinutes)`,
symmetric with `IsIntervalBoundary`. The alternative was `% 15` inline in a `CheckUpdate` seam, which §7
forbids and `context.md` already records as the reason `StepMinuteAt` exists. The gain flush needed no new
function at all — `IsIntervalBoundary(startHour * 60 + startMinute, 6)` is exactly "the start instant is a
six-hour boundary", in one call.

#### T7.2 — the cooldown runs from waking (D16a)

One expression: `StampCooldown` writes `ReadAbsoluteGameHours(time) + SKIP_HOURS` (`OVT_SleepService.c:456`).
The sequence in `PerformSleepNow` is untouched and the **post-skip clock is deliberately not read** —
`preSkipAbs + SKIP_HOURS` *is* the wake instant by definition, keeps the stamp on the same clock read the
accounting catch-up used, and does not depend on the `SetDate` ladder having worked.

**Every place the "about four hours" claim was chased to:**

| Where | What it said |
|---|---|
| `OVT_SleepService.c:33-38` | `COOLDOWN_HOURS` doc: "measured from the moment they lay down … wake with four left" |
| `OVT_SleepService.c` `PerformSleepNow` step 3 | "eight of them are spent by the skip and they wake with four" |
| `OVT_SleepService.c` `StampCooldown` doc | rewritten end to end (wake instant, why not the post-skip clock, the one accepted edge) |
| `OVT_PlayerData.c:85-102` | field doc: "at the moment this player last performed the Sleep action … wake with four left" |
| `OVT_PlayerManagerSerializer.c:64-74` + VERSION HISTORY | "at the moment this player last slept" → "last **woke**", plus a note that the MEANING changed with no format change |
| `Language/localization_Overthrow.st` `OVT-FieldManual_Sleep_Text4` | "so about four are left on waking" — text **and** its `FACT-CHECKED` ledger |
| all seven `localization_Overthrow.*.conf` | the same English sentence, mirrored |
| `implementation.md` | §3.2 step (c) and the label sketch, §3.4's "Consequence, intended", D16 (superseded, not deleted) + new D16a, F8, Verification step 7, §8's cooldown row |
| `context.md` | play-test items 7, 10, 11, 14 |

**Migration.** No serializer change: same float, same position, same write. A stamp written under the old
meaning reads eight in-game hours early and expires that one cooldown eight hours sooner — the fail-open
direction — and the feature is unreleased, so no version bump and no migration were warranted. This is
recorded in the field's own comment so a later reader does not go looking for a version 6.

**Judgement call: two `Print`s were added to `AdvanceClock`/`AdvanceDate`.** `SetTimeOfTheDay` and `SetDate`
both return `bool` and both returns were being discarded. Nothing branches on them now either — by the time
they are called the accounting has run and the stamp is written, and there is nothing sensible to undo — but
a refusal is now logged, because D16a made a frozen clock degrade *worse* than it used to: the stamp is then
in the future, `CooldownRemainingHours` fails open on it (D17), and the player could sleep repeatedly and
replay the accounting each time. Under the old lie-down stamp the same failure blocked for twelve hours
instead. The precondition is "the eight-hour skip visibly did nothing", which `context.md`'s play-test item 6
already calls a real defect, so the exposure is bounded — but it must not be silent. This is the only change
in this phase that neither the review nor the user asked for.

**One accepted edge, written into `StampCooldown`'s doc.** `AbsoluteGameHours` is monotonic rather than
calendar-exact (`DAYS_PER_YEAR` is 366 for every year), so on the one night a year a skip crosses new year
the real post-skip stamp is 24 h larger than `pre-skip + 8` and the cooldown reads as already expired. Same
fail-open direction as D17, at most one extra sleep per in-game year, and the alternative — reading the
post-skip clock — would trade it for a dependency on the `SetDate` ladder, which is the worse of the two.

#### T7.3 — the Logic tier

One case changed, two added. The changed rows and both new cases carry their own can-fail entries in the case
preambles; the fault ids continue the file's existing series.

| Fault | Injected into | Compiles | First row it turns red |
|---|---|---|---|
| **M14** (new) | `CooldownRemainingHours`: `float remaining = (cooldownHours - 8) - elapsed;` — the rejected four-hour behaviour, re-created numerically | exit 0 | "the instant the sleeper wakes … answered 4.000000 h remaining, expected 12.000000" |
| **M15** (new) | `IsStepBoundary`: `return absoluteMinute == 0;` | exit 0 | IsStepBoundary case, "Minute 15 …"; window-edges case, named row 10:0, "32 … 33". **Nothing that existed before today catches this fault.** |
| **M1** (re-injected) | `CountIntervalCrossings` closed at the start | exit 0 | window-edges case, named row 12:0, "3 times … 2 six-hour boundaries" |
| **M8** (re-injected) | `LandingHour` without the 24-hour wrap | exit 0 | window-edges sweep at 16:0, "answered 24 but the window ends in hour 0" |

Each fault was injected alone, compiled, and the file restored; the restored file was diffed byte-for-byte
against the backup and re-compiled clean. **The expected values above are not guesses** — all three window-edge
claims were first verified against an independent model over all 1440 start minutes (0 disagreements with the
real implementation), and the same model produced each fault's first failing coordinate.

**The window-edges case uses an independent oracle on purpose.** Every `Count*` function on the subject is a
closed-form expression; checking one closed form against another written the same way is how two
implementations agree on the same mistake. `CountBoundariesInClosedWindow` visits every minute of the window
and asks — 1440 starts x 481 minutes, which is why that case alone carries `timeoutS: 60`.

**Not done here.** No suite was run (`.claude/test-policy.md`); the in-harness red/green pass for all three
cases is owed to the orchestrator, and the play-test items in `context.md` are unchanged apart from the four
numbers T7.2 corrected.
