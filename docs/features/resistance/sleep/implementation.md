# Sleep — Implementation Plan

**Status:** Ready for Review
**Started:** 2026-08-18
**Completed:** 2026-08-19
**Last Updated:** 2026-08-19 (post-review: D16a, D18/D18a; suites green 179/179)

---

## 1. Executive Summary

A single-player player can lie down on a bed and skip eight in-game hours. The screen fades to black, the world clock jumps forward, and **every accounting sweep that the skipped hours contained actually runs** — donations and taxes, NPC shop buying, the 7 a.m. restock, midnight rent, occupying-faction resource gain and spend, and the quarter-hourly threat decay. The player wakes up richer, restocked, and facing an occupying faction that spent eight hours reinforcing.

The action is offered on the seven vanilla bed prefabs (via same-path prefab overrides) and on **one new placeable cot**, which is the only new prefab that needs an `RplComponent`. It only appears when the bed is inside an owned house, the player's own camp, a deployed FOB, or a captured base — the same ordered location test the placement system already uses. It is blocked during a QRF and while the player is wanted, and it carries a 12 in-game-hour cooldown that is displayed as a countdown in the action label and survives save/load.

**Two shipped managers gain one public method each.** `OVT_EconomyManagerComponent.HandleTimeSkip(int hours)` and `OVT_OccupyingFactionManager.HandleTimeSkip(int hours)` replay their own payloads the correct number of times and leave their hour latches asserted at the landing hour. No hour-elapsed-event refactor: the shipped `CheckUpdate()` bodies keep their shape, and the catch-up path reuses the same protected methods they call.

**BUG-179 rides along.** The economy manager's three hour latches are not persisted and re-initialise to `-1` on load, so loading a save taken during hour 0/6/12/18 pays the income sweep a second time — a repeatable money exploit shipped today. The fix is the *same operation* the time skip needs afterwards: assert all three latches to the current in-game hour. One private helper, called from startup, from the skip, and from the save-apply hook.

---

## 2. Goals

### Primary

- **G1** A "Sleep" action on any bed inside an owned house, own camp, deployed FOB, or captured base skips 8 in-game hours in single player, with a fade to black and back.
- **G2** Nothing is lost to the skip: income/taxes/donations, NPC shop buying, restock, rent, occupying-faction resource gain and spend, and threat decay all occur exactly as many times as the skipped window contained — no more, no less.
- **G3** A 12 in-game-hour cooldown is enforced, is displayed as a live countdown in the action label, and survives quit → Continue.
- **G4** The action is refused, visibly and with a localized reason, during an active QRF and while the player's wanted level is above zero; it never appears at all outside single player.
- **G5** A new "Cot" placeable can be built at camps/FOBs/bases, carries the Sleep action, and persists like every other placeable.

### Secondary

- **G6** **BUG-179 closed**: loading a save no longer re-fires the income/restock/rent sweep for the hour the save was taken in.
- **G7** The boundary-counting arithmetic lands in a world-free class the Logic tier can pin, so "how many payouts does an 8-hour skip from 11:47 contain" is answered by an asserted function and not by inline arithmetic nobody can test.

### Non-goals (explicitly out of scope)

- **Multiplayer of any kind.** Not listen-host, not dedicated. There are **no new RPCs, no new controller component, and no replicated state** in this feature. A dev agent must not cargo-cult the recruit-ux client→server seam here — in single player `PerformAction` runs on the authority and calls the managers directly.
- Any hour-elapsed event bus, tick refactor, or restructuring of the two `CheckUpdate()` bodies beyond extracting two methods on the occupying-faction manager.
- Catching up **town modifier timers, the job manager, wanted decay, vehicle/recruit offline timers, or radio-tower downtime**. These are wall-clock by design and stay wall-clock (user decision).
- Sleeping to a *specific* time, sleeping in vehicles, sleep needs/fatigue, or an unconscious/animated player body. The player stands there; the screen is black.
- Config knobs. The skip length, the cooldown and the fade durations are constants in one class each.

---

## 3. Architecture Overview

### 3.1 What is new and what changes

| File | New/Changed | Purpose |
|---|---|---|
| `Scripts/Game/Services/OVT_SleepSchedule.c` | **new** | Pure boundary-counting + cooldown arithmetic (Logic tier). No world, no `OVT_Global`. |
| `Scripts/Game/Services/OVT_SleepService.c` | **new** | Static world-side orchestrator: validation, catch-up, clock advance, cooldown stamp, fade. |
| `Scripts/Game/UserActions/OVT_SleepAction.c` | **new** | The user action: visibility gate (cached), disabled reasons, countdown label. |
| `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c` | changed | `HandleTimeSkip(int)`, `AssertHourLatches(int)` (**BUG-179**) |
| `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` | changed | `HandleTimeSkip(int)`, three extractions (`SpendResourcesOnBases(int)`, `DecayThreatStep()`, `GainAndSpendResources()`) and **two live-tick latches** (`m_iHourGainedResources`, `m_iMinuteDecayedThreat` — D18, 2026-08-19) |
| `Scripts/Game/Data/OVT_PlayerData.c` | changed | `float m_fLastSleepGameHours` (persisted, server-only, never replicated) |
| `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c` | changed | version **5**: `lastSleepGameHours` appended last + `version < 5` clear |
| `Prefabs/Props/Furniture/Bed_01.et` … ×7 (+ `.meta`) | **new** (overrides) | Same-path overrides adding an `ActionsManagerComponent` with the Sleep action |
| `Prefabs/Props/Military/Furniture/OVT_Cot_Placed.et` (+ `.meta`) | **new** | The cot placeable — the one prefab with an `RplComponent` |
| `Configs/Resistance/placeables.conf` | changed | One new `OVT_Placeable` entry, "Cot" |
| `Language/localization_Overthrow.st` + the 7 runtime `.conf` exports | changed | New `#OVT-Sleep*` keys |
| `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_SleepSchedule.c` | **new** | Pins the arithmetic |
| `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` | changed | Cooldown stamp survives save → reload |

**No change to:** `OVT_Global` (no new accessor — the service is static, like `OVT_FastTravelService`), any prefab on the game mode or the controller, any JIP payload, any RPC, `Configs/Systems/Persistence/Overthrow.conf` (see D9).

### 3.2 Control flow

```
OVT_SleepAction (client == authority, SP only)
  CanBeShownScript   -> SP? + location gate (1 s cache)          -> OVT_SleepService.IsSleepLocation()
  CanBePerformedScript -> QRF? wanted? cooldown?                 -> OVT_SleepService.CanSleep(user, out reasonKey)
  GetActionNameScript  -> "#OVT-Sleep" | "#OVT-Sleep (11h 12m)"  -> OVT_SleepSchedule.CooldownRemainingHours()
  PerformAction        -> OVT_SleepService.PerformSleep(user)
                              |
                              v
       1. re-validate (CanSleep) ................................. refuse silently if stale
       2. fade to black (1.0 s) .................................. SCR_FadeInOutEffect, null-safe
       3. CallLater(+1.2 s):
            a. economy.HandleTimeSkip(8)   \  READ THE PRE-SKIP CLOCK
            b. occupying.HandleTimeSkip(8) /  (contract: catch-up runs BEFORE the clock moves)
            c. stamp cooldown on OVT_PlayerData (pre-skip absolute game hours + SKIP_HOURS = the WAKE instant, D16a)
            d. advance the clock: SetTimeOfTheDay(landing, true); if wrapped past midnight, SetDate(next day)
            e. hint "#OVT-SleptWell"
            f. CallLater(+0.5 s): fade back in (1.0 s)
```

### 3.3 The catch-up contract

Both `HandleTimeSkip(int hours)` methods share one contract, stated in both doc headers:

> **Called on the authority, BEFORE the world clock is advanced.** The current in-game time is the *start* of the skipped window; the window is `(start, start + hours]` — half-open at the start, closed at the end. Anything owed exactly at `start` is the live tick's job, not ours.

Everything the window contains is derived by `OVT_SleepSchedule`, which knows nothing about managers, the world, or `OVT_Global`:

| Function | Answers |
|---|---|
| `CountIntervalCrossings(h, m, skip, intervalHours)` | how many `hh:00` with `hh % interval == 0` fall in the window (1 or 2 for an 8 h skip at interval 6) |
| `CountHourEntries(h, m, skip, targetHour)` | how many times a specific hour-of-day is entered (restock hour 7, rent hour 0) |
| `CountStepCrossings(h, m, skip, stepMinutes)` | how many 15-minute boundaries are crossed (32 for 8 h, from any start minute) |
| `IsIntervalBoundary(absoluteMinute, intervalHours)` | is this step also a 6-hour boundary (drives the chronological replay) |
| `LandingHour(h, skip)` | the hour-of-day the player wakes in |
| `AbsoluteGameHours(year, dayInYear, hour, minute)` | one monotonic number for cooldown comparison |
| `CooldownRemainingHours(nowAbs, lastAbs, cooldownHours)` | `<= 0` means ready |
| `FormatRemaining(hours)` | `"4h 12m"` for the action label |

**Economy replay** (order mirrors `CheckUpdate` :153-201):

1. `CheckUpdate()` once first — flushes any boundary work owed at the *current* minute that the 10-second tick has not observed yet. This is what makes the half-open window safe.
2. `CalculateIncome()` + `UpdateShops()` once per 6-hour crossing.
3. `ReplenishStock()` if hour 7 was entered.
4. `UpdateRents()` if hour 0 was entered.
5. `AssertHourLatches(LandingHour(...))` — all three latches set to the landing hour.

Step 5 is what makes double-pay impossible: `m_iHourPaidIncome == landingHour` means the resumed tick cannot re-fire on the hour we just landed in, and the next genuine boundary still differs from it. The same holds for stock and rent — the live code compares the latch against a fixed hour (7 / 0), so "landing hour" is the right value in every case.

**Occupying-faction replay** is *chronological*, not batched, because `GainResources()` reads `m_iThreat` and threat decays between gains:

```
(1) FLUSH THE START INSTANT, if it is a boundary the live tick has not latched:   <- added 2026-08-19
        if IsIntervalBoundary(startAbs, 6) and m_iHourGainedResources != startHour:
            m_iHourGainedResources = startHour; GainAndSpendResources();
        if IsStepBoundary(startAbs, 15)      and m_iMinuteDecayedThreat != startMinute:
            m_iMinuteDecayedThreat = startMinute; DecayThreatStep();

(2) for each 15-minute step in the window, in order:
        if the step lands on a 6-hour boundary:  GainAndSpendResources();
        DecayThreatStep();

(3) ASSERT BOTH LATCHES AT THE LANDING INSTANT:                                   <- added 2026-08-19
        m_iHourGainedResources  = LandingHour(startHour, hours)
        m_iMinuteDecayedThreat  = startMinute            (== the landing minute-of-hour)
```

**Steps 1 and 3 are the 2026-08-19 review fix (D18).** The economy manager has always had both edge defences — a flush `CheckUpdate()` for the open start and `AssertHourLatches(landingHour)` for the closed end — and this manager had *neither*, nor any latch on its live gates to assert. Without step 3 a skip landing exactly on a boundary hour paid a **third** `Gaining Resources` pair when the live tick resumed inside the same in-game minute (breaks **Q1**/**F7**); without step 1 a skip beginning exactly on one **lost** that payday (breaks **Q2**). The same argument applies one grid finer to the quarter-hourly decay, which is why both latches exist. `CheckUpdate()` cannot be used as the flush here the way it is on the economy manager — it also rolls the counter-attack, scans towns and returns early on a QRF.

That is the same in-tick order `CheckUpdate` uses (:1178-1258). Two things are deliberately **excluded** from the replay:

- the **counter-attack roll** (:1234-1247) — a random surplus check that would get 32 rolls and is not "accounting";
- the **town-uprising scan** (:1261-1268) — `PlayerInRange`-gated and world-side. A sleeping player is within 300 m of the town they are sleeping in; replaying it 32 times could start a town QRF on top of an unconscious screen-black player.

Both exclusions are written into the method's doc comment so a later reader does not "fix" them.

### 3.4 Cooldown, stored as a game-clock stamp

`OVT_PlayerData.m_fLastSleepGameHours` holds the **absolute in-game hours at the moment sleep was performed** (pre-skip), computed from the engine's own calendar:

```
AbsoluteGameHours(year, dayInYear, hour, minute) = (year * 366 + dayInYear) * 24 + hour + minute / 60
```

Sentinel `-1` = never slept. Remaining cooldown is `12 - (nowAbs - lastAbs)`; the countdown therefore does not care that the clock runs at 6× by day and 12× by night, and it survives save/load for free because the engine persists the time of day and the date.

**Consequence, intended — SUPERSEDED 2026-08-19 (D16a).** ~~the 12 hours are counted from the moment the player lies down, and the skip consumes 8 of them, so a player wakes with **4 in-game hours** of cooldown left. If that ever wants to be wake-relative it is one constant.~~ It did want to be wake-relative, and it was one expression: `StampCooldown` writes `AbsoluteGameHours(pre-skip calendar) + SKIP_HOURS`, i.e. **the wake instant**. The player therefore wakes with the **full 12 in-game hours** ahead of them and cannot sleep again until **20 in-game hours** after lying down. Everything else in this section is unchanged — the stamp is still one float, still read off the pre-skip clock, still compared by subtraction, still `-1` for never.

**Fail-open on a nonsensical delta.** If `nowAbs - lastAbs` is negative (a clock that moved backwards, a date that failed to advance), `CooldownRemainingHours` returns `0` — ready. Fail-closed would lock the action *permanently* on that save, because the stamp could never be reached again. Pinned by a Logic case.

### 3.5 The location gate — one ordering, already written

`OVT_ItemLimitChecker.CountItemsAtLocation` (`Scripts/Game/Utilities/OVT_ItemLimitChecker.c:87-141`) already implements exactly the ordered test the requirement asks for, with the distances the placement system uses:

| Order | Source | Range |
|---|---|---|
| 1 | `OVT_RealEstateManagerComponent.GetNearestOwned(playerId, pos)` | `MAX_HOUSE_PLACE_DIS` 30 m |
| 2 | `OVT_ResistanceFactionManager.GetNearestCampData(pos)`, `camp.owner == playerId` | `MAX_CAMP_PLACE_DIS` 75 m |
| 3 | `OVT_ResistanceFactionManager.GetNearestFOBData(pos)` | `MAX_FOB_PLACE_DIS` 100 m |
| 4 | `OVT_OccupyingFactionManager.GetNearestBase(pos)`, `!IsOccupyingFaction()` | `m_Difficulty.baseRange` |

`OVT_SleepService.IsSleepLocation(vector pos, string playerPersistentId)` reproduces this **ordering and these constants by reference**, not by copy: the four constants are re-declared on the service with a comment naming `OVT_ItemLimitChecker` as the source of truth, and a task in Phase 3 checks they still match. Duplicating the *ordering* is unavoidable (the checker's method returns an item count, not a boolean, and its members are protected) — duplicating the *numbers* silently is what must not happen.

⚠️ `GetNearest*` have **no** maximum range — every one of the four returns the nearest record on the map. The distance check is not optional.

### 3.6 The action

`OVT_SleepAction : ScriptedUserAction`, `HasLocalEffectOnlyScript() => true`.

- **`CanBeShownScript`** — `RplSession.Mode() == RplMode.None` (single player, per `OVT_PlayerStartMenuHandlerComponent.c:77`) **and** the location gate. Both answers cached for 1000 ms behind an expiry stamp, exactly as `OVT_RearmVehicleAction` does (`:23-30`, `RefreshCache`): the gate walks four manager collections and this runs every frame while a player looks at a bed.
- **`CanBePerformedScript`** — visible but disabled, with `SetCannotPerformReason`, for: active QRF (`OVT_Global.GetOccupyingFaction().m_bQRFActive`, :161) → `#OVT-SleepQRF`; wanted level > 0 (`OVT_PlayerWantedComponent.GetWantedLevel()`, :157, read live off the user's character — wanted level is deliberately never persisted) → `#OVT-SleepWanted`; cooldown → `#OVT-SleepCooldown`. Follows `OVT_SabotageTowerAction`'s rule that a relevant blocked action stays visible.
- **`GetActionNameScript`** — returns `false` (prefab-configured name) when ready; when on cooldown returns `"#OVT-Sleep (" + OVT_SleepSchedule.FormatRemaining(h) + ")"`. Key-plus-literal concatenation is the project's established form (`OVT_SabotageTowerAction.c:63`, `OVT_RearmVehicleAction.c:98`) and works because the engine translates the leading `#key` and passes the remainder through.
- **`PerformAction`** — `OVT_SleepService.PerformSleep(pUserEntity)`. No RPC. The service re-validates before doing anything.

### 3.7 Prefabs

**Beds — seven same-path overrides.** None of the seven vanilla bed prefabs exists in this repo today, and none of them has an `ActionsManagerComponent` or an `RplComponent`. Each override is a diff-only file following `Prefabs/Structures/Signs/Traffic/SignBusStop_01.et:1-47`: line 1 repeats **vanilla's own header** (`GenericEntity : "{4CC2C69DE1AEF103}Prefabs/Props/Core/DestructibleMultiPhase_Props_Base.et"` for six of them; `BunkBed_02/BunkBed_02.et` inherits `{A43461050704B41B}…/BunkBed_02_base.et`), line 2 repeats vanilla's `ID`, and the body adds **one** component: an `ActionsManagerComponent` with an `ActionContexts { UserActionContext "default" }` block and `additionalActions { OVT_SleepAction }`. Everything else is inherited.

The `.et.meta` must carry **the vanilla prefab's own resource GUID** — that is what makes the file a delta over vanilla rather than an unreferenced new prefab. The reference tree ships no `.meta` files, so each GUID is recovered from an inbound reference in a furniture composition (e.g. `Bed_01.et` → `{0164749D6EAA4518}`, from `ArmaReforger/Prefabs/Structures/Houses/Farm/FarmHouse_E_1L01/Furniture/FarmHouse_E_1L01_furniture_01.et:4`). Getting it wrong is **safe and visible**: the override becomes an orphan prefab nothing instances, so the action simply never appears on that bed model — no data loss, and the play-test enumerates all seven.

**Do not override `DestructibleMultiPhase_Props_Base.et`** (every destructible prop in the game), and **do not override `FurnitureMilitary_base.et`** (already overridden by Overthrow to add `OVT_PlaceableComponent`; the military cots inherit it).

**The cot — one new mod-owned prefab.** `Prefabs/Props/Military/Furniture/OVT_Cot_Placed.et`, modelled on `Prefabs/Props/Furniture/OVT_CabinetMetal_01_grey_V1.et` (the existing "vanilla prop wrapped as a placeable" precedent), inheriting the vanilla cot `{C9CFDED29542A968}Prefabs/Props/Military/Furniture/CotMilitary_US_01.et` and adding: `OVT_PlaceableComponent { m_sPlaceableType "Cot" }`, `OVT_PlayerOwnerComponent`, `RplComponent { Enabled 1 }`, `SCR_EditableEntityComponent` (`m_Flags PLACEABLE`), and the `ActionsManagerComponent` with the Sleep action. New GUIDs from the reserved series.

It rides the **existing** placement pipeline unchanged: one entry in `Configs/Resistance/placeables.conf` and `OVT_ResistanceFactionManager.PlaceItem` does the rest, including `OVT_PersistenceTracking.Track` (:773).

---

## 4. Implementation Phases

Each phase ends with `tools/compile-check.sh` exit 0. **No phase runs `tools/run-tests.sh`** — the orchestrator runs the suites after a phase completes (`.claude/test-policy.md`).

**GUID series reserved for this feature: `{6B5D0000000000XX}`** — verified free (no `6B5D`, `6B5E`, `6B60` or `6B61` GUID exists anywhere in the tree). Use it for new components in the override prefabs, the cot prefab, the placeable config entry and the new `.st` string items.

---

### Phase 1 — Accounting catch-up, and BUG-179

**Agent:** `component-developer-advanced` · **Estimate:** 6-8 h
**Advanced because:** it touches the `CheckUpdate` seams of two shipped managers, extracts live code out of a running loop, and fixes a shipped money exploit. A wrong latch here is silent duplicated income.

**Tasks**

1. **T1.1** `Scripts/Game/Services/OVT_SleepSchedule.c` — the pure class of §3.3/§3.4. Class header states the rule: **no `OVT_Global`, no manager, no entity, no world, ever** (the Logic tier greps the directory for those identifiers, including in comments). Constants: `INCOME_INTERVAL_HOURS = 6`, `RESTOCK_HOUR = 7`, `RENT_HOUR = 0`, `THREAT_STEP_MINUTES = 15`, `HOURS_PER_DAY = 24`, `MINUTES_PER_HOUR = 60`.
   ⚠️ Integer division in EnforceScript truncates in pure-int expressions but is context-dependent — write every floor as `(int)Math.Floor((float)a / b)` rather than relying on `a / b`.
2. **T1.2** `OVT_EconomyManagerComponent.AssertHourLatches(int hour)` — protected, sets `m_iHourPaidIncome`, `m_iHourPaidStock` and `m_iHourPaidRent` all to `hour`. Doc comment explains why one value is correct for all three (the live branches compare the latch against a *fixed* hour, so "the hour we are in" always suppresses exactly one re-fire and nothing else).
3. **T1.3** **BUG-179.** Call `AssertHourLatches(currentHour)` from three places:
   - a one-shot at the **top of `CheckUpdate()`**, behind a `bool m_bLatchesAsserted` — this is the load-order-proof point, because the first tick lands ~10 s after `Init` and the persisted clock is certainly restored by then;
   - `Init()` (`:1278-1291`), before the `CallLater` — belt and braces, harmless if the clock is not restored yet since the one-shot re-asserts;
   - `ApplyPersistedEconomy()` (`:1254`), which runs whenever a save payload is applied, including a live re-apply.
   Resolve `m_Time` defensively in the helper the way `CheckUpdate` does (`:155-159`). Reference BUG-179 in the commit message and in the helper's doc comment.
4. **T1.4** `OVT_EconomyManagerComponent.HandleTimeSkip(int hours)` — public, `if (!Replication.IsServer()) return;`, the five steps of §3.3. Doc header carries the "called before the clock moves, window is half-open at the start" contract and the note that it must **not** inherit `CheckUpdate`'s player-count-0 early return.
5. **T1.5** `OVT_OccupyingFactionManager` extraction 1: `protected void SpendResourcesOnBases(int newResources)` — the inline block currently at `:1188-1229` (target computation, base sort by threat, per-base budget, `SpendResources`, resource clamping). `CheckUpdate` calls it. **Name it `SpendResourcesOnBases`, not `SpendResources`** — `OVT_BaseControllerComponent.SpendResources` already exists and reads at the same call site.
6. **T1.6** `OVT_OccupyingFactionManager` extraction 2: `protected void DecayThreatStep()` — the four lines at `:1255-1259` only (reduce, clamp at 0, `Print`). The town-uprising scan below it stays in `CheckUpdate`.
7. **T1.7** `OVT_OccupyingFactionManager.HandleTimeSkip(int hours)` — public, server-guarded, the chronological loop of §3.3, with the two exclusions documented.
8. **T1.8** Logic tier, new file `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_SleepSchedule.c` — see §8 for the case list.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- `grep -n "m_iHourPaid" OVT_EconomyManagerComponent.c` shows every assignment outside `CheckUpdate` going through `AssertHourLatches`.
- The occupying-faction `CheckUpdate` is *shorter* and its behaviour byte-for-byte unchanged: the extracted bodies moved, nothing was rewritten.
- `OVT_SleepSchedule.c` contains no occurrence of `OVT_Global`, `GetGame`, `IEntity` or `World`.
- Every new Logic case has a recorded can-fail proof in its preamble. No `maxAttempts`.
- `docs/bugs/BUG-179.md` stays `status: open` until its three play-test checks pass (Verification Method steps 12-14); the orchestrator flips it to `closed` then, not the implementing agent.

---

### Phase 2 — Sleep service: clock advance and cooldown persistence

**Agent:** `component-developer` · **Estimate:** 4-6 h

**Tasks**

1. **T2.1** `Scripts/Game/Data/OVT_PlayerData.c` — `float m_fLastSleepGameHours = -1;` in the persisted block, with a doc comment stating: absolute in-game hours at the moment of the last sleep; `-1` = never; **server-only, never replicated** (not in the player manager's `RplSave`/`RplLoad`, same rule as the tutorial fields).
2. **T2.2** `OVT_PlayerManagerSerializer` → version **5**: `float lastSleepGameHours;` appended **last** on `OVT_PersistedPlayer` (after `tutorialsDisabled`), written in `Serialize`, adopted in `ApplyPersistedPlayers`, and `if (version < 5) ClearSleepCooldown(records);` beside the existing clears — same "whatever the reader left there is not ours" reasoning, sentinel `-1`. Extend the class-header VERSION HISTORY block.
3. **T2.3** `Scripts/Game/Services/OVT_SleepService.c` — static, no instance state. Constants `SKIP_HOURS = 8`, `COOLDOWN_HOURS = 12`. Members:
   - `static bool IsSleepLocation(vector pos, string playerPersistentId)` — §3.5, with the four distance constants re-declared and attributed.
   - `static bool CanSleep(IEntity user, out string reasonKey)` — SP mode, resolvable player record, QRF, wanted, cooldown. One place; both the action's gates and `PerformSleep` call it.
   - `static float GetCooldownRemainingHours(string playerPersistentId)`.
   - `static void PerformSleep(IEntity user)` — the sequence of §3.2 **without the fade** in this phase (fade lands in Phase 5; the `CallLater` structure is already in place so Phase 5 only wraps it).
   - `static protected void AdvanceClock(int hours)` — read `GetHoursMinutesSeconds`, `SetTimeOfTheDay(landing, true)` (immediate), and when the landing hour wrapped past midnight advance the date: try `SetDate(y, m, d + 1)`, and on `false` try `(y, m + 1, 1)`, and on `false` `(y + 1, 1, 1)`. **No hand-written calendar** — `SetDate` validates and returns false, which is cheaper and more correct than month lengths and leap years in EnforceScript. Guard every call on a non-null `TimeAndWeatherManagerEntity`, following `SCR_CaptureAndHoldManager.c:289-306`.
4. **T2.4** Cooldown stamp written **before** `AdvanceClock`, from the pre-skip calendar (`GetYear()`, `GetDayInYear()`, `GetHoursMinutesSeconds`) through `OVT_SleepSchedule.AbsoluteGameHours`.
5. **T2.5** Logic tier: extend `OVT_TEST_Logic_SleepSchedule.c` with the `AbsoluteGameHours` / `CooldownRemainingHours` / `FormatRemaining` cases (§8).
6. **T2.6** Persistence tier: new case in `OVT_TEST_PersistenceRoundTripSuite.c` modelled on `OVT_TEST_PersistenceRoundTrip_PlayerLastKnownPosition_SurvivesSaveAndReload` (`:2963`) — set `m_fLastSleepGameHours`, save, dirty the value, re-apply, assert it came back.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- `grep -n "version" OVT_PlayerManagerSerializer.c` shows `5` written and a `< 5` guard on read; the new field is the **last** member of `OVT_PersistedPlayer`.
- `PerformSleep` calls `HandleTimeSkip` on both managers **before** `AdvanceClock`, and the ordering is asserted by a comment naming §3.3.
- `OVT_SleepService` contains no `Rpc`, no `RplRpc` and no new component class.

---

### Phase 3 — The user action and the bed overrides

**Agent:** `component-developer` · **Estimate:** 4-6 h

**Tasks**

1. **T3.1** `Scripts/Game/UserActions/OVT_SleepAction.c` per §3.6, with the 1-second cache shape of `OVT_RearmVehicleAction` (`CHECK_TTL_MS`, `m_fCacheExpiresAt`, `m_bHasCache`, `RefreshCache()`), and `m_bHasCache = false` after a successful perform so the label switches to the countdown immediately.
   ⚠️ Do **not** gate on `RplComponent.IsOwner()` (the pattern in `OVT_SetHomeAction.c:22-26`) — the bed prefabs have no `RplComponent` and single player has no replication. Identify the actor as `SCR_PlayerController.GetLocalControlledEntity()`.
2. **T3.2** Seven override prefabs + `.meta` files per §3.7: `Bed_01.et`, `Bed_02.et`, `BedDouble_01.et`, `BedDouble_02.et`, `BunkBed_01.et`, `BunkBed_01_Double.et`, `BunkBed_02/BunkBed_02.et`, all under `Prefabs/Props/Furniture/`.
   - Read **each** vanilla file's line 1 and line 2 directly out of `/mnt/n/Projects/Arma 4/ArmaReforger/Prefabs/Props/Furniture/…` and copy them verbatim. Do not trust a table — two beds were reported with the same `ID` during research, which is almost certainly a transcription error.
   - Recover each `.meta` GUID from an inbound reference in the vanilla tree (`grep -rn "Prefabs/Props/Furniture/Bed_01.et" /mnt/n/Projects/Arma 4/ArmaReforger --include=*.et`), and record the reference you used in a comment in this feature's `tasks.md`.
   - Per-prefab `UserActionContext` offset/radius tuned to the model (a double bed needs a wider radius than a bunk); `UIInfo { Name "#OVT-Sleep" }` on the action instance.
3. **T3.3** Localization: `OVT-Sleep`, `OVT-SleepCooldown`, `OVT-SleepQRF`, `OVT-SleepWanted`, `OVT-SleptWell`. Add a `CustomStringTableItem` per key to `Language/localization_Overthrow.st` with a GUID from the reserved series, **and** mirror each key into all seven runtime `.conf` exports exactly as commit `6085521a` did: the key in the `Ids { }` block in alphabetical position, and the English text at the **same index** in the parallel `Texts { }` block. Non-English exports get the English text as the placeholder (that is what `6085521a` did). ⚠️ An unbalanced brace in the `.st` costs the whole file on the next Workbench save — verify the file's brace balance before and after.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- All seven override files carry a parent header and an `ID` line copied from their vanilla counterpart, and a `.meta` whose `Name` GUID came from a recorded inbound reference.
- No raw `#OVT-` key renders on screen in the play-test.
- `grep -c "OVT-Sleep" Language/localization_Overthrow.en-us.conf` returns the same count for the `Ids` and `Texts` halves.

---

### Phase 4 — The cot placeable

**Agent:** `component-developer` · **Estimate:** 2-3 h

**Tasks**

1. **T4.1** `Prefabs/Props/Military/Furniture/OVT_Cot_Placed.et` (+ `.meta`, new GUID from the reserved series) per §3.7, copying the component set and the explicit `ActionContexts` block of `Prefabs/Props/Furniture/OVT_CabinetMetal_01_grey_V1.et` (`:1-28`). `OVT_PlaceableComponent { m_sPlaceableType "Cot" }`.
2. **T4.2** One `OVT_Placeable` entry in `Configs/Resistance/placeables.conf` — the minimal five-field shape of the Ammobox entry (`:44-56`): `m_sName "Cot"`, `m_sTitle "#OVT-Place_Cot"`, `m_sDescription "#OVT-Place_Cot_Description"`, `m_aPrefabs { the new prefab }`, `m_tPreview` (reuse a vanilla cot editor preview `.edds`, as the Furniture entry does), `m_iCost` ~30 (in line with Furniture at 25 — a cot is furniture that does something). **Do not touch the existing "Furniture" entry** (`:122-147`), which randomises across the raw vanilla cots.
3. **T4.3** Localization for the two new placeable keys, same procedure as T3.3.
4. **T4.4** Verify persistence needs **no** config change: the `EntityPersistenceConfig` at `Configs/Systems/Persistence/Overthrow.conf:152-171` matches on `ComponentClass "OVT_PlaceableComponent"` with `SelfSpawn 1`, and `PlaceItem` already tracks the entity (`OVT_ResistanceFactionManager.c:773`). Record the check in `tasks.md`; change nothing.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- `Configs/Systems/Persistence/Overthrow.conf` is unmodified.
- The cot appears in the build menu, places, persists across a save/load, and carries a working Sleep action.

---

### Phase 5 — Fade, polish and the pad pass

**Agent:** `component-developer` · **Estimate:** 2-3 h

**Tasks**

1. **T5.1** Fade in `OVT_SleepService`, following `SCR_FastTravelComponent.c:264-296`: `SCR_ScreenEffectsManager.GetScreenEffectsDisplay()` → `SCR_FadeInOutEffect.Cast(manager.GetEffect(SCR_FadeInOutEffect))` → `FadeOutEffect(true, FADE_DURATION)`; the work runs on a `CallLater` of `(FADE_DURATION + BLACK_DURATION) * 1000`; then `FadeOutEffect(false, FADE_DURATION)`. Constants `FADE_DURATION = 1.0`, `BLACK_DURATION = 0.5`.
   ⚠️ `FadeOutEffect` silently does nothing for `seconds < 0.1`. **Every step is null-guarded and the time skip must happen regardless** — if the display or the effect is missing, the `CallLater` still fires and the player simply gets no fade.
2. **T5.2** Confirm the effect is actually registered at runtime. It should be: `Prefabs/Characters/Core/OVT_PlayerController.et` adds only two components to `DefaultPlayerControllerMP.et` and never touches `SCR_HUDManagerComponent`, so the vanilla `SCR_ScreenEffectsManager` tree — including `SCR_FadeInOutEffect` at `ArmaReforger/Prefabs/Characters/Core/DefaultPlayerController.et:49-53` — is inherited intact. **Verify in a play-test, not by reading**; if it is absent, the fallback is registering the effect on Overthrow's player controller prefab, not building a UI.
3. **T5.3** Success hint on wake (`SCR_HintManagerComponent.ShowCustomHint("#OVT-SleptWell", "", 4)`, the shape `OVT_SetHomeAction.c:12` uses). The income/payout notifications the replay fires arrive over the black screen and immediately after — **leave them**: they are the player's evidence that the accounting ran.
4. **T5.4** Gamepad pass: the action is reachable and performable with a pad alone on each bed type, the countdown label is legible, and the disabled reasons render translated.
5. **T5.5** Final localization sweep: every key used by the action, the service and the placeable exists in `.st` and in all seven exports.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- Deliberately nulling the screen-effects lookup (temporary local edit) still produces a correct 8-hour skip.
- No raw `#OVT-` key on screen; pad-only pass completed.

---

### Phase 6 — Help and documentation sync

**Agent:** `help-docs-sync` · **Estimate:** 1-2 h

Sleep is player-facing, so the closing phase is mandatory: a Field Manual entry under the resistance/building category describing what sleeping does (8 hours, the accounting, the 12-hour cooldown, where it can be used, and that it is single-player only), the cot in whatever placeable listing the manual carries, and the public wiki page kept in step. **Every sentence must cite a file:line or be cut** — two shipped tips have described mechanics that do not exist.

---

### Phase 7 — Review fixes (post-build, 2026-08-19)

**Agent:** `component-developer-advanced` · **Estimate:** 2-3 h
**Advanced because:** it changes a live war-state gate on a shipped manager (R6) and re-points a persisted stamp.

**Tasks**

1. **T7.1** D18/D18a — two latches on `OVT_OccupyingFactionManager`, the `GainAndSpendResources()` extraction, the start-boundary flush and the landing-instant latch assertion in `HandleTimeSkip`; new `OVT_SleepSchedule.IsStepBoundary`.
2. **T7.2** D16a — `StampCooldown` writes the WAKE instant (`pre-skip + SKIP_HOURS`); every doc comment and Field Manual sentence that claimed "about four hours" corrected, in the `.st` and all seven runtime exports.
3. **T7.3** Logic tier: one changed case (cooldown → 12 h on waking) and two new ones (`IsStepBoundary`, window edges), each with a recorded can-fail proof. No `maxAttempts`.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- The occupying-faction live gates differ from their pre-fix form by exactly one condition and one assignment each.
- All seven localization exports stay at 795 `Ids` / 795 `Texts`, and the only byte that changed in each is the corrected sentence.

---

## 5. Key Technical Decisions

| # | Decision | Rationale |
|---|---|---|
| **D1** | **Targeted catch-up**: one public `HandleTimeSkip(int hours)` per affected manager, replaying its own payloads. No hour-elapsed event bus, no restructuring of the shipped `CheckUpdate` bodies. | *(User decision.)* The two `CheckUpdate` bodies are live economy and live war state; an event refactor would put every payout in the blast radius of a feature that only needs to say "do that eight hours' worth". The catch-up path calls the same protected methods (`CalculateIncome` :455, `UpdateShops` :376, `ReplenishStock` :297, `UpdateRents` :243, `GainResources` :1426) the live path calls, so there is exactly one implementation of each payout. |
| **D2** | Catch-up scope is **economy + occupying-faction resources + threat decay** only. Town modifiers, jobs, wanted decay, offline timers and tower downtime stay wall-clock. | *(User decision.)* Those systems are tuned in real seconds and several of them are `PlayerInRange`-gated; replaying them would either do nothing or fire 32 world events at a player who cannot see the screen. |
| **D3** | The window is **half-open at the start** — `(start, start + 8h]` — and the economy path calls `CheckUpdate()` once before replaying. | The alternative (closed at both ends) double-pays whenever the player sleeps within the same minute a boundary fell in. The flush call reuses the shipped latch logic to settle anything owed *at* the start, so the replay never has to reason about what the last tick did or did not observe. |
| **D4** | After the replay, **all three economy latches are set to the landing hour** by `AssertHourLatches(int)`. | The live branches compare each latch against a fixed hour (any of 0/6/12/18, or 7, or 0), so "the hour we woke in" suppresses exactly one re-fire and permits every later one. Landing on a boundary hour is the case that would otherwise pay three times for two crossings. |
| **D5** | **BUG-179 is fixed by latch-init-to-current-hour, not by persisting the latches.** | Zero serializer churn, zero new format, and it fixes the live re-apply path for free. The only cost is the ≤10 s window in which the hour flipped just before the save and the tick had not observed it yet — that payout is skipped. Skipping is the safe direction; duplicating is the exploit. The one-shot lives at the top of `CheckUpdate` rather than only in `Init` because the persisted clock's restore order relative to `Init` is not guaranteed, and the first tick is ~10 s later. |
| **D6** | The occupying-faction replay is **chronological** (15-minute steps, gain/spend when a step lands on a 6-hour boundary), not "2 gains then 32 decays". | `GainResources` scales with `m_iThreat` (`:1430-1432`) and threat decays between gains, so batching changes the numbers. The chronological loop *is* the simulation, at 15-minute resolution. |
| **D7** | The counter-attack roll and the town-uprising scan are **excluded** from the replay. | Neither is accounting. The uprising scan is `PlayerInRange(town, 300)`-gated and a sleeping player is inside that radius of the town they slept in; 32 replays could launch a town QRF onto a black screen. |
| **D8** | Cooldown is stored as an **absolute game-clock stamp on `OVT_PlayerData`**, serializer version 5, and compared by subtraction. | *(User decision.)* A countdown decremented in real time would be wrong by 6× or 12× depending on day/night acceleration (`OVT_TimeAndWeatherHandlerComponent.GetDayTimeMultiplier` :9); a game-clock difference is correct at any acceleration and durable for free, because the engine persists the clock (`TimeAndWeatherManagerEntitySerializer`) and the record persists the stamp. |
| **D9** | The cot needs **no persistence config change**. | `Configs/Systems/Persistence/Overthrow.conf:152-171` matches on `ComponentClass "OVT_PlaceableComponent"` with `SelfSpawn 1`, and `PlaceItem` tracks the entity (`OVT_ResistanceFactionManager.c:773`). A prefab-keyed rule would have needed an entry; a component-keyed one does not. Confirmed by `OVT_PlaceableComponentSerializer.c:8-10`. |
| **D10** | The cot is a **new mod-owned prefab**, not an override of `CotMilitary_US_01.et`. | *(Requirement.)* It is the only prefab that needs an `RplComponent`, and the vanilla cots are already used raw by the "Furniture" placeable — overriding them would put a Sleep action on decorative cots that carry no `OVT_PlaceableComponent` and therefore do not persist. |
| **D11** | Beds get **seven per-prefab overrides**; no shared base is touched. | Six of the seven inherit `DestructibleMultiPhase_Props_Base.et`, which is every destructible prop in the game. The military cots' base, `FurnitureMilitary_base.et`, is already overridden by Overthrow for a different purpose. Per-prefab is the only correct granularity. |
| **D12** | **No RPCs, no controller component, no `OVT_Global` accessor.** The action calls a static service directly. | Single player is `RplMode.None`: the machine that clicks *is* the authority. The recruit-ux client→server seam exists for multiplayer features; copying it here would add four moving parts that can only ever no-op. `OVT_FastTravelService` / `OVT_RespawnService` are the precedent for a static service in `Scripts/Game/Services/`. |
| **D13** | The visibility gate is **single-player-only and cached for 1 s**; the QRF/wanted/cooldown gates are *disabled-with-reason*, not hidden. | Hiding a relevant action makes it look broken (`OVT_SabotageTowerAction`'s established rule). The cache exists because the gate walks four manager collections every frame a player looks at a bed (`OVT_RearmVehicleAction:23-30`). |
| **D14** | The countdown is built by **key-plus-literal concatenation** (`"#OVT-Sleep (" + … + ")"`), not `WidgetManager.Translate` with a `%1`. | It is the project's established form (`OVT_SabotageTowerAction.c:63`, `OVT_RearmVehicleAction.c:98`) and keys embed inside larger strings correctly. A parameterised `Translate` would be equally valid but would introduce a second convention for no gain. |
| **D15** | The day-boundary wrap uses **`SetDate` + the engine's own validation ladder** (d+1 → month+1/1 → year+1/1/1), never hand-written calendar maths. | `SetTimeOfTheDay` does not roll the date (`ArmaReforger/scripts/GameLib/generated/Entities/BaseWeatherManagerEntity.c:256`) and there is no "advance day" helper anywhere. `SetDate` (:309) returns false for an invalid date, which is a leap-year-correct month-length table we do not have to write or test. |
| **D16** | ~~Cooldown is measured from the **moment of lying down**, so a player wakes with 4 hours left.~~ **SUPERSEDED 2026-08-19 — see D16a.** | ~~The requirement says "cannot perform it again for 12 in-game hours"; the skip happens inside those 12. Wake-relative would be a 20-hour effective cooldown. One constant if the user prefers the other reading.~~ The original reasoning is kept because it is the reading the requirement's wording supports, and because it says exactly what changing it would cost — which is what was then changed. |
| **D16a** | Cooldown is measured from the **moment of waking**: the stamp written is the pre-skip clock read **plus `SKIP_HOURS`**, so the full 12 in-game hours are ahead of the player when the screen fades back in and the door-to-door interval is **20 hours**. | *(User decision, 2026-08-19, after play-reading D16's behaviour and rejecting it.)* Waking with only ~4 hours left reads as the cooldown having been consumed by the very thing it is supposed to gate. D16 anticipated the reversal — "one constant if the user prefers the other reading" — and it was one expression: `OVT_SleepService.StampCooldown` writes `ReadAbsoluteGameHours(time) + SKIP_HOURS` (`:456`). The sequence in `PerformSleepNow` is unchanged and the post-skip clock is deliberately **not** read: `preSkipAbs + SKIP_HOURS` *is* the wake instant by definition, it keeps the stamp on the same clock read the accounting catch-up used, and it does not depend on the `SetDate` ladder having worked. No serializer change — same float, same position, only the value's meaning. **One accepted degradation:** a stamp in the future is fail-open (D17), so if `SetTimeOfTheDay` ever refused, the player could sleep again at once instead of being blocked for 12 h as the old convention would have been. Bounded — the precondition is "the skip visibly did nothing", a hard play-test gate — and `AdvanceClock` now logs the refusal rather than letting it pass in silence. |
| **D17** | `CooldownRemainingHours` **fails open** on a negative elapsed time. | Fail-closed would lock the action forever on any save whose stamp is in the future — unrecoverable. Fail-open costs, at worst, one extra sleep on a save whose clock already misbehaved, and the day-wrap that could cause it is itself a play-test gate. |
| **D18** | `OVT_OccupyingFactionManager` gains **two live-tick latches** — `m_iHourGainedResources` and `m_iMinuteDecayedThreat`, both `-1` armed — and `HandleTimeSkip` gains a **start-boundary flush** and a **landing-instant latch assertion**. *(Review fix, 2026-08-19.)* | Its two payload gates were minute-exact with **no latch at all**, which is the one thing the economy manager's equivalents have always had. That left both edges of the skipped window undefended: the landing boundary was paid **twice** (replay + the tick that resumes inside the same in-game minute — Q1/F7) and a boundary sitting exactly on the start was paid **zero** times (half-open replay + a tick that had not fired yet — Q2). The latch mirrors `m_iHourPaidIncome` exactly, including its lack of an else-reset, because consecutive boundaries are always different numbers. **D1 is not violated:** no event bus and no restructuring — the gates gain one condition and one assignment each, and the four-line payload becomes `GainAndSpendResources()` so the flush is not a third copy of it. Normal play is unchanged by construction: the gate is only reachable at an exact boundary minute, the latch starts armed at `-1`, and it can therefore only suppress a **repeat** inside one in-game minute. Note this also de-duplicates on **dedicated servers**, which is correct and is stated in the code.
| **D18a** | The **threat decay gets the same treatment**, not just the resource gain. The town-uprising scan in the same `if` block deliberately does **not**. | The review called the 33rd decay step cosmetic and left the choice open. Both were fixed because the error is not only an extra step at the end: a start on a quarter hour also **loses** one at the front, which is the same class of defect as Q2, and it costs the same five-line idiom. `Reduced Threat to:` now prints exactly 32 times for a start off the quarter-hour grid, and 33 (one flush + 32) for a start on it — the flush line being a boundary the live tick owed and had not run. The uprising scan is left unlatched because it is not a payload owed once per boundary; it is a `PlayerInRange`-gated world scan, and latching it would change behaviour with no accounting argument for it. |

---

## 6. Definition of Done

### Functional

- [ ] **F1** A "Sleep" action appears on a bed inside a house the player owns, in the player's own camp, at a deployed FOB, and at a captured base — one verification per location type.
- [ ] **F2** The same bed model shows **no** action when it is none of those places (a random village house the player does not own).
- [ ] **F3** Performing the action fades the screen to black, advances the clock by exactly 8 in-game hours, and fades back in.
- [ ] **F4** The action works at any time of day, including in daylight.
- [ ] **F5** Income/taxes/donations are paid once per 6-hour boundary crossed by the skip (1 or 2 times, depending on the start time), with the player's money increasing by the matching amount and the usual notification firing per payout.
- [ ] **F6** Sleeping across 07:00 restocks shops once; sleeping across 00:00 charges/pays rent once; sleeping across neither does neither.
- [ ] **F7** Occupying-faction reserve resources increase, and the console shows one "Gaining Resources"/"Reserve Resources" pair per 6-hour boundary crossed; threat is lower after the skip than before.
- [ ] **F8** The action remains visible for the following 12 in-game hours **counted from waking** (D16a) with a live countdown in its label (`Sleep (12h 0m)` the instant the screen comes back), and cannot be performed until the countdown reaches zero — 20 in-game hours door to door.
- [ ] **F9** During an active QRF the action is visible but disabled, with a localized reason.
- [ ] **F10** With a wanted level above zero the action is visible but disabled, with a localized reason.
- [ ] **F11** The action does not appear at all on a listen-host or dedicated server session.
- [ ] **F12** A "Cot" placeable can be built at a camp/FOB/base, carries the Sleep action, and survives a save/load in place.

### Quality

- [ ] **Q1** **No double pay.** Sleeping so that the landing hour is exactly a boundary hour (e.g. 10:00 → 18:00) pays exactly twice, not three times, and the next boundary at 00:00 still pays. **This must hold for the occupying faction too** — exactly two `Gaining Resources`/`Reserve Resources` pairs in the console, not three (D18).
- [ ] **Q2** **No missed pay.** Sleeping from exactly a boundary minute (e.g. 12:00 → 20:00) pays for 18:00 and does not lose the 12:00 payout the live tick owed. **Both managers**: the economy flushes with `CheckUpdate()`, the occupying faction with its own start-boundary flush (D18).
- [ ] **Q3** **BUG-179 closed** — see the three checks in *Verification Method*.
- [ ] **Q4** A missing screen-effects display or fade effect does not prevent the time skip: the clock still advances and the accounting still runs.
- [ ] **Q5** Sleeping across midnight advances the **date**, and the cooldown countdown afterwards decreases (never jumps to a negative or an absurd value).
- [ ] **Q6** No raw `#OVT-` key reaches the screen; every disabled reason is translated.
- [ ] **Q7** The action, its countdown and the placement of the cot are fully operable with a gamepad alone.
- [ ] **Q8** `OVT_SleepSchedule.c` has no world access of any kind, and every function in it is covered by a Logic case with a recorded can-fail proof.
- [ ] **Q9** The occupying-faction `CheckUpdate` behaves exactly as before the extraction: a normal campaign hour still gains, spends and decays on schedule.

### Integration

- [ ] **I1** The cot rides the existing `PlaceItem` pipeline — no new server placement code, and `Configs/Systems/Persistence/Overthrow.conf` is unmodified.
- [ ] **I2** The cooldown stamp round-trips: sleep, quit to menu, Continue → the countdown resumes from the correct remaining value, not from zero and not from 12.
- [ ] **I3** A save written before this feature (player payload version 4) loads clean and every player is treated as never having slept.
- [ ] **I4** The seven bed overrides do not change how beds look, block, or take damage; furniture compositions inside houses still show their beds.
- [ ] **I5** Existing suites stay green — in particular the persistence round-trip suite and the campaign economy cases.

### Verification Method

**Automated (orchestrator, after each phase completes — never inside an agent):**

1. `tools/compile-check.sh` → exit 0.
2. `tools/run-tests.sh` — Fast group after Phase 1 and 3; **All group after Phase 2** (persistence state is touched).

**Single-player play-test (F1-F12, Q1-Q9):**

1. Start an SP campaign. Buy/claim a house. Note the clock and the player's money.
2. Stand at a bed inside the owned house → the Sleep action is offered. Walk to a bed in a house you do **not** own → no action. **(F1, F2.)**
3. Note the money, the in-game clock and the occupying faction's threat (console `Reduced Threat to:` lines). Sleep. → screen fades, clock is +8 h, screen fades back. **(F3, F4.)**
4. Count the income notifications during/after the black screen and check the money delta against the number of 6-hour boundaries the window crossed. **(F5, Q1/Q2 — do this twice: once starting at 10:00 to land exactly on 18:00, once starting at 12:00 exactly.)**
5. Sleep from 04:00 (crosses 07:00) → shop stock is refilled. Sleep from 20:00 (crosses 00:00) → rent moves. Sleep from 08:00 (crosses neither) → neither. **(F6.)**
6. Watch the console over the skip: one `Gaining Resources` / `Reserve Resources` pair per boundary, and `Reduced Threat to:` **exactly 32 times** from a start that is not on a quarter hour, **33** from one that is (the extra line is the start-boundary flush paying a step the live tick owed — D18a). **(F7.)**
7. Immediately look at the bed again → `Sleep (12h 0m)`-ish label, action disabled. Wait (or sleep-blocked) and watch the countdown fall. **(F8, D16a.)**
8. Trigger a QRF (attack a base) and look at a bed → visible, disabled, reason shown. Raise the wanted level and look again → visible, disabled, different reason. **(F9, F10.)**
9. Build a Cot at a camp; sleep on it; save; Continue → the cot is still there and still works. **(F12, I1.)**
10. Sleep at ~22:00 so the skip crosses midnight → the in-game **date** advanced (check the map/journal clock or the sun), and the cooldown afterwards counts down normally. **(Q5.)**
11. Pad-only pass: approach a bed, read the countdown, perform, place a cot. **(Q7.)**

**BUG-179 (Q3):**

12. Play until the 12:00 income notification fires. Note the money. Save, exit to menu, Continue → **no second income payout**; money unchanged by the load.
13. Repeat during hour 7 (after restock — shop stock must not jump again) and during hour 0 (after rent).
14. Let the clock reach the next 6-hour boundary after loading → income pays exactly **once**.

**Persistence (I2, I3):**

15. Sleep, then quit → Continue. The countdown resumes at roughly where it was, not at 0 and not at 12.
16. `git stash` the feature branch, start a campaign, save, restore the branch, Continue → loads clean, the Sleep action is available (never slept).

**Multiplayer (F11):**

17. `tools/launch-server.sh`, then `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`. Walk to a bed → **no Sleep action**. (Pass a long `--timeout`; the default 600 s kills the client mid-test.)

---

## 7. Quality Bar

This is a gameplay/simulation feature. It is judged on three things.

**Accounting correctness — the only thing that can silently corrupt a campaign.**

- Every payout happens **exactly once** per boundary the skipped window contains. Not "about right" — the failure modes here are a money printer and a missing payday, and both are invisible until a player notices.
- The arithmetic that decides "how many" lives in one world-free class with assertions on it. Inline `hours / 6` in a manager body is not acceptable; nobody can test it and integer division in EnforceScript is context-dependent.
- The latch state after a skip is part of the payout, not an afterthought. `AssertHourLatches` is called on every path that ends with the clock somewhere new — the skip, startup, and save-apply.
- The replay uses the **same methods** the live tick uses. If a future change alters `CalculateIncome`, the skip changes with it, for free. A second implementation of any payout is a defect by construction.
- BUG-179 is the proof that this bar matters: an unpersisted latch has been handing out free money in shipped builds.

**Save/load integrity of the cooldown.**

- Version 5 is append-only, positional, with a `< 5` clear on read. Older saves load as "never slept" — the value version 4 promised.
- The stamp is a game-clock quantity, so it is correct at 6× and at 12×, across a save, across a Continue, and across a date change.
- The cooldown can never become permanently unsatisfiable. That is what D17's fail-open is for, and there is a Logic case that says so.

**Action UX.**

- The countdown is in the label, updates while the player looks at the bed, and is legible on a controller at TV distance.
- A blocked action stays **visible with a reason**, always localized, never a raw key and never a silent disappearance.
- The action never appears where it cannot work: not outside single player, not away from an owned location.
- The fade is flavour and is treated as flavour: it is null-guarded end to end and its absence costs nothing but atmosphere.

---

## 8. Testing Strategy

**The orchestrator runs the suites, after implementation — planning never runs them.** What follows is which tier each phase extends.

### Logic tier (Fast) — `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_SleepSchedule.c` *(new)*

| Group | Cases |
|---|---|
| 6-hour crossings | 8 h from 11:47 → 2 (12:00 and 18:00); from 08:00 → 1 (12:00 only — the window ends at 16:00); from **exactly** 12:00 → 1 (18:00 only; the 12:00 boundary is excluded by the half-open start); from 10:00, landing **exactly** on 18:00 → 2 (the endpoint is included) |
| restock / rent entries | 8 h from 04:00 crosses hour 7 exactly once; from 08:00 crosses it zero times; from 20:00 crosses hour 0 exactly once (across midnight); from 01:00 crosses hour 0 zero times |
| quarter-hour steps | 32 for an 8 h skip from minute 0, minute 7, minute 14, minute 59 — the count is start-minute-independent, which is the property that breaks if the floors are written with raw int division |
| boundary predicate | `IsIntervalBoundary` true at 0/360/720/1080 minutes, false at 359/361/375; `IsStepBoundary` true at 0/15/30/45/1440/1455, false at 14/16/1439 (added 2026-08-19) |
| landing hour | 22:00 + 8 → 6; 00:00 + 8 → 8; 16:00 + 8 → 0 |
| **window edges** (added 2026-08-19) | over all 1440 start minutes: start flush + replay covers every 6-hour boundary of the CLOSED window `[start, start+8h]` exactly once (10:00 and 12:00 named explicitly); `LandingHour` is always the hour-of-day the resumed tick reads; and the same statement one grid finer for the quarter-hour decay. Oracle is an independent enumeration, not a second copy of the formula. |
| absolute hours | strictly increasing across an hour, across a day, and across a year boundary; 00:30 is 0.5 h after 00:00 |
| cooldown | at the wake instant → **12 h** remaining (D16a: the stamp IS that instant); 12 h after lying down → 8 h; 20 h after lying down → `<= 0`; sentinel `-1` → ready; **negative elapsed → ready** (D17) |
| formatting | `FormatRemaining(4.2)` → `"4h 12m"`; `0.5` → `"0h 30m"`; rounding at `0.999` does not produce `"0h 60m"` |

⚠️ Tier rules: the Logic directory is grepped for Overthrow's static manager accessor and the engine's game-mode getter — **neither identifier may appear anywhere under `TestSuites/Logic/`, including in comments**. `new` applies no `[Attribute]` defvalues. Floats compared with `OVT_TEST_LogicFixture.EPSILON`.

### Persistence tier (All)

| File | Covers |
|---|---|
| `OVT_TEST_PersistenceRoundTripSuite.c` *(extend)* | `m_fLastSleepGameHours` survives save → dirty → re-apply, modelled on `…_PlayerLastKnownPosition_SurvivesSaveAndReload` (`:2963`) |

### Every new case must be proven able to fail once

Break the subject deliberately (invert the half-open bound, drop the latch assert, remove the fail-open branch), confirm the case fails, restore, confirm it passes, record the method in a preamble comment. **No `maxAttempts`.**

### Not automatable, and why

| Area | Why | Substitute |
|---|---|---|
| The clock actually moving, and the date wrap | needs the live weather manager and the authority | play-test steps 3, 10 |
| Payout counts against real money | needs a running campaign with towns, shops and rent | play-test steps 4-6 |
| The fade | no headless screen-effect assertions exist | play-test step 3 + the deliberate-null check in T5.2 |
| Action visibility per location type | needs a world, an owned house, a camp, an FOB and a captured base | play-test steps 1-2 |
| BUG-179 | a true quit-and-continue restarts the autotest harness | play-test steps 12-14 |
| The `RplMode.None` gate | the test world is one machine | play-test step 17 |

---

## 9. Dependencies

### Internal

- **`economy/market`** — `OVT_EconomyManagerComponent` and its three hour latches. Phase 1 changes its startup path and adds a public entry point; BUG-179 is filed against this feature area.
- **`occupying/*`** — `OVT_OccupyingFactionManager`: `GainResources`, the base spend loop, threat decay, `m_bQRFActive`, `GetNearestBase`. Phase 1 extracts two methods out of its `CheckUpdate`.
- **`resistance/core`** — `GetNearestCampData` / `GetNearestFOBData` for the location gate, and `PlaceItem` + `OVT_PersistenceTracking.Track` for the cot.
- **`resistance/building`** — the placeables config pipeline; the cot is one config entry and one prefab, with no handler.
- **`resistance/wanted-system`** — `OVT_PlayerWantedComponent.GetWantedLevel()` read live off the character (wanted level is deliberately not persisted).
- **`core/persistence`** — `OVT_PlayerManagerSerializer` version 5; the binding in `Configs/Systems/Persistence/Overthrow.conf` already exists and a version bump needs no config change.
- **`core/real-estate`** — `GetNearestOwned` for the owned-house branch of the gate.

### External

- **Vanilla time API** — `BaseWeatherManagerEntity.SetTimeOfTheDay` / `SetDate` / `GetDate` / `GetDayInYear` / `GetHoursMinutesSeconds`, authority-only and auto-broadcast.
- **Vanilla screen effects** — `SCR_ScreenEffectsManager` + `SCR_FadeInOutEffect`, inherited through `DefaultPlayerControllerMP.et`.
- **Vanilla bed prefabs** — seven files in `Prefabs/Props/Furniture/`, overridden by path.

No new mod dependency.

---

## 10. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **Double or missing payout** at the window's edges — the landing hour, or a boundary the live tick had not yet observed. | Medium | Free money or a lost payday, both silent | The half-open window (D3) plus the flush `CheckUpdate()` plus `AssertHourLatches(landingHour)` (D4). The two nastiest start times (land exactly on a boundary; start exactly on one) are both Logic cases *and* explicit play-test steps 4. |
| **R2** | **The fade effect is not registered** at runtime, so `GetScreenEffectsDisplay()` or `GetEffect()` returns null. | Low (the vanilla tree is inherited intact) | No fade | Every step null-guarded; the `CallLater` that does the work is scheduled unconditionally (T5.1). Verified deliberately by nulling the lookup (Q4). Fallback if it really is absent: register the effect on `OVT_PlayerController.et`. |
| **R3** | **The date does not advance across midnight**, so the cooldown stamp goes backwards. | Medium (needs explicit `SetDate` — there is no helper) | Cooldown behaves absurdly | D15's validated ladder, play-test step 10 as a hard gate, and D17's fail-open so the worst case is a permissive cooldown rather than an action locked forever. |
| **R4** | **A bed override's `.meta` GUID is wrong**, so the file is an orphan prefab and the action never appears on that model. | Medium (7 hand-authored GUIDs recovered from inbound references) | One or more bed types have no action | Recovery method and a recorded citation per bed (T3.2); the play-test enumerates all seven models. The failure mode is safe — an orphan prefab changes nothing in the world. |
| **R5** | **Beds inside houses are baked children, not prefab instances**, so an override does not reach them. | Low-Medium (furniture compositions instance them) | Sleeping only works on placed cots | Play-test step 1 tests a bed *inside a house* first, before any other step. If it fails, the cot still ships and the fallback is a proximity-based action on the player rather than more prefab work. |
| **R6** | **The extraction changes occupying-faction behaviour** — the spend loop and decay step are live war-state code. | Medium | The war stops progressing, or progresses twice | T1.5/T1.6 are pure moves: the bodies are cut and pasted, not rewritten, and `CheckUpdate` keeps calling them in the same order at the same gates. **2026-08-19:** T7.1 adds a third pure move (`GainAndSpendResources`) plus one latch condition per gate (D18); the gate expressions are otherwise untouched and the latch is armed at `-1`, so a normal boundary still fires exactly once. Q9 verifies a normal campaign hour still behaves. Reviewed by an advanced agent. |
| **R7** | **Sleep becomes the optimal way to farm money**, because it compresses real time. | Certain (it is the feature) | Balance | Income is time-based, not sleep-based: a 12-hour cooldown allows at most 16 skipped hours per game-day, and the totals per game-day are unchanged. Only *real* time compresses, which is the point. No knob added (YAGNI); if it ever matters, `SKIP_HOURS` and `COOLDOWN_HOURS` are two constants. |
| **R8** | **Sleep becomes the optimal way to shed heat** — 32 decay steps at once. | Medium | The stealth layer weakens | At Normal (`threatReductionFactor 0.007`) 32 steps remove ~20% of threat plus the per-step `Math.Ceil` floor — meaningful but not a reset, and it is exactly what eight real hours of waiting would do. The wanted gate (F10) already forbids sleeping *while* hot. Revisit only with play-test evidence. |
| **R9** | **Payout notifications land on a black screen** and the player misses them. | High | Confusion about whether anything happened | Deliberate: the notifications persist past the fade-in, and the wake hint (T5.3) tells the player time passed. They are the player's evidence the accounting ran — do not suppress them. |
| **R10** | **`OVT_PlayerData` grows a field for a feature 90% of sessions never use.** | Certain | 4 bytes per player record | Accepted. The alternative (a side table keyed by persistent id, with its own serializer) is more format, not less. |
| **R11** | **Discovered, not fixed:** both `CheckUpdate` timers are scheduled at `FREQUENCY / GetDayTimeMultiplier()` — the **day** multiplier only (`OVT_EconomyManagerComponent.c:1291`, `OVT_OccupyingFactionManager.c:306`). At night the clock runs at 12×, so a tick advances ~2 in-game minutes and the occupying faction's `minutes == 0` and `minutes % 15 == 0` gates can be **skipped entirely**. Resource gain and threat decay are therefore unreliable at night. The economy is immune because it latches on the hour. | Certain (present today) | Occupying faction occasionally loses a night payday | Out of scope. **File as a new bug** (next id after BUG-179) against `occupying/*`, noting that the sleep catch-up replays these steps *exactly*, so a slept-through night is more accurate than a lived-through one. |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Accounting catch-up and BUG-179 | `component-developer-advanced` | **yes** — two shipped `CheckUpdate` seams, a live-code extraction, and a shipped money exploit |
| 2 — Sleep service, clock advance, cooldown persistence | `component-developer` | no |
| 3 — User action and bed overrides | `component-developer` | no |
| 4 — Cot placeable | `component-developer` | no |
| 5 — Fade, polish, pad pass | `component-developer` | no |
| 6 — Help and documentation sync | `help-docs-sync` | no |
| 7 — Review fixes (D16a, D18/D18a) | `component-developer-advanced` | **yes** — a live war-state gate and a persisted stamp's meaning |
