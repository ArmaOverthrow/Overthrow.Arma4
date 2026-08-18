# Sleep - Context & Decisions

**Last Updated:** 2026-08-19 (post-review)
**Current Phase:** Complete — six phases + Phase 7 review fixes, 32/33 (T6.3 blocked)
**Status:** 🟢 Ready for Review — automated gates all green (All group 179/179); the play-test checklist and the wiki sync remain

---

## Quick Status

**What's Done:**
- ✅ Planning complete (`implementation.md`, 6 phases, D1-D17 decided, R1-R11 assessed)
- ✅ Dev docs scaffolded (`tasks.md` 29 tasks across 6 phases)
- ✅ **Phase 1** (T1.1-T1.8) — `OVT_SleepSchedule`, `AssertHourLatches` + BUG-179's three call sites,
  `HandleTimeSkip` on both managers, the two `OVT_OccupyingFactionManager` extractions, and the Logic
  suite. `tools/compile-check.sh` exit 0. Suite run and BUG-179's play-test checks are the
  orchestrator's; BUG-179 stays `open` until they pass.
- ✅ **Phase 2** (T2.1-T2.6) — `OVT_PlayerData.m_fLastSleepGameHours`, `OVT_PlayerManagerSerializer`
  version 5 (+ `ClearSleepCooldown`), `OVT_SleepService` (location gate, `CanSleep`, cooldown, the
  deferred skip and the `SetDate` ladder), the Phase 2 half of the Logic suite, and a persistence
  round-trip case for the stamp. `tools/compile-check.sh` exit 0. Suite runs are the orchestrator's;
  §4 says the **All** group after this phase, because persistence state changed.
- ✅ **Phase 3** (T3.1-T3.3) — `OVT_SleepAction` (SP + location gate cached for 1 s, QRF/wanted/cooldown
  as disabled-with-reason, the countdown label), the **seven bed override prefabs** (+ `.et.meta` carrying
  each vanilla resource GUID), and the five `#OVT-Sleep*` keys in the `.st` **and** all seven runtime
  `.conf` exports. `tools/compile-check.sh` exit 0.

- ✅ **Phase 4** (T4.1-T4.4) — `Prefabs/Props/Military/Furniture/OVT_Cot_Placed.et` (+ `.et.meta`), the
  `OVT_Placeable` "Cot" entry in `Configs/Resistance/placeables.conf`, the two `#OVT-Place_Cot*` keys in the
  `.st` and all seven exports, and the D9 persistence verification — which changed nothing:
  `Configs/Systems/Persistence/Overthrow.conf` is clean in `git status`. `tools/compile-check.sh` exit 0 and
  `tools/check-placeables.py` exit 0 (`OK Cot type 'Cot' across 1 prefab(s)`).

- ✅ **Phase 5** (T5.1-T5.6) — the fade (`SetFade(bool)` on `OVT_SleepService`, null-guarded end to end,
  `SCR_FastTravelComponent.c:264-296`'s shape), the wake hint `#OVT-SleptWell`, the **static halves** of the
  effect-registration check (T5.2) and the pad pass (T5.4), the seven-key localization audit (T5.5) and this
  document's play-test checklist (T5.6). `tools/compile-check.sh` exit 0.
  **Q4 is structural**: `ScheduleSleep` calls `SetFade(true)` and then `CallLater(PerformSleepNow, …)` with no
  branch between them, so no missing screen effect can reach the schedule. `WORK_DELAY_MS` unchanged at 1500.

- 🟡 **Phase 6** (T6.1-T6.2 ✅, T6.3 ⏸️) — a new "Sleeping" Field Manual entry last in the **The Resistance**
  category (nine content pieces), a new cot paragraph under *Camps and Placing*'s "Where Placing Works"
  header, and **eleven** new `#OVT-FieldManual_*` keys in the `.st` and all seven runtime exports
  (784 → 795 entries per half; `.st` braces 1570/1570 → 1592/1592). Every one of the eleven items carries a
  `FACT-CHECKED … file:line` ledger in its `Comment`, the established Field Manual form. One drafted claim
  was **cut** for lack of a citation (see the Phase 6 note in `tasks.md`).
  `tools/compile-check.sh` exit 0. **T6.3, the public wiki, was not touched at all** — the `mcp__wikijs__*`
  tools were absent from the Phase 6 session, so there was no wiki to audit or update. Nothing was faked.

- ✅ **Phase 7 — REVIEW FIXES** (T7.1-T7.3, 2026-08-19) — two things the six built phases got wrong or that the
  user changed their mind about:
  **(a) D18/D18a**, the occupying faction had *neither* of the two edge defences the economy manager has: it
  double-counted the boundary a skip lands on (a third `Gaining Resources` pair, breaking **Q1**/**F7**) and
  lost the one a skip starts on (breaking **Q2**), plus the same error one grid finer on the threat decay.
  Fixed with two live-tick latches mirroring `m_iHourPaidIncome`, a start-boundary flush and a
  landing-instant assertion in `HandleTimeSkip`; the payload became `GainAndSpendResources()` so the flush is
  not a third copy of it, and `OVT_SleepSchedule` gained `IsStepBoundary`.
  **(b) D16a**, the user rejected the four-hours-on-waking cooldown: `StampCooldown` now writes the **wake**
  instant (`pre-skip + SKIP_HOURS`), so the full twelve hours are ahead on waking and it is twenty hours door
  to door. One Logic case changed, two added, four fault injections recorded.
  `tools/compile-check.sh` exit 0.

**What's Next:**
- ⏭️ **The play-test checklist below** — the only thing standing between this feature and Done. Item 1 first
  (a bed inside an owned house), since it is the one step that could still disprove R5.
- ⏭️ **T6.3** — the public wiki sync, from a session that has the wikijs MCP server attached.
- ⏭️ **The Workbench localization re-export** — eighteen hand-mirrored keys. This buys *translations* for the
  six non-English locales; it is not needed for the keys to render (see the Gotchas).
- ⏭️ **BUG-179** stays `open` until play-test items 18-20 pass; **BUG-182** (the R11 night-tick finding, filed
  2026-08-18 against `occupying/*`) is discovered-not-fixed and out of this feature's scope.

**Blockers:**
- ✅ **The suite gate is DISCHARGED.** It was deferred through the build (the permission classifier denied
  `tools/run-tests.sh` on 2026-08-18); the user granted permission on 2026-08-19 and it was run twice — **All
  group 177/177** over the as-built tree, then **All group 179/179** over the post-review tree, both exit 0
  with zero failures. Every case this feature added was confirmed present in `junit.xml` and green.
- ⏸️ **T6.3 (public wiki sync) is not done and is not doable from this session** — the `mcp__wikijs__*` tools
  are not attached here at all. Not an auth failure, not a stale render: there is no server to talk to.
  Re-run T6.3 from a session with the wikijs MCP server attached; the source text to mirror is the eleven
  English Field Manual strings, in player language.

---

## Needs human verification

**Nothing below has been done.** It is everything about this feature that no agent can check: the whole
feature's Verification Method (`implementation.md` §6) merged with the manual items Phases 3, 4 and 5 each
recorded as owed. Work down it in order — the early items are the cheapest and the ones most likely to
invalidate the later ones. DoD ids in **bold** are `implementation.md` §6.

**Before you start:** nothing. Both prerequisites this section used to name are discharged.

- ✅ **The suites are green** — `tools/run-tests.sh` **All** group, **179/179, exit 0**, 0 failures, run by the
  orchestrator on 2026-08-19 over the post-review tree. All eleven `OVT_TEST_Logic_SleepSchedule_*` cases and
  `OVT_TEST_PersistenceRoundTrip_PlayerSleepCooldown_SurvivesSaveAndReload` were confirmed present in
  `junit.xml` and green, so the phases' compile-plus-model can-fail proofs now have a real in-harness pass
  behind them.
- ✅ **The Workbench re-export is NOT a prerequisite.** An earlier version of this line said it was; that was
  backwards. The seven runtime `.conf` exports **are** the shipped path and they are fully mirrored — the
  cross-phase review verified all seven structurally at 795 `Ids` / 795 `Texts`, aligned and alphabetical,
  with every one of this feature's 18 keys resolving to its English text in every export, and the `.st` id set
  exactly equal to the export id set. **Q6 is met today.** The re-export is translator/tidiness debt (it is
  what gives non-English locales real translations instead of the English placeholder), not a play-test
  blocker — see item 26.

### A. The action exists and is offered in the right places

1. Start an SP campaign. Buy/claim a house. Note the clock, the money and the occupying faction's threat
   (console `Reduced Threat to:` lines). Stand at a bed **inside the owned house** → the Sleep action is
   offered. **(F1.)** ⚠️ *Do this first:* it is the single step that would disprove **R5** (beds inside houses
   being baked children an override cannot reach). Phase 3 retired R5 on paper — the inbound references are
   `GenericEntity : "{GUID}…Bed_XX.et"` inside furniture compositions, i.e. prefab instances — but paper is
   not a world.
2. Walk to a bed in a house you do **not** own → no action at all. **(F2.)**
3. Enumerate **all seven bed models** — `Bed_01`, `Bed_02`, `BedDouble_01`, `BedDouble_02`, `BunkBed_01`,
   `BunkBed_01_Double`, `BunkBed_02` — and confirm each one offers the action. A wrong `.et.meta` GUID makes
   exactly one model silently actionless (**R4**); the failure is safe but invisible from a script.
4. On each of those seven, check the **approach offset and radius**: the action should appear when you stand
   beside the bed and disappear a step or two away. These are a play-test dial and were reasoned, not measured
   (the `.xob` is binary and records no extents) — the table and the reasoning are in `tasks.md`'s Phase 3
   note. Too small = nothing while standing at the side; too large = the action follows you around the room.
   Retune in the prefab, nowhere else.
5. Offer-location coverage, one verification each: a bed in the player's **own camp**, at a **deployed FOB**,
   and at a **captured base**. **(F1.)**

### B. The skip itself

6. Sleep. → the screen **fades to black** (1.0 s), holds (0.5 s), the clock is **+8 in-game hours**, and it
   fades back in. **(F3, F4 — repeat once in broad daylight.)** This is also T5.2's runtime half: the fade
   proves `SCR_FadeInOutEffect` is registered. If there is **no fade but the clock still moved**, that is Q4
   behaving correctly and the fallback is registering the effect on
   `Prefabs/Characters/Core/OVT_PlayerController.et` — **not** building a UI. If the clock did **not** move,
   that is a real defect.
7. Count the income notifications during/after the black screen and check the money delta against the number
   of 6-hour boundaries the window crossed. Do it **twice**: once starting at **10:00** (lands exactly on
   18:00 → exactly two payouts, not three) and once starting at **exactly 12:00** (→ 18:00 only, and the
   12:00 payout the live tick owed is not lost). **(F5, Q1, Q2.)**
   ⚠️ **Count the console's `Gaining Resources`/`Reserve Resources` pairs on the same two runs** — the
   occupying faction had neither edge defence until 2026-08-19 (D18) and these are the exact two starts that
   used to produce three pairs and one pair respectively. Two pairs, then two pairs.
8. The wake hint `You slept for 8 hours` shows for ~4 s, and the payout notifications are **still visible**
   beside it — they are deliberately not suppressed (**R9**). **(Q6.)**
9. Sleep from **04:00** (crosses 07:00) → shop stock refilled. From **20:00** (crosses 00:00) → rent moves.
   From **08:00** (crosses neither) → neither. **(F6.)**
10. Watch the console across a skip: one `Gaining Resources` / `Reserve Resources` pair per 6-hour boundary,
    and `Reduced Threat to:` **exactly 32 times** when the start minute is not on a quarter hour, **33** when
    it is (the extra line is the start-boundary flush paying a step the live tick owed — D18a); threat is
    lower after than before. **(F7.)**
11. Immediately look at the bed again → the label reads roughly `Sleep (12h 0m)` — the **full** twelve, since
    2026-08-19's D16a moved the cooldown's origin to waking — and the action is disabled. Watch the countdown
    fall as time passes; a bed will not take you again until **twenty in-game hours** after you lay down.
    **(F8, D16a.)** Check it is legible at TV distance.
12. Sleep at ~**22:00** so the skip crosses midnight → the in-game **date** advanced (map/journal clock or the
    sun), and the cooldown afterwards counts **down** normally — never negative, never absurd. **(Q5, R3.)**
13. Trigger a QRF (attack a base) and look at a bed → **visible, disabled**, localized reason. Raise the
    wanted level and look again → visible, disabled, a **different** localized reason. **(F9, F10, Q6.)**
14. Confirm a normal campaign hour still gains, spends and decays on schedule without any sleeping — the
    Phase 1 extraction out of `OVT_OccupyingFactionManager.CheckUpdate` must be behaviour-neutral, **and so
    must Phase 7's two latches (D18)**. Sit through at least two consecutive six-hour boundaries (e.g. 12:00
    then 18:00) and two consecutive quarter hours without sleeping: each must produce exactly one
    `Gaining Resources`/`Reserve Resources` pair and one `Reduced Threat to:` line, and the second boundary
    must not be suppressed by the first. **(Q9, R6.)**

### C. The cot

15. Build a **Cot** at a camp (cost 30, between Furniture and Pirate Radio in the build menu), place it, sleep
    on it, save, **Continue** → the cot is still there, in place, and still carries a working Sleep action.
    **(F12, I1.)** Nothing static can check any of this; `tools/check-placeables.py` exits 0 and is the only
    gate that exists.

### D. Gamepad (T5.4's runtime half)

16. **Pad only, no keyboard or mouse:** approach a bed of each type, read the countdown label, perform the
    action, and place a cot from the build menu. The action adds no keybinding — it rides the standard
    user-action interaction bind — so a failure here is an offset/radius or focus problem, not a binding one.
    **(Q7.)**

### E. BUG-179 — the shipped money exploit this feature fixes

17. Play until the **12:00** income notification fires. Note the money. Save, exit to menu, **Continue** →
    **no second income payout**; money unchanged by the load.
18. Repeat during hour **7** (after restock — shop stock must not jump again) and during hour **0** (after
    rent).
19. Let the clock reach the next 6-hour boundary after loading → income pays exactly **once**.
    **(Q3.)** `docs/bugs/BUG-179.md` stays `status: open` until 17-19 pass, and the **orchestrator** flips it,
    not an implementing agent.

### F. Persistence

20. Sleep, then quit → **Continue**. The countdown resumes at roughly where it was — not 0, not 12. **(I2.)**
21. Load a save written **before** this feature (player payload version 4): it loads clean and every player is
    treated as never having slept. **(I3.)**
22. Furniture inside houses still shows its beds, and the beds still look, block and take damage exactly as
    before. **(I4.)**

### G. Multiplayer — the one thing that must NOT work

23. `tools/launch-server.sh`, then
    `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`.
    Walk to a bed → **no Sleep action at all**. **(F11.)** ⚠️ Pass a long `--timeout`; the default 600 s kills
    the client mid-test.

### H. The help surfaces (Phase 6)

24. Open the Field Manual → **The Resistance** → **Sleeping** is the last entry, its four sections render,
    and no `#OVT-` key shows as a raw key (if one does, it is a mirroring mistake in the seven `.conf`
    exports, **not** a missing Workbench re-export — see the Gotchas). The entry
    uses the shared `default_ui.edds` tile because no `sleeping_ui` art exists; **a dedicated tile is owed**
    and every other entry has one.
25. **Camps and Placing** → the "Where Placing Works" section ends with the new cot sentence.

---

## Key Files

### Core Implementation
- `Scripts/Game/Services/OVT_SleepSchedule.c` *(new)* — the world-free arithmetic; the Logic tier's subject
- `Scripts/Game/Services/OVT_SleepService.c` *(new)* — static orchestrator (gates, catch-up, clock, cooldown, fade)
- `Scripts/Game/UserActions/OVT_SleepAction.c` *(new)* — the action, its cache and its countdown label
- `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c` — `HandleTimeSkip`, `AssertHourLatches` (BUG-179)
- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` — `HandleTimeSkip` + two extractions out of `CheckUpdate`
- `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c` — version 5
- `docs/features/resistance/sleep/implementation.md` — the plan; §3 architecture, §4 phases, §5 decisions (D1-D17), §6 DoD, §10 risks

### Related Files
- `Scripts/Game/Utilities/OVT_ItemLimitChecker.c:87-141` — the source of truth for the location gate's ordering and its four distance constants
- `Scripts/Game/UserActions/OVT_RearmVehicleAction.c:23-30` — the 1-second action cache shape
- `Scripts/Game/UserActions/OVT_SabotageTowerAction.c:63` — key-plus-literal label concatenation; blocked-actions-stay-visible rule
- `Prefabs/Props/Furniture/OVT_CabinetMetal_01_grey_V1.et` — the "vanilla prop wrapped as a placeable" precedent for the cot
- `Prefabs/Structures/Signs/Traffic/SignBusStop_01.et:1-47` — the same-path override precedent for the bed deltas

---

## Important Decisions

Plan decisions **D1-D17** live in `implementation.md` §5 — not repeated here. The ones that constrain
implementation hardest:

- **D12 — single player only, no networking.** No RPCs, no controller component, no replicated state, no
  `OVT_Global` accessor. The machine that clicks is the authority. Do not cargo-cult the recruit-ux
  client→server seam.
- **D3/D4 — the window is half-open at the start** (`(start, start+8h]`) and the economy path calls
  `CheckUpdate()` once first; afterwards all three latches are asserted to the landing hour. This is the
  entire defence against double- and missed pay.
- **D6 — the occupying-faction replay is chronological**, 15-minute steps, gain/spend on 6-hour boundaries,
  because `GainResources` scales with a threat value that decays between gains.
- **D7 — the counter-attack roll and the town-uprising scan are excluded** from the replay, deliberately,
  and the exclusion is documented in the method so nobody "fixes" it later.
- **D5 — BUG-179 is fixed by asserting latches to the current hour, not by persisting them.** Zero serializer
  churn; also fixes the live re-apply path.
- **D15 — the date wrap uses `SetDate`'s own validation ladder** (d+1 → month+1/1 → year+1/1/1). No
  hand-written calendar maths.
- **D17 — `CooldownRemainingHours` fails open** on a negative elapsed time; fail-closed would lock the action
  permanently on a save whose clock misbehaved.
- **D16a (2026-08-19, user) — the cooldown runs from WAKING.** `StampCooldown` writes
  `ReadAbsoluteGameHours(time) + SKIP_HOURS`. D16's lie-down-relative rule is superseded, not deleted; the
  stamp is still one float read off the pre-skip clock and the serializer is untouched. **Anything reading
  `m_fLastSleepGameHours` is reading a wake time.**
- **D18/D18a (2026-08-19, review) — the occupying faction's live gates have latches now.**
  `m_iHourGainedResources` and `m_iMinuteDecayedThreat`, both armed at `-1`, mirroring `m_iHourPaidIncome`
  including its lack of an else-reset. Do NOT add an else-reset "for symmetry with the stock/rent latches" —
  consecutive boundaries are always different numbers, and a reset would re-open the duplicate. The
  town-uprising scan in the same `if` block is deliberately **not** latched.

---

## Gotchas / Notes

- ⚠️ **Integer division in EnforceScript is context-dependent** — every floor in `OVT_SleepSchedule` is written
  `(int)Math.Floor((float)a / b)`.
- 🔴 **`AbsoluteGameHours` is epoch-relative (`EPOCH_YEAR = 1989`), not §3.4's literal `year * 366`.** An
  EnforceScript `float` is IEEE binary32 and the engine's default campaign year is 1989, so the literal
  formula lands at ~1.75e7 where the float increment is **2 hours** — a twelve-minute difference between two
  stamps measures as exactly 0.0, and the cooldown countdown would never move. Do not "restore" the plan's
  formula. Signature and monotonicity are unchanged; the stamp is still one float.
- **`OVT_SleepSchedule.StepMinuteAt(...)` exists beyond T1.1's list** — §3.3's chronological loop needs the
  absolute minute of each step and the listed eight cannot produce it. The alternative was `% 15` inline in a
  `CheckUpdate` seam, which §7 forbids.
- ⚠️ **Phase 2's `PerformSleep` MUST re-validate through `CanSleep` before calling either `HandleTimeSkip`.**
  `OVT_OccupyingFactionManager.HandleTimeSkip` deliberately does not re-check `m_CurrentQRF` (a silent
  no-op there would replay the economy but not the war), so the QRF gate lives entirely in the service.
- ⚠️ **The Logic tier is grepped** for Overthrow's static manager accessor and the engine's game-mode getter —
  neither identifier may appear anywhere under `TestSuites/Logic/`, *including in comments*.
- ⚠️ **An unbalanced brace in `Language/localization_Overthrow.st` costs the whole file** on the next Workbench
  save. Check brace balance before and after every localization edit.
- ⚠️ **`GetNearest*` have no maximum range** — all four location lookups return the nearest record on the map,
  so the distance check is mandatory.
- ✅ **The hand-mirrored `.conf` exports ARE the shipped path — a raw `#OVT-` key on screen would mean a
  mirroring mistake, not a missing re-export.** Every phase mirrored its new keys into all seven runtime
  exports (commit `6085521a` is the recipe), and the cross-phase review verified the result structurally:
  795 `Ids` / 795 `Texts` in all seven, aligned and alphabetical, `.st` id set exactly equal to the export id
  set, all 18 of this feature's keys resolving to English text everywhere. The Workbench re-export is still
  owed, but it buys **translations for the six non-English locales** (which currently carry the English text
  as a placeholder) — it is not what makes the keys render.
- 🔴 **`CallLater` cannot schedule a static function** — it binds a method on an OBJECT
  (`OVT_NavmeshRebuild.c:53`). `OVT_SleepService` therefore keeps one lazily-created instance holding only the
  pending sleeper's id and a re-entry flag; every entry point is still static. Phase 5 wraps
  `ScheduleSleep`/`PerformSleepNow` with the fade and must not "simplify" the instance away.
- ⚠️ **`WORK_DELAY_MS = 1500`, not §3.2's 1.2 s.** The plan contradicts itself: §3.2 sketches +1.2 s, T5.1
  specifies `(FADE_DURATION 1.0 + BLACK_DURATION 0.5) * 1000`. Phase 2 used the Phase 5 number so Phase 5 only
  had to add the two fade calls. It is still a literal, not a derived expression — the call queue takes an int
  and there is nothing to round; if either duration changes, change the delay too (its doc comment says so).
- 🔴 **Q4 is a STRUCTURAL guarantee, and keep it that way.** `OVT_SleepService.SetFade(bool)` returns `void`,
  swallows both null cases itself, and `ScheduleSleep` calls it on the statement *before* the unconditional
  `CallLater(PerformSleepNow, …)` — there is **no branch and no value** between the fade and the schedule, so
  no missing `SCR_ScreenEffectsManager` and no missing `SCR_FadeInOutEffect` can prevent the 8-hour skip. Do
  not "improve" this into `if (SetFade(true)) { … }`.
- ⚠️ **`SCR_FadeInOutEffect.FadeOutEffect` silently does nothing below 0.1 s.** `FADE_DURATION` is 1.0 and is
  deliberately not a config knob.
- **`SetFade(true)` lives inside `ScheduleSleep`, after the re-entry guard** — not in `PerformSleep` — so a
  second perform while a skip is in flight cannot fade to black without a matching fade-in. One fade out per
  scheduled skip, always paired.
- **The fade back in is at the end of `PerformSleepNow()`, not on a second `CallLater(+0.5 s)`.** §3.2 step (f)
  sketches the extra timer; T5.1 specifies the `SCR_FastTravelComponent.c:264-296` shape, where the work
  function fades back in in the same frame. That shape was used.
- **`SCR_FadeInOutEffect` is INHERITED, never registered by this mod.**
  `Prefabs/Characters/Core/OVT_PlayerController.et:1-9` is a nine-line delta that only *adds* two components
  and never declares `SCR_HUDManagerComponent`; vanilla's `DefaultPlayerControllerMP.et:12-31` re-declares the
  HUD manager and `SCR_ScreenEffectsManager` under the **same GUIDs** as its own parent (a delta, not a
  replacement), and the effect itself is at `ArmaReforger/Prefabs/Characters/Core/DefaultPlayerController.et:49-53`.
  If the play-test finds no fade, the fallback is registering the effect on `OVT_PlayerController.et` — not
  building a UI.
- **This feature adds no keybinding.** `OVT_SleepAction` is a `ScriptedUserAction` inside a `UserActionContext`
  on each prefab and rides the standard interaction bind; no input `.conf` is touched.
  `.claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` exits 0 with 0 errors / 0 warnings
  (its three combo notes are the shipped menu binds). Its known blind spot — inline `ActionContext` actions —
  does not apply, because this feature declares none.
- 🔴 **`m_fLastSleepGameHours` is a WAKE time, not a lie-down time** (D16a, 2026-08-19). `StampCooldown` adds
  `SKIP_HOURS` to the pre-skip clock read. Do **not** "simplify" it to read the post-skip clock instead: the
  addition keeps the stamp on the same read the accounting catch-up used and does not depend on the `SetDate`
  ladder having worked. **Two accepted costs**, both written into the method's doc, both fail-open:
  (1) `AbsoluteGameHours` uses `DAYS_PER_YEAR = 366` for every year, so a skip across new year reads as
  already expired — once per in-game year; (2) a clock that refuses to move leaves the stamp in the FUTURE,
  which D17 fails open on, so the player could sleep again at once and replay the accounting each time. That
  second one is the only way this convention degrades *worse* than the lie-down one it replaced, its
  precondition is "the skip visibly did nothing" (a hard play-test gate), and `AdvanceClock` now `Print`s
  when `SetTimeOfTheDay` or the whole `SetDate` ladder is refused so it cannot happen silently.
- ⚠️ **The occupying faction's live gates are latched now, the economy's have always been** (D18). Both are
  init `-1` and neither has an else-reset, and that is correct: consecutive boundaries are always different
  numbers. The latch can therefore only suppress a **repeat inside one in-game minute** — which also means it
  changes behaviour on **dedicated servers**, not only on the SP sleep path. Intended; it is a strict
  de-duplication.
- ⚠️ **`HandleTimeSkip` cannot use `CheckUpdate()` as its start flush** the way the economy manager does. That
  `CheckUpdate` also decrements the counter-attack timer, rolls a counter-attack, scans towns for uprisings
  and returns early on a QRF — none of which the replay may do (D7). Hence the two explicit flush blocks.
- 🔴 **A twelve-minute difference between two `AbsoluteGameHours` stamps measures `0.2001953`, not `0.2`.**
  Even epoch-relative, a campaign date lands at ~4.8e3 where the binary32 increment is ~0.00049 h, so the
  Logic tier's `EPSILON` (0.0001) is TOO TIGHT for any assertion on a difference of two stamps. The new case
  uses its own `STAMP_TOLERANCE = 0.01` and says why. Anything later that compares cooldown remainders must do
  the same; exact halves and whole hours are still exact.
- **`SetFailure` takes at most three format parameters** (same limit as `PrintFormat`). A fourth is a compile
  error, not a silent truncation — build a combined string instead.
- **`CanSleep` does NOT check the location.** That is deliberate and matches §4 T2.3: location is the action's
  *visibility* gate (`IsSleepLocation` from `CanBeShownScript`), not a refusal with a reason. So
  `PerformSleep`'s re-validation covers QRF/wanted/cooldown only.
- **Refusal reason keys live on the service** — `OVT_SleepService.REASON_QRF/REASON_WANTED/REASON_COOLDOWN`.
  A refusal with an EMPTY key is deliberate ("not single player", "no record", "no clock"): those are states in
  which the action is not shown at all, so Phase 3 must not invent a sixth localization key for them.
- **The four location distances were re-verified against `OVT_ItemLimitChecker.c:13-15` on 2026-08-18** and
  still match (30 / 75 / 100 + `m_Difficulty.baseRange`). Phase 3's check task can cite that.
- **GUID series reserved for this feature: `{6B5D0000000000XX}`** (verified free across `6B5D`/`6B5E`/`6B60`/`6B61`).
  **Consumed by Phase 3: `01`-`05`** (the five `.st` items, in the order Sleep / SleepCooldown / SleepQRF /
  SleepWanted / SleptWell) and **`10`-`14`, `20`-`24`, `30`-`34`, `40`-`44`, `50`-`54`, `60`-`64`, `70`-`74`**
  (five per bed override: ActionsManagerComponent, UserActionContext, PointInfo, OVT_SleepAction, UIInfo).
  **Consumed by Phase 6: `93`** (the cot Field Manual paragraph), **`94`-`9D`** (the *Sleeping* entry and its
  nine content pieces) and **`A0`-`AA`** (the eleven `CustomStringTableItem`s). **`9E`-`9F` and `AB` upward
  remain free.**
  **Consumed by Phase 4: `80`-`88`** (the cot prefab's resource GUID and its five added component /
  sub-object GUIDs), **`90`** (the `OVT_Placeable` entry) and **`91`-`92`** (the two placeable `.st`
  items). **`06`-`09`, `89`, `8A`-`8F`, `9E`-`9F` and `AB` upward remain free.**
- ⚠️ **A `Texts` entry in the runtime `.conf` exports can span several physical lines** — joined by a trailing
  `\` with a `""\` line between the halves — and can contain `\"` escapes. Inserting a key by LINE offset
  therefore desynchronises `Ids` from `Texts` for every key after the insertion point, silently. Phase 3 parsed
  both blocks into parallel ENTRY lists, inserted by entry index, and re-verified by looking unrelated keys back
  up. All seven exports were 777 entries per half before and 782 after.
- ⚠️ **`grep -c "OVT-Sleep" localization_Overthrow.en-us.conf` is NOT a usable acceptance check** (the plan's
  §4 Phase 3 criterion). It returns 4, not 5 — `OVT-SleptWell` does not contain the substring — and the `Texts`
  half returns 0 because texts are prose, not keys. Use the entry-count/alignment check instead.
- **No vanilla bed carries a `UserActionContext`**, and neither the `.et` nor the binary `.xob` records model
  extents, so the seven action offsets/radii were reasoned from the model each prefab references
  (`0 0.6 0` / r1.2 single, r1.5 double; `0 0.9 0` / r1.3 bunk, r1.7 double bunk — Y is up in `Offset`, as
  `ArmaReforger/Prefabs/Props/Furniture/BarTap_01.et:26-28` shows). **They are a play-test dial**, tuned in the
  prefabs and nowhere else; the table and the reasoning are in `tasks.md`'s Phase 3 note.
- **`Omnidirectional` is deliberately left inherited on the bed contexts.** Vanilla furniture sets it to 0
  (one approach side), which is right for a tap and wrong for a bed a player can walk around.
- **`OVT_SleepAction` resolves the actor as `SCR_PlayerController.GetLocalControlledEntity()` in all three
  gates**, never the `user` argument — `GetActionNameScript` receives no `user` at all, and two different
  sources for "who is asking" is the actual hazard. Do not add an `RplComponent.IsOwner()` gate: the bed
  prefabs have no `RplComponent`.
- 🔴 **A component the cot INHERITS must be overridden by reusing its GUID, never by declaring a new one.**
  `CotMilitary_US_01.et` inherits `FurnitureMilitary_base.et`, which this repo already overrides
  (`Prefabs/Props/Military/Furniture/FurnitureMilitary_base.et:4`) to add
  `OVT_PlaceableComponent "{65CE9A2ECEC30E10}" { m_sPlaceableType "Furniture" }`. `OVT_Cot_Placed.et` therefore
  re-declares **that same GUID** with `m_sPlaceableType "Cot"`; a fresh GUID would have given the entity two
  `OVT_PlaceableComponent`s and made `FindComponent` (used by `PlaceItem` and every type-matching job condition)
  ambiguous. Same rule for `RplComponent "{5624A88DC2D9928D}"`, which
  `DestructibleMultiPhase_Props_Base.et:16` already declares (disabled) and the `{ Enabled 1 }` block turns on.
  Only genuinely NEW components take reserved-series GUIDs.
- **`tools/check-placeables.py` is the static gate for anything placeable** — it walks the whole inheritance
  chain across mod and base game and reports the resolved `m_sPlaceableType` plus replication, so it catches
  exactly the mistake above. Exit 0 with `OK Cot type 'Cot' across 1 prefab(s)` after Phase 4.
- **D9 held exactly as planned:** the persistence rule is component-keyed
  (`Configs/Systems/Persistence/Overthrow.conf:152-170`, `ComponentClass "OVT_PlaceableComponent"` at `:154`,
  `SelfSpawn 1` at `:158`) and `OVT_ResistanceFactionManager.c:773` tracks the placed entity. The plan's
  `:152-171` was one line long — `:171` is the next config (`OVT_BuildableComponent`). Nothing was changed.
- **R11 is discovered-not-fixed, and is now `BUG-182`** (filed 2026-08-18 against `occupying/*`) — both
  `CheckUpdate` timers schedule at `FREQUENCY / GetDayTimeMultiplier()` (the **day** multiplier only:
  `OVT_OccupyingFactionManager.c:306`, `OVT_EconomyManagerComponent.c:1449`,
  `OVT_TimeAndWeatherHandlerComponent.c:9-12`), while the engine swaps in a *night* acceleration at dusk
  (`ArmaReforger/.../SCR_TimeAndWeatherHandlerComponent.c:273/:281`). At night a tick strides ~2 in-game
  minutes and the occupying faction's minute-exact gates (`:1184`, `:1211-1214`) are skipped entirely.
  The economy is immune because it latches on the hour. Not fixed here.

---

## Session Notes

### 2026-08-18 — Autorun start
Feature resolved as `resistance/sleep` inside the **resistance** epic. `implementation.md` already existed
(planned same day), so `/autorun-feature` skipped planning and scaffolded `context.md` + `tasks.md` from the
plan's six phases. Status flipped to In Progress. Phase 1 is the only ADVANCED phase.

### 2026-08-18 — Phase 2 complete
`m_fLastSleepGameHours` (server-only, never replicated — it is NOT in the hand-written `RplSave`/`RplLoad`,
which was left untouched), serializer version 5 with the `version < 5` clear, and `OVT_SleepService`. The
service contains no `Rpc`, no `RplRpc` and no component class, per D12. `PerformSleepNow()` calls both
`HandleTimeSkip`s and stamps the cooldown BEFORE `AdvanceClock`, with the §3.3 ordering written into the
method's doc comment. Full detail and the judgement calls are in `tasks.md`'s Phase 2 session note.

### 2026-08-18 — Phase 4 complete
The cot ships as a NEW mod-owned prefab (D10) whose only genuinely new components are the player-owner,
editable-entity and actions blocks; `OVT_PlaceableComponent` and `RplComponent` are inherited-GUID overrides
(see the gotcha above). One `OVT_Placeable` entry, cost 30, minimal five-field shape, inserted between
`Furniture` and `PirateRadio`; the `Furniture` entry was not touched. Two localization keys through the same
structural seven-export procedure (782 → 784 entries per half, alignment re-verified). T4.4 changed nothing and
its findings, with file:line citations, are in `tasks.md`'s Phase 4 session note.

### 2026-08-19 — Phase 5 complete
The fade is one helper (`SetFade(bool)`) and three call sites, all in `OVT_SleepService.c`: `SetFade(true)`
immediately before `ScheduleSleep`'s `CallLater`, and `ShowCustomHint("#OVT-SleptWell", "", 4)` then
`SetFade(false)` as steps 5 and 6 at the end of `PerformSleepNow()`. The lazily-created instance and the
`ScheduleSleep`/`PerformSleepNow` split were left exactly as Phase 2 built them. **Q4's guarantee is
structural** and was additionally proven by temporarily nulling the screen-effects lookup (compile stayed exit
0; reverted, and the real `GetScreenEffectsDisplay()` call is back). T5.2 and T5.4 were done as **static
halves only** — the runtime confirmations are items 6 and 16 of the checklist above and are not claimed.
T5.5 audited all **seven** keys this feature references (five from the scripts, two from the cot's config and
prefab) against the `.st` and all seven exports with the structural entry parser: every key present exactly
once, all exports 784/784 aligned, `.st` braces 1570/1570. Full detail and the four judgement calls are in
`tasks.md`'s Phase 5 session note.

### 2026-08-19 — Phase 6 (T6.1/T6.2 complete, T6.3 blocked)
Three surfaces audited; two changed. The Field Manual gained a **"Sleeping"** entry (last in *The Resistance*,
nine pieces: an opening paragraph then *Where a Bed Works* / *The Hours Still Count* / *Twelve Hours Between
Sleeps* / *When It Will Not Perform*) and a **cot paragraph** appended to *Camps and Placing*'s "Where Placing
Works" section — that section is the only thing in the manual resembling a placeable listing; there is no
enumerated list to add to. The cot sentence is a **new** string item rather than an edit to
`OVT-FieldManual_Camps_Text2`, so that paragraph's existing German and Ukrainian translations stay whole.
**No tutorial popup was added**, deliberately: `OVT_TutorialTrigger.c:12-45` has no "an action became
available" event, and its nearest match `PLAYER_PLACE` (`:21`) would only ever catch the cot path, never the
bed path. Eleven new keys, each carrying its own `file:line` ledger in its `Comment`; one drafted sentence
(wanted level decaying on its own) was **cut** because this feature only reads `GetWantedLevel()` and never
its decay. **The wiki was not reachable from this session at all** (no `mcp__wikijs__*` tooling), so T6.3 is
genuinely owed and nothing about the wiki should be assumed done. Full detail in `tasks.md`'s Phase 6 note.

### 2026-08-19 — Phase 7, review fixes (post-build)

Two changes, one from the cross-phase review and one from the user, both landed as a numbered Phase 7 rather
than as edits smuggled into the built phases.

**The review defect (D18/D18a).** `OVT_EconomyManagerComponent` has always had both edge defences — a flush
`CheckUpdate()` for the open start and `AssertHourLatches(landingHour)` for the closed end.
`OVT_OccupyingFactionManager` had neither, and its two minute-exact payload gates had no latch to assert, so
a sleep landing exactly on a boundary hour produced a **third** `Gaining Resources`/`Reserve Resources` pair
and a sleep starting exactly on one **lost** its payday; the quarter-hourly threat decay had the same error at
both ends. Fixed with the economy's own idiom: two latches armed at `-1`, one extra condition and one
assignment per gate, a start-boundary flush and a landing-instant assertion in `HandleTimeSkip`. The four-line
boundary payload became `GainAndSpendResources()` — a third pure move in the T1.5/T1.6 spirit — so the flush
is not a fourth copy of it, and `OVT_SleepSchedule` gained one pure predicate, `IsStepBoundary`. **The 33rd
decay step was fixed rather than accepted**, because the same start-side loss applies to it; the
town-uprising scan sharing that `if` block was deliberately left alone.

**The user decision (D16a).** The cooldown now runs from **waking**: `StampCooldown` writes
`ReadAbsoluteGameHours(time) + SKIP_HOURS`. The player wakes with the full twelve hours ahead and cannot
sleep again until twenty in-game hours after lying down. No serializer change — the same float in the same
position, only its meaning — and the Field Manual sentence that promised "about four hours" was corrected in
the `.st` **and** all seven runtime exports with the structural entry parser (795/795 entries before and
after; each export verified byte-for-byte to differ from its pre-edit self by exactly that one sentence).

**Tests.** One Logic case changed (12 h on waking, plus a door-to-door row and a row that is red under the old
convention) and two added (`IsStepBoundary`; the window-edges case that states both edges as one property over
all 1440 start minutes with an independent enumeration as the oracle). Four fault injections recorded, each
compiled at exit 0 and reverted. Full detail and every judgement call are in `tasks.md`'s Phase 7 note.
