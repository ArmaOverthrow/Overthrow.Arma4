//------------------------------------------------------------------------------------------------
//! TIER A cases - the sleep time-skip boundary arithmetic (OVT_SleepSchedule).
//!
//! Every subject here is a static function of its arguments. Nothing in this file resolves a
//! manager, a controller or a world, and nothing needs to: OVT_SleepSchedule exists precisely so
//! that "how many payouts does an eight-hour skip starting at 11:47 contain" is answered by a
//! function this tier can pin. See OVT_TEST_LogicSuite.c for the tier rule and the house rules.
//!
//! WHY THIS FILE MATTERS MORE THAN MOST. The two failure modes it guards are a money printer and a
//! missing payday, and both are invisible in play - a player who is silently paid twice for one
//! six-hour boundary has no way to notice, and neither has anyone reading the manager. There is no
//! other assertion anywhere in the tree that would go red for either.
//!
//! THE PROPERTY BEING PINNED, stated once: the skipped window is (start, start + hours] - HALF-OPEN
//! at the start, CLOSED at the end (implementation.md 3.3 / D3). A start sitting exactly on a
//! boundary does NOT count that boundary (the live tick owes it, and the economy path flushes
//! CheckUpdate() once before replaying); a window ENDING exactly on a boundary DOES count it (the
//! live tick will never observe it, because the hour latches are asserted to the landing hour the
//! instant the replay finishes).
//!
//! COVERAGE MAP. Phase 1's half of the plan's case list (crossings, restock/rent entries,
//! quarter-hour steps, the boundary predicate, the landing hour, and the replay-walk invariants the
//! occupying-faction loop depends on) is the first six classes below. Phase 2's half - the cooldown
//! machinery: AbsoluteGameHours, CooldownRemainingHours and FormatRemaining - is the three classes
//! at the bottom, added exactly as this header predicted. Every function on OVT_SleepSchedule is
//! covered.
//!
//! 2026-08-19, POST-REVIEW. Two cases were ADDED and one was CHANGED, all for defects found by the
//! cross-phase review rather than by anything in this file going red:
//!   - ..._IsStepBoundary_ExactMinutesOnly pins the new quarter-hour predicate the start-boundary
//!     flush needs;
//!   - ..._WindowEdges_FlushPlusReplayCoversEachBoundaryOnce states BOTH edges of the window as one
//!     property, which is the statement whose absence let the occupying faction double-count its
//!     landing boundary and lose its starting one;
//!   - ..._CooldownRemaining_ClampsAndFailsOpen now asserts TWELVE hours remaining on waking rather
//!     than four, because the user moved the cooldown's origin from lying down to waking.
//!
//! CAN-FAIL PROOFS, AND HOW THEY WERE OBTAINED (2026-08-18, extended 2026-08-19). Running a suite is
//! the orchestrator's job, not an implementation agent's (.claude/test-policy.md), so the proofs here
//! are the same two halves OVT_TEST_Logic_GroupRecruits.c uses, and they are recorded per case below:
//!
//!   1. EACH FAULT WAS INJECTED INTO OVT_SleepSchedule.c AND COMPILED, one at a time - eight for
//!      Phase 1 (M1-M8), five more for Phase 2's cooldown half (M9-M13, 2026-08-18) and two more for
//!      the post-review work (M14-M15, 2026-08-19, plus a re-injection of M1 and M8 against the new
//!      window-edges case). Every one exited tools/compile-check.sh with 0 - which IS the point: a
//!      boundary counted at the wrong end is not a syntax error, nothing in the tree would stop it
//!      reaching players, and the assertion below is the only thing that would. The file was restored
//!      and re-compiled clean afterwards.
//!   2. THE RESULTING VALUE WAS COMPUTED ON A FLOAT32-FAITHFUL REFERENCE MODEL of the subject, so
//!      the exact number each fault produces - and therefore the exact failure text - is recorded
//!      rather than guessed. binary32 is not a detail here: an EnforceScript float IS IEEE binary32,
//!      and the FormatRemaining rounding trap documented in OVT_SleepSchedule.c only misbehaves at
//!      that precision (4.2 x 60 lands just under 252).
//!
//! No maxAttempts anywhere: every subject is a static function of int arguments with no clock, no
//! RNG and no world, and cannot flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! OVT_SleepSchedule.CountIntervalCrossings - how many six-hour income boundaries a skip contains.
//!
//! The four rows from the plan's case list, which between them fix both ends of the window:
//!   - 11:47 + 8 h -> 2 (12:00 and 18:00) - the ordinary case, from an arbitrary minute;
//!   - 08:00 + 8 h -> 1 (12:00 only; the window ends at 16:00, short of 18:00);
//!   - exactly 12:00 + 8 h -> 1 (18:00 only - the boundary AT the start is excluded);
//!   - 10:00 + 8 h -> 2, landing exactly on 18:00 (the endpoint IS included).
//! Rows three and four are the whole contract: one says the start is open, the other says the end is
//! closed, and a single implementation has to satisfy both at once.
//!
//! Also here: a window that crosses midnight still counts against the same grid (18:00 + 8 h reaches
//! 02:00 and contains only 00:00), because a day is a whole number of six-hour intervals.
//!
//! CAN-FAIL, two faults, injected and compiled separately because the case makes two independent
//! claims and one fault must not stand in for the other:
//!   M1. CLOSE THE WINDOW AT THE START - `if (toFirst == intervalMinutes) toFirst = 0;` appended to
//!       the offset line, so a start sitting exactly on a boundary counts it. Compiled clean
//!       (exit 0). The 12:00 row then answers 2 and the case fails on "an eight-hour skip from
//!       exactly 12:00 must NOT re-count 12:00 (12:0 + 8 h) counted 2 six-hour crossings,
//!       expected 1" - the first of five rows this fault breaks, every one of them a start sitting
//!       exactly on a boundary. The 11:47 and 08:00 rows survive it untouched, which is exactly why
//!       a row starting ON a boundary had to be written down.
//!   M2. DROP THE "1 +" that counts the first boundary. Compiled clean (exit 0). The 11:47 row then
//!       answers 1 and the case fails on "an eight-hour skip from 11:47 crosses 12:00 and 18:00
//!       (11:47 + 8 h) counted 1 six-hour crossings, expected 2".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_SleepSchedule_IntervalCrossings_HalfOpenStartClosedEnd : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The plan's four rows.
		if (!ExpectCrossings(11, 47, 8, 2, "an eight-hour skip from 11:47 crosses 12:00 and 18:00"))
			return true;

		if (!ExpectCrossings(8, 0, 8, 1, "an eight-hour skip from 08:00 ends at 16:00 and crosses only 12:00"))
			return true;

		if (!ExpectCrossings(12, 0, 8, 1, "an eight-hour skip from exactly 12:00 must NOT re-count 12:00"))
			return true;

		if (!ExpectCrossings(10, 0, 8, 2, "an eight-hour skip from 10:00 lands exactly on 18:00 and must count it"))
			return true;

		// --- The same two edge rules stated at other hours, so the case is not pinning one lucky pair.
		if (!ExpectCrossings(0, 0, 8, 1, "an eight-hour skip from exactly 00:00 crosses only 06:00"))
			return true;

		if (!ExpectCrossings(6, 0, 6, 1, "a six-hour skip from exactly 06:00 lands exactly on 12:00: one, not two"))
			return true;

		if (!ExpectCrossings(23, 59, 8, 2, "a skip from 23:59 crosses 00:00 and 06:00"))
			return true;

		if (!ExpectCrossings(17, 59, 8, 2, "a skip from 17:59 crosses 18:00 one minute later, and 00:00"))
			return true;

		// --- Across midnight the grid does not restart: 18:00 + 8 h reaches 02:00 and contains 00:00 only.
		if (!ExpectCrossings(18, 0, 8, 1, "a skip from exactly 18:00 crosses midnight only"))
			return true;

		if (!ExpectCrossings(20, 0, 8, 1, "a skip from 20:00 ends at 04:00 and crosses midnight only"))
			return true;

		// --- A full day contains every boundary exactly once.
		if (!ExpectCrossings(0, 0, 24, 4, "a twenty-four hour skip contains all four boundaries"))
			return true;

		if (!ExpectCrossings(13, 22, 24, 4, "a twenty-four hour skip from any minute contains all four boundaries"))
			return true;

		// --- Degenerate arguments never produce a payout.
		if (!ExpectCrossings(11, 47, 0, 0, "a zero-hour skip"))
			return true;

		if (!ExpectCrossings(11, 47, -8, 0, "a negative skip"))
			return true;

		int badInterval = OVT_SleepSchedule.CountIntervalCrossings(11, 47, 8, 0);
		if (badInterval != 0)
		{
			SetFailure("A zero interval returned %1 crossings, expected 0", badInterval.ToString());
			return true;
		}

		Print("SleepSchedule crossings: the window is half-open at the start (12:00 + 8 h = 1) and closed at the end (10:00 + 8 h = 2), and the six-hour grid survives midnight");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one crossing count at the income interval, naming the row in the failure message.
	//! \param[in] startHour Hour of day the skip starts in.
	//! \param[in] startMinute Minute the skip starts at.
	//! \param[in] skipHours Length of the skip.
	//! \param[in] expected Number of boundaries the window must contain.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectCrossings(int startHour, int startMinute, int skipHours, int expected, string label)
	{
		int actual = OVT_SleepSchedule.CountIntervalCrossings(startHour, startMinute, skipHours, OVT_SleepSchedule.INCOME_INTERVAL_HOURS);

		if (actual == expected)
			return true;

		string window = startHour.ToString() + ":" + startMinute.ToString() + " + " + skipHours.ToString() + " h";

		SetFailure("%1 (%2) counted %3 six-hour crossings, expected " + expected.ToString(),
			label, window, actual.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_SleepSchedule.CountHourEntries - how many times a named hour-of-day is entered.
//!
//! Two payouts ride on this: the 07:00 shop restock and the 00:00 rent sweep. Both are once-a-day
//! events, so the question is not "how many" so much as "did it happen at all" - but the count form
//! is what keeps a skip longer than a day correct without a second code path, and the last row below
//! is the one that says so.
//!
//! The plan's rows: 04:00 + 8 h enters hour 7 once; 08:00 + 8 h enters it zero times; 20:00 + 8 h
//! enters hour 0 once (across midnight); 01:00 + 8 h enters hour 0 zero times. Plus the two edges
//! that matter for the same reason as in the crossings case - starting exactly ON the target hour
//! does not count it, landing exactly on it does.
//!
//! CAN-FAIL, two faults, injected and compiled separately:
//!   M3. CLOSE THE WINDOW AT THE START - the day-forward push relaxed from `if (toFirst <= 0)` to
//!       `if (toFirst < 0)`, so a start sitting exactly on the target hour counts it. Compiled clean
//!       (exit 0). The case then fails on "starting exactly at 07:00 must not re-restock (7:0 + 8 h,
//!       target hour 7) counted 1 entries, expected 0" - which in play is a second restock the
//!       moment a player sleeps at 07:00 sharp.
//!   M4. DROP THE "1 +" that counts the first entry. Compiled clean (exit 0). The case then fails on
//!       "sleeping from 04:00 crosses the 07:00 restock (4:0 + 8 h, target hour 7) counted 0
//!       entries, expected 1" - a restock silently skipped by every sleep that crosses 07:00.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_SleepSchedule_HourEntries_RestockAndRent : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Restock, hour 7.
		if (!ExpectEntries(4, 0, 8, OVT_SleepSchedule.RESTOCK_HOUR, 1, "sleeping from 04:00 crosses the 07:00 restock"))
			return true;

		if (!ExpectEntries(8, 0, 8, OVT_SleepSchedule.RESTOCK_HOUR, 0, "sleeping from 08:00 has already missed 07:00"))
			return true;

		// --- Rent, hour 0.
		if (!ExpectEntries(20, 0, 8, OVT_SleepSchedule.RENT_HOUR, 1, "sleeping from 20:00 crosses midnight rent"))
			return true;

		if (!ExpectEntries(1, 0, 8, OVT_SleepSchedule.RENT_HOUR, 0, "sleeping from 01:00 has already missed midnight"))
			return true;

		// --- The two edges, exactly as in the crossings case.
		if (!ExpectEntries(7, 0, 8, OVT_SleepSchedule.RESTOCK_HOUR, 0, "starting exactly at 07:00 must not re-restock"))
			return true;

		if (!ExpectEntries(23, 0, 8, OVT_SleepSchedule.RESTOCK_HOUR, 1, "landing exactly on 07:00 must restock"))
			return true;

		if (!ExpectEntries(0, 0, 8, OVT_SleepSchedule.RENT_HOUR, 0, "starting exactly at midnight must not re-charge rent"))
			return true;

		if (!ExpectEntries(16, 0, 8, OVT_SleepSchedule.RENT_HOUR, 1, "landing exactly on midnight must charge rent"))
			return true;

		// --- A window that contains neither.
		if (!ExpectEntries(8, 0, 8, OVT_SleepSchedule.RENT_HOUR, 0, "08:00 to 16:00 crosses neither restock nor rent"))
			return true;

		// --- Count, not boolean: a skip longer than a day enters the same hour twice.
		if (!ExpectEntries(4, 0, 30, OVT_SleepSchedule.RESTOCK_HOUR, 2, "a thirty-hour skip from 04:00 enters hour 7 twice"))
			return true;

		if (!ExpectEntries(1, 0, 48, OVT_SleepSchedule.RENT_HOUR, 2, "a forty-eight hour skip enters midnight twice"))
			return true;

		// --- Degenerate arguments.
		if (!ExpectEntries(23, 0, 0, OVT_SleepSchedule.RENT_HOUR, 0, "a zero-hour skip"))
			return true;

		if (!ExpectEntries(23, 0, 8, 24, 0, "an hour-of-day out of range"))
			return true;

		if (!ExpectEntries(23, 0, 8, -1, 0, "a negative target hour"))
			return true;

		Print("SleepSchedule hour entries: 04:00 + 8 h restocks once and 20:00 + 8 h charges rent once; starting exactly ON the hour never counts it, landing exactly on it always does");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one hour-entry count, naming the row in the failure message.
	//! \param[in] startHour Hour of day the skip starts in.
	//! \param[in] startMinute Minute the skip starts at.
	//! \param[in] skipHours Length of the skip.
	//! \param[in] targetHour Hour-of-day being counted.
	//! \param[in] expected How many entries the window must contain.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectEntries(int startHour, int startMinute, int skipHours, int targetHour, int expected, string label)
	{
		int actual = OVT_SleepSchedule.CountHourEntries(startHour, startMinute, skipHours, targetHour);

		if (actual == expected)
			return true;

		string window = startHour.ToString() + ":" + startMinute.ToString() + " + " + skipHours.ToString()
			+ " h, target hour " + targetHour.ToString();

		SetFailure("%1 (%2) counted %3 entries, expected " + expected.ToString(),
			label, window, actual.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_SleepSchedule.CountStepCrossings - how many quarter-hour steps a skip contains.
//!
//! This is the number of threat-decay steps the occupying faction owes for the skip, and it has one
//! property worth more than any single row: FOR A WHOLE-HOUR SKIP THE ANSWER DOES NOT DEPEND ON THE
//! START MINUTE. Eight hours is thirty-two quarter hours whether the player lies down at :00, at :07
//! or at :59, because the window is half-open at the start and closed at the end - whatever fraction
//! of a step is lost at the front is recovered at the back.
//!
//! That invariant is the plan's stated reason for the case, and it is exactly what breaks when the
//! floors are written with raw int division or the first step is miscounted. It is asserted here
//! over ALL 1440 start minutes rather than the four the plan lists, because the sweep is free.
//!
//! CAN-FAIL, two faults, injected and compiled separately:
//!   M5. DROP THE "1 +" that counts the first step. Compiled clean (exit 0). Every row then answers
//!       31 and the case fails on "an eight-hour skip from exactly :00 (11:0 + 8 h) counted 31
//!       steps, expected 32" - one threat-decay step short on every sleep.
//!   M6. INVERT THE OFFSET to the first step, `toFirst = startMinutes % stepMinutes` in place of
//!       `stepMinutes - (startMinutes % stepMinutes)`. Compiled clean (exit 0). This one is
//!       start-minute SENSITIVE, which is precisely what the sweep is for: :07 still answers 32 and
//!       survives, while :00 answers 33 - the extra step being the one AT the start, the double
//!       count the half-open window exists to prevent - and the case fails on "an eight-hour skip
//!       from exactly :00 (11:0 + 8 h) counted 33 steps, expected 32".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_SleepSchedule_StepCrossings_StartMinuteIndependent : SCR_AutotestCaseBase
{
	//! Steps an eight-hour skip must contain: 8 h / 15 min.
	static const int EXPECTED_STEPS_8H = 32;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The plan's four start minutes, stated explicitly so a failure names one.
		if (!ExpectSteps(11, 0, 8, EXPECTED_STEPS_8H, "an eight-hour skip from exactly :00"))
			return true;

		if (!ExpectSteps(11, 7, 8, EXPECTED_STEPS_8H, "an eight-hour skip from :07"))
			return true;

		if (!ExpectSteps(11, 14, 8, EXPECTED_STEPS_8H, "an eight-hour skip from :14, one minute short of a step"))
			return true;

		if (!ExpectSteps(11, 59, 8, EXPECTED_STEPS_8H, "an eight-hour skip from :59"))
			return true;

		// --- And the invariant itself, over every start minute of the day. One wrong minute is one
		// campaign where the occupying faction decays a step too much or too little every sleep.
		for (int hour = 0; hour < OVT_SleepSchedule.HOURS_PER_DAY; hour++)
		{
			for (int minute = 0; minute < OVT_SleepSchedule.MINUTES_PER_HOUR; minute++)
			{
				int steps = OVT_SleepSchedule.CountStepCrossings(hour, minute, 8, OVT_SleepSchedule.THREAT_STEP_MINUTES);

				if (steps == EXPECTED_STEPS_8H)
					continue;

				string start = hour.ToString() + ":" + minute.ToString();

				SetFailure("An eight-hour skip starting at %1 counted %2 quarter-hour steps; the count must be %3 from EVERY start minute",
					start, steps.ToString(), EXPECTED_STEPS_8H.ToString());

				return true;
			}
		}

		// --- Shorter and longer windows scale the same way.
		if (!ExpectSteps(3, 0, 1, 4, "a one-hour skip"))
			return true;

		if (!ExpectSteps(3, 22, 1, 4, "a one-hour skip from an arbitrary minute"))
			return true;

		if (!ExpectSteps(3, 0, 24, 96, "a full day"))
			return true;

		// --- Degenerate arguments.
		if (!ExpectSteps(3, 0, 0, 0, "a zero-hour skip"))
			return true;

		if (!ExpectSteps(3, 0, -1, 0, "a negative skip"))
			return true;

		int badStep = OVT_SleepSchedule.CountStepCrossings(3, 0, 8, 0);
		if (badStep != 0)
		{
			SetFailure("A zero step size returned %1 steps, expected 0", badStep.ToString());
			return true;
		}

		Print("SleepSchedule step crossings: an eight-hour skip is 32 quarter-hour steps from all 1440 start minutes - the count is start-minute independent");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one step count at the threat-decay resolution.
	//! \param[in] startHour Hour of day the skip starts in.
	//! \param[in] startMinute Minute the skip starts at.
	//! \param[in] skipHours Length of the skip.
	//! \param[in] expected Steps the window must contain.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectSteps(int startHour, int startMinute, int skipHours, int expected, string label)
	{
		int actual = OVT_SleepSchedule.CountStepCrossings(startHour, startMinute, skipHours, OVT_SleepSchedule.THREAT_STEP_MINUTES);

		if (actual == expected)
			return true;

		string window = startHour.ToString() + ":" + startMinute.ToString() + " + " + skipHours.ToString() + " h";

		SetFailure("%1 (%2) counted %3 steps, expected " + expected.ToString(),
			label, window, actual.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_SleepSchedule.IsIntervalBoundary - the predicate that drives the chronological replay.
//!
//! The occupying-faction replay walks quarter-hour steps and asks this of each one to decide whether
//! it is also a resource-gain boundary. It therefore has to be EXACT: true at 0, 360, 720 and 1080
//! minutes, false one minute either side, and false at a step that merely looks close (375).
//!
//! It takes an ABSOLUTE minute, not an hour, so the replay keeps working past midnight - 1440 is
//! 00:00 the next day and must read as a boundary, 1800 is 06:00 the next day and likewise. That is
//! only true because a day is a whole number of six-hour intervals, which is why the constant is
//! documented as having to divide 24.
//!
//! CAN-FAIL:
//!   M7. REPLACE THE MODULO with `return absoluteMinute == 0;` - the shape a "simplification" would
//!       produce, and note that it keeps the FIRST row (midnight) green. Compiled clean (exit 0).
//!       The case fails on the second row, "Minute 360 (06:00) was reported as boundary=0, expected
//!       the opposite", and every later true row behind it. In play this fault stops the occupying
//!       faction gaining resources at all during a skip.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_SleepSchedule_IsIntervalBoundary_ExactMinutesOnly : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The four boundaries of a day.
		if (!ExpectBoundary(0, true, "midnight"))
			return true;

		if (!ExpectBoundary(360, true, "06:00"))
			return true;

		if (!ExpectBoundary(720, true, "12:00"))
			return true;

		if (!ExpectBoundary(1080, true, "18:00"))
			return true;

		// --- And past midnight, which is where a wrapped skip spends half its steps.
		if (!ExpectBoundary(1440, true, "00:00 the next day"))
			return true;

		if (!ExpectBoundary(1800, true, "06:00 the next day"))
			return true;

		// --- One minute either side, and a quarter-hour step that is not a boundary.
		if (!ExpectBoundary(359, false, "one minute before 06:00"))
			return true;

		if (!ExpectBoundary(361, false, "one minute after 06:00"))
			return true;

		if (!ExpectBoundary(375, false, "06:15, a step but not a boundary"))
			return true;

		if (!ExpectBoundary(1439, false, "one minute before midnight"))
			return true;

		// --- Degenerate arguments are false, never true by accident.
		bool negative = OVT_SleepSchedule.IsIntervalBoundary(-360, OVT_SleepSchedule.INCOME_INTERVAL_HOURS);
		if (negative)
		{
			SetFailure("A negative absolute minute was reported as a boundary");
			return true;
		}

		bool zeroInterval = OVT_SleepSchedule.IsIntervalBoundary(0, 0);
		if (zeroInterval)
		{
			SetFailure("A zero interval was reported as a boundary");
			return true;
		}

		Print("SleepSchedule boundary predicate: exact at 0/360/720/1080 and past midnight at 1440/1800, false one minute either side and at the 06:15 step");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the boundary predicate at one absolute minute.
	//! \param[in] absoluteMinute Minutes from midnight of the starting day.
	//! \param[in] expected Whether it must be reported as a boundary.
	//! \param[in] label Human description of the minute, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectBoundary(int absoluteMinute, bool expected, string label)
	{
		bool actual = OVT_SleepSchedule.IsIntervalBoundary(absoluteMinute, OVT_SleepSchedule.INCOME_INTERVAL_HOURS);

		if (actual == expected)
			return true;

		SetFailure("Minute %1 (%2) was reported as boundary=%3, expected the opposite",
			absoluteMinute.ToString(), label, actual.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_SleepSchedule.IsStepBoundary - the same predicate one grid finer.
//!
//! ADDED 2026-08-19 with the start-boundary flush. The occupying faction's replay is half-open at the
//! start, so a quarter-hour decay step owed EXACTLY at the instant the player lay down belongs to the
//! live tick - and if that tick had not observed it yet, nothing would ever run it. The flush in
//! HandleTimeSkip asks this predicate whether the start instant is such a step, which is the only
//! reason the function exists: the alternative was "% 15" written inside a CheckUpdate seam, where
//! nothing could assert it.
//!
//! It takes an ABSOLUTE minute for the same reason IsIntervalBoundary does - the caller feeds it
//! (hour * 60 + minute), and a window that runs past midnight keeps answering correctly because an
//! hour is a whole number of quarter hours.
//!
//! CAN-FAIL:
//!   M15. REPLACE THE MODULO with `return absoluteMinute == 0;` - the shape a "simplification" would
//!        produce, and note that it keeps the FIRST row (midnight) green. Injected into
//!        OVT_SleepSchedule.c and compiled clean (exit 0). The case fails on its second row, "Minute
//!        15 (the first quarter hour) was reported as step=0, expected the opposite". In play this
//!        fault stops the flush from ever noticing a start that sits on a quarter hour, which is one
//!        threat-decay step silently lost per sleep begun on :00, :15, :30 or :45.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_SleepSchedule_IsStepBoundary_ExactMinutesOnly : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The four steps of an hour.
		if (!ExpectStep(0, true, "midnight"))
			return true;

		if (!ExpectStep(15, true, "the first quarter hour"))
			return true;

		if (!ExpectStep(30, true, "the half hour"))
			return true;

		if (!ExpectStep(45, true, "the third quarter"))
			return true;

		// --- Every six-hour boundary is also a step, which is what lets the replay walk one grid and
		// ask about the other.
		if (!ExpectStep(360, true, "06:00"))
			return true;

		if (!ExpectStep(720, true, "12:00"))
			return true;

		// --- And past midnight, where a wrapped skip spends half its steps.
		if (!ExpectStep(1440, true, "00:00 the next day"))
			return true;

		if (!ExpectStep(1455, true, "00:15 the next day"))
			return true;

		// --- One minute either side of a step.
		if (!ExpectStep(14, false, "one minute before the first quarter"))
			return true;

		if (!ExpectStep(16, false, "one minute after the first quarter"))
			return true;

		if (!ExpectStep(1439, false, "one minute before midnight"))
			return true;

		// --- Degenerate arguments are false, never true by accident.
		bool negative = OVT_SleepSchedule.IsStepBoundary(-15, OVT_SleepSchedule.THREAT_STEP_MINUTES);
		if (negative)
		{
			SetFailure("A negative absolute minute was reported as a step boundary");
			return true;
		}

		bool zeroStep = OVT_SleepSchedule.IsStepBoundary(0, 0);
		if (zeroStep)
		{
			SetFailure("A zero step size was reported as a step boundary");
			return true;
		}

		Print("SleepSchedule step predicate: exact at 0/15/30/45 and past midnight at 1440/1455, false one minute either side, and false for a negative minute or a zero step");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the step predicate at one absolute minute.
	//! \param[in] absoluteMinute Minutes from midnight of the starting day.
	//! \param[in] expected Whether it must be reported as a step boundary.
	//! \param[in] label Human description of the minute, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectStep(int absoluteMinute, bool expected, string label)
	{
		bool actual = OVT_SleepSchedule.IsStepBoundary(absoluteMinute, OVT_SleepSchedule.THREAT_STEP_MINUTES);

		if (actual == expected)
			return true;

		SetFailure("Minute %1 (%2) was reported as step=%3, expected the opposite",
			absoluteMinute.ToString(), label, actual.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_SleepSchedule.LandingHour - the hour the player wakes in.
//!
//! Small, and load-bearing twice over: it is the label the action shows, and it is the value all
//! three economy hour latches are asserted to after a replay (D4). A landing hour that failed to
//! wrap would be 30 rather than 6, which no latch comparison could ever match again - the resumed
//! tick would then pay a third time for two crossings on any skip that landed on a boundary.
//!
//! CAN-FAIL:
//!   M8. DROP THE 24-HOUR WRAP, `int landing = startHour + skipHours;`. Compiled clean (exit 0). The
//!       case fails on its first row, "a night sleep wraps past midnight (22 + 8 h) landed on hour
//!       30, expected 6", and on the legal-hour sweep behind it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_SleepSchedule_LandingHour_WrapsAtMidnight : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The plan's three rows.
		if (!ExpectLanding(22, 8, 6, "a night sleep wraps past midnight"))
			return true;

		if (!ExpectLanding(0, 8, 8, "sleeping from midnight"))
			return true;

		if (!ExpectLanding(16, 8, 0, "landing exactly on midnight"))
			return true;

		// --- And the row the whole latch argument rests on: landing exactly on a boundary hour.
		if (!ExpectLanding(10, 8, 18, "landing exactly on the 18:00 boundary"))
			return true;

		if (!ExpectLanding(4, 8, 12, "landing exactly on the 12:00 boundary"))
			return true;

		// --- Every hour of the day wraps into a legal hour.
		for (int hour = 0; hour < OVT_SleepSchedule.HOURS_PER_DAY; hour++)
		{
			int landing = OVT_SleepSchedule.LandingHour(hour, 8);

			if (landing >= 0 && landing < OVT_SleepSchedule.HOURS_PER_DAY)
				continue;

			SetFailure("An eight-hour skip from hour %1 landed on hour %2, which is not a legal hour of day",
				hour.ToString(), landing.ToString());

			return true;
		}

		// --- A full day is a no-op on the hour, which is the identity the wrap has to preserve.
		if (!ExpectLanding(13, 24, 13, "a twenty-four hour skip"))
			return true;

		if (!ExpectLanding(13, 0, 13, "a zero-hour skip"))
			return true;

		Print("SleepSchedule landing hour: 22 + 8 = 6, 16 + 8 = 0, and every hour of the day wraps into a legal hour");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one landing hour.
	//! \param[in] startHour Hour of day the skip starts in.
	//! \param[in] skipHours Length of the skip.
	//! \param[in] expected Hour of day the skip must land in.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectLanding(int startHour, int skipHours, int expected, string label)
	{
		int actual = OVT_SleepSchedule.LandingHour(startHour, skipHours);

		if (actual == expected)
			return true;

		string window = startHour.ToString() + " + " + skipHours.ToString() + " h";

		SetFailure("%1 (%2) landed on hour %3, expected " + expected.ToString(),
			label, window, actual.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The replay walk - CountStepCrossings, StepMinuteAt and IsIntervalBoundary as one machine.
//!
//! OVT_OccupyingFactionManager.HandleTimeSkip walks the window in quarter-hour steps and gains and
//! spends resources on the steps that are also six-hour boundaries. That loop is only correct if
//! three separate functions agree with each other, and NOTHING in the individual cases above would
//! notice if they disagreed. This case asserts the agreement directly, over every start minute of
//! the day:
//!
//!  1. every step the walk visits is a genuine quarter-hour boundary, and lies strictly after the
//!     start and no later than the end - the half-open window, restated as a property of the walk;
//!  2. the steps are strictly increasing, so the replay really is chronological (D6 - the order
//!     matters, because resource gain scales with a threat value the decay is lowering underneath it);
//!  3. THE NUMBER OF BOUNDARY STEPS THE WALK FINDS EQUALS CountIntervalCrossings. This is the one
//!     that ties the occupying faction's payout count to the economy's: if these two ever disagreed,
//!     one system would gain resources a different number of times than the other paid income for
//!     the very same window, and no other assertion anywhere would go red.
//!
//! CAN-FAIL. This case is caught by four of the eight faults, at four different start times, and
//! each was computed on the reference model rather than assumed:
//!   M1 (crossings closed at the start): at 12:00 the walk finds 1 boundary step while
//!      CountIntervalCrossings answers 2 -> claim 3 red.
//!   M5 (step count short by one): at 10:00 the dropped step IS the 18:00 boundary, so the walk
//!      finds 1 against 2 -> claim 3 red. NOTE it is start-dependent: at 11:00 both still answer 2
//!      and this case survives, which is why the sweep runs over all 1440 starts and not one.
//!   M6 (offset inverted): at any :00 start the walk's step 32 lands on absolute minute 1155 against
//!      an end of 1140 -> claim 1b red, "outside the half-open window".
//!   M7 (boundary predicate): the walk finds 0 boundary steps at every start against a crossing
//!      count of 1 or 2 -> claim 3 red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_SleepSchedule_ReplayWalk_StepsAgreeWithCrossings : SCR_AutotestCaseBase
{
	//! The skip the feature actually performs. The walk is asserted at this length because that is
	//! the one the replay runs; the arithmetic itself is length-agnostic and covered above.
	static const int SKIP_HOURS = 8;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		for (int hour = 0; hour < OVT_SleepSchedule.HOURS_PER_DAY; hour++)
		{
			for (int minute = 0; minute < OVT_SleepSchedule.MINUTES_PER_HOUR; minute++)
			{
				if (!WalkOneStart(hour, minute))
					return true;
			}
		}

		Print("SleepSchedule replay walk: over all 1440 start minutes the quarter-hour steps stay on the grid and inside the half-open window, increase strictly, and the boundary steps they contain match CountIntervalCrossings exactly");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Walks one start time's worth of steps and asserts the three claims on it.
	//! \param[in] startHour Hour of day the skip starts in.
	//! \param[in] startMinute Minute the skip starts at.
	//! \return True when the walk was sound; false after recording the failure.
	protected bool WalkOneStart(int startHour, int startMinute)
	{
		int startMinutes = (startHour * OVT_SleepSchedule.MINUTES_PER_HOUR) + startMinute;
		int endMinutes = startMinutes + (SKIP_HOURS * OVT_SleepSchedule.MINUTES_PER_HOUR);

		int steps = OVT_SleepSchedule.CountStepCrossings(startHour, startMinute, SKIP_HOURS, OVT_SleepSchedule.THREAT_STEP_MINUTES);

		int previous = startMinutes;
		int boundarySteps = 0;

		for (int i = 0; i < steps; i++)
		{
			int stepMinute = OVT_SleepSchedule.StepMinuteAt(startHour, startMinute, OVT_SleepSchedule.THREAT_STEP_MINUTES, i);

			string start = startHour.ToString() + ":" + startMinute.ToString();

			// 1a. On the grid.
			if ((stepMinute % OVT_SleepSchedule.THREAT_STEP_MINUTES) != 0)
			{
				SetFailure("Starting at %1, step %2 landed on absolute minute %3, which is not a quarter-hour boundary",
					start, i.ToString(), stepMinute.ToString());
				return false;
			}

			// 1b. Strictly after the start (half-open) and no later than the end (closed).
			if (stepMinute <= startMinutes || stepMinute > endMinutes)
			{
				SetFailure("Starting at %1, step %2 landed on absolute minute %3, outside the half-open window",
					start, i.ToString(), stepMinute.ToString());
				return false;
			}

			// 2. Chronological.
			if (stepMinute <= previous)
			{
				SetFailure("Starting at %1, step %2 landed on absolute minute %3, which did not advance past the step before it",
					start, i.ToString(), stepMinute.ToString());
				return false;
			}

			previous = stepMinute;

			if (OVT_SleepSchedule.IsIntervalBoundary(stepMinute, OVT_SleepSchedule.INCOME_INTERVAL_HOURS))
				boundarySteps++;
		}

		// 3. The two counts agree.
		int crossings = OVT_SleepSchedule.CountIntervalCrossings(startHour, startMinute, SKIP_HOURS, OVT_SleepSchedule.INCOME_INTERVAL_HOURS);

		if (boundarySteps != crossings)
		{
			string startLabel = startHour.ToString() + ":" + startMinute.ToString();

			SetFailure("Starting at %1, the replay walk found %2 boundary steps but CountIntervalCrossings answered %3; the occupying faction would gain resources a different number of times than the economy paid income",
				startLabel, boundarySteps.ToString(), crossings.ToString());

			return false;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! BOTH EDGES OF THE WINDOW AT ONCE - the arithmetic behind the 2026-08-19 review fix.
//!
//! WHAT WENT WRONG, AND WHY NO EXISTING CASE SAW IT. OVT_OccupyingFactionManager's live tick had no
//! latch of any kind on its two payload gates, and its HandleTimeSkip had neither the flush the
//! economy manager opens with nor the latch assertion it closes with. The consequences were both
//! silent: a skip from 10:00 lands exactly on 18:00, so the replay pays for 18:00 and the resumed
//! tick - still inside that same in-game minute - pays for it AGAIN (a third "Gaining Resources"
//! pair, Q1/F7); and a skip from exactly 12:00 excludes 12:00 by the half-open start, so the payday
//! the live tick still owed is simply lost (Q2). Every case above stayed green through both, because
//! each function was individually correct - what was missing was a statement about the pair of edges
//! TOGETHER.
//!
//! THE THREE CLAIMS, swept over all 1440 start minutes and named explicitly at the two starts the
//! plan calls out:
//!
//!  1. THE SIX-HOUR PAYLOAD RUNS ONCE PER BOUNDARY IN THE CLOSED WINDOW [start, start + 8 h]. The
//!     manager runs it once for the start instant when that instant IS a boundary and its latch does
//!     not already hold it (the flush), then once per boundary the half-open window contains. Those
//!     two counts must add up to the closed-closed total - no more, which is Q1, and no less, which
//!     is Q2. The oracle is an INDEPENDENT enumeration: every minute of the window is visited and
//!     tested, rather than a second copy of the subject's formula.
//!  2. LandingHour IS THE HOUR THE RESUMED TICK WILL READ. The latch is asserted to LandingHour, and
//!     it can only suppress the duplicate if it holds the hour of day the clock actually lands in. A
//!     landing hour that failed to wrap would be 24, which no TimeContainer ever reports, so the
//!     latch would never match and the duplicate pair would come straight back.
//!  3. THE SAME STATEMENT FOR THE QUARTER-HOUR DECAY GRID, which is the thirty-third decay step: for
//!     one start minute in fifteen the window's last step falls exactly on the landing instant, and
//!     the same start-and-end pair of defences is what makes the total come to the closed-closed
//!     count instead of one too many at the end or one too few at the front.
//!
//! CAN-FAIL, three faults, injected into OVT_SleepSchedule.c and compiled separately (all exit 0 -
//! not one of them is a syntax error, which is the point):
//!   M1.  CLOSE THE WINDOW AT THE START in CountIntervalCrossings (`if (toFirst == intervalMinutes)
//!        toFirst = 0;`). The flush would then be paying for a boundary the replay also counts, and
//!        the case fails on its FIRST named row: "Starting at 12:0, the start flush plus the replay
//!        would run the six-hour resource payload 3 times, but the closed window ... contains 2
//!        six-hour boundaries". The 10:00 row survives this fault, which is why both are written out.
//!   M8.  DROP THE 24-HOUR WRAP in LandingHour (`int landing = startHour + skipHours;`). Both named
//!        rows survive it (18 and 20 need no wrap), and the sweep fails at the first start that does:
//!        "Starting at 16:0, LandingHour answered 24 but the window ends in hour 0".
//!   M15. REPLACE THE MODULO in IsStepBoundary with `return absoluteMinute == 0;`. The decay flush
//!        then stops recognising a start that sits on a quarter hour, and the case fails on its first
//!        named row: "Starting at 10:0, the start flush plus the replay would run the threat decay 32
//!        times, but the closed window ... contains 33 quarter-hour steps". NOTHING THAT EXISTED
//!        BEFORE 2026-08-19 CATCHES THIS FAULT - it is the coverage this case and the IsStepBoundary
//!        case were added for.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 60)]
class OVT_TEST_Logic_SleepSchedule_WindowEdges_FlushPlusReplayCoversEachBoundaryOnce : SCR_AutotestCaseBase
{
	//! The skip the feature performs. The edges only matter at the length actually used.
	static const int SKIP_HOURS = 8;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The two starts the plan names, stated first so a failure names one of them rather than
		// an arbitrary minute out of the sweep.
		if (!CheckOneStart(10, 0))
			return true;

		if (!CheckOneStart(12, 0))
			return true;

		// --- And every other start minute of the day. The sweep is what turns "these two work" into
		// "the pair of defences is correct", and it is cheap enough to be free.
		for (int hour = 0; hour < OVT_SleepSchedule.HOURS_PER_DAY; hour++)
		{
			for (int minute = 0; minute < OVT_SleepSchedule.MINUTES_PER_HOUR; minute++)
			{
				if (!CheckOneStart(hour, minute))
					return true;
			}
		}

		Print("SleepSchedule window edges: over all 1440 start minutes the start flush plus the replay covers every six-hour boundary and every quarter-hour step of the CLOSED window exactly once, and LandingHour is always the hour the resumed tick reads");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the three edge claims at one start time.
	//! \param[in] startHour Hour of day the skip starts in.
	//! \param[in] startMinute Minute the skip starts at.
	//! \return True when all three held; false after recording the failure.
	protected bool CheckOneStart(int startHour, int startMinute)
	{
		int startMinutes = (startHour * OVT_SleepSchedule.MINUTES_PER_HOUR) + startMinute;
		int endMinutes = startMinutes + (SKIP_HOURS * OVT_SleepSchedule.MINUTES_PER_HOUR);

		string start = startHour.ToString() + ":" + startMinute.ToString();

		// --- 1. The six-hour resource payload.
		int intervalFlush = 0;
		if (OVT_SleepSchedule.IsIntervalBoundary(startMinutes, OVT_SleepSchedule.INCOME_INTERVAL_HOURS))
			intervalFlush = 1;

		int intervalRuns = intervalFlush
			+ OVT_SleepSchedule.CountIntervalCrossings(startHour, startMinute, SKIP_HOURS, OVT_SleepSchedule.INCOME_INTERVAL_HOURS);

		int intervalOwed = CountBoundariesInClosedWindow(startMinutes, endMinutes,
			OVT_SleepSchedule.INCOME_INTERVAL_HOURS * OVT_SleepSchedule.MINUTES_PER_HOUR);

		if (intervalRuns != intervalOwed)
		{
			SetFailure("Starting at %1, the start flush plus the replay would run the six-hour resource payload %2 times, but the closed window [start, start + 8 h] contains %3 six-hour boundaries",
				start, intervalRuns.ToString(), intervalOwed.ToString());

			return false;
		}

		// --- 2. The landing hour the gain latch is asserted to.
		int landingHour = OVT_SleepSchedule.LandingHour(startHour, SKIP_HOURS);

		int endHourOfDay = (int)Math.Floor((float)endMinutes / OVT_SleepSchedule.MINUTES_PER_HOUR);
		endHourOfDay = endHourOfDay % OVT_SleepSchedule.HOURS_PER_DAY;

		if (landingHour != endHourOfDay)
		{
			SetFailure("Starting at %1, LandingHour answered %2 but the window ends in hour %3; the gain latch would be asserted to an hour the resumed tick never reads, and the boundary the replay just paid for would be paid a second time",
				start, landingHour.ToString(), endHourOfDay.ToString());

			return false;
		}

		// --- 3. The quarter-hour threat decay.
		int stepFlush = 0;
		if (OVT_SleepSchedule.IsStepBoundary(startMinutes, OVT_SleepSchedule.THREAT_STEP_MINUTES))
			stepFlush = 1;

		int stepRuns = stepFlush
			+ OVT_SleepSchedule.CountStepCrossings(startHour, startMinute, SKIP_HOURS, OVT_SleepSchedule.THREAT_STEP_MINUTES);

		int stepsOwed = CountBoundariesInClosedWindow(startMinutes, endMinutes, OVT_SleepSchedule.THREAT_STEP_MINUTES);

		if (stepRuns != stepsOwed)
		{
			SetFailure("Starting at %1, the start flush plus the replay would run the threat decay %2 times, but the closed window [start, start + 8 h] contains %3 quarter-hour steps",
				start, stepRuns.ToString(), stepsOwed.ToString());

			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The independent oracle: how many multiples of a period lie in the CLOSED window [first, last].
	//!
	//! Deliberately an ENUMERATION rather than a formula. Every Count* function on the subject is a
	//! closed-form expression, and checking one closed form against another written the same way is
	//! how two implementations agree on the same mistake. This visits every minute of the window and
	//! asks, which is slow, obviously right, and the reason this case carries a longer timeout.
	//! \param[in] firstMinute First minute of the window, inclusive.
	//! \param[in] lastMinute Last minute of the window, inclusive.
	//! \param[in] periodMinutes Spacing of the boundaries being counted.
	//! \return How many boundaries fall in the closed window.
	protected int CountBoundariesInClosedWindow(int firstMinute, int lastMinute, int periodMinutes)
	{
		if (periodMinutes <= 0)
			return 0;

		int found = 0;

		for (int minute = firstMinute; minute <= lastMinute; minute++)
		{
			if ((minute % periodMinutes) == 0)
				found++;
		}

		return found;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_SleepSchedule.AbsoluteGameHours - the monotonic clock stamp the cooldown is compared with.
//!
//! Three claims, and the third is the one that made this function look different from the plan:
//!
//!  1. STRICTLY INCREASING through an hour, through a day and through a YEAR boundary. Everything
//!     downstream is a subtraction of two of these, so a stamp that ever went backwards would read
//!     as "the clock moved backwards" and - by D17's fail-open - hand the player a free sleep.
//!  2. A HALF HOUR IS 0.5. The minute term is real, not decorative: without it the cooldown would
//!     only ever move in whole hours.
//!  3. IT CAN STILL SEE TWELVE MINUTES AT A DEFAULT CAMPAIGN DATE. This is the claim the EPOCH_YEAR
//!     departure from implementation.md 3.4 exists for. An EnforceScript float is IEEE binary32; the
//!     engine's default campaign year is 1989, so the plan's literal (year * 366 + day) * 24 lands
//!     at ~1.75e7 where the float increment is TWO HOURS and every sub-two-hour difference collapses
//!     to exactly 0.0 - a countdown that never moves. Measuring from the epoch puts the same instant
//!     at ~4.8e3. Nothing else in the tree would notice the difference; this row is it.
//!
//! WHY THIS CASE HAS ITS OWN TOLERANCE. At ~4.8e3 the binary32 increment is 2^-11, about
//! 0.00049 h - larger than the tier's EPSILON. STAMP_TOLERANCE is 0.01 h (36 in-game seconds):
//! comfortably above the representation limit, and three orders of magnitude below the twelve-hour
//! interval the cooldown actually cares about. The exact 0.5 row is asserted at EPSILON, because a
//! half hour IS exactly representable at these magnitudes.
//!
//! CAN-FAIL, two faults, injected and compiled separately (both exit 0):
//!   M12. DROP THE EPOCH - `int daysSinceEpoch = (year * DAYS_PER_YEAR) + dayInYear;`, i.e. restore
//!        the plan's literal formula. The twelve-minute row then measures the difference as
//!        0.000000 (computed on a binary32 model of the subject, not assumed) and the case fails on
//!        "twelve minutes apart at a default campaign date measured 0.000000 h apart, expected
//!        0.200000 +/- 0.010000 - at this magnitude a binary32 stamp cannot resolve it". The
//!        monotonicity rows all SURVIVE this fault, which is exactly why the resolution row exists.
//!   M13. DROP THE MINUTE TERM, `return (daysSinceEpoch * HOURS_PER_DAY) + hour;`. The half-hour row
//!        then measures 0.000000 and the case fails on "00:30 must be half an hour after 00:00 ...".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_SleepSchedule_AbsoluteHours_MonotonicAndFineGrained : SCR_AutotestCaseBase
{
	//! A date a default campaign really runs at - the year the engine's own time handler defaults to.
	static const int CAMPAIGN_YEAR = 1989;

	//! An arbitrary mid-year day, so no row accidentally sits on a boundary of its own.
	static const int CAMPAIGN_DAY = 200;

	//! See the header: the binary32 increment at campaign magnitudes is ~0.00049 h, so differences
	//! are compared at 0.01 h (36 in-game seconds) rather than at the tier's EPSILON.
	static const float STAMP_TOLERANCE = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- 1. Strictly increasing across an hour.
		if (!ExpectIncreasing(
			OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY, 5, 0),
			OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY, 6, 0),
			"05:00 then 06:00 on the same day"))
			return true;

		if (!ExpectIncreasing(
			OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY, 5, 0),
			OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY, 5, 30),
			"05:00 then 05:30 on the same day"))
			return true;

		// --- 1. Strictly increasing across a day. 23:00 to 00:00 the next morning is the shape a
		// night sleep takes, and the one that would go backwards if the date failed to advance.
		if (!ExpectIncreasing(
			OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY, 23, 0),
			OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY + 1, 0, 0),
			"23:00 then midnight the next day"))
			return true;

		// --- 1. And across a year boundary, where the day number itself restarts.
		if (!ExpectIncreasing(
			OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, 365, 23, 0),
			OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR + 1, 1, 0, 0),
			"new year's eve then new year's day"))
			return true;

		// --- 2. Half an hour is half an hour.
		float midnight = OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY, 0, 0);
		float halfPast = OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY, 0, 30);

		if (!OVT_TEST_LogicFixture.FloatEquals(halfPast - midnight, 0.5))
		{
			SetFailure("00:30 must be half an hour after 00:00, but the two stamps are %1 h apart",
				(halfPast - midnight).ToString());
			return true;
		}

		// --- 2. An hour is an hour, stated the same way.
		float oneHour = OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY, 1, 0);
		if (!OVT_TEST_LogicFixture.FloatEquals(oneHour - midnight, 1.0))
		{
			SetFailure("01:00 must be one hour after 00:00, but the two stamps are %1 h apart",
				(oneHour - midnight).ToString());
			return true;
		}

		// --- 2. And a whole day is twenty-four hours, which is what makes the day term the right size.
		float nextDay = OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY + 1, 0, 0);
		if (Math.AbsFloat((nextDay - midnight) - OVT_SleepSchedule.HOURS_PER_DAY) > STAMP_TOLERANCE)
		{
			SetFailure("The same clock time one day later must be %1 h later, but the two stamps are %2 h apart",
				OVT_SleepSchedule.HOURS_PER_DAY.ToString(), (nextDay - midnight).ToString());
			return true;
		}

		// --- 3. THE RESOLUTION ROW. Twelve minutes, at a date a real campaign runs at.
		float noon = OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY, 12, 0);
		float twelvePast = OVT_SleepSchedule.AbsoluteGameHours(CAMPAIGN_YEAR, CAMPAIGN_DAY, 12, 12);
		float gap = twelvePast - noon;

		if (Math.AbsFloat(gap - 0.2) > STAMP_TOLERANCE)
		{
			SetFailure("Twelve minutes apart at a default campaign date measured %1 h apart, expected 0.200000 +/- %2 - at this magnitude a binary32 stamp cannot resolve it, so the sleep cooldown would never count down",
				gap.ToString(), STAMP_TOLERANCE.ToString());
			return true;
		}

		Print("SleepSchedule absolute hours: strictly increasing across an hour, a day and a year, exactly 0.5 h for a half hour, and still able to resolve twelve minutes at a 1989 campaign date");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that the later of two stamps really is larger.
	//! \param[in] earlier Stamp of the earlier instant.
	//! \param[in] later Stamp of the later instant.
	//! \param[in] label Human description of the pair, used only in the failure message.
	//! \return True when it increased; false after recording the failure.
	protected bool ExpectIncreasing(float earlier, float later, string label)
	{
		if (later > earlier)
			return true;

		SetFailure("%1: the later stamp (%2) is not greater than the earlier one (%3) - a stamp that does not increase makes the cooldown comparison meaningless",
			label, later.ToString(), earlier.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_SleepSchedule.CooldownRemainingHours - how much of the twelve-hour cooldown is left.
//!
//! THE ARITHMETIC IS TRIVIAL AND THE EDGE CASES ARE NOT. Four of the six rows below are about what
//! happens when the inputs are not a well-behaved pair of stamps:
//!
//!  - THE SENTINEL. A player who has never slept carries -1, and -1 must read as READY rather than
//!    as a stamp thirteen hours in the past that happens to also read as ready. It is banded rather
//!    than compared exactly because the value has been through a serializer.
//!  - THE FAIL-OPEN ON NEGATIVE ELAPSED (D17). A stamp in the FUTURE - a clock that moved backwards,
//!    a date that failed to advance across midnight, a save from a different calendar - must return
//!    ready. Failing closed there would lock the Sleep action PERMANENTLY on that campaign, because
//!    a future stamp can never be reached; failing open costs at most one extra sleep on a save
//!    whose clock has already misbehaved. This is the row with the most severe consequence in the
//!    whole file and it is the row a "tidier" implementation would delete.
//!  - THE CLAMP. Long past the cooldown the answer is 0, never a negative number, because the caller
//!    formats it for a label.
//!
//! And the ordinary rows, which are the feature's own numbers. THESE CHANGED ON 2026-08-19, by user
//! decision, and the change is the whole point of half the rows below. The stamp used to be written
//! at the moment the player lay DOWN, so the eight-hour skip ate eight of the twelve and they woke
//! with four; it is now written at the instant they WAKE (OVT_SleepService.StampCooldown adds
//! SKIP_HOURS to the pre-skip clock read), so the full twelve are ahead of them when the screen comes
//! back and the door-to-door interval is TWENTY in-game hours.
//!
//! WHAT THIS CASE CAN AND CANNOT PIN. The stamp CONVENTION is a decision made in the world-side
//! service, which this tier cannot touch; what is pinned here is the arithmetic that convention
//! produces - given a stamp at the wake instant, twelve hours remain at waking, eight remain twelve
//! hours after lying down, and nothing is left twenty hours after lying down. The middle row is the
//! one that would have been GREEN under the old convention and is red under it now, so it is the row
//! that actually records the decision.
//!
//! CAN-FAIL, two faults, injected into OVT_SleepSchedule.c and compiled separately (both exit 0):
//!   M11. DELETE THE FAIL-OPEN BRANCH (`if (elapsed < 0) return 0;`). The negative-elapsed row then
//!        answers 16.000000 - twelve hours plus the four the stamp is in the future - and the case
//!        fails on "a stamp four hours in the FUTURE must fail open ... answered 16.000000 h
//!        remaining, expected 0.000000". Every other row survives it, which is why the row is written
//!        down rather than assumed to follow from the others.
//!   M14. RE-CREATE THE OLD FOUR-HOUR BEHAVIOUR INSIDE THE ARITHMETIC - `float remaining =
//!        (cooldownHours - 8) - elapsed;`, which is numerically what a stamp taken at lie-down time
//!        produces. Compiled clean (exit 0), because the rejected behaviour was never a syntax error
//!        and nothing else in the tree would have gone red for it. The case then fails on its FIRST
//!        row, "the instant the sleeper wakes: the stamp IS that instant, so the whole cooldown is
//!        ahead (now 108.000000, stamp 108.000000) answered 4.000000 h remaining, expected 12.000000"
//!        - the literal four hours the user rejected - with the twelve-hours-after-lying-down and
//!        nineteen-hours rows red behind it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_SleepSchedule_CooldownRemaining_ClampsAndFailsOpen : SCR_AutotestCaseBase
{
	//! The feature's cooldown, in in-game hours (OVT_SleepService.COOLDOWN_HOURS). Restated as a
	//! literal rather than read off the service, which is a world-side class this tier cannot touch.
	static const float COOLDOWN_HOURS = 12;

	//! The feature's skip length (OVT_SleepService.SKIP_HOURS), restated for the same reason.
	static const float SKIP_HOURS = 8;

	//! An arbitrary instant at which the player lies down, small enough that every row's arithmetic is
	//! exact in binary32. Nothing is stamped here any more - it is the reference the door-to-door rows
	//! are measured from.
	static const float LAY_DOWN = 100.0;

	//! The instant the player wakes, LAY_DOWN + SKIP_HOURS, and therefore the value the service now
	//! writes into the record. Written out rather than derived from the two constants above, following
	//! OVT_SleepSchedule's own rule about consts initialised from other consts.
	static const float WAKE = 108.0;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- THE ROW THE 2026-08-19 DECISION IS ABOUT. The stamp is the wake instant, so at the wake
		// instant nothing has elapsed and the whole twelve hours are still ahead. Under the old
		// lie-down-relative stamp this row read four.
		if (!ExpectRemaining(WAKE, WAKE, COOLDOWN_HOURS, "the instant the sleeper wakes: the stamp IS that instant, so the whole cooldown is ahead"))
			return true;

		if (!ExpectRemaining(WAKE + 4, WAKE, COOLDOWN_HOURS - 4, "four hours after waking"))
			return true;

		// --- THE SAME DECISION, STATED FROM THE OTHER END. Twelve hours after LYING DOWN - which is
		// four hours after waking - the player is still on cooldown. This is the row that would have
		// been ready under the old convention.
		if (!ExpectRemaining(LAY_DOWN + 12, WAKE, 8, "twelve hours after lying down is NOT ready: the twelve run from waking, not from the bed"))
			return true;

		// --- Door to door: eight hours asleep plus twelve awake is twenty, and not an hour less.
		if (!ExpectRemaining(LAY_DOWN + 19, WAKE, 1, "nineteen hours after lying down: one hour still to go"))
			return true;

		if (!ExpectRemaining(LAY_DOWN + 20, WAKE, 0, "twenty hours after lying down: door to door, the cooldown is exactly spent"))
			return true;

		// --- Long past it: ready, and never negative.
		if (!ExpectRemaining(WAKE + 100, WAKE, 0, "a hundred hours later - clamped, not negative"))
			return true;

		// --- The never-slept sentinel.
		if (!ExpectRemaining(WAKE, OVT_SleepSchedule.NEVER_SLEPT, 0, "a player who has never slept"))
			return true;

		// --- THE FAIL-OPEN (D17): a stamp in the future must not lock the action forever.
		if (!ExpectRemaining(WAKE, WAKE + 4, 0, "a stamp four hours in the FUTURE must fail open"))
			return true;

		// --- And an intermediate value, so the function is pinned as a subtraction and not as a
		// three-way switch that happens to satisfy the rows above.
		if (!ExpectRemaining(WAKE + 3.5, WAKE, COOLDOWN_HOURS - 3.5, "three and a half hours after waking"))
			return true;

		Print("SleepSchedule cooldown: the stamp is the WAKE instant, so 12 h remain on waking, 8 h twelve hours after lying down, 0 at twenty hours door to door, 0 for the never-slept sentinel, and 0 - never negative - for a stamp in the future");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one remaining-cooldown answer.
	//! \param[in] nowAbsoluteHours The current stamp.
	//! \param[in] lastAbsoluteHours The stamp on the player's record - since D16a, the instant they last
	//! WOKE from sleeping.
	//! \param[in] expected Hours the function must report as remaining.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectRemaining(float nowAbsoluteHours, float lastAbsoluteHours, float expected, string label)
	{
		float actual = OVT_SleepSchedule.CooldownRemainingHours(nowAbsoluteHours, lastAbsoluteHours, COOLDOWN_HOURS);

		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		// Three format parameters, not four: the formatter takes no more than that.
		string stamps = "now " + nowAbsoluteHours.ToString() + ", stamp " + lastAbsoluteHours.ToString();

		SetFailure("%1 (%2) answered %3 h remaining, expected " + expected.ToString(),
			label, stamps, actual.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_SleepSchedule.FormatRemaining - the countdown in the action's label.
//!
//! COSMETIC, AND STILL WORTH PINNING, because the obvious implementation is wrong in two different
//! directions and binary32 is the reason for both. The subject rounds to WHOLE MINUTES FIRST and
//! splits afterwards, which is the only order that gets all three of the plan's rows right:
//!
//!  - 4.2 h -> "4h 12m". Flooring the total gives "4h 11m", because 4.2 as a binary32 is
//!    4.19999980926513671875 and times sixty lands just UNDER 252.
//!  - 0.5 h -> "0h 30m". Every order gets this one right; it is here as the control.
//!  - 0.999 h -> "1h 0m", NOT "0h 60m". Splitting first and rounding the fraction produces a sixty
//!    on the clock, which is the specific nonsense the plan calls out.
//!
//! THE EXPECTED STRINGS ARE THE ONES THE SHIPPED CODE PRODUCES, computed on a binary32 model of the
//! subject rather than by mental arithmetic - "1h 0m" for 0.999 in particular is a consequence of
//! the rounding order and not an assumption about it.
//!
//! CAN-FAIL, two faults, injected and compiled separately (both exit 0), one per failure direction:
//!   M9.  SPLIT FIRST, ROUND THE FRACTION - `wholeHours = Floor(hours); minutes = Round((hours -
//!        wholeHours) * 60)`. The 0.999 row then produces "0h 60m" and the case fails on
//!        "FormatRemaining(0.999000) produced '0h 60m', expected '1h 0m'". The 4.2 row SURVIVES this
//!        fault (it still produces "4h 12m"), which is why both rows are written down.
//!   M10. FLOOR THE TOTAL instead of rounding it - `(int)Math.Floor(hours * MINUTES_PER_HOUR)`. The
//!        4.2 row then produces "4h 11m" and the case fails on "FormatRemaining(4.200000) produced
//!        '4h 11m', expected '4h 12m'"; the 0.999 row would report "0h 59m" behind it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_SleepSchedule_FormatRemaining_RoundsTotalMinutesFirst : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The plan's three rows.
		if (!ExpectFormat(4.2, "4h 12m"))
			return true;

		if (!ExpectFormat(0.5, "0h 30m"))
			return true;

		if (!ExpectFormat(0.999, "1h 0m"))
			return true;

		// --- The label the player sees the instant they wake, and the one at the far end.
		if (!ExpectFormat(4.0, "4h 0m"))
			return true;

		if (!ExpectFormat(12.0, "12h 0m"))
			return true;

		if (!ExpectFormat(11.75, "11h 45m"))
			return true;

		// --- Ready, and past ready: both are "0h 0m" rather than a negative clock.
		if (!ExpectFormat(0, "0h 0m"))
			return true;

		if (!ExpectFormat(-1.5, "0h 0m"))
			return true;

		Print("SleepSchedule formatting: 4.2 h reads '4h 12m' (not '4h 11m'), 0.999 h reads '1h 0m' (not '0h 60m'), and a non-positive remainder reads '0h 0m'");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one formatted duration.
	//! \param[in] hours The duration to format.
	//! \param[in] expected The exact string the label must carry.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectFormat(float hours, string expected)
	{
		string actual = OVT_SleepSchedule.FormatRemaining(hours);

		if (actual == expected)
			return true;

		SetFailure("FormatRemaining(%1) produced '%2', expected '" + expected + "'",
			hours.ToString(), actual);

		return false;
	}
}
