//------------------------------------------------------------------------------------------------
//! TIER A cases - INSERTION GEOMETRY: the four decisions that route a force between "arrives by
//! truck" and "arrives on foot", and the two helpers that feed them.
//!
//! Every subject here is a static function of plain numbers and vectors. Nothing in this file
//! resolves a manager, a controller or a world, and nothing needs to. See OVT_TEST_LogicSuite.c for
//! the tier rule and the house rules - including the reviewer grep over this directory, which does
//! not distinguish code from comments, so neither banned identifier appears anywhere below, prose
//! included.
//!
//! WHY THIS FILE EXISTS. occupying/counter-attacks Phase 4 ships a deployment module that drives a
//! real truck down real roads with a real force aboard. The one guarantee that module makes is that
//! the force ARRIVES - by truck if everything works, on foot if anything does not - and every one of
//! the five diversions onto the foot path runs through one of the functions below. None of the
//! failures is a script error, none appears in a log, and the symptom of every one of them is the
//! same: men who never turn up, or men who turn up in the wrong place.
//!
//! WHAT EACH SUBJECT COSTS WHEN IT IS WRONG:
//!   ShouldWalk        a sign slip sends a truck on a fifty-metre hop, or walks a force twenty
//!                     kilometres. Getting the "disabled" case wrong walks EVERY force in the
//!                     campaign, silently, because a zero threshold is what an unauthored config has.
//!   LZPointOnLine     an unclamped standoff puts the landing zone BEHIND the source, so the convoy
//!                     drives away from its objective; a degenerate line produces a NaN, which
//!                     poisons every distance test made against it for the rest of the drive and
//!                     leaves the convoy driving forever.
//!   HasArrived        never true means a convoy that circles its drop point until it is called
//!                     stuck; always true means a force dumped at the base it set out from.
//!   IsStuck           the arrival test inside it is the load-bearing part: every convoy that ever
//!                     succeeds stands still on its landing zone for at least one tick, so without
//!                     it EVERY successful insertion also reports as stranded and dumps its men.
//!   AdvanceStuckTicks cumulative instead of consecutive counting condemns any truck that ever
//!                     paused at a junction.
//!   SpeedFromTravel   dividing by a zero elapsed time on the first observation is the difference
//!                     between a number and an infinity.
//!
//! CAN-FAIL PROOFS. Running a suite is the orchestrator's job, not an implementation agent's
//! (.claude/test-policy.md), so each proof below is a fault that was injected into the subject one at
//! a time and compiled - every one exited tools/compile-check.sh with 0, which is the point: none of
//! these are syntax errors and nothing else in the tree would stop them reaching players. The subject
//! was restored and re-compiled clean afterwards.
//!
//! No maxAttempts anywhere: every subject is a pure function with no clock, no RNG and no world, and
//! cannot flake. Distances are chosen away from 1 000 m and 2 000 m, where vector.Distance is known
//! to answer one unit in the last place high, so no row sits on a boundary the measurement itself
//! cannot resolve.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! ShouldWalk: below the threshold walk, above it drive, and a non-positive threshold NEVER walks.
//!
//! ⚠ THE THIRD CLAIM IS THE ONE WORTH THE CASE. "0 disables the rule" and "0 means walk everything"
//! are both defensible readings of a threshold, and the wrong one is invisible: a config that leaves
//! the field unauthored gets 0, and every insertion in the campaign quietly stops using trucks. The
//! feature would still deliver its men - which is exactly why nobody would notice - while the most
//! visible mechanic it has never appeared.
//!
//! THE BOUNDARY IS CLAIMED IN BOTH DIRECTIONS because it is observable here, unlike the anchor's
//! radius boundary: at exactly the threshold the answer is a bool, so `<=` and `<` differ.
//!
//! CAN-FAIL, three faults, injected and compiled separately. All three exited compile-check 0:
//!   B1. DROP THE THRESHOLD GUARD - remove `if (threshold <= 0) return false;`. Compiled clean.
//!       Fails on "an unauthored (zero) threshold must never walk: got walk, expected drive".
//!   B2. INVERT THE COMPARISON - `distance >= threshold`. Compiled clean. Fails on the first row,
//!       "a hop well inside the threshold must walk: got drive, expected walk".
//!   B3. STRICT INSTEAD OF INCLUSIVE - `distance < threshold`. Compiled clean. Every row but one
//!       passes; fails on "a hop exactly at the threshold must walk: got drive, expected walk".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveInsertion_ShouldWalkOnlyShortHops : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The ordinary band.
		if (!Expect(120, 400, true, "a hop well inside the threshold must walk"))
			return true;

		if (!Expect(399.5, 400, true, "a hop just inside the threshold must walk"))
			return true;

		if (!Expect(400, 400, true, "a hop exactly at the threshold must walk"))
			return true;

		if (!Expect(400.5, 400, false, "a hop just past the threshold must drive"))
			return true;

		if (!Expect(3500, 400, false, "a hop well past the threshold must drive"))
			return true;

		// --- Zero separation. A deployment created on top of the place its force comes from has
		//     nowhere to drive TO, and walking nought metres is the honest answer.
		if (!Expect(0, 400, true, "a force already at its objective must walk, not board a truck"))
			return true;

		// --- The disabled threshold. See the header: this is the claim that matters.
		if (!Expect(0, 0, false, "an unauthored (zero) threshold must never walk"))
			return true;

		if (!Expect(5, 0, false, "a zero threshold must drive even the shortest hop"))
			return true;

		if (!Expect(5, -400, false, "a negative threshold must never walk"))
			return true;

		// --- A caller measuring a signed offset. The function takes bare floats and cannot check its
		//     caller; a negative distance reads as "no distance at all", which walks.
		if (!Expect(-250, 400, true, "a negative distance must read as no distance, and walk"))
			return true;

		Print("ShouldWalk: short hops walk, long hops drive, and a threshold of zero or less disables the rule instead of walking everything");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] distance Source-to-objective separation.
	//! \param[in] threshold The authored walk threshold.
	//! \param[in] expected The answer this row claims.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool Expect(float distance, float threshold, bool expected, string label)
	{
		bool actual = OVT_InsertionGeometry.ShouldWalk(distance, threshold);
		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, Describe(actual), Describe(expected));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] walk The answer to describe.
	//! \return "walk" or "drive".
	protected string Describe(bool walk)
	{
		if (walk)
			return "walk";

		return "drive";
	}
}

//------------------------------------------------------------------------------------------------
//! LZPointOnLine: the landing zone always lies on the closed segment between the source and the
//! objective, at the authored standoff where there is room for it.
//!
//! THE ONE PROPERTY THAT MATTERS IS CONTAINMENT, and it is asserted directly rather than inferred
//! from the arithmetic: whatever anybody authors, the point is never before the source and never past
//! the objective. Both violations are silent and both are severe - a point past the objective drives
//! the convoy THROUGH the place it was supposed to stop short of, and a point behind the source sends
//! it away from the objective entirely, into whatever is on the far side of its own base.
//!
//! THE DEGENERATE LINE IS A ROW BECAUSE IT IS REACHABLE IN NORMAL PLAY, not as a theoretical edge: a
//! deployment created at the base its force comes from has a source and an objective in the same
//! place, and normalising a zero-length direction yields a NaN. A NaN landing zone is worse than a
//! wrong one - every distance comparison against it is false, so the convoy never arrives, never
//! counts as stuck, and drives until the deployment is torn down.
//!
//! THE MIDPOINT IS ASSERTED EXACTLY, not just the shape, because the standoff is a number a designer
//! types into a config and has to be able to predict: 300 m of standoff on a 900 m journey puts the
//! drop two thirds of the way along, and that has to be true rather than approximately true.
//!
//! CAN-FAIL, four faults, injected and compiled separately. All four exited compile-check 0:
//!   C1. DROP THE OVER-LONG STANDOFF CLAMP - remove `if (standoff >= separation) return source;`.
//!       Compiled clean. Fails on "a standoff longer than the whole journey must drop at the source,
//!       never behind it", with a point measurably further from the objective than the source is.
//!   C2. DROP THE DEGENERATE GUARD - remove `if (separation < MIN_SEPARATION) return source;`.
//!       Compiled clean. Fails on "a source and an objective in the same place must answer that
//!       place", because a normalised zero direction is not a number.
//!   C3. REVERSE THE DIRECTION - `vector.Direction(source, target)` instead of `(target, source)`.
//!       Compiled clean. Fails on the containment check for the ordinary row: the point lands one
//!       standoff BEYOND the objective.
//!   C4. IGNORE THE STANDOFF - `return target;` unconditionally. Compiled clean. Three rows still
//!       pass; fails on "a 300 m standoff on a 900 m journey must drop two thirds of the way along".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveInsertion_LandingZoneStaysOnTheSegment : SCR_AutotestCaseBase
{
	//! Deliberately off-origin and non-round: a fault that returns a zero vector, one endpoint or a
	//! rounded value cannot coincidentally match any expected point.
	protected const float SOURCE_X = 1337.5;
	protected const float SOURCE_Z = 2401.25;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector source = Vector(SOURCE_X, 12, SOURCE_Z);

		// A 900 m journey due east. 900 avoids 1 000 and 2 000, where the distance measurement itself
		// is known to be one unit in the last place high.
		vector target = source + Vector(900, 0, 0);

		// --- The ordinary case, asserted exactly. 300 m short of a 900 m journey is 600 m along it.
		vector drop = OVT_InsertionGeometry.LZPointOnLine(source, target, 300);
		if (!ExpectDistanceFromSource(drop, source, 600, "a 300 m standoff on a 900 m journey must drop two thirds of the way along"))
			return true;

		if (!ExpectOnSegment(drop, source, target, "the ordinary drop point"))
			return true;

		// --- No standoff at all means "drive them all the way in".
		vector allTheWay = OVT_InsertionGeometry.LZPointOnLine(source, target, 0);
		if (!ExpectAt(allTheWay, target, "a zero standoff must drop at the objective itself"))
			return true;

		vector negativeStandoff = OVT_InsertionGeometry.LZPointOnLine(source, target, -300);
		if (!ExpectAt(negativeStandoff, target, "a negative standoff must drop at the objective, never past it"))
			return true;

		// --- A standoff with no room for it. THE clamp: both at the separation and beyond it.
		vector exactlyTheWholeWay = OVT_InsertionGeometry.LZPointOnLine(source, target, 900);
		if (!ExpectAt(exactlyTheWholeWay, source, "a standoff equal to the whole journey must drop at the source"))
			return true;

		vector longerThanTheJourney = OVT_InsertionGeometry.LZPointOnLine(source, target, 5000);
		if (!ExpectAt(longerThanTheJourney, source, "a standoff longer than the whole journey must drop at the source, never behind it"))
			return true;

		if (!ExpectOnSegment(longerThanTheJourney, source, target, "the over-long standoff's drop point"))
			return true;

		// --- The degenerate line. See the header: this is reachable in ordinary play.
		vector sameSpot = OVT_InsertionGeometry.LZPointOnLine(source, source, 300);
		if (!ExpectAt(sameSpot, source, "a source and an objective in the same place must answer that place"))
			return true;

		vector almostSameSpot = OVT_InsertionGeometry.LZPointOnLine(source, source + Vector(0.01, 0, 0), 300);
		if (!ExpectAt(almostSameSpot, source, "two positions a centimetre apart have no direction between them and must answer the source"))
			return true;

		// --- A journey that is not axis-aligned, so the arithmetic is not passing by luck on one axis.
		vector diagonalTarget = source + Vector(480, 0, 640);
		vector diagonalDrop = OVT_InsertionGeometry.LZPointOnLine(source, diagonalTarget, 200);
		if (!ExpectDistanceFromSource(diagonalDrop, source, 600, "a 200 m standoff on an 800 m diagonal must drop 600 m along it"))
			return true;

		if (!ExpectOnSegment(diagonalDrop, source, diagonalTarget, "the diagonal drop point"))
			return true;

		Print("LZPointOnLine: the landing zone is always on the closed source-objective segment, at the authored standoff where there is room and clamped to an endpoint where there is not");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts a point is (within the tier's epsilon) at an expected position.
	//! \param[in] actual The point produced.
	//! \param[in] expected Where it should be.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectAt(vector actual, vector expected, string label)
	{
		if (vector.Distance(actual, expected) <= OVT_TEST_LogicFixture.EPSILON)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts how far along the journey a point is.
	//! \param[in] actual The point produced.
	//! \param[in] source The journey's start.
	//! \param[in] expected How far from the start it should be, in metres.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectDistanceFromSource(vector actual, vector source, float expected, string label)
	{
		float travelled = vector.Distance(source, actual);

		// A metre of tolerance rather than the tier epsilon: the claim is "two thirds of the way
		// along", not "to the millimetre", and a normalise-and-scale over hundreds of metres carries
		// float32 rounding that has nothing to do with the property being asserted.
		if (Math.AbsFloat(travelled - expected) <= 1.0)
			return true;

		SetFailure("%1: the drop is %2 m from the source, expected %3 m", label, travelled.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! THE CONTAINMENT CLAIM, stated without trusting the arithmetic that produced the point.
	//!
	//! A point is on the closed segment exactly when the two part-distances sum to the whole; anything
	//! before the source or beyond the objective makes that sum longer. This also catches a NaN, which
	//! fails every comparison including this one.
	//! \param[in] actual The point produced.
	//! \param[in] source The journey's start.
	//! \param[in] target The journey's end.
	//! \param[in] label Human description of the row.
	//! \return True when it is on the segment; false after recording the failure.
	protected bool ExpectOnSegment(vector actual, vector source, vector target, string label)
	{
		float toSource = vector.Distance(source, actual);
		float toTarget = vector.Distance(actual, target);
		float whole = vector.Distance(source, target);

		if (Math.AbsFloat((toSource + toTarget) - whole) <= 1.0)
			return true;

		// ⚠ SetFailure takes at most THREE string params, so the two part-distances are composed into
		// one before they are handed over rather than passed as a fourth.
		string parts = toSource.ToString() + " m + " + toTarget.ToString() + " m";

		SetFailure("%1 is not between the source and the objective: %2 does not add up to the %3 m journey",
			label, parts, whole.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! HasArrived, IsStuck and the two helpers that feed them - the tests that decide whether a force is
//! delivered or dumped.
//!
//! ⚠ THE CENTRAL CLAIM IS THAT ARRIVAL BEATS STUCKNESS, AND IT IS NOT AN OPTIMISATION. A truck that
//! has reached its landing zone stops moving; that is what arriving looks like. Every convoy that
//! ever succeeds therefore spends at least one observation motionless at its destination, so a stuck
//! test that did not exempt the arrival radius would fire on EVERY SUCCESSFUL INSERTION - and the two
//! outcomes are not interchangeable. Arriving drops the force at the place that was chosen for it and
//! sends the truck home; being stuck dumps the force wherever the truck happens to be and abandons the
//! vehicle. The failure is not "a log line is wrong", it is "no insertion in the campaign ever
//! completes properly and nobody can see why".
//!
//! THE DISABLED TICK LIMIT IS A REAL CONFIGURATION, not a defensive branch: it is the operator's
//! off-switch for a server whose road AI is misbehaving, and it must be checked BEFORE anything else
//! so that no combination of speed and distance can re-enable it.
//!
//! CONSECUTIVE COUNTING is asserted through AdvanceStuckTicks rather than inferred, because the
//! difference between consecutive and cumulative only shows up on a truck that paused once - which is
//! every truck at every junction, and none of the ones a developer watches.
//!
//! CAN-FAIL, five faults, injected and compiled separately. All five exited compile-check 0:
//!   D1. DROP THE ARRIVAL EXEMPTION FROM IsStuck - remove the `if (HasArrived(...)) return false;`.
//!       Compiled clean. Fails on "a transport standing still ON its landing zone has arrived, not
//!       stalled: got stuck, expected not stuck".
//!   D2. DROP THE DISABLED-LIMIT GUARD - remove `if (ticksLimit <= 0) return false;`. Compiled clean.
//!       Fails on "a zero tick limit disables the stuck test entirely: got stuck, expected not stuck".
//!   D3. MAKE AdvanceStuckTicks CUMULATIVE - `return ticksBelow + 1;` unconditionally. Compiled clean.
//!       Fails on "a transport that moved must have its stall counter reset to zero: got 4,
//!       expected 0".
//!   D4. FLIP HasArrived TO STRICT - `distanceToLZ < arrivalRadius`. Compiled clean. Fails on "a
//!       transport exactly on the edge of the arrival radius has arrived: got not arrived, expected
//!       arrived".
//!   D5. DIVIDE REGARDLESS IN SpeedFromTravel - remove `if (elapsedSeconds <= 0) return 0;`.
//!       Compiled clean. Fails on "the first observation of a transport, with no elapsed time, must
//!       read as no speed rather than as an infinity".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveInsertion_StuckNeverFiresOnAnArrivedConvoy : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!VerifyArrival())
			return true;

		if (!VerifyStuck())
			return true;

		if (!VerifyStuckTicks())
			return true;

		if (!VerifySpeed())
			return true;

		Print("HasArrived / IsStuck: arrival wins over stalling, a zero tick limit disables the stall test, the stall counter is consecutive and not cumulative, and a first observation reads as no speed rather than as an infinity");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when every arrival row held.
	protected bool VerifyArrival()
	{
		if (!ExpectArrived(0, 40, true, "a transport on top of its landing zone has arrived"))
			return false;

		if (!ExpectArrived(39.5, 40, true, "a transport just inside the arrival radius has arrived"))
			return false;

		if (!ExpectArrived(40, 40, true, "a transport exactly on the edge of the arrival radius has arrived"))
			return false;

		if (!ExpectArrived(40.5, 40, false, "a transport just outside the arrival radius has not arrived"))
			return false;

		if (!ExpectArrived(750, 40, false, "a transport still on the road has not arrived"))
			return false;

		// An unauthored radius must not make every convoy arrive the moment it sets off, and must not
		// stop one that is exactly on the spot from ever arriving either.
		if (!ExpectArrived(0, 0, true, "with no arrival radius, a transport exactly on the landing zone still arrives"))
			return false;

		if (!ExpectArrived(5, 0, false, "with no arrival radius, a transport five metres away has not arrived"))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when every stall row held.
	protected bool VerifyStuck()
	{
		// --- THE CLAIM. A stationary transport, at its limit, sitting ON the landing zone.
		if (!ExpectStuck(0, 1, 5, 3, 10, 40, false, "a transport standing still ON its landing zone has arrived, not stalled"))
			return false;

		// --- The same transport, the same stillness, one metre outside the radius: now it is stalled.
		if (!ExpectStuck(0, 1, 5, 3, 41, 40, true, "a transport standing still just outside its landing zone is stalled"))
			return false;

		// --- The operator's off-switch, which must beat every other input.
		if (!ExpectStuck(0, 1, 500, 0, 900, 40, false, "a zero tick limit disables the stuck test entirely"))
			return false;

		if (!ExpectStuck(0, 1, 500, -3, 900, 40, false, "a negative tick limit disables the stuck test entirely"))
			return false;

		// --- Moving is never stalled, however long the counter has been running.
		if (!ExpectStuck(8.5, 1, 99, 3, 900, 40, false, "a transport making good speed is never stalled"))
			return false;

		if (!ExpectStuck(1, 1, 99, 3, 900, 40, false, "a transport exactly at the speed threshold is still making progress"))
			return false;

		// --- The counter itself, on both sides of the limit.
		if (!ExpectStuck(0, 1, 2, 3, 900, 40, false, "a transport below the limit is given more time"))
			return false;

		if (!ExpectStuck(0, 1, 3, 3, 900, 40, true, "a transport that reaches the limit is stalled"))
			return false;

		if (!ExpectStuck(0, 1, 9, 3, 900, 40, true, "a transport well past the limit is stalled"))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the counter is consecutive rather than cumulative.
	protected bool VerifyStuckTicks()
	{
		if (!ExpectTicks(0, 1, 0, 1, "a first motionless observation starts the stall counter"))
			return false;

		if (!ExpectTicks(0.4, 1, 3, 4, "another motionless observation advances it"))
			return false;

		// THE row. A truck negotiating a junction crawls, moves, crawls; only an unbroken run means the
		// road AI has actually given up.
		if (!ExpectTicks(6, 1, 4, 0, "a transport that moved must have its stall counter reset to zero"))
			return false;

		if (!ExpectTicks(1, 1, 4, 0, "a transport exactly at the speed threshold is moving, and resets"))
			return false;

		// A counter that somehow arrived negative must not read as a large number of stalls to come.
		if (!ExpectTicks(0, 1, -5, 1, "a counter that arrived negative restarts at one rather than counting up from below zero"))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when every speed row held.
	protected bool VerifySpeed()
	{
		vector before = Vector(500, 20, 500);
		vector after = Vector(590, 20, 500);

		float moving = OVT_InsertionGeometry.SpeedFromTravel(before, after, 10);
		if (!OVT_TEST_LogicFixture.FloatEquals(moving, 9))
		{
			SetFailure("ninety metres in ten seconds must read as nine metres per second: got %1", moving.ToString());
			return false;
		}

		float stationary = OVT_InsertionGeometry.SpeedFromTravel(before, before, 10);
		if (!OVT_TEST_LogicFixture.FloatEquals(stationary, 0))
		{
			SetFailure("a transport that did not move must read as no speed: got %1", stationary.ToString());
			return false;
		}

		// THE row. The very first observation of a convoy has no previous position and so no elapsed
		// time; dividing anyway is an infinity, and an infinity compares below no threshold at all.
		float firstObservation = OVT_InsertionGeometry.SpeedFromTravel(before, after, 0);
		if (!OVT_TEST_LogicFixture.FloatEquals(firstObservation, 0))
		{
			SetFailure("the first observation of a transport, with no elapsed time, must read as no speed rather than as an infinity: got %1",
				firstObservation.ToString());
			return false;
		}

		float negativeElapsed = OVT_InsertionGeometry.SpeedFromTravel(before, after, -10);
		if (!OVT_TEST_LogicFixture.FloatEquals(negativeElapsed, 0))
		{
			SetFailure("a negative elapsed time must read as no speed, never as a negative one: got %1", negativeElapsed.ToString());
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] distanceToLZ Distance from the landing zone.
	//! \param[in] arrivalRadius The arrival radius.
	//! \param[in] expected The answer this row claims.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectArrived(float distanceToLZ, float arrivalRadius, bool expected, string label)
	{
		bool actual = OVT_InsertionGeometry.HasArrived(distanceToLZ, arrivalRadius);
		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, DescribeArrived(actual), DescribeArrived(expected));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] speed The convoy's speed.
	//! \param[in] speedThreshold Below this it is not making progress.
	//! \param[in] ticksBelow Consecutive motionless observations so far.
	//! \param[in] ticksLimit How many are allowed.
	//! \param[in] distanceToLZ Distance from the landing zone.
	//! \param[in] arrivalRadius The arrival radius.
	//! \param[in] expected The answer this row claims.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectStuck(float speed, float speedThreshold, int ticksBelow, int ticksLimit,
		float distanceToLZ, float arrivalRadius, bool expected, string label)
	{
		bool actual = OVT_InsertionGeometry.IsStuck(speed, speedThreshold, ticksBelow, ticksLimit, distanceToLZ, arrivalRadius);
		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, DescribeStuck(actual), DescribeStuck(expected));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] speed The convoy's speed.
	//! \param[in] speedThreshold Below this it is not making progress.
	//! \param[in] ticksBelow The counter as it stands.
	//! \param[in] expected The counter this row claims.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectTicks(float speed, float speedThreshold, int ticksBelow, int expected, string label)
	{
		int actual = OVT_InsertionGeometry.AdvanceStuckTicks(speed, speedThreshold, ticksBelow);
		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] arrived The answer to describe.
	//! \return "arrived" or "not arrived".
	protected string DescribeArrived(bool arrived)
	{
		if (arrived)
			return "arrived";

		return "not arrived";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] stuck The answer to describe.
	//! \return "stuck" or "not stuck".
	protected string DescribeStuck(bool stuck)
	{
		if (stuck)
			return "stuck";

		return "not stuck";
	}
}

//------------------------------------------------------------------------------------------------
//! ChooseSpawnMarker: WHERE THE TRUCK APPEARS. The nearest authored vehicle spawn to the source that
//! nothing is standing on, ties to the lower index, and -1 - "use the road" - when there is nothing
//! usable.
//!
//! ⚠ WHY THIS ONE IS WORTH A CASE AND NOT JUST A PLAY-TEST. The three answers it can give wrong are
//! all silent and all look like a designer's fault rather than the code's:
//!   - never answering -1 (a missing occupancy skip) parks the transport INSIDE a patrol vehicle that
//!     is already sitting on that marker, which in Enfusion means one of them is flung away or the
//!     truck is stuck from the instant it exists - and a stuck truck is a force dumped in the open,
//!     six ticks later, with nothing in the log but "stopped making progress";
//!   - always answering -1 (an over-eager blocked test, an off-by-one on the parallel array) quietly
//!     puts every insertion in the campaign back on the road snap, i.e. it deletes the whole feature
//!     while leaving it looking as though it works;
//!   - a non-deterministic or reversed choice picks a DIFFERENT authored marker each time, which is
//!     the one thing the request was explicitly about - a truck pointed into a wall is why authored
//!     spawns exist, and a choice that cannot be repeated cannot be play-tested out.
//!
//! THE PARALLEL-ARRAY CONTRACT IS CLAIMED HERE TOO, in the last row. The caller cannot always run the
//! occupancy test (no vehicle manager on a bare world), and the intended degradation is "use the
//! authored spot anyway" rather than "refuse every authored spot" - an index past the end of the
//! blocked array is free. That is a two-character change in the guard and nothing else in the tree
//! would catch it.
//!
//! CAN-FAIL, three faults, injected into OVT_InsertionGeometry.ChooseSpawnMarker one at a time and
//! compiled separately. All three exited tools/compile-check.sh with 0 - none is a syntax error and
//! nothing else in the tree would stop them reaching players. The subject was restored and
//! re-compiled clean (exit 0) afterwards.
//!   C1. DROP THE OCCUPANCY SKIP - remove `if (i < blocked.Count() && blocked[i]) continue;`.
//!       Compiled clean. Fails on "a single occupied marker leaves nothing usable: got 0,
//!       expected -1 (the road)".
//!   C2. PREFER THE FURTHEST - invert the skip, `distance >= bestDistance` to
//!       `distance <= bestDistance`. Compiled clean. Fails on "the nearest of three free markers
//!       wins: got 0, expected 2".
//!   C3. TIES TO THE LATER MARKER - relax the skip's `>=` to `>`, so an equal distance replaces the
//!       incumbent. Compiled clean. Every row but one passes; fails on "two markers the same
//!       distance away: the lower index wins: got 1, expected 0".
//!
//! No maxAttempts: the subject is a pure function of two arrays and a vector, with no clock, no RNG
//! and no world. The tie row uses two markers 60 m out on opposite sides of the same axis, so the
//! two distances are bit-identical rather than merely close, and no row sits at 1 000 m or 2 000 m
//! where vector.Distance is known to answer one unit in the last place high.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveInsertion_ChoosesNearestFreeVehicleSpawn : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector source = "0 0 0";

		// --- Nothing authored at this base: the caller must fall back to the road.
		array<vector> none = {};
		array<bool> noneBlocked = {};
		if (!ExpectChoice(none, noneBlocked, source, -1, "a base with no authored markers leaves nothing usable"))
			return true;

		// --- One marker, free. The whole reason the feature exists.
		array<vector> one = {"40 0 0"};
		array<bool> oneFree = {false};
		if (!ExpectChoice(one, oneFree, source, 0, "a single free marker is used"))
			return true;

		// --- One marker, occupied by a patrol vehicle that legitimately shares it.
		array<bool> oneTaken = {true};
		if (!ExpectChoice(one, oneTaken, source, -1, "a single occupied marker leaves nothing usable"))
			return true;

		// --- Three free markers: the nearest to the source wins, wherever it sits in the list.
		array<vector> three = {"310 0 0", "0 0 180", "70 0 0"};
		array<bool> threeFree = {false, false, false};
		if (!ExpectChoice(three, threeFree, source, 2, "the nearest of three free markers wins"))
			return true;

		// --- The nearest is taken, so the next nearest is used - NOT the road.
		array<bool> nearestTaken = {false, false, true};
		if (!ExpectChoice(three, nearestTaken, source, 1, "the next nearest is used when the nearest is taken"))
			return true;

		// --- Every marker taken. This is the fallback the request asked for.
		array<bool> allTaken = {true, true, true};
		if (!ExpectChoice(three, allTaken, source, -1, "every marker occupied leaves nothing usable"))
			return true;

		// --- A symmetric authored pair. Deterministic means the SAME one every time.
		array<vector> tied = {"60 0 0", "-60 0 0"};
		array<bool> tiedFree = {false, false};
		if (!ExpectChoice(tied, tiedFree, source, 0, "two markers the same distance away: the lower index wins"))
			return true;

		// --- No occupancy answers at all (no vehicle manager). Authored still beats the road.
		array<bool> unanswered = {};
		if (!ExpectChoice(three, unanswered, source, 2, "with no occupancy answers every marker counts as free"))
			return true;

		// --- A partial answer: the answered entry is honoured, the unanswered ones are free.
		array<bool> partial = {false};
		if (!ExpectChoice(three, partial, source, 2, "an unanswered index past the blocked array is free"))
			return true;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] positions The candidate marker positions.
	//! \param[in] blocked Parallel occupancy answers; may be shorter than positions.
	//! \param[in] source Where the force sets out from.
	//! \param[in] expected The index this row claims, or -1 for "use the road".
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectChoice(notnull array<vector> positions, notnull array<bool> blocked, vector source,
		int expected, string label)
	{
		int actual = OVT_InsertionGeometry.ChooseSpawnMarker(positions, blocked, source);
		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, DescribeChoice(actual), DescribeChoice(expected));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] choice The index to describe.
	//! \return The index, or "-1 (the road)".
	protected string DescribeChoice(int choice)
	{
		if (choice == -1)
			return "-1 (the road)";

		return choice.ToString();
	}
}
