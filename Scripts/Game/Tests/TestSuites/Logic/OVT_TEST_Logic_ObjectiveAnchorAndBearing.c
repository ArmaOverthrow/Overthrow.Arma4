//------------------------------------------------------------------------------------------------
//! TIER A cases - the objective ANCHOR: the arithmetic that leans the occupying faction's routine
//! deployment spending toward the place it has decided to take back.
//!
//! Every subject here is a static function of plain floats. Nothing in this file resolves a manager,
//! a controller or a world, and nothing needs to. See OVT_TEST_LogicSuite.c for the tier rule and the
//! house rules - including the reviewer grep over this directory, which does not distinguish code
//! from comments, so neither banned identifier appears anywhere below, prose included.
//!
//! WHY THIS FILE EXISTS, AND WHY IT IS WORTH ITS OWN ONE. occupying/counter-attacks Phase 3 put a
//! single call to OVT_ObjectiveSelection.ApplyAnchorBias() inside the deployment evaluator's
//! candidate loop - the loop that decides where every deployment in the campaign is created, and that
//! thirteen shipped configs already depend on. The whole safety argument for that edit is two claims
//! about this one function:
//!
//!   1. WITH NO ANCHOR IT IS THE IDENTITY. Not "close to", not "within an epsilon" - it returns the
//!      caller's own float, having performed no arithmetic on it. That is what makes a campaign with
//!      no objective, a faction nobody biases and every test world that never starts one sort exactly
//!      as they did before the feature existed.
//!   2. WITH AN ANCHOR IT IS BOUNDED BY weight AND NEVER SUBTRACTS. The most an objective can do is
//!      move comparable work up the order; it can never suppress a candidate, and it can never
//!      overtake a candidate whose own score is already better by more than weight.
//!
//! Neither claim is a compile error when it breaks, and neither shows up in a log. A falloff with the
//! wrong sign quietly inverts the feature - the occupying faction would garrison everywhere EXCEPT
//! the place it is attacking. A missing radius guard turns "no objective" into "an objective at the
//! origin with infinite reach", which pulls the whole campaign's spending to one corner of the map.
//! A bias that could subtract would let an objective make its own surroundings the LAST place worth
//! defending.
//!
//! THE BEARING HALF, added by Phase 8, is the second half of this file and starts below the three
//! anchor cases. It covers the compass arithmetic behind a battle wave's landing zone - which way the
//! bearing points and which convention it is expressed in - and it carries its own headline warning,
//! because a sign error there is the single most likely defect in that phase.
//!
//! ALSO NOT HERE, AND ON PURPOSE: that the evaluator writes the biased number into the candidate's
//! SORT KEY and not into its threat. That is a claim about a live component's fields rather than
//! about arithmetic, so it is pinned one tier up (OVT_TEST_Init_ObjectiveAnchor.c). It is the more
//! dangerous of the two halves - biasing the threat instead would change which configs a position may
//! BUY - so it is called out here so nobody reads this file as the whole story.
//!
//! CAN-FAIL PROOFS. Running a suite is the orchestrator's job, not an implementation agent's
//! (.claude/test-policy.md), so each proof below is a fault that was injected into the subject one at
//! a time and compiled - every one exited tools/compile-check.sh with 0, which is the point: none of
//! these are syntax errors and nothing else in the tree would stop them reaching players. The subject
//! was restored and re-compiled clean afterwards.
//!
//! No maxAttempts anywhere: every subject is a pure function with no clock, no RNG and no world, and
//! cannot flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! With no anchor, the bias is the identity function - and "identity" is meant literally.
//!
//! THREE WAYS TO HAVE NO ANCHOR, AND ALL THREE ARE REAL. A non-positive radius is what an unset
//! anchor looks like and what a caller says when it means "stop". A non-positive weight is the same
//! statement made the other way, and is also what a hand-built record holds before anything fills it
//! in. A candidate at or beyond the radius is the ordinary case: almost every candidate in a campaign
//! is outside the objective's band, so this branch is the one that runs millions of times.
//!
//! ⚠ THE RETURN IS COMPARED WITH ==, WHICH THIS TIER OTHERWISE FORBIDS, AND THAT IS THE POINT. An
//! epsilon comparison would pass for an implementation that returned score * 1.0 or score + 0.0 - and
//! those are not the same claim. "The evaluator sorts on the same float it sorted on before" is only
//! true if the function hands the argument straight back, so the assertion has to be exact. Every
//! other comparison in this file uses the tier's epsilon.
//!
//! ⚠ ONE BOUNDARY IS DELIBERATELY NOT CLAIMED, BECAUSE IT IS PROVABLY UNOBSERVABLE. Whether the range
//! test is `>=` or `>` cannot be detected from outside: at exactly the radius the falloff evaluates
//! to score + weight * (1 - 1), and adding an exact zero to a float returns that float bit for bit.
//! The guard is there to skip arithmetic, not to change an answer, and a case that pretended to pin
//! it would be asserting nothing. The rows on either side of the radius are still worth having - they
//! pin the SHAPE of the band - they are simply not a proof about that operator.
//!
//! CAN-FAIL, three faults, injected and compiled separately because the case makes three independent
//! claims and one fault must not stand in for another. All three exited tools/compile-check.sh 0:
//!   A1. DROP THE RADIUS GUARD - remove `if (radius <= 0) return score;`. Compiled clean (exit 0).
//!       ⚠ Only ONE row catches this, and finding that out is why the row exists: with the range test
//!       still in place, a non-positive radius is unreachable for any non-negative distance, so the
//!       obvious rows pass. It takes a distance more negative than the radius to reach the division -
//!       whereupon the clamped distance divided by a negative radius pays the FULL weight for a
//!       candidate nobody anchored. The case then fails on
//!       "a nonsense anchor must stay inert however the distance was measured: got 187.25, expected
//!       137.25".
//!   A2. DROP THE WEIGHT GUARD - remove `if (weight <= 0) return score;`. Compiled clean (exit 0).
//!       The case then fails on
//!       "a negative weight must not SUBTRACT from a candidate's score: got 87.25, expected 137.25".
//!   A3. NEVER BIAS AT ALL - replace the whole body with `return score;`. Compiled clean (exit 0).
//!       Every row above passes, which is exactly the failure mode this fault exists to expose, and
//!       the last check catches it:
//!       "a candidate one metre inside the radius must be biased, or every row above passes
//!       vacuously: got 137.25, expected more than 137.25".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveAnchor_AbsentAnchorIsTheIdentity : SCR_AutotestCaseBase
{
	//! An arbitrary but non-round candidate score, chosen so a fault that returns a constant, a zero
	//! or a rounded value cannot coincidentally match it.
	protected const float SCORE = 137.25;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: a non-positive radius is "no anchor".
		if (!ExpectUntouched(0, 0, 50, "a zero radius is what 'no anchor' looks like and must not bias"))
			return true;

		if (!ExpectUntouched(0, -600, 50, "a negative radius must not bias"))
			return true;

		// --- ...INCLUDING when the distance is itself nonsense. This is the only row that reaches the
		//     radius guard at all: with a non-negative distance the range test below already refuses
		//     every non-positive radius, so a missing radius guard hides behind it until a distance
		//     turns up that is more negative still.
		if (!ExpectUntouched(-700, -600, 50, "a nonsense anchor must stay inert however the distance was measured"))
			return true;

		// --- Claim 2: a non-positive weight is "no anchor", and above all must never subtract.
		if (!ExpectUntouched(0, 600, 0, "a zero weight must not bias"))
			return true;

		if (!ExpectUntouched(0, 600, -50, "a negative weight must not SUBTRACT from a candidate's score"))
			return true;

		// --- Claim 3: a candidate at or beyond the radius is outside the band. Both sides of the
		//     boundary, and the ordinary far-away case that most candidates take.
		if (!ExpectUntouched(600, 600, 50, "a candidate exactly at the radius is outside the band"))
			return true;

		if (!ExpectUntouched(600.5, 600, 50, "a candidate just past the radius is outside the band"))
			return true;

		if (!ExpectUntouched(50000, 600, 50, "a candidate on the far side of the map is outside the band"))
			return true;

		// --- ...and the other side of that same boundary really is biased, so the rows above are not
		//     passing because the function never biases anything at all.
		float justInside = OVT_ObjectiveSelection.ApplyAnchorBias(SCORE, 599, 600, 50);
		if (justInside <= SCORE)
		{
			SetFailure("a candidate one metre inside the radius must be biased, or every row above passes vacuously: got %1, expected more than %2",
				justInside.ToString(), SCORE.ToString());
			return true;
		}

		Print("ApplyAnchorBias: with no radius, no weight, or a candidate outside the band, the caller's score is handed back untouched - so a campaign with no objective sorts on exactly the number it sorted on before");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that one set of inputs leaves the score EXACTLY as it was.
	//! \param[in] distance Distance from the candidate to the anchor.
	//! \param[in] radius The anchor's radius.
	//! \param[in] weight The anchor's weight.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectUntouched(float distance, float radius, float weight, string label)
	{
		float actual = OVT_ObjectiveSelection.ApplyAnchorBias(SCORE, distance, radius, weight);
		if (actual == SCORE)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), SCORE.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! With an anchor, the bias is bounded above by weight, floored at the untouched score, and rises as
//! the candidate gets closer.
//!
//! WHY EACH BOUND MATTERS SEPARATELY:
//!   - BOUNDED ABOVE BY weight. This is the sentence the whole design rests on: "objective-adjacent
//!     work is bought first, but only up to weight". A bias that could exceed its weight would let an
//!     objective monopolise a faction's entire spending, which is what the per-faction ceiling and
//!     the per-pass cap exist to prevent and what a scoring bias must not route around.
//!   - NEVER BELOW THE INPUT. A bias that could subtract would make an objective's own surroundings
//!     the last place a faction reinforced - the exact inverse of the feature.
//!   - MONOTONIC IN DISTANCE. Without this the falloff could be anything: a step, a plateau, or a
//!     shape that peaks at the edge of the band rather than at the objective.
//!
//! THE EXACT MIDPOINT IS ASSERTED TOO, not just the shape, because "linear" is the property that
//! makes the number tunable by hand: half the radius pays half the weight, and a tuner who sets a
//! weight of 40 can predict what a candidate 300 m into a 600 m band is worth without reading code.
//!
//! A NEGATIVE DISTANCE IS A ROW BECAUSE THE FUNCTION IS PURE AND CANNOT CHECK ITS CALLER. It takes
//! four bare floats; nothing about the signature stops a future caller measuring a signed offset, a
//! difference of two distances, or a value that has been through a clamp of its own. The clamp inside
//! the function is what makes "the bias is bounded by weight" true for ANY input rather than only for
//! the inputs today's one call site happens to produce - without it a negative distance pays MORE
//! than the full weight, and the bound the whole design rests on is only a convention.
//!
//! CAN-FAIL, three faults, injected and compiled separately. All three exited compile-check 0:
//!   A4. INVERT THE FALLOFF - `weight * (distance / radius)` in place of `weight * (1 - distance /
//!       radius)`. Compiled clean (exit 0). The case then fails on its FIRST row,
//!       "a candidate ON the objective must pay the full weight: got 100, expected 140".
//!   A5. DROP THE NEGATIVE-DISTANCE CLAMP - remove the `if (distance < 0) distance = 0;` pair.
//!       Compiled clean (exit 0). The four rows above it pass; the case then fails on
//!       "a caller that hands in a negative distance must read as being ON the objective, never as
//!       being worth more than it: got 144, expected 140".
//!   A6. SQUARE THE FALLOFF - `weight * (1 - distance / radius) * (1 - distance / radius)`. Compiled
//!       clean (exit 0). ⚠ Both ends of the band still pay exactly the right amount and the walk is
//!       still monotonic and still bounded, so ONLY the interior rows catch it - which is why "half
//!       the radius pays half the weight" is asserted as a value and not merely as an inequality. The
//!       case fails on "half the radius pays half the weight: got 110, expected 120".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveAnchor_BiasIsBoundedAndFallsOffLinearly : SCR_AutotestCaseBase
{
	//! A candidate score to bias. Non-round for the same reason as the case above.
	protected const float SCORE = 100.0;

	//! The anchor under test: the harassment-phase reach and a weight that is easy to halve by eye.
	protected const float RADIUS = 600.0;
	protected const float WEIGHT = 40.0;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The two ends of the band, exactly.
		if (!ExpectBias(0, SCORE + WEIGHT, "a candidate ON the objective must pay the full weight"))
			return true;

		if (!ExpectBias(RADIUS, SCORE, "a candidate at the far edge of the band pays nothing"))
			return true;

		// --- Linear in between, which is what makes the weight tunable by hand.
		if (!ExpectBias(RADIUS * 0.5, SCORE + (WEIGHT * 0.5), "half the radius pays half the weight"))
			return true;

		if (!ExpectBias(RADIUS * 0.25, SCORE + (WEIGHT * 0.75), "a quarter of the radius pays three quarters of the weight"))
			return true;

		// --- A negative distance reads as "on the objective", never as worth more than being on it.
		if (!ExpectBias(-60, SCORE + WEIGHT, "a caller that hands in a negative distance must read as being ON the objective, never as being worth more than it"))
			return true;

		// --- Monotonic, and bounded at both ends, walked across the whole band rather than sampled at
		//     the three points above - a step or a plateau would survive those and not this.
		float previous = SCORE - 1;
		for (int metres = 600; metres >= 0; metres -= 25)
		{
			float biased = OVT_ObjectiveSelection.ApplyAnchorBias(SCORE, metres, RADIUS, WEIGHT);

			if (biased < SCORE)
			{
				SetFailure("the bias must never SUBTRACT, but at %1 m it returned %2 against an input of %3",
					metres.ToString(), biased.ToString(), SCORE.ToString());
				return true;
			}

			if (biased > SCORE + WEIGHT)
			{
				SetFailure("the bias must never exceed its own weight, but at %1 m it returned %2 against a ceiling of %3",
					metres.ToString(), biased.ToString(), (SCORE + WEIGHT).ToString());
				return true;
			}

			if (biased < previous)
			{
				SetFailure("the bias must rise as a candidate gets closer, but %1 m scored %2 after the previous, further step scored %3",
					metres.ToString(), biased.ToString(), previous.ToString());
				return true;
			}

			previous = biased;
		}

		Print("ApplyAnchorBias: inside the band the bias rises linearly as a candidate approaches, pays exactly its weight at the objective, pays nothing at the edge, and never subtracts");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one biased score against an expected value, within the tier's epsilon.
	//! \param[in] distance Distance from the candidate to the anchor.
	//! \param[in] expected The score expected out.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectBias(float distance, float expected, string label)
	{
		float actual = OVT_ObjectiveSelection.ApplyAnchorBias(SCORE, distance, RADIUS, WEIGHT);
		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The ordering claim, stated as two candidates rather than as one number: an objective moves
//! comparable work up the queue and cannot move better work down it.
//!
//! THIS IS THE CASE THAT SAYS WHAT THE FEATURE DOES. Everything above is arithmetic; this is the
//! behaviour a player would notice. Three rows, and the middle one is the design:
//!
//!   1. A quieter candidate ON the objective outranks a busier one outside the band, PROVIDED the gap
//!      between them is smaller than the weight. This is the bias doing its job.
//!   2. A candidate whose own score already beats the objective-adjacent one by MORE than the weight
//!      still wins. This is the bound doing its job, and it is why the weight is the tuning knob: a
//!      genuine hotspot is never abandoned because the occupying faction has an objective elsewhere.
//!   3. With no anchor at all the original order is preserved exactly, so the two rows above are
//!      claims about the anchor rather than about the comparison.
//!
//! ⚠ THE GAP IN ROW 2 IS DELIBERATELY ONE UNIT BEYOND THE WEIGHT, not double it. A weight applied
//! twice, or a falloff that pays a little more than it should at distance zero, is the realistic
//! failure, and only a row that sits right on the boundary catches it.
//!
//! ⚠ ROW 3 IS A CONTROL, NOT AN INDEPENDENT CLAIM. No plausible fault in the subject fails it - it is
//! there so that a future reader can see the two comparisons above are comparing the right way round,
//! and so a rewrite that made the function bias unconditionally would be caught here rather than
//! silently inverting rows 1 and 2 into tautologies.
//!
//! CAN-FAIL, two faults, both exiting compile-check 0:
//!   A7. INVERT THE FALLOFF as in A4 - the objective stops being worth anything. Compiled clean
//!       (exit 0). The case fails on row 1,
//!       "an objective-adjacent candidate must outrank a busier one outside the band when the gap is
//!       smaller than the weight: near 100, far 124".
//!   A8. DOUBLE THE TERM - `score + (2 * weight * (1 - distance / radius))`, so the bias overruns its
//!       own bound. Compiled clean (exit 0). ⚠ Row 1 still PASSES, more emphatically than before,
//!       which is exactly why row 2 exists: a bias that is too strong looks like a bias that is
//!       working. The case fails on row 2,
//!       "a candidate whose own score beats the biased one by more than the weight must still win -
//!       the bias is bounded, not a veto: near 150, far 126".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveAnchor_OrderingMovesOnlyWithinTheWeight : SCR_AutotestCaseBase
{
	//! The anchor: a forward-phase reach and a weight in the range the director actually pushes.
	protected const float RADIUS = 1200.0;
	protected const float WEIGHT = 25.0;

	//! The quieter candidate, sitting on the objective.
	protected const float NEAR_SCORE = 100.0;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Row 1: a busier candidate outside the band, but by less than the weight. The objective
		//     wins, which is the entire point of the feature.
		float farBeatable = NEAR_SCORE + WEIGHT - 1;

		float near = OVT_ObjectiveSelection.ApplyAnchorBias(NEAR_SCORE, 0, RADIUS, WEIGHT);
		float far = OVT_ObjectiveSelection.ApplyAnchorBias(farBeatable, 5000, RADIUS, WEIGHT);

		if (near <= far)
		{
			SetFailure("an objective-adjacent candidate must outrank a busier one outside the band when the gap is smaller than the weight: near %1, far %2",
				near.ToString(), far.ToString());
			return true;
		}

		// --- Row 2: the same comparison one unit the other side of the bound. The bias runs out and
		//     the genuinely busier place keeps its place at the front.
		float farUnbeatable = NEAR_SCORE + WEIGHT + 1;

		far = OVT_ObjectiveSelection.ApplyAnchorBias(farUnbeatable, 5000, RADIUS, WEIGHT);

		if (near >= far)
		{
			SetFailure("a candidate whose own score beats the biased one by more than the weight must still win - the bias is bounded, not a veto: near %1, far %2",
				near.ToString(), far.ToString());
			return true;
		}

		// --- Row 3: with no anchor, the same two numbers keep their original order, so neither row
		//     above is an artefact of the comparison.
		float unbiasedNear = OVT_ObjectiveSelection.ApplyAnchorBias(NEAR_SCORE, 0, 0, 0);
		float unbiasedFar = OVT_ObjectiveSelection.ApplyAnchorBias(farBeatable, 5000, 0, 0);

		if (unbiasedNear >= unbiasedFar)
		{
			SetFailure("with no anchor the original order must be preserved, or the rows above are measuring the comparison rather than the bias: near %1, far %2",
				unbiasedNear.ToString(), unbiasedFar.ToString());
			return true;
		}

		Print("ApplyAnchorBias: an objective moves comparable work to the front of the queue and cannot move better work behind it - the reorder is bounded by the weight, and with no anchor the order is untouched");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 THE BEARING SIGN. This is the case the whole landing-zone change rests on.
//!
//! WHAT IS BEING CLAIMED, IN THE FORM THE MISTAKE WOULD TAKE. The battle controller places a wave's
//! landing zone at `target + dir * distance`, so `dir` points FROM the place under attack OUT TO the
//! ground the attackers are put down on. The bearing that feeds it must therefore be measured from the
//! target TOWARD the source - the side the men are coming from - and never from the source toward the
//! target.
//!
//! The inverse compiles, runs, produces a perfectly valid landing zone, and puts every single wave on
//! the FAR SIDE of the objective from the base that sent it. In play that reads as a pathing fault or
//! an AI fault; nothing in any log says "sign". No other tier can see it: the live path needs a battle,
//! a base list and a world, and by the time it is observable it is a bug report about troops walking
//! the wrong way round a town.
//!
//! SO THE SIGN IS ASSERTED TWICE, IN TWO DIFFERENT LANGUAGES:
//!   1. AS A NUMBER - a source due north of its target reads 0 degrees (North), not 180.
//!   2. AS GEOMETRY - the landing zone built from that bearing is CLOSER to the source than the target
//!      is. That second form is the one a reader can check without knowing the convention, and it is
//!      the form that stays true if the convention itself is ever re-based.
//!
//! ⚠ TOLERANCES, NOT EQUALITY, EVEN THOUGH THE FUNCTION RETURNS AN INT. The bearing comes out of a
//! transcendental applied to differences of world-scale coordinates, and vector arithmetic at those
//! magnitudes is not correctly rounded; a diagonal that lands on 44 instead of 45 is a rounding
//! artefact and not a defect. The comparison is also CIRCULAR - 359 and 1 are two degrees apart, not
//! 358 - because an off-by-one at due north would otherwise read as a maximal failure.
//!
//! CAN-FAIL, four faults, injected one at a time and compiled. All four exited tools/compile-check.sh
//! with 0, which is the point - not one of them is a script error:
//!   B1. INVERT THE BEARING - swap the deltas to `target - source`. Compiled clean (exit 0). The case
//!       fails on its first row, "a source due NORTH of its target must read as North: got 180,
//!       expected 0 (+/- 1)".
//!   B2. DROP THE Z NEGATION - `Math.Atan2(dx, dz)`, i.e. measure in the raw engine frame instead of
//!       the compass one. Compiled clean (exit 0). ⚠ The four cardinals still LOOK plausible because
//!       the answer is mirrored about the east-west line, so north and south simply swap; the case
//!       fails on the first row all the same, and the geometry half fails on every cardinal.
//!   B3. RETURN THE DEFAULT UNCONDITIONALLY - `return DEFAULT_DEGREES;` as the whole body. Compiled
//!       clean (exit 0). Every wave would land due north of every objective forever. The north row
//!       passes; the case fails on "a source due EAST of its target must read as East: got 0,
//!       expected 90 (+/- 1)".
//!   B4. DELETE THE COINCIDENT-SOURCE GUARD. Compiled clean (exit 0). ⚠ It does NOT fail the value
//!       row, because the platform's atan2 happens to answer 0 for (0, 0) - it fails the RANGE row
//!       only on a platform where it does not, which is exactly why that row is written as "inside
//!       [0, 360)" rather than as "equals 0". Recorded as a fault whose detection is
//!       platform-dependent rather than claimed as a proof.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveBearing_PointsFromTheTargetTowardTheSource : SCR_AutotestCaseBase
{
	//! Degrees of slack allowed on a bearing. One degree at 1 000 m is under 18 m of arc - far tighter
	//! than the landing-zone variance the caller then applies, so nothing this loose could hide a real
	//! error while still allowing for rounding.
	protected const float BEARING_TOLERANCE = 1.0;

	//! A target that is nowhere near the world origin, so a fault that measures from (0, 0, 0) instead
	//! of from the target cannot pass.
	protected const vector TARGET = "5000 120 -3000";

	//! How far each fixture source stands from the target.
	protected const float SOURCE_RANGE = 1200.0;

	//! Landing-zone distance used by the geometry half. Shorter than SOURCE_RANGE so a correct landing
	//! zone lands BETWEEN the target and the source rather than beyond it.
	protected const float LZ_RANGE = 400.0;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The four cardinals. North is -Z and East is +X, which is the convention the battle
		//     controller documents in file and which DirectionForDegrees mirrors.
		if (!ExpectBearing("0 0 -1", 0, "a source due NORTH of its target must read as North"))
			return true;

		if (!ExpectBearing("1 0 0", 90, "a source due EAST of its target must read as East"))
			return true;

		if (!ExpectBearing("0 0 1", 180, "a source due SOUTH of its target must read as South"))
			return true;

		if (!ExpectBearing("-1 0 0", 270, "a source due WEST of its target must read as West"))
			return true;

		// --- Both diagonals asked for, which pin that the two axes are not swapped: a swap leaves every
		//     cardinal correct-looking and moves 45 to 315.
		if (!ExpectBearing("1 0 -1", 45, "a source to the NORTH-EAST must read as 45"))
			return true;

		if (!ExpectBearing("-1 0 1", 225, "a source to the SOUTH-WEST must read as 225"))
			return true;

		// --- Height is not a compass reading. A source 400 m up a hill due north is still due north.
		vector uphill = TARGET + Vector(0, 400, -SOURCE_RANGE);
		int uphillBearing = OVT_QRFBearing.PreferredDegreesFromSource(uphill, TARGET);
		if (CircularDifference(uphillBearing, 0) > BEARING_TOLERANCE)
		{
			SetFailure("height must not enter a compass bearing: a source 400 m above and due north read as %1, expected 0",
				uphillBearing.ToString());
			return true;
		}

		// --- A source standing on its own target has no bearing to give, and must still answer a
		//     DEFINED one. This row is about the RANGE, not about the value: what must never happen is
		//     a value that is not a number reaching the caller, where it would propagate into a landing
		//     zone the ocean test and the trace would both happily accept.
		int coincident = OVT_QRFBearing.PreferredDegreesFromSource(TARGET, TARGET);
		if (!(coincident >= 0) || !(coincident < 360))
		{
			SetFailure("a source coincident with its target must answer a defined bearing inside [0, 360): got %1",
				coincident.ToString());
			return true;
		}

		// --- THE GEOMETRY HALF, and the one that says what the sign MEANS. For each cardinal, build the
		//     landing zone the way the battle controller builds it and check it moved toward the source.
		if (!ExpectLandingZoneLeansTowardSource("0 0 -1", "north"))
			return true;

		if (!ExpectLandingZoneLeansTowardSource("1 0 0", "east"))
			return true;

		if (!ExpectLandingZoneLeansTowardSource("0 0 1", "south"))
			return true;

		if (!ExpectLandingZoneLeansTowardSource("-1 0 0", "west"))
			return true;

		Print("QRF bearing: the bearing runs from the TARGET toward the SOURCE, so a landing zone built from it lands on the side of the objective the attackers are actually coming from");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the bearing of a source placed one SOURCE_RANGE along a unit offset from the target.
	//! \param[in] offset Unit direction from the target to the source.
	//! \param[in] expected The compass bearing the convention demands.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectBearing(vector offset, float expected, string label)
	{
		vector source = TARGET + (offset.Normalized() * SOURCE_RANGE);

		int actual = OVT_QRFBearing.PreferredDegreesFromSource(source, TARGET);

		if (CircularDifference(actual, expected) <= BEARING_TOLERANCE)
			return true;

		// SetFailure takes at most three parameters after the format, so the tolerance is folded into
		// the literal rather than passed as a fourth.
		SetFailure("%1: got %2, expected %3 (+/- 1 degree)", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the landing zone exactly as the battle controller does - target + direction * distance -
	//! and asserts it ended up nearer the source than the target is.
	//!
	//! ⚠ THIS IS THE ASSERTION A SIGN ERROR CANNOT SURVIVE, and it needs no knowledge of the convention:
	//! whichever way the compass is defined, a wave has to land between its objective and the men
	//! sending it.
	//! \param[in] offset Unit direction from the target to the source.
	//! \param[in] label Which cardinal this row is, for the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectLandingZoneLeansTowardSource(vector offset, string label)
	{
		vector source = TARGET + (offset.Normalized() * SOURCE_RANGE);

		int bearing = OVT_QRFBearing.PreferredDegreesFromSource(source, TARGET);
		vector landingZone = TARGET + (OVT_QRFBearing.DirectionForDegrees(bearing) * LZ_RANGE);

		float fromTarget = vector.Distance(source, TARGET);
		float fromLandingZone = vector.Distance(source, landingZone);

		if (fromLandingZone < fromTarget)
			return true;

		SetFailure("a wave from the %1 must land on the %1 side of its objective, but its landing zone is %2 m from the source while the objective itself is only %3 m from it - the bearing is inverted and every wave lands on the far side",
			label, fromLandingZone.ToString(), fromTarget.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The shorter way round a compass between two bearings, so 359 and 1 are two degrees apart.
	//! \param[in] actual Observed bearing.
	//! \param[in] expected Expected bearing.
	//! \return The absolute separation in degrees, never more than 180.
	protected float CircularDifference(float actual, float expected)
	{
		float difference = Math.AbsFloat(actual - expected);

		if (difference > 180)
			difference = 360 - difference;

		return difference;
	}
}

//------------------------------------------------------------------------------------------------
//! The direction vector and the bearing are exact inverses of each other, in the convention the
//! battle controller documents in file: 0 = North = -Z, 90 = East = +X.
//!
//! WHY THIS IS A SEPARATE CASE FROM THE ONE ABOVE. That case pins which WAY the bearing points; this
//! one pins the CONVENTION both halves are expressed in. They fail independently: a derived direction
//! could point correctly toward the source while being built in a frame that disagrees with the
//! authored path's own GetRandomDirection(), in which case a battle whose source is known would place
//! its waves somewhere different from one whose direction a map author typed in by hand - with nothing
//! in any log to say the two disagree.
//!
//! THE ROUND TRIP IS THE STRONGEST FORM AVAILABLE HERE. Walking every 15 degrees and requiring the
//! bearing of the resulting offset to come back to where it started catches a mirror, a swap, a
//! quarter-turn and an off-by-one wrap in one loop, without any of them having to be enumerated.
//!
//! CAN-FAIL, three faults, all exiting compile-check 0:
//!   B5. DROP THE NEGATION in DirectionForDegrees - `{sin, 0, cos}`. Compiled clean (exit 0). The case
//!       fails on "0 degrees must point NORTH, which is -Z: got <0,0,1>".
//!   B6. SWAP SIN AND COS - `{-cos, 0, sin}`. Compiled clean (exit 0). Every cardinal is a quarter turn
//!       out; the case fails on the same first row.
//!   B7. MAKE WrapDegrees A SINGLE ADD instead of a loop. Compiled clean (exit 0). The ordinary rows
//!       all pass - nothing in the round trip is more than one turn out of range - and the case fails
//!       on the explicit multi-turn row, "an angle several turns out of range must still wrap into
//!       [0, 360): 1085 wrapped to 725".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveBearing_DirectionAndBearingAreInverses : SCR_AutotestCaseBase
{
	//! Slack on a component of a unit vector.
	protected const float UNIT_TOLERANCE = 0.001;

	//! Slack on a bearing that has been through a direction vector and back.
	protected const float BEARING_TOLERANCE = 1.0;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The four cardinals as vectors, which is the convention itself written out.
		if (!ExpectDirection(0, 0, -1, "0 degrees must point NORTH, which is -Z"))
			return true;

		if (!ExpectDirection(90, 1, 0, "90 degrees must point EAST, which is +X"))
			return true;

		if (!ExpectDirection(180, 0, 1, "180 degrees must point SOUTH, which is +Z"))
			return true;

		if (!ExpectDirection(270, -1, 0, "270 degrees must point WEST, which is -X"))
			return true;

		// --- The round trip, every 15 degrees all the way round.
		for (int degrees = 0; degrees < 360; degrees += 15)
		{
			vector offset = OVT_QRFBearing.DirectionForDegrees(degrees) * 800;
			int back = OVT_QRFBearing.PreferredDegreesFromSource(offset, vector.Zero);

			float difference = Math.AbsFloat(back - degrees);
			if (difference > 180)
				difference = 360 - difference;

			if (difference > BEARING_TOLERANCE)
			{
				SetFailure("a direction built from %1 degrees must read back as %1 degrees, but it read back as %2",
					degrees.ToString(), back.ToString());
				return true;
			}
		}

		// --- The wrap, on both sides and several turns out, because the caller adds a signed variance
		//     roll to a derived bearing and nothing clamps the sum before it arrives.
		if (!ExpectWrap(-1, 359, "one degree below zero must wrap to the top of the circle"))
			return true;

		if (!ExpectWrap(360, 0, "a full turn must wrap to zero"))
			return true;

		if (!ExpectWrap(1085, 5, "an angle several turns out of range must still wrap into [0, 360)"))
			return true;

		if (!ExpectWrap(-725, 355, "an angle several turns BELOW range must still wrap into [0, 360)"))
			return true;

		Print("QRF bearing: the direction vector and the bearing are inverses in the 0 = North = -Z convention the battle controller documents, and any angle wraps into [0, 360) however far out it starts");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the horizontal components of a direction vector.
	//! \param[in] degrees The bearing to build.
	//! \param[in] expectedX Expected X component.
	//! \param[in] expectedZ Expected Z component.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectDirection(float degrees, float expectedX, float expectedZ, string label)
	{
		vector dir = OVT_QRFBearing.DirectionForDegrees(degrees);

		if (Math.AbsFloat(dir[0] - expectedX) <= UNIT_TOLERANCE && Math.AbsFloat(dir[2] - expectedZ) <= UNIT_TOLERANCE)
			return true;

		SetFailure("%1: got %2", label, dir.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one wrap.
	//! \param[in] degrees The angle handed in.
	//! \param[in] expected Where it must land.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectWrap(float degrees, float expected, string label)
	{
		float wrapped = OVT_QRFBearing.WrapDegrees(degrees);

		if (Math.AbsFloat(wrapped - expected) <= UNIT_TOLERANCE)
			return true;

		// Three parameters is the ceiling, so the expected value goes into the literal.
		SetFailure("%1 (expected " + expected.ToString() + "): %2 wrapped to %3", label, degrees.ToString(), wrapped.ToString());

		return false;
	}
}
