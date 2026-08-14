//------------------------------------------------------------------------------------------------
//! TIER A case - the Game Master schedule and gain-prediction arithmetic.
//!
//! OVT_GMSchedule is a class of pure statics: every answer is a function of the arguments alone.
//! That is precisely why it exists, and it is the only part of the GM state seam an automated gate
//! can see at all - the request component, the authorization gate, the snapshot fan and the poll
//! lifecycle are all multiplayer-only and are covered by the manual verification script instead.
//!
//! WHAT IT PINS:
//!  - THE MARK BOUNDARY. InGameSecondsToNextMark returns the time to the next mark STRICTLY AFTER
//!    the given instant, so exactly on a mark it returns a full interval (21600) and never 0. That
//!    is a documented decision, not an accident: at 06:00:00 the tick has just fired. If it ever
//!    returned 0 there, a countdown would sit pinned at zero for a whole in-game minute after every
//!    distribution and look broken;
//!  - the midnight wrap. 19:30 is 16200 s from the 00:00 mark of the FOLLOWING day, which is the
//!    one case a naive "next mark greater than now, else give up" implementation gets wrong;
//!  - second-level precision, at 11:59:59 -> 1 s. A minute-resolution implementation returns 60
//!    here and every other assertion in this case still passes;
//!  - THE DEGENERATE DAY DURATION. RealSecondsFor divides by a duration supplied by the caller. If
//!    that duration is unavailable it arrives as 0, and the honest answer is the input unchanged
//!    rather than a division by zero;
//!  - THE THREAT CLAMP AND THE PLAYER BANDS of the resource-gain prediction. This arithmetic was
//!    extracted verbatim out of OVT_OccupyingFactionManager.GainResources(), which had never been
//!    covered by anything, and it drives campaign balance directly. The bands are strictly-greater
//!    tests, so the interesting values are the PAIRS either side of each boundary - 4/5, 8/9,
//!    16/17, 24/25 and 32/33 - and an off-by-one that turned any of them into >= would be invisible
//!    to a case that only sampled band midpoints;
//!  - PURITY. Two identical calls return identical results. The prediction path is used by a
//!    strictly read-only feature; if it ever grew state or a side effect, that guarantee is the
//!    thing that broke.
//!
//! The expected values below are derived by hand from the extracted arithmetic (base + perTick x
//! clamped threat factor, then the band multiplier), never by calling the subject to find out what
//! it does.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_GMSchedule : SCR_AutotestCaseBase
{
	//! Flat component of one distribution tick, used by every gain assertion below.
	protected static const int BASE_PER_TICK = 100;

	//! Threat-scaled component of one distribution tick.
	protected static const int PER_TICK = 200;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- THE SIX-HOUR MARKS. Marks sit at in-game hours 0, 6, 12 and 18.
		if (!ExpectNextMark(3, 20, 0, 9600, "mid-period, heading for 06:00"))
			return true;

		if (!ExpectNextMark(6, 0, 30, 21570, "half a minute past a mark"))
			return true;

		// THE DOCUMENTED BOUNDARY: exactly on a mark is a FULL interval, not zero.
		if (!ExpectNextMark(6, 0, 0, 21600, "exactly on a mark"))
			return true;

		if (!ExpectNextMark(0, 0, 0, 21600, "exactly on the midnight mark"))
			return true;

		// The wrap past midnight.
		if (!ExpectNextMark(19, 30, 0, 16200, "past the last mark of the day"))
			return true;

		// Second-level precision, one second short of a mark.
		if (!ExpectNextMark(11, 59, 59, 1, "one second before a mark"))
			return true;

		// --- REAL-SECONDS CONVERSION. A 3600 s day runs 24x, so six in-game hours take 900 s.
		if (!ExpectRealSeconds(21600, 3600, 900, "six in-game hours on a one-hour day"))
			return true;

		if (!ExpectRealSeconds(86400, 3600, 3600, "a whole in-game day on a one-hour day"))
			return true;

		// A day duration equal to the in-game day is the identity conversion.
		if (!ExpectRealSeconds(21600, 86400, 21600, "an unaccelerated day"))
			return true;

		// Degenerate durations return the input unchanged instead of dividing by zero.
		if (!ExpectRealSeconds(21600, 0, 21600, "a day duration of zero"))
			return true;

		if (!ExpectRealSeconds(1234, 0, 1234, "a day duration of zero, odd input"))
			return true;

		if (!ExpectRealSeconds(21600, -60, 21600, "a negative day duration"))
			return true;

		// --- THE THREAT CLAMP. threat/1000 is capped at 4, so everything at or above 4000 agrees.
		// 100 + 200 x 4 = 900, at a player count inside the lowest band.
		if (!ExpectGain(4000, 1, 900, "threat exactly at the clamp"))
			return true;

		if (!ExpectGain(9000, 1, 900, "threat far above the clamp"))
			return true;

		int atClamp = OVT_GMSchedule.PredictResourceGain(BASE_PER_TICK, PER_TICK, 4000, 1);
		int aboveClamp = OVT_GMSchedule.PredictResourceGain(BASE_PER_TICK, PER_TICK, 9000, 1);
		if (atClamp != aboveClamp)
		{
			SetFailure("Threat 9000 gained %1 and threat 4000 gained %2; the threat factor is clamped at 4, so they must agree",
				aboveClamp.ToString(), atClamp.ToString());
			return true;
		}

		// Below the clamp the factor is proportional: 100 + 200 x 1 = 300, 100 + 200 x 2 = 500.
		if (!ExpectGain(1000, 1, 300, "threat 1000, below the clamp"))
			return true;

		if (!ExpectGain(2000, 1, 500, "threat 2000, below the clamp"))
			return true;

		if (!ExpectGain(0, 1, 100, "no threat at all"))
			return true;

		// --- THE FIVE PLAYER BANDS, asserted at both sides of every boundary. At threat 1000 the
		// unscaled tick is 300, so the bands are 300 / 600 / 900 / 1200 / 1500 / 1800.
		if (!ExpectGain(1000, 4, 300, "four players, below the first band"))
			return true;

		if (!ExpectGain(1000, 5, 600, "five players, first band"))
			return true;

		if (!ExpectGain(1000, 8, 600, "eight players, still the first band"))
			return true;

		if (!ExpectGain(1000, 9, 900, "nine players, second band"))
			return true;

		if (!ExpectGain(1000, 16, 900, "sixteen players, still the second band"))
			return true;

		if (!ExpectGain(1000, 17, 1200, "seventeen players, third band"))
			return true;

		if (!ExpectGain(1000, 24, 1200, "twenty-four players, still the third band"))
			return true;

		if (!ExpectGain(1000, 25, 1500, "twenty-five players, fourth band"))
			return true;

		if (!ExpectGain(1000, 32, 1500, "thirty-two players, still the fourth band"))
			return true;

		if (!ExpectGain(1000, 33, 1800, "thirty-three players, top band"))
			return true;

		// An empty session is charged the unscaled amount, not zero.
		if (!ExpectGain(1000, 0, 300, "nobody online"))
			return true;

		// --- PURITY. Same inputs, same answer, and no accumulation between calls.
		int first = OVT_GMSchedule.PredictResourceGain(BASE_PER_TICK, PER_TICK, 2500, 17);
		int second = OVT_GMSchedule.PredictResourceGain(BASE_PER_TICK, PER_TICK, 2500, 17);
		int third = OVT_GMSchedule.PredictResourceGain(BASE_PER_TICK, PER_TICK, 2500, 17);
		if (first != second || second != third)
		{
			SetFailure("Three identical predictions returned %1, %2 and %3; the predictor holds no state and must be repeatable",
				first.ToString(), second.ToString(), third.ToString());
			return true;
		}

		// The same purity claim for the schedule half, interleaved with a different query so a
		// cached-last-answer implementation cannot pass.
		float markA = OVT_GMSchedule.InGameSecondsToNextMark(3, 20, 0);
		OVT_GMSchedule.InGameSecondsToNextMark(19, 30, 0);
		float markB = OVT_GMSchedule.InGameSecondsToNextMark(3, 20, 0);
		if (!OVT_TEST_LogicFixture.FloatEquals(markA, markB))
		{
			SetFailure("The same instant gave %1 then %2 seconds to the next mark; the schedule maths must be repeatable",
				markA.ToString(), markB.ToString());
			return true;
		}

		Print("GM schedule: marks are six-hourly with a full-interval answer exactly on a mark, the midnight wrap and second-level precision hold, a non-positive day duration returns its input unchanged, and the gain prediction clamps threat at 4 with band boundaries at 4/5, 8/9, 16/17, 24/25 and 32/33 players");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the seconds remaining to the next schedule mark, naming the case on failure.
	//! \param[in] hours In-game hour of day.
	//! \param[in] minutes In-game minute.
	//! \param[in] seconds In-game second.
	//! \param[in] expected Seconds the schedule specifies.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectNextMark(int hours, int minutes, int seconds, float expected, string label)
	{
		float actual = OVT_GMSchedule.InGameSecondsToNextMark(hours, minutes, seconds);

		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		string clock = hours.ToString() + ":" + minutes.ToString() + ":" + seconds.ToString();

		SetFailure("%1 (%2) reported %3 seconds to the next mark, expected " + expected.ToString(),
			label, clock, actual.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the in-game to real seconds conversion, naming the case on failure.
	//! \param[in] inGameSeconds Duration in in-game seconds.
	//! \param[in] dayDuration Real seconds per in-game day; <= 0 disables the conversion.
	//! \param[in] expected Real seconds the conversion specifies.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectRealSeconds(float inGameSeconds, float dayDuration, float expected, string label)
	{
		float actual = OVT_GMSchedule.RealSecondsFor(inGameSeconds, dayDuration);

		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		string inputs = inGameSeconds.ToString() + " in-game s at a day duration of " + dayDuration.ToString();

		SetFailure("%1 (%2) converted to %3 real seconds, expected " + expected.ToString(),
			label, inputs, actual.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one predicted resource gain, naming the case on failure.
	//! \param[in] threat Campaign threat.
	//! \param[in] playerCount Players online.
	//! \param[in] expected Amount the arithmetic specifies.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectGain(float threat, int playerCount, int expected, string label)
	{
		int actual = OVT_GMSchedule.PredictResourceGain(BASE_PER_TICK, PER_TICK, threat, playerCount);

		if (actual == expected)
			return true;

		string inputs = "threat " + threat.ToString() + ", " + playerCount.ToString() + " players, "
			+ BASE_PER_TICK.ToString() + " base + " + PER_TICK.ToString() + "/tick";

		SetFailure("%1 (%2) predicted %3, expected " + expected.ToString(),
			label, inputs, actual.ToString());

		return false;
	}
}
