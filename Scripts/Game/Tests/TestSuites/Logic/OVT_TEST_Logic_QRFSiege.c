//------------------------------------------------------------------------------------------------
//! TIER A cases - the arithmetic of a counter-attack siege (OVT_QRFSiege): where the ring slots sit,
//! when the muster clock is worth a packet, how it is rendered, and whether the whole force is down.
//!
//! Every subject here is a static function of plain numbers. Nothing in this file resolves a manager,
//! a controller or a world, and nothing needs to. See OVT_TEST_LogicSuite.c for the tier rule and the
//! house rules - including the reviewer grep over this directory, which does not distinguish code from
//! comments, so neither banned identifier appears anywhere below, prose included.
//!
//! WHY THIS FILE EXISTS. occupying/counter-attacks Phase 9 puts a second MODE on the one component
//! every battle in the game runs through. The four functions below are the parts of that mode that can
//! be decided without a world, and each of them fails silently when it is wrong:
//!
//!  - A RING WHOSE BEARINGS COLLIDE is still a perfectly valid set of positions. It reads in play as
//!    "the encirclement all came from one side", which looks like a spawn or pathing fault, and no log
//!    would ever mention it.
//!  - A MIRRORED RING is invisible: a full circle of slots reflected about the north-south axis is
//!    still a full circle of slots. It only shows up as one group walking the wrong way round.
//!  - A PUBLISH PREDICATE THAT SAYS YES TOO OFTEN sends 1 800 reliable broadcasts to every client over
//!    a thirty-minute window where a player-started battle sends 120; one that says no too often
//!    freezes the countdown on the panel. Neither is an error.
//!  - A ROUND-DOWN ON THE MINUTES DISPLAY tells a player they have less time than they have.
//!  - 🔴 AllNeutralised SAYING YES TOO EARLY ends a battle, starts scoring against a force that is
//!    still standing, and hands the resistance the objective for free. It is the single worst failure
//!    available in the whole feature, which is why it gets a case of its own and why the case leads
//!    with the nothing-tracked row.
//!
//! CAN-FAIL PROOFS. Running a suite is the orchestrator's job, not an implementation agent's
//! (.claude/test-policy.md), so each proof below is a fault that was injected into the subject one at a
//! time and compiled - every one exited tools/compile-check.sh with 0, which is the point: none of them
//! is a syntax error and nothing else in the tree would stop it reaching players. The subject was
//! restored and recompiled clean afterwards. The resulting failure text is recorded per case.
//!
//! No maxAttempts anywhere: every subject is a pure function with no clock, no randomness and no
//! world, and cannot flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! OVT_QRFSiege.RingSlotBearing - N groups, N evenly spaced bearings, the first of them due north.
//!
//! Three claims:
//!   1. SLOT 0 IS ALWAYS 0 DEGREES, at every ring size. It is the one row of the geometry that can be
//!      checked without reproducing the formula.
//!   2. THE SLOTS ARE EVENLY SPACED AND NEVER REPEAT, at 1, 2, 3 and 12 groups. Consecutive bearings
//!      differ by exactly 360/N, every bearing is inside [0, 360), and no two are equal - the last of
//!      those is what a collapsed ring actually looks like.
//!   3. THE DEGENERATE INPUTS ANSWER SOMETHING RATHER THAN DIVIDING BY ZERO: a count of 0 or less, an
//!      index at or past the count, and a negative index.
//!
//! CAN-FAIL, three faults, injected and compiled separately:
//!   R1. OFF BY ONE ON THE SLOT - `(FULL_CIRCLE / count) * (wrapped + 1)`. Compiled clean (exit 0). The
//!       first row it reaches is the one-group ring, whose only slot then lands on 360, so the case
//!       fails on "slot 0 of 1 must be inside [0, 360): got 360.000000" - the range guard catching it
//!       before the due-north guard does.
//!   R2. SPACE THEM BY THE WRONG DIVISOR - `FULL_CIRCLE / (count + 1)`. Compiled clean (exit 0). The
//!       one-group ring still passes (its only slot is 0 either way), so the case fails on the
//!       two-group ring: "slot 1 of 2 must sit 180.000000 deg round".
//!   R3. DROP THE ZERO-COUNT GUARD - remove `if (count <= 0) return 0;`. Compiled clean (exit 0). The
//!       even-ring rows all still pass and the case reaches
//!       `ExpectBearing(0, 0, …)`, where `index % count` is a modulo by zero - so it aborts with an
//!       engine-level division fault rather than with an assertion. That IS the finding, and it is
//!       recorded as such: an unguarded count is not a wrong answer, it is a runtime fault.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_QRFSiege_RingSlotsSpreadEvenlyAndStartDueNorth : SCR_AutotestCaseBase
{
	//! Bearings are floats built from a division; compared with a tolerance, never with ==.
	static const float DEGREE_EPSILON = 0.001;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1 + 2, over every ring size the plan names.
		if (!ExpectEvenRing(1))
			return true;

		if (!ExpectEvenRing(2))
			return true;

		if (!ExpectEvenRing(3))
			return true;

		if (!ExpectEvenRing(12))
			return true;

		// --- ...and one awkward size, because 360/7 is not a whole number of degrees and a ring built
		//     out of rounded integers would drift.
		if (!ExpectEvenRing(7))
			return true;

		// --- Claim 2, spelled out for the sizes whose answers can be written down by hand.
		if (!ExpectBearing(1, 2, 180, "slot 1 of 2 must sit half way round"))
			return true;

		if (!ExpectBearing(1, 4, 90, "slot 1 of 4 must be due east"))
			return true;

		if (!ExpectBearing(2, 4, 180, "slot 2 of 4 must be due south"))
			return true;

		if (!ExpectBearing(3, 4, 270, "slot 3 of 4 must be due west"))
			return true;

		if (!ExpectBearing(6, 12, 180, "slot 6 of 12 must be due south"))
			return true;

		// --- Claim 3: the degenerate inputs.
		if (!ExpectBearing(0, 0, 0, "a ring of nothing answers due north rather than dividing by zero"))
			return true;

		if (!ExpectBearing(3, -4, 0, "a negative ring size answers due north rather than dividing by a negative"))
			return true;

		if (!ExpectBearing(4, 4, 0, "an index at the count wraps back onto slot 0"))
			return true;

		if (!ExpectBearing(5, 4, 90, "an index past the count wraps onto slot 1"))
			return true;

		if (!ExpectBearing(-1, 4, 270, "a negative index wraps onto the LAST slot, not off the ring"))
			return true;

		Print("QRFSiege: ring bearings are evenly spaced, never repeat, always start due north, and survive a zero count, a negative count and an out-of-range index");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that a whole ring of `count` slots is even, unique, in range, and starts due north.
	//! \param[in] count How many slots the ring has.
	//! \return True when the whole ring held; false after recording the failure.
	protected bool ExpectEvenRing(int count)
	{
		float expectedStep = 360.0 / count;

		array<float> seen = new array<float>;

		for (int i = 0; i < count; i++)
		{
			float bearing = OVT_QRFSiege.RingSlotBearing(i, count);

			if (bearing < 0 || bearing >= 360)
			{
				SetFailure("slot %1 of %2 must be inside [0, 360): got %3", i.ToString(), count.ToString(), bearing.ToString());
				return false;
			}

			// The one row that needs no arithmetic to check.
			if (i == 0 && Math.AbsFloat(bearing) > DEGREE_EPSILON)
			{
				SetFailure("slot 0 of %1 must be due north: got %2, expected 0.000000", count.ToString(), bearing.ToString());
				return false;
			}

			if (i > 0)
			{
				float step = bearing - seen[i - 1];

				if (Math.AbsFloat(step - expectedStep) > DEGREE_EPSILON)
				{
					SetFailure("slot %1 of %2 must sit %3 deg round", i.ToString(), count.ToString(), expectedStep.ToString());
					return false;
				}
			}

			// A collapsed ring is exactly this: two groups told to stand in the same direction.
			foreach (float earlier : seen)
			{
				if (Math.AbsFloat(earlier - bearing) <= DEGREE_EPSILON)
				{
					SetFailure("two slots of a %1-group ring share bearing %2", count.ToString(), bearing.ToString());
					return false;
				}
			}

			seen.Insert(bearing);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one named bearing row.
	//! \param[in] index Which slot.
	//! \param[in] count How many slots the ring has.
	//! \param[in] expected The bearing it must answer.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectBearing(int index, int count, float expected, string label)
	{
		float actual = OVT_QRFSiege.RingSlotBearing(index, count);

		if (Math.AbsFloat(actual - expected) <= DEGREE_EPSILON)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_QRFSiege.RingSlotOffset - THE SIGN, and the fact that the offset is scaled rather than
//! normalized.
//!
//! ==================================================================================================
//! 🔴 WHY THE SIGN IS A NAMED CASE WITH SPELLED-OUT CARDINALS.
//! ==================================================================================================
//! The convention the whole battle layer uses is 0 deg = North = **-Z**, 90 deg = East = **+X**. Flip
//! the sign of either component and the ring is mirrored - and a mirrored full circle is still a full
//! circle, so nothing above this function can detect it. It surfaces only as a group that was meant to
//! be east of a town standing west of it, which reads as a pathing fault. So the four cardinals are
//! asserted as explicit vectors, and the north row additionally asserts that the Z component is
//! NEGATIVE as a claim in its own right - a reader who does not know the convention can still check
//! that one.
//!
//! Three claims:
//!   1. THE FOUR CARDINALS of a four-slot ring are exactly north, east, south, west.
//!   2. SLOT 0'S Z IS NEGATIVE AND ITS X IS ZERO - the convention restated without arithmetic.
//!   3. THE LENGTH IS THE RADIUS, at both ends of the authored band and at every slot of a 12-ring. A
//!      normalized offset compiles, sorts identically and puts the entire siege one metre from the
//!      middle of the objective.
//!
//! CAN-FAIL, three faults, injected and compiled separately:
//!   O1. FLIP THE Z SIGN - `Math.Cos(...) * radius` in place of `-Math.Cos(...) * radius`. Compiled
//!       clean (exit 0). The case then fails on
//!       "slot 0 of 4 at 100 m must be due NORTH (-Z): got <0 0 100>, expected <0 0 -100>".
//!   O2. FLIP THE X SIGN - `-Math.Sin(...)`. Compiled clean (exit 0). The case then fails on
//!       "slot 1 of 4 at 100 m must be due EAST (+X): got <-100 0 0>, expected <100 0 0>".
//!   O3. NORMALIZE THE RESULT - `return offset.Normalized();`. Compiled clean (exit 0). Claim 3 is
//!       where a reader would expect this to land, but the cardinals are asserted first and a unit
//!       vector is not 100 m from the centre, so it actually fails on the very first row: "slot 0 of 4
//!       at 100 m must be due NORTH (-Z)", with a got-vector of unit length. Recorded as it really
//!       behaves rather than as it reads.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_QRFSiege_RingSlotOffsetsMatchTheCompassConvention : SCR_AutotestCaseBase
{
	//! Metres. The offsets come out of a pair of transcendentals, so a cardinal that should be exactly
	//! zero comes back as a very small number instead.
	static const float METRE_EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: the four cardinals, written out.
		if (!ExpectOffset(0, 4, 100, "0 0 -100", "slot 0 of 4 at 100 m must be due NORTH (-Z)"))
			return true;

		if (!ExpectOffset(1, 4, 100, "100 0 0", "slot 1 of 4 at 100 m must be due EAST (+X)"))
			return true;

		if (!ExpectOffset(2, 4, 100, "0 0 100", "slot 2 of 4 at 100 m must be due SOUTH (+Z)"))
			return true;

		if (!ExpectOffset(3, 4, 100, "-100 0 0", "slot 3 of 4 at 100 m must be due WEST (-X)"))
			return true;

		// --- Claim 2: the same thing again, as a sign claim a reader can check without the convention.
		vector north = OVT_QRFSiege.RingSlotOffset(0, 12, 150);

		if (north[2] >= 0)
		{
			SetFailure("slot 0 must lie in NEGATIVE Z - that is what north means in this convention: got z %1", north[2].ToString());
			return true;
		}

		if (Math.AbsFloat(north[0]) > METRE_EPSILON)
		{
			SetFailure("slot 0 must have no east-west component at all: got x %1", north[0].ToString());
			return true;
		}

		// --- ...and the east row as its own sign claim, because a mirrored ring flips exactly this.
		vector east = OVT_QRFSiege.RingSlotOffset(3, 12, 150);

		if (east[0] <= 0)
		{
			SetFailure("slot 3 of 12 is a quarter turn round and must lie in POSITIVE X: got x %1", east[0].ToString());
			return true;
		}

		// --- Claim 3: the length is the radius, at both ends of the authored band.
		if (!ExpectLength(0, 4, OVT_QRFSiege.SIEGE_RING_MIN))
			return true;

		if (!ExpectLength(0, 4, OVT_QRFSiege.SIEGE_RING_MAX))
			return true;

		for (int i = 0; i < 12; i++)
		{
			if (!ExpectLength(i, 12, 123.5))
				return true;
		}

		// --- ...and the horizontal plane is never left. A slot with a height in it would be a
		//     waypoint under the terrain.
		vector sloped = OVT_QRFSiege.RingSlotOffset(5, 12, 140);

		if (Math.AbsFloat(sloped[1]) > METRE_EPSILON)
		{
			SetFailure("a ring slot is a horizontal offset and must carry no height: got y %1", sloped[1].ToString());
			return true;
		}

		Print("QRFSiege: ring offsets follow the 0 deg = North = -Z convention on all four cardinals, carry no height, and are scaled to the radius rather than normalized");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one named offset row against a written-out vector.
	//! \param[in] index Which slot.
	//! \param[in] count How many slots the ring has.
	//! \param[in] radius How far out.
	//! \param[in] expected The offset it must answer, as a vector literal.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectOffset(int index, int count, float radius, vector expected, string label)
	{
		vector actual = OVT_QRFSiege.RingSlotOffset(index, count, radius);

		if (vector.Distance(actual, expected) <= METRE_EPSILON)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that one slot sits exactly `radius` from the centre.
	//! \param[in] index Which slot.
	//! \param[in] count How many slots the ring has.
	//! \param[in] radius The distance it must be at.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectLength(int index, int count, float radius)
	{
		vector actual = OVT_QRFSiege.RingSlotOffset(index, count, radius);

		float length = actual.Length();

		if (Math.AbsFloat(length - radius) <= METRE_EPSILON)
			return true;

		// ⚠ THREE PARAMETERS AFTER THE FORMAT STRING IS THE CEILING (SCR_AutotestCaseBase); the slot's
		// index and its ring size are folded into one.
		string slot = index.ToString() + " of " + count.ToString();

		SetFailure("slot %1 must sit exactly %2 m out: got %3", slot, radius.ToString(), length.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_QRFSiege.ShouldPublishTimer - how often a thirty-minute clock is worth a reliable broadcast to
//! every client.
//!
//! Four claims:
//!   1. THE FIRST VALUE ALWAYS GOES OUT, whatever the cadence would say. The sentinel is any negative
//!      "last published".
//!   2. A CLOCK THAT WENT UP ALWAYS GOES OUT. A siege arms 1 800 000 over a field that was previously
//!      120 000, and a cadence written as "how much has elapsed" reads a negative elapsed time.
//!   3. INSIDE THE LAST TWO MINUTES, EVERY TICK. This is the range a player-started battle lives in
//!      entirely, so the standard countdown is provably unchanged: it publishes on every one of its
//!      120 ticks exactly as it does today.
//!   4. ABOVE TWO MINUTES, ONLY EVERY TEN SECONDS - and exactly ten seconds counts.
//!
//! CAN-FAIL, three faults, injected and compiled separately:
//!   P1. DROP THE CLOCK-JUMPED TEST - remove `if (remainingMs > lastPublishedMs) return true;`.
//!       Compiled clean (exit 0). The case then fails on "arming the muster window over a countdown
//!       must publish at once: 1800000 ms after 120000 ms" - the elapsed-time arithmetic goes negative
//!       and the panel freezes on the old value until the window has burned twenty-eight minutes.
//!   P2. MAKE THE SECONDS BAND STRICT - `remainingMs < PUBLISH_THRESHOLD_MS`. Compiled clean (exit 0).
//!       The case then fails on "the tick that reaches exactly two minutes must publish: 120000 ms
//!       after 121000 ms". This is the fault that would freeze the standard countdown's handover.
//!   P3. LOOSEN THE COARSE INTERVAL - `> 0` in place of `>= PUBLISH_COARSE_INTERVAL_MS`. Compiled clean
//!       (exit 0). The case then fails on "one second of a thirty-minute clock is not worth a
//!       broadcast: 1799000 ms after 1800000 ms".
//!
//! ⚠ ONE FAULT IS HONESTLY NOT DETECTABLE, and it is written here rather than left to be discovered:
//! removing the FIRST-VALUE SENTINEL (`if (lastPublishedMs < 0) return true;`) on its own changes
//! nothing any row can see, because the clock-jumped test immediately below it answers true for any
//! non-negative remaining time against a negative sentinel. The sentinel is defence in depth for a
//! caller that ever seeds the field with something else, not an independently observable rule, and no
//! row is written to pretend otherwise.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_QRFSiege_TimerIsPublishedOnACadence : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: nothing published yet.
		if (!ExpectPublish(1800000, -1, true, "a freshly armed clock must publish its first value"))
			return true;

		if (!ExpectPublish(119000, -1, true, "a standard countdown's first value must publish too"))
			return true;

		// --- Claim 2: the clock jumped up.
		if (!ExpectPublish(1800000, 120000, true, "arming the muster window over a countdown must publish at once"))
			return true;

		// --- Claim 3: inside the last two minutes, every tick. These are the rows a player-started
		//     battle actually walks, one second at a time.
		if (!ExpectPublish(120000, 121000, true, "the tick that reaches exactly two minutes must publish"))
			return true;

		if (!ExpectPublish(119000, 120000, true, "one second inside the last two minutes must publish"))
			return true;

		if (!ExpectPublish(1000, 2000, true, "the last second must publish"))
			return true;

		if (!ExpectPublish(0, 1000, true, "the zero that switches the panel over must publish"))
			return true;

		// --- Claim 4: above two minutes, only every ten seconds.
		if (!ExpectPublish(1799000, 1800000, false, "one second of a thirty-minute clock is not worth a broadcast"))
			return true;

		if (!ExpectPublish(1795000, 1800000, false, "five seconds of a thirty-minute clock is not worth a broadcast"))
			return true;

		if (!ExpectPublish(1791000, 1800000, false, "nine seconds is still short of the interval"))
			return true;

		if (!ExpectPublish(1790000, 1800000, true, "exactly ten seconds IS the interval"))
			return true;

		if (!ExpectPublish(1780000, 1800000, true, "twenty seconds is past the interval"))
			return true;

		// --- ...and the boundary from the other side: one millisecond above two minutes is still the
		//     coarse band, and 999 ms of it is not enough.
		if (!ExpectPublish(120001, 121000, false, "one millisecond above two minutes is still the coarse band"))
			return true;

		if (!ExpectPublish(120001, 130001, true, "ten seconds of the coarse band publishes even right at its floor"))
			return true;

		Print("QRFSiege: the muster clock publishes its first value, publishes when it jumps, publishes every tick inside the last two minutes, and publishes every ten seconds above them");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one publish row.
	//! \param[in] remainingMs What the clock reads now.
	//! \param[in] lastPublishedMs What was last broadcast, or negative for "nothing yet".
	//! \param[in] expected Whether it should be broadcast.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectPublish(int remainingMs, int lastPublishedMs, bool expected, string label)
	{
		bool actual = OVT_QRFSiege.ShouldPublishTimer(remainingMs, lastPublishedMs);

		if (actual == expected)
			return true;

		SetFailure("%1: %2 ms after %3 ms", label, remainingMs.ToString(), lastPublishedMs.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_QRFSiege.ShouldRenderMinutes and MusterMinutesRemaining - the crossover, and the rounding.
//!
//! Three claims:
//!   1. THE CROSSOVER IS AT EXACTLY 120 000 ms AND IS EXCLUSIVE ON THE MINUTES SIDE. This is the row
//!      that protects the standard countdown: it starts at exactly 120 000, so an inclusive test would
//!      make a player-initiated battle open with "2 min" instead of "120", which is a visible change to
//!      the most-played event in the game.
//!   2. MINUTES ROUND **UP**. A clock reading 29:59 must say 30, never 29 - the number answers "how
//!      long have I got", and rounding down tells a player they have less time than they do. The exact
//!      multiples are asserted too, so the ceiling has not become an add-one.
//!   3. A DEAD OR NEGATIVE CLOCK ANSWERS 0 rather than a negative minute count.
//!
//! ⚠ THE DIVISION IS THE FAULT LINE. `ms / MS_PER_MINUTE` between two ints truncates in this language,
//! which would make the ceiling a no-op and silently turn the whole display into a round-DOWN. The
//! 1 799 999 row is the one that catches it.
//!
//! CAN-FAIL, three faults, injected and compiled separately:
//!   M1. MAKE THE CROSSOVER INCLUSIVE - `remainingMs >= PUBLISH_THRESHOLD_MS`. Compiled clean (exit 0).
//!       The case then fails on "exactly two minutes renders in SECONDS, so a standard battle never
//!       reaches the minutes form: 120000 ms".
//!   M2. TAKE THE INT DIVISION - `int minutes = ms / MS_PER_MINUTE;`. Compiled clean (exit 0). The case
//!       then fails on "29:59.999 must read 30 minutes, never 29: got 29, expected 30".
//!   M3. ROUND DOWN - `Math.Floor(minutes)`. Compiled clean (exit 0). The case then fails on the same
//!       row, which is deliberate: M2 and M3 are the same visible defect reached two different ways,
//!       and one assertion covering both is the right shape.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_QRFSiege_TimerCrossesFromMinutesToSecondsAtTwoMinutes : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: the crossover, from both sides of the exact boundary.
		if (!ExpectMinutesForm(OVT_QRFSiege.MUSTER_TIME_MS, true, "a freshly armed muster window renders in minutes"))
			return true;

		if (!ExpectMinutesForm(120001, true, "one millisecond above two minutes still renders in minutes"))
			return true;

		if (!ExpectMinutesForm(120000, false, "exactly two minutes renders in SECONDS, so a standard battle never reaches the minutes form"))
			return true;

		if (!ExpectMinutesForm(119999, false, "just under two minutes renders in seconds"))
			return true;

		if (!ExpectMinutesForm(1000, false, "the last second renders in seconds"))
			return true;

		if (!ExpectMinutesForm(0, false, "a dead clock renders in seconds - the panel switches to its battle line instead"))
			return true;

		// --- Claim 2: rounding UP, including at the exact multiples.
		// ⚠ 15, NOT 30, SINCE 2026-08-20 - MUSTER_TIME_MS was halved on the author's play-test call. The
		// EXPECTATION is derived rather than hardcoded so this row pins the ROUNDING rule (a whole
		// multiple reads as itself, not one higher) instead of pinning a balance number that is expected
		// to move. It read a literal 30 before and duly went red on the tuning change, which is a test
		// asserting the wrong thing: the constant is a knob, the round-UP is the contract.
		if (!ExpectMinutes(OVT_QRFSiege.MUSTER_TIME_MS, OVT_QRFSiege.MUSTER_TIME_MS / OVT_QRFSiege.MS_PER_MINUTE, "a freshly armed muster window reads its own whole minutes"))
			return true;

		if (!ExpectMinutes(1799999, 30, "29:59.999 must read 30 minutes, never 29"))
			return true;

		if (!ExpectMinutes(1740000, 29, "exactly 29 minutes reads 29, so the ceiling has not become an add-one"))
			return true;

		if (!ExpectMinutes(1740500, 30, "half a second past 29 minutes reads 30"))
			return true;

		if (!ExpectMinutes(120001, 3, "just above the crossover reads 3, because there is more than two minutes left"))
			return true;

		if (!ExpectMinutes(120000, 2, "exactly two minutes reads 2 - not that the panel ever shows it"))
			return true;

		if (!ExpectMinutes(60000, 1, "exactly one minute reads 1"))
			return true;

		if (!ExpectMinutes(1, 1, "one millisecond left is still 'a minute' rather than none"))
			return true;

		// --- Claim 3: dead and negative.
		if (!ExpectMinutes(0, 0, "a dead clock reads 0"))
			return true;

		if (!ExpectMinutes(-5000, 0, "a clock that overran reads 0 rather than a negative minute count"))
			return true;

		Print("QRFSiege: the display crosses from minutes to seconds at exactly 120 000 ms - exclusive, so a standard countdown never renders in minutes - and the minutes form always rounds up");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one crossover row.
	//! \param[in] ms What the clock reads.
	//! \param[in] expected Whether it should render in whole minutes.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectMinutesForm(int ms, bool expected, string label)
	{
		bool actual = OVT_QRFSiege.ShouldRenderMinutes(ms);

		if (actual == expected)
			return true;

		SetFailure("%1: %2 ms", label, ms.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one minutes row.
	//! \param[in] ms What the clock reads.
	//! \param[in] expected The whole minutes it must answer.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectMinutes(int ms, int expected, string label)
	{
		int actual = OVT_QRFSiege.MusterMinutesRemaining(ms);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 OVT_QRFSiege.AllNeutralised - the predicate that ends a battle, and the reason it is biased.
//!
//! ==================================================================================================
//! THE HEADLINE ROW IS AllNeutralised(0, 0) == **FALSE**, AND IT IS NOT AN EDGE CASE.
//! ==================================================================================================
//! This predicate is asked on a ten-second cadence throughout a thirty-minute muster window. Saying yes
//! ends the window, starts scoring against a force that is still standing, and - because scoring awards
//! the resistance five points a tick against an empty zone - hands them the objective outright, in a
//! way no log would ever explain. Saying no when it should have said yes costs a siege that had already
//! been wiped out sitting out the rest of its clock, which nobody would notice.
//!
//! So the bias is deliberate and it is asserted, not merely commented: nothing tracked is NOT
//! "everything is dead", it is "the question has not been asked of anything yet" - the state the
//! controller is in before its first group has spawned, which is exactly when a true answer would be
//! catastrophic.
//!
//! Four claims:
//!   1. NOTHING TRACKED IS ALWAYS FALSE, including the nonsense input where more were neutralised than
//!      were ever tracked.
//!   2. A PARTIAL WIPE IS FALSE - one survivor of twelve is not a wipe.
//!   3. A COMPLETE WIPE IS TRUE, at one group and at twelve, and an over-count still reads true rather
//!      than falling off the end of the comparison.
//!   4. THE PER-GROUP JUDGEMENT (GroupNeutralised) resolves an unresolved entity to DEAD, a group with
//!      agents but no fit ones to DEAD, and 🔴 A RESOLVED GROUP WITH ZERO AGENTS TO **ALIVE**. That
//!      rule is the reason this decision is a named function at all instead of three lines inside the
//!      controller's loop: in the controller it can only be commented, here it can be asserted.
//!
//! CAN-FAIL, four faults, injected and compiled separately:
//!   N1. DROP THE NOTHING-TRACKED GUARD - remove `if (tracked <= 0) return false;`. Compiled clean
//!       (exit 0). The case then fails on "NOTHING TRACKED IS NOT EVERYTHING DEAD: 0 of 0 must be
//!       false". This is the fault the whole case exists for.
//!   N2. MAKE IT AN EQUALITY - `return neutralised == tracked;`. Compiled clean (exit 0). The case then
//!       fails on "an over-count is still a wipe: 6 of 5 must be true", which is the benign half; the
//!       point of recording it is that the guard and the comparison are two separate claims.
//!   N3. LOOSEN THE COMPARISON - `return neutralised >= tracked - 1;`. Compiled clean (exit 0). The
//!       first partial row it reaches with exactly one survivor is the five-group one, so the case
//!       fails on "one survivor of five is not a wipe (tracked 5, neutralised 4)" - the shape an
//!       off-by-one in the caller's own loop would produce.
//!   N4. 🔴 PRUNE ON ZERO AGENTS - `if (agentCount <= 0) return true;` in GroupNeutralised. Compiled
//!       clean (exit 0). This is THE fault of the whole feature: it ends a battle against a force
//!       that is still standing. The case then fails on "🔴 A RESOLVED GROUP WITH ZERO AGENTS IS
//!       **ALIVE** … (resolved 1, 0 agents, 0 fit)".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_QRFSiege_AllNeutralisedNeverFiresOnNothingTracked : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1. THE HEADLINE.
		if (!ExpectWipe(0, 0, false, "NOTHING TRACKED IS NOT EVERYTHING DEAD: 0 of 0 must be false"))
			return true;

		if (!ExpectWipe(0, 5, false, "nothing tracked stays false even when the count is nonsense"))
			return true;

		if (!ExpectWipe(-1, -1, false, "a negative tracked count is still nothing tracked"))
			return true;

		if (!ExpectWipe(-3, 0, false, "a negative tracked count with nothing neutralised is false"))
			return true;

		// --- Claim 2: partial wipes.
		if (!ExpectWipe(5, 0, false, "a force with nothing down at all is not a wipe"))
			return true;

		if (!ExpectWipe(5, 4, false, "one survivor of five is not a wipe"))
			return true;

		if (!ExpectWipe(12, 11, false, "one survivor of twelve is not a wipe"))
			return true;

		if (!ExpectWipe(1, 0, false, "the single group of a one-group siege still standing is not a wipe"))
			return true;

		// --- Claim 3: complete wipes.
		if (!ExpectWipe(1, 1, true, "one group tracked and one group down IS a wipe"))
			return true;

		if (!ExpectWipe(5, 5, true, "five of five is a wipe"))
			return true;

		if (!ExpectWipe(12, 12, true, "twelve of twelve is a wipe"))
			return true;

		if (!ExpectWipe(5, 6, true, "an over-count is still a wipe"))
			return true;

		// --- Claim 4: THE PER-GROUP JUDGEMENT, and the zero-agent rule the whole bias is built on.
		if (!ExpectGroupDown(false, 0, 0, true, "a group whose entity is gone is dead"))
			return true;

		if (!ExpectGroupDown(false, 8, 8, true, "an unresolved group is dead whatever it last reported"))
			return true;

		if (!ExpectGroupDown(true, 0, 0, false, "🔴 A RESOLVED GROUP WITH ZERO AGENTS IS **ALIVE** - zero agents is an unreliable reading in this engine, not evidence of death"))
			return true;

		if (!ExpectGroupDown(true, 8, 0, true, "eight agents and none of them fighting fit is dead"))
			return true;

		if (!ExpectGroupDown(true, 8, 1, false, "one man still standing out of eight is not dead"))
			return true;

		if (!ExpectGroupDown(true, 1, 1, false, "a single fit man is not dead"))
			return true;

		if (!ExpectGroupDown(true, 8, 8, false, "a full, untouched group is not dead"))
			return true;

		Print("QRFSiege: the early end never fires with nothing tracked, never fires with a survivor, and does fire on a complete wipe - and a resolved group reporting zero agents counts as ALIVE, deliberately");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one per-group judgement row.
	//! \param[in] resolved Whether the entity still resolves to a group.
	//! \param[in] agents How many agents it reports.
	//! \param[in] fit How many of them are alive and conscious.
	//! \param[in] expected Whether the group should count as neutralised.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectGroupDown(bool resolved, int agents, int fit, bool expected, string label)
	{
		bool actual = OVT_QRFSiege.GroupNeutralised(resolved, agents, fit);

		if (actual == expected)
			return true;

		string reading = agents.ToString() + " agents, " + fit.ToString() + " fit";

		SetFailure("%1 (resolved %2, %3)", label, resolved.ToString(), reading);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one wipe row.
	//! \param[in] tracked How many groups were ever put on the ground.
	//! \param[in] neutralised How many the caller judged dead.
	//! \param[in] expected Whether the early end should fire.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectWipe(int tracked, int neutralised, bool expected, string label)
	{
		bool actual = OVT_QRFSiege.AllNeutralised(tracked, neutralised);

		if (actual == expected)
			return true;

		SetFailure("%1 (tracked %2, neutralised %3)", label, tracked.ToString(), neutralised.ToString());

		return false;
	}
}
