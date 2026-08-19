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
//!                     stuck; always true means a force dumped at the base it set out from. Arriving
//!                     the instant the radius is entered - before the transport has stopped - drops
//!                     men out of a braking truck and injures the force it just delivered.
//!   IsSettleGraceExpired
//!                     the bound on waiting for that stop. Never expiring is an insertion that never
//!                     delivers at all, which is worse than any rough drop: the men exist, they cost
//!                     resources, and they never appear.
//!   IsStuck           the arrival test inside it is the load-bearing part: every convoy that ever
//!                     succeeds stands still on its landing zone for at least one tick, so without
//!                     it EVERY successful insertion also reports as stranded and dumps its men.
//!   AdvanceStuckTicks cumulative instead of consecutive counting condemns any truck that ever
//!                     paused at a junction.
//!   SpeedFromTravel   dividing by a zero elapsed time on the first observation is the difference
//!                     between a number and an infinity.
//!   IsAbandonedTruckCollectable
//!                     collecting too eagerly deletes a transport in front of the player walking
//!                     towards it; never collecting at all silts a route up with every truck it ever
//!                     stranded, which is what the forward base's never-ending deployment did.
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
//! ⚠ ARRIVING IS PLACE AND STILLNESS, AND THE STILLNESS HALF IS NOT COSMETIC. A passenger is teleported
//! out of a vehicle carrying that vehicle's velocity, so opening the doors on the first tick the truck
//! is inside the radius - which is the tick it is braking hardest - throws the force across the road and
//! injures it (user play-test, 2026-08-19, after the transport prefab's friction was raised). The rows
//! below claim both halves independently: stopped-but-far is not arrived, and close-but-moving is not
//! arrived either.
//!
//! ⚠ AND THE WAIT FOR THAT STILLNESS IS BOUNDED, WHICH IS THE ROW THAT MATTERS MOST IN THIS FILE. A
//! speed condition with no deadline is an insertion that hangs on a slope, on a nudge or on plain
//! physics jitter, with its force still in the truck, forever - a failure with no log line and no
//! symptom except reinforcements that never come. IsSettleGraceExpired is that deadline, it spends the
//! stall test's own tick budget rather than a second clock, and a DISABLED budget means "do not wait at
//! all" rather than "wait forever" - which is the direction the off-switch has to fail in, and is
//! asserted directly.
//!
//! ⚠ THE STALL TEST'S EXEMPTION IS THE RADIUS, NOT THE ARRIVAL TEST, and one row exists purely to hold
//! that line. Once arriving requires stillness, an exemption written in terms of arriving would stop
//! exempting a truck that is inside the radius and still creeping - and a creeping truck trips the
//! stationary-for-N-ticks counter, so the settling transport would be called stranded and dump its men
//! at the very drop point it had already reached. The exemption must therefore be about WHERE the truck
//! is and nothing else.
//!
//! CAN-FAIL, nine faults, injected and compiled separately. All nine exited compile-check 0:
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
//!   D6. DROP THE SPEED CONDITION FROM HasArrived - `return IsInsideArrivalRadius(...)`, i.e. the
//!       distance-only test this function used to be. Compiled clean. Fails on "a transport braking
//!       hard into its landing zone has not arrived yet: got arrived, expected not arrived", which is
//!       the injury bug itself.
//!   D7. DROP HasArrived's SPEED OFF-SWITCH - remove `if (settleSpeedThreshold <= 0) return true;`.
//!       Compiled clean. Fails on "with the speed condition disabled, a transport inside the radius
//!       arrives however fast it is going: got not arrived, expected arrived" - the caller that passes
//!       no threshold (the return leg) would silently stop arriving.
//!   D8. MAKE A DISABLED SETTLE BUDGET WAIT FOREVER - `if (graceTicks <= 0) return false;` in
//!       IsSettleGraceExpired. Compiled clean. Fails on "a disabled tick budget means no settling wait
//!       at all, never an unbounded one: got still settling, expected grace expired". This is the
//!       hang, and it is the fault the whole function exists to make impossible.
//!   D9. MAKE THE STALL EXEMPTION SPEED-AWARE - `if (HasArrived(distanceToLZ, arrivalRadius, speed,
//!       0.5)) return false;` in IsStuck, which reads as the tighter and therefore safer test and is
//!       neither. Compiled clean. Fails on "a transport still creeping ON its landing zone is settling,
//!       not stalled: got stuck, expected not stuck".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveInsertion_StuckNeverFiresOnAnArrivedConvoy : SCR_AutotestCaseBase
{
	//! The settling speed the rows below are written around, in m/s. Mirrors the module's shipped
	//! OVT_InsertionSpawningDeploymentModule.ARRIVAL_SETTLE_SPEED_MS rather than reading it, because the
	//! subject is the pure function and it takes the threshold as an argument: these rows must go on
	//! claiming what `<=` means at a boundary even if the module retunes what it passes.
	static const float SETTLE_SPEED = 0.5;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!VerifyArrival())
			return true;

		if (!VerifySettleGrace())
			return true;

		if (!VerifyStuck())
			return true;

		if (!VerifyStuckTicks())
			return true;

		if (!VerifySpeed())
			return true;

		Print("HasArrived / IsSettleGraceExpired / IsStuck: arriving needs the transport to be stopped as well as close, the wait for it to stop is bounded by the stall tick budget and a disabled budget never waits, being at the landing zone wins over stalling, the stall counter is consecutive and not cumulative, and a first observation reads as no speed rather than as an infinity");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when every arrival row held.
	protected bool VerifyArrival()
	{
		// --- THE PLACE HALF, on a transport that has already stopped. These are the rows that predate
		// the speed condition and every one of them still claims exactly what it claimed before.
		if (!ExpectArrived(0, 40, 0, SETTLE_SPEED, true, "a transport stopped on top of its landing zone has arrived"))
			return false;

		if (!ExpectArrived(39.5, 40, 0, SETTLE_SPEED, true, "a stopped transport just inside the arrival radius has arrived"))
			return false;

		if (!ExpectArrived(40, 40, 0, SETTLE_SPEED, true, "a stopped transport exactly on the edge of the arrival radius has arrived"))
			return false;

		if (!ExpectArrived(40.5, 40, 0, SETTLE_SPEED, false, "a stopped transport just outside the arrival radius has not arrived"))
			return false;

		if (!ExpectArrived(750, 40, 0, SETTLE_SPEED, false, "a transport still on the road has not arrived"))
			return false;

		// An unauthored radius must not make every convoy arrive the moment it sets off, and must not
		// stop one that is exactly on the spot from ever arriving either.
		if (!ExpectArrived(0, 0, 0, SETTLE_SPEED, true, "with no arrival radius, a stopped transport exactly on the landing zone still arrives"))
			return false;

		if (!ExpectArrived(5, 0, 0, SETTLE_SPEED, false, "with no arrival radius, a transport five metres away has not arrived"))
			return false;

		// --- THE STILLNESS HALF, all of it well inside the radius so only the speed can decide. THE row
		// is the first one: that is a transport braking into its drop point, and answering "arrived" to
		// it is the bug this condition exists for - the men come out of a moving vehicle with its
		// velocity on them.
		if (!ExpectArrived(12, 40, 7.5, SETTLE_SPEED, false, "a transport braking hard into its landing zone has not arrived yet"))
			return false;

		if (!ExpectArrived(12, 40, 1.4, SETTLE_SPEED, false, "a transport still rolling at walking pace has not arrived yet"))
			return false;

		if (!ExpectArrived(12, 40, 0.55, SETTLE_SPEED, false, "a transport just above the settling speed has not arrived yet"))
			return false;

		// The boundary in both directions - at a settling speed expressed as a bool, `<=` and `<` differ.
		if (!ExpectArrived(12, 40, SETTLE_SPEED, SETTLE_SPEED, true, "a transport exactly at the settling speed has arrived"))
			return false;

		if (!ExpectArrived(12, 40, 0.2, SETTLE_SPEED, true, "a transport shuffling below the settling speed has arrived"))
			return false;

		// Stillness NEVER substitutes for place. A transport parked halfway down the road has not
		// arrived however long it has been parked there - that case belongs to the stall test.
		if (!ExpectArrived(750, 40, 0, SETTLE_SPEED, false, "a transport stopped nine hundred metres short has not arrived, it has stalled"))
			return false;

		// --- THE OFF-SWITCH. A non-positive threshold is the distance-only test, which is what the
		// return leg passes: an empty truck rolling into its own base at speed has got home, because
		// nobody is being put on the ground there.
		if (!ExpectArrived(12, 40, 12, 0, true, "with the speed condition disabled, a transport inside the radius arrives however fast it is going"))
			return false;

		if (!ExpectArrived(12, 40, 12, -1, true, "a negative speed threshold disables the speed condition as well"))
			return false;

		// A speed can only arrive at this function through SpeedFromTravel, which never answers below
		// zero - but a negative one must read as stopped rather than as a transport that can never
		// settle, for the same reason every other input in this file is clamped.
		if (!ExpectArrived(12, 40, -3, SETTLE_SPEED, true, "a negative speed reads as stopped rather than as never settling"))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! THE BOUND ON THE WAIT. See the case header: without it the speed condition above is an insertion
	//! that can hang with its force still in the truck.
	//! \return True when every settle-grace row held.
	protected bool VerifySettleGrace()
	{
		// --- THE CLAIM THAT MAKES THE WAIT SAFE. A disabled tick budget - the operator's stall off-switch
		// - must mean "do not wait", not "wait forever". Getting this backwards is the hang.
		if (!ExpectSettleExpired(1, 0, true, "a disabled tick budget means no settling wait at all, never an unbounded one"))
			return false;

		if (!ExpectSettleExpired(1, -4, true, "a negative tick budget means no settling wait either"))
			return false;

		// --- The ordinary budget, on both sides of its limit.
		if (!ExpectSettleExpired(1, 6, false, "a transport on its first tick at the landing zone is given time to stop"))
			return false;

		if (!ExpectSettleExpired(5, 6, false, "a transport one tick short of the budget is still given time to stop"))
			return false;

		if (!ExpectSettleExpired(6, 6, true, "a transport that has spent the whole budget without stopping is put down anyway"))
			return false;

		if (!ExpectSettleExpired(30, 6, true, "a transport well past the budget is long overdue to be put down"))
			return false;

		// A counter that somehow arrived negative must not read as a wait already served.
		if (!ExpectSettleExpired(-5, 6, false, "a counter that arrived negative has not served the budget"))
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

		// --- THE ROW THAT HOLDS THE EXEMPTION TO THE RADIUS. This transport is ON its landing zone and
		// still creeping: below the stall test's own speed threshold, so its stall counter is running and
		// at the limit, but ABOVE the settling speed, so it has not arrived yet either. It is a truck
		// coming to a stop at its drop point, and it must not be called stranded - if it were, the settle
		// wait would end by dumping the force and abandoning the transport at the very place both were
		// supposed to arrive. An exemption written as "has arrived" instead of "is inside the radius"
		// fails exactly here and nowhere else.
		if (!ExpectStuck(0.9, 1, 5, 3, 10, 40, false, "a transport still creeping ON its landing zone is settling, not stalled"))
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
	//! \param[in] speed How fast the transport is going, in m/s.
	//! \param[in] settleSpeedThreshold At or below this it counts as stopped; non-positive disables.
	//! \param[in] expected The answer this row claims.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectArrived(float distanceToLZ, float arrivalRadius, float speed, float settleSpeedThreshold,
		bool expected, string label)
	{
		bool actual = OVT_InsertionGeometry.HasArrived(distanceToLZ, arrivalRadius, speed, settleSpeedThreshold);
		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, DescribeArrived(actual), DescribeArrived(expected));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] ticksInsideRadius Consecutive ticks the transport has been at the landing zone.
	//! \param[in] graceTicks The budget it may spend settling.
	//! \param[in] expected The answer this row claims.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectSettleExpired(int ticksInsideRadius, int graceTicks, bool expected, string label)
	{
		bool actual = OVT_InsertionGeometry.IsSettleGraceExpired(ticksInsideRadius, graceTicks);
		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, DescribeSettle(actual), DescribeSettle(expected));

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
	//! \param[in] expired The answer to describe.
	//! \return "grace expired" or "still settling".
	protected string DescribeSettle(bool expired)
	{
		if (expired)
			return "grace expired";

		return "still settling";
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

//------------------------------------------------------------------------------------------------
//! WHEN A TRANSPORT NOBODY IS COMING BACK FOR IS TAKEN AWAY.
//!
//! WHY THIS CASE EXISTS. The insertion module deliberately does NOT delete a stranded truck at the
//! moment it strands - "a stuck one is a landmark and a lootable, and it is released with everything
//! else when the deployment ends" - and that reasoning is sound right up until the deployment does not
//! end. The forward base's stands for as long as the base does, so on the one route that reliably
//! strands trucks they piled up with nothing ever collecting them (user play-test, 2026-08-19). The
//! deadline below is what turns "never" into "eventually", and every one of its three inputs is a
//! different way to get it wrong:
//!
//!   THE DEADLINE ITSELF     too eager and the landmark is gone before anyone can walk to it; the
//!                           whole reason the truck was left standing is thrown away and the only
//!                           symptom is that players stop finding abandoned enemy transports.
//!   THE PROXIMITY HOLD      the one that must never fail open. A truck deleted in front of somebody
//!                           looting it, fighting over it or driving towards it is an object vanishing
//!                           on screen, which reads as a bug in a way that "it is still there" never
//!                           does. It is an absolute hold rather than a delay: it must not expire.
//!   THE OFF-SWITCH          a non-positive limit is the old behaviour, written down. Getting it
//!                           backwards - collecting IMMEDIATELY when the limit is zero - is the worst
//!                           available failure, because zero is what an unset field holds.
//!
//! ⚠ WHAT THIS DOES **NOT** COVER, said plainly. Only the DECISION is pure. Whether the countdown is
//! armed on the right paths, whether it is disarmed when the vehicle goes away by other means, whether
//! the ownership veto still wins, and whether the collection actually deletes anything are all
//! properties of a live module holding a live Vehicle, and there is no honest way to assert them in
//! this tier. They are manual checks, and the module carries the argument for why the counter cannot
//! leak in place of a test.
//!
//! CAN-FAIL, three faults injected into OVT_InsertionGeometry.IsAbandonedTruckCollectable one at a time
//! and compiled separately. All three exited tools/compile-check.sh with 0 - none is a syntax error and
//! nothing else in the tree would stop them reaching players. The subject was restored and re-compiled
//! clean (exit 0) afterwards.
//!   D1. DROP THE PROXIMITY HOLD - remove `if (playerNearby) return false;`. Compiled clean. Fails on
//!       "a player standing next to an overdue transport holds it: got collect, expected keep".
//!   D2. STRICTLY PAST THE DEADLINE - `ticksSinceAbandoned > ticksLimit`. Compiled clean. Every row but
//!       one passes; fails on "a transport exactly at the deadline is collected: got keep, expected
//!       collect", which is the row the arming tick makes reachable in practice.
//!   D3. DROP THE OFF-SWITCH - remove `if (ticksLimit <= 0) return false;`. Compiled clean. A limit of
//!       zero then collects on the very first tick; fails on "a limit of zero never collects, however
//!       long the transport has stood: got collect, expected keep".
//!
//! No maxAttempts: the subject is a pure function of two ints and a bool, with no clock, no RNG and no
//! world.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveInsertion_CollectsAbandonedTransportsOnlyWhenDue : SCR_AutotestCaseBase
{
	//! The shipped budget, so the rows read against the real number rather than a made-up one. It is
	//! deliberately referenced rather than copied: raising the timeout must not silently move the
	//! boundary these rows claim.
	protected const int LIMIT = OVT_InsertionSpawningDeploymentModule.ABANDONED_TRUCK_TIMEOUT_TICKS;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The countdown, with nobody about.
		if (!Expect(0, LIMIT, false, "a transport abandoned this very tick is kept"))
			return true;

		if (!Expect(LIMIT - 1, LIMIT, false, "a transport one tick short of the deadline is kept"))
			return true;

		if (!Expect(LIMIT, LIMIT, true, "a transport exactly at the deadline is collected"))
			return true;

		if (!Expect(LIMIT + 40, LIMIT, true, "a transport well past the deadline is collected"))
			return true;

		// --- The proximity hold. See the header: this is the row that matters.
		if (!Expect(LIMIT, LIMIT, true, false, "a player standing next to an overdue transport holds it"))
			return true;

		if (!Expect(LIMIT * 9, LIMIT, true, false, "and the hold does not expire, however long he stays"))
			return true;

		if (!Expect(4, LIMIT, true, false, "a player near a transport that is not yet due changes nothing"))
			return true;

		// --- The off-switch, which is the pre-2026-08-19 behaviour written down. ⚠ Zero is what an
		//     unset budget holds, so "collect nothing" and "collect everything immediately" are the two
		//     readings and only one of them is safe.
		if (!Expect(LIMIT * 9, 0, false, "a limit of zero never collects, however long the transport has stood"))
			return true;

		if (!Expect(LIMIT * 9, -30, false, "and neither does a negative one"))
			return true;

		// --- A caller that has not counted anything yet, or has counted backwards. The subject takes bare
		//     ints and cannot check its caller.
		if (!Expect(-7, LIMIT, false, "a negative count never collects"))
			return true;

		Print("IsAbandonedTruckCollectable: an abandoned transport is collected once its tick budget is spent and never while a player is near it, and a non-positive budget collects nothing at all");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one row with nobody nearby, which is the ordinary case.
	//! \param[in] ticks Ticks since the transport was abandoned.
	//! \param[in] limit The tick budget.
	//! \param[in] expected Whether it must be collected.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool Expect(int ticks, int limit, bool expected, string label)
	{
		return Expect(ticks, limit, false, expected, label);
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one row.
	//! \param[in] ticks Ticks since the transport was abandoned.
	//! \param[in] limit The tick budget.
	//! \param[in] playerNearby Whether a live player is close enough to notice it go.
	//! \param[in] expected Whether it must be collected.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool Expect(int ticks, int limit, bool playerNearby, bool expected, string label)
	{
		bool actual = OVT_InsertionGeometry.IsAbandonedTruckCollectable(ticks, limit, playerNearby);
		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, DescribeOutcome(actual), DescribeOutcome(expected));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] collect The answer to describe.
	//! \return "collect" or "keep".
	protected string DescribeOutcome(bool collect)
	{
		if (collect)
			return "collect";

		return "keep";
	}
}
