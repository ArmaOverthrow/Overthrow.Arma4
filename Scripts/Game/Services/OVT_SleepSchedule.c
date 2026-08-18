//------------------------------------------------------------------------------------------------
//! Boundary-counting and cooldown arithmetic for the sleep time skip.
//!
//! ===========================================================================================
//! THE RULE FOR THIS FILE, NON-NEGOTIABLE:
//!   NO MANAGER, NO STATIC ACCESSOR, NO ENTITY, NO ENGINE STATE, NO SIMULATION STATE. EVER.
//! ===========================================================================================
//!
//! Everything here is a pure function of its arguments. That is not tidiness - it is the only
//! reason the Logic tier (Scripts/Game/Tests/TestSuites/Logic/) can pin it, and pinning it is the
//! whole point: "how many income payouts does an eight-hour skip starting at 11:47 contain" is a
//! question whose wrong answer is a money printer or a lost payday, and both are invisible in play.
//! Inline arithmetic in a manager body cannot be asserted by anything.
//!
//! THE WINDOW CONTRACT (implementation.md 3.3), which every Count* function below implements:
//!   the skip starts at the CURRENT in-game time and covers (start, start + hours] - HALF-OPEN at
//!   the start, CLOSED at the end. Anything owed exactly AT the start belongs to the live tick that
//!   is about to be flushed, not to the replay; anything owed exactly at the landing instant belongs
//!   to the replay, because the live tick will not observe it (the latch is asserted to the landing
//!   hour immediately afterwards).
//!
//! INTEGER DIVISION. EnforceScript's division is context-dependent - it truncates in a pure-int
//! expression, but "pure-int expression" is not something a reader can see at a glance. Every floor
//! in this file is therefore written as (int)Math.Floor((float)a / b), with no exceptions, and no
//! function below relies on / doing anything at all.
//!
//! FLOOR ON A NEGATIVE IS NEVER TAKEN. Every Count* function shifts into a frame where the numerator
//! is >= 0 (the offset to the FIRST occurrence strictly after the start, then whole periods from
//! there), so the answers do not depend on whether the engine's Math.Floor is a true floor or a
//! truncation for negative inputs.
//------------------------------------------------------------------------------------------------
class OVT_SleepSchedule
{
	//! Income, NPC shop buying and occupying-faction resource gain happen on this hour grid
	//! (00:00 / 06:00 / 12:00 / 18:00). Must divide HOURS_PER_DAY, or the grid would not line up
	//! across midnight.
	static const int INCOME_INTERVAL_HOURS = 6;

	//! Hour-of-day at which shops restock.
	static const int RESTOCK_HOUR = 7;

	//! Hour-of-day at which rent is charged and paid.
	static const int RENT_HOUR = 0;

	//! Resolution of the occupying faction's threat decay, and therefore of the chronological replay.
	static const int THREAT_STEP_MINUTES = 15;

	static const int HOURS_PER_DAY = 24;
	static const int MINUTES_PER_HOUR = 60;

	//! HOURS_PER_DAY * MINUTES_PER_HOUR, written out rather than derived: a const initialised from
	//! other consts is not worth the risk of a compiler that evaluates them in file order.
	static const int MINUTES_PER_DAY = 1440;

	//! Calendar year the absolute-hours stamp is measured from. See AbsoluteGameHours for why this
	//! exists at all; 1989 is the engine's own default campaign year
	//! (ArmaReforger/scripts/Game/Components/Environment/SCR_TimeAndWeatherHandlerComponent.c:79,
	//! defvalue "1989", slider range 1899-2050).
	static const int EPOCH_YEAR = 1989;

	//! Upper bound on days in a year, so the stamp is strictly increasing across a year boundary.
	//! Deliberately not a real calendar: this number only has to be monotonic, never accurate.
	static const int DAYS_PER_YEAR = 366;

	//! "This player has never slept" stamp.
	static const float NEVER_SLEPT = -1;

	//! Half-width of the band around NEVER_SLEPT that still counts as the sentinel. A float that has
	//! made a round trip through a serializer is not guaranteed to compare exactly, and the band is
	//! far narrower than any interval the cooldown cares about.
	static const float NEVER_SLEPT_BAND = 0.5;

	//------------------------------------------------------------------------------------------------
	// WINDOW COUNTING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! How many interval boundaries (hh:00 where hh % intervalHours == 0) fall inside the skipped
	//! window (start, start + skipHours].
	//!
	//! This is the number of times income + NPC shop buying must be replayed, and - fed the same
	//! arguments - the number of times the occupying faction gains and spends resources.
	//!
	//! Counted as "offset to the first boundary strictly after the start, then one per whole interval
	//! remaining", which is why an eight-hour skip from exactly 12:00 answers 1 (18:00) and not 2:
	//! the boundary AT the start is the live tick's, not ours.
	//! \param[in] startHour In-game hour of day at the start of the skip, 0-23.
	//! \param[in] startMinute In-game minute at the start of the skip, 0-59.
	//! \param[in] skipHours Length of the skip in whole in-game hours.
	//! \param[in] intervalHours Boundary spacing in hours. Must divide 24 to be meaningful across midnight.
	//! \return Number of boundaries in the window. 0 for a non-positive skip or interval.
	static int CountIntervalCrossings(int startHour, int startMinute, int skipHours, int intervalHours)
	{
		if (skipHours <= 0 || intervalHours <= 0)
			return 0;

		int intervalMinutes = intervalHours * MINUTES_PER_HOUR;
		int startMinutes = (startHour * MINUTES_PER_HOUR) + startMinute;
		int skipMinutes = skipHours * MINUTES_PER_HOUR;

		if (startMinutes < 0)
			return 0;

		// Minutes from the start to the NEXT boundary. Lands in (0, intervalMinutes], so a start that
		// sits exactly on a boundary gets a full interval - the half-open start, in one line.
		int toFirst = intervalMinutes - (startMinutes % intervalMinutes);

		if (toFirst > skipMinutes)
			return 0;

		return 1 + (int)Math.Floor((float)(skipMinutes - toFirst) / intervalMinutes);
	}

	//------------------------------------------------------------------------------------------------
	//! How many times a specific hour-of-day is ENTERED inside the window (start, start + skipHours].
	//!
	//! Drives the restock (hour 7) and rent (hour 0) replays. Counting entries rather than answering
	//! a yes/no keeps a skip longer than a day correct for free.
	//! \param[in] startHour In-game hour of day at the start of the skip, 0-23.
	//! \param[in] startMinute In-game minute at the start of the skip, 0-59.
	//! \param[in] skipHours Length of the skip in whole in-game hours.
	//! \param[in] targetHour The hour-of-day whose hh:00 instant is being counted, 0-23.
	//! \return How many times targetHour:00 occurs in the window. 0 for a non-positive skip.
	static int CountHourEntries(int startHour, int startMinute, int skipHours, int targetHour)
	{
		if (skipHours <= 0)
			return 0;

		if (targetHour < 0 || targetHour >= HOURS_PER_DAY)
			return 0;

		int startMinutes = (startHour * MINUTES_PER_HOUR) + startMinute;
		int skipMinutes = skipHours * MINUTES_PER_HOUR;

		if (startMinutes < 0)
			return 0;

		int targetMinutes = targetHour * MINUTES_PER_HOUR;
		int startMinuteOfDay = startMinutes % MINUTES_PER_DAY;

		// Minutes from the start to the next occurrence of targetHour:00. A start sitting exactly on
		// the target hour is pushed a whole day forward, which is the half-open start again.
		int toFirst = targetMinutes - startMinuteOfDay;
		if (toFirst <= 0)
			toFirst += MINUTES_PER_DAY;

		if (toFirst > skipMinutes)
			return 0;

		return 1 + (int)Math.Floor((float)(skipMinutes - toFirst) / MINUTES_PER_DAY);
	}

	//------------------------------------------------------------------------------------------------
	//! How many step boundaries (every stepMinutes past midnight) are crossed inside the window
	//! (start, start + skipHours].
	//!
	//! Drives the occupying faction's threat decay, which the live tick runs at :00/:15/:30/:45. For a
	//! whole-hour skip the answer is INDEPENDENT of the start minute - 32 for eight hours from any
	//! minute whatsoever - and that independence is precisely the property that breaks if the floors
	//! are written with raw int division or the "+ 1" for the first step is dropped.
	//! \param[in] startHour In-game hour of day at the start of the skip, 0-23.
	//! \param[in] startMinute In-game minute at the start of the skip, 0-59.
	//! \param[in] skipHours Length of the skip in whole in-game hours.
	//! \param[in] stepMinutes Step spacing in minutes. Must divide 60 to be meaningful.
	//! \return Number of steps in the window. 0 for a non-positive skip or step.
	static int CountStepCrossings(int startHour, int startMinute, int skipHours, int stepMinutes)
	{
		if (skipHours <= 0 || stepMinutes <= 0)
			return 0;

		int startMinutes = (startHour * MINUTES_PER_HOUR) + startMinute;
		int skipMinutes = skipHours * MINUTES_PER_HOUR;

		if (startMinutes < 0)
			return 0;

		int toFirst = stepMinutes - (startMinutes % stepMinutes);

		if (toFirst > skipMinutes)
			return 0;

		return 1 + (int)Math.Floor((float)(skipMinutes - toFirst) / stepMinutes);
	}

	//------------------------------------------------------------------------------------------------
	//! The absolute minute (measured from midnight of the day the skip started) of the index-th step
	//! boundary inside the window.
	//!
	//! Exists so the chronological replay of implementation.md 3.3 can walk its own steps without
	//! writing "% 15" in a manager body - the arithmetic that decides WHEN something happens lives
	//! here, where it can be asserted, and the manager only decides WHAT happens.
	//!
	//! The returned minute may exceed 1440 when the window crosses midnight. That is deliberate and
	//! is exactly what IsIntervalBoundary expects: a day is a whole number of intervals, so the
	//! modulo keeps working past midnight.
	//! \param[in] startHour In-game hour of day at the start of the skip, 0-23.
	//! \param[in] startMinute In-game minute at the start of the skip, 0-59.
	//! \param[in] stepMinutes Step spacing in minutes.
	//! \param[in] index 0-based step number. Only meaningful below CountStepCrossings' answer.
	//! \return Absolute minute of that step, or -1 when the arguments are nonsensical.
	static int StepMinuteAt(int startHour, int startMinute, int stepMinutes, int index)
	{
		if (stepMinutes <= 0 || index < 0)
			return -1;

		int startMinutes = (startHour * MINUTES_PER_HOUR) + startMinute;
		if (startMinutes < 0)
			return -1;

		int toFirst = stepMinutes - (startMinutes % stepMinutes);

		return startMinutes + toFirst + (index * stepMinutes);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an absolute minute is also an interval boundary.
	//!
	//! The predicate the chronological replay uses to decide "this step is also a resource-gain
	//! boundary". Takes an absolute minute rather than an hour so it keeps working past midnight.
	//! \param[in] absoluteMinute Minutes from midnight of some reference day; may exceed 1440.
	//! \param[in] intervalHours Boundary spacing in hours.
	//! \return True when the minute falls exactly on a boundary.
	static bool IsIntervalBoundary(int absoluteMinute, int intervalHours)
	{
		if (intervalHours <= 0 || absoluteMinute < 0)
			return false;

		return (absoluteMinute % (intervalHours * MINUTES_PER_HOUR)) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an absolute minute is also a STEP boundary - the quarter-hour grid the occupying
	//! faction's threat decay runs on.
	//!
	//! The same shape as IsIntervalBoundary, in minutes rather than hours, and it exists for the same
	//! reason StepMinuteAt does: the alternative was writing "% 15" inside a CheckUpdate seam, and
	//! implementation.md 7 forbids timing arithmetic that nothing can assert living in a manager body.
	//!
	//! ITS ONE CALLER IS THE START-BOUNDARY FLUSH (2026-08-19 review fix). The skipped window is
	//! half-open at the start, so a decay step owed EXACTLY at the instant the player lay down belongs
	//! to the live tick - and if that tick had not observed it yet, it is lost. The flush asks this
	//! predicate whether the start instant is such a step.
	//! \param[in] absoluteMinute Minutes from midnight of some reference day; may exceed 1440.
	//! \param[in] stepMinutes Step spacing in minutes.
	//! \return True when the minute falls exactly on a step.
	static bool IsStepBoundary(int absoluteMinute, int stepMinutes)
	{
		if (stepMinutes <= 0 || absoluteMinute < 0)
			return false;

		return (absoluteMinute % stepMinutes) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The hour-of-day the skip lands in.
	//!
	//! This is the value the economy manager's three hour latches are asserted to after a replay
	//! (implementation.md D4): it suppresses exactly one re-fire - the one for the hour just landed
	//! in - and permits every later boundary, because the live branches compare each latch against a
	//! FIXED hour.
	//! \param[in] startHour In-game hour of day at the start of the skip, 0-23.
	//! \param[in] skipHours Length of the skip in whole in-game hours.
	//! \return The landing hour, 0-23.
	static int LandingHour(int startHour, int skipHours)
	{
		int landing = (startHour + skipHours) % HOURS_PER_DAY;

		// EnforceScript's % on a negative left operand is not worth trusting; skipHours is never
		// negative in practice, and this makes sure the answer is a legal hour if it ever is.
		if (landing < 0)
			landing += HOURS_PER_DAY;

		return landing;
	}

	//------------------------------------------------------------------------------------------------
	// COOLDOWN
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! One monotonic number for the whole in-game calendar, in hours, for cooldown comparison.
	//!
	//! WHY THE EPOCH. implementation.md 3.4 states the stamp as (year * 366 + dayInYear) * 24 + ...
	//! That formula is correct as maths and unusable as a float: the engine's default campaign year
	//! is 1989, so the term is ~1.75e7, and IEEE binary32 - which is what an EnforceScript float is -
	//! has an increment of 2 up there. Both the minute term AND the hour term vanish, and every
	//! difference of two stamps snaps to a multiple of two hours. Measuring from EPOCH_YEAR instead
	//! puts a default campaign at ~4.8e3, where the increment is under two hundredths of a second.
	//! Verified numerically before this was written: at the raw magnitude a twelve-minute difference
	//! comes out as exactly 0.0.
	//!
	//! Nothing else about the contract changes - the value is still one number, still strictly
	//! increasing through an hour, a day and a year boundary, and still compared only by subtraction.
	//! Years before EPOCH_YEAR simply produce negative stamps, which subtract just as well.
	//! \param[in] year Calendar year.
	//! \param[in] dayInYear Day within that year.
	//! \param[in] hour Hour of day, 0-23.
	//! \param[in] minute Minute of hour, 0-59.
	//! \return Absolute in-game hours since midnight on day 0 of EPOCH_YEAR.
	static float AbsoluteGameHours(int year, int dayInYear, int hour, int minute)
	{
		int daysSinceEpoch = ((year - EPOCH_YEAR) * DAYS_PER_YEAR) + dayInYear;

		return (daysSinceEpoch * HOURS_PER_DAY) + hour + ((float)minute / MINUTES_PER_HOUR);
	}

	//------------------------------------------------------------------------------------------------
	//! How much of the sleep cooldown is left, in in-game hours.
	//!
	//! Comparing two game-clock stamps rather than counting down real seconds is what makes the
	//! cooldown correct at both time accelerations and durable across a save for free.
	//!
	//! FAILS OPEN, DELIBERATELY (implementation.md D17). A negative elapsed time means the clock went
	//! backwards - a date that failed to advance, a restored save from a different calendar - and
	//! failing closed there would lock the action PERMANENTLY on that campaign, because a stamp in
	//! the future can never be reached. Failing open costs at most one extra sleep.
	//!
	//! The never-slept sentinel needs no arithmetic of its own: it is banded out explicitly here, and
	//! even without that band it would read as "an extremely long time ago", which is also ready.
	//! \param[in] nowAbsoluteHours Current stamp, from AbsoluteGameHours.
	//! \param[in] lastAbsoluteHours Stamp taken when the player last slept, or NEVER_SLEPT.
	//! \param[in] cooldownHours Length of the cooldown in in-game hours.
	//! \return Hours remaining, clamped at 0. 0 means ready.
	static float CooldownRemainingHours(float nowAbsoluteHours, float lastAbsoluteHours, float cooldownHours)
	{
		if (Math.AbsFloat(lastAbsoluteHours - NEVER_SLEPT) <= NEVER_SLEPT_BAND)
			return 0;

		float elapsed = nowAbsoluteHours - lastAbsoluteHours;

		if (elapsed < 0)
			return 0;

		float remaining = cooldownHours - elapsed;

		if (remaining < 0)
			return 0;

		return remaining;
	}

	//------------------------------------------------------------------------------------------------
	//! Formats a duration in hours as "4h 12m", for the action label.
	//!
	//! Rounds to WHOLE MINUTES FIRST and splits afterwards. Splitting first and rounding the fraction
	//! produces "0h 60m" at 0.999 h; flooring the total produces "4h 11m" at 4.2 h, because 4.2 is
	//! 4.19999980926513671875 as a float and 4.2 x 60 therefore lands just under 252. Rounding the
	//! total is the only order that gets both right.
	//! \param[in] hours Duration in hours. Non-positive is formatted as "0h 0m".
	//! \return The formatted duration.
	static string FormatRemaining(float hours)
	{
		if (hours <= 0)
			return "0h 0m";

		int totalMinutes = (int)Math.Round(hours * MINUTES_PER_HOUR);

		int wholeHours = (int)Math.Floor((float)totalMinutes / MINUTES_PER_HOUR);
		int minutes = totalMinutes - (wholeHours * MINUTES_PER_HOUR);

		return wholeHours.ToString() + "h " + minutes.ToString() + "m";
	}
}
