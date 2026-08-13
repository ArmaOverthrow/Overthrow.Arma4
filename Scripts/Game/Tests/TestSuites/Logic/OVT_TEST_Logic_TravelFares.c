//------------------------------------------------------------------------------------------------
//! TIER A case - the travel fare arithmetic.
//!
//! OVT_FastTravelService.ComputeFare is the one part of map/fast-travel that can be asserted
//! world-free: it takes a distance, a unit price, a floor flag and a recruit count, and returns an
//! amount. No config, no economy, no player entity. The rest of the service reads the world and
//! belongs to the manual gate.
//!
//! WHAT IT PINS:
//!  - WHICH VERB GETS THE FLOOR, asserted directly against OVT_FastTravelService.VerbUsesKmFloor -
//!    the one place that mapping lives, and the one CalculateTravelCost consults. Until that helper
//!    existed the mapping was two inline literals inside CalculateTravelCost, which reads the world
//!    and so cannot be exercised in this tier; swapping them would have made buses cost a
//!    one-kilometre minimum and fast travel free over short hops with every assertion below still
//!    green. The verb-to-floor cases come first for that reason;
//!  - the 1 km floor is what separates the two verbs. Fast travel has it, buses do not, and a
//!    0 m trip therefore costs one full unit by one verb and nothing at all by the other. If the
//!    flag ever stops being honoured, one of the two fare models is silently wrong and nothing else
//!    in the tree would notice;
//!  - the floor is a FLOOR, not a fixed price: past 1 km both verbs charge the same per-kilometre;
//!  - the recruit multiplier is exactly (1 + N). Recruits are charged a full fare each, which is a
//!    deliberate, user-approved behaviour change (implementation plan K3) and therefore worth
//!    pinning as an intention rather than leaving to a play-test;
//!  - rounding is Math.Round of (kilometres x unit price) BEFORE the multiplier, so N recruits cost
//!    exactly N extra solo fares. Rounding after the multiplier would make a party fare drift off
//!    N x the price the panel showed for one.
//!
//! INTEGER DISCIPLINE. Every assertion here is on an int, so they are exact comparisons - no
//! epsilon. The one place a float enters is the kilometre term, and the rounding cases below assert
//! against Math.Round of the same expression rather than a hand-written literal, so the case pins
//! "matches Math.Round" rather than pinning one compiler's float unit in the last place.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TravelFares : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- WHICH VERB GETS THE FLOOR. Everything below asserts what the floor DOES; these two assert
		// who it is applied TO, which is the half no fare assertion can reach.
		if (!OVT_FastTravelService.VerbUsesKmFloor(OVT_TravelVerb.FAST_TRAVEL))
		{
			SetFailure("Fast travel must be charged at a one-kilometre minimum, but VerbUsesKmFloor(FAST_TRAVEL) returned false");
			return true;
		}

		if (OVT_FastTravelService.VerbUsesKmFloor(OVT_TravelVerb.BUS))
		{
			SetFailure("A bus fare must be strictly pro rata, but VerbUsesKmFloor(BUS) returned true");
			return true;
		}

		// And the consequence, stated end to end: the SAME zero-distance trip costs a full unit by one
		// verb and nothing by the other, with the floor flag taken from the verb rather than hand-written.
		// This is what breaks if the two arguments are ever swapped at the call site.
		int flooredZero = OVT_FastTravelService.ComputeFare(0, 100, OVT_FastTravelService.VerbUsesKmFloor(OVT_TravelVerb.FAST_TRAVEL), 0);
		if (flooredZero != 100)
		{
			SetFailure("A zero-distance trip priced with the FAST_TRAVEL floor cost %1, expected one full unit of 100", flooredZero.ToString());
			return true;
		}

		int unflooredZero = OVT_FastTravelService.ComputeFare(0, 100, OVT_FastTravelService.VerbUsesKmFloor(OVT_TravelVerb.BUS), 0);
		if (unflooredZero != 0)
		{
			SetFailure("A zero-distance trip priced with the BUS floor setting cost %1, expected 0", unflooredZero.ToString());
			return true;
		}

		// --- THE 1 KM FLOOR, which is the whole difference between the two verbs.
		// A 0 m trip: one full unit by fast travel, nothing at all by bus.
		if (!ExpectFare(0, 100, true, 0, 100, "a zero-distance fast travel"))
			return true;

		if (!ExpectFare(0, 100, false, 0, 0, "a zero-distance bus trip"))
			return true;

		// Anything under a kilometre is charged as a kilometre with the floor, and pro rata without.
		if (!ExpectFare(1, 100, true, 0, 100, "a one-metre fast travel"))
			return true;

		if (!ExpectFare(500, 100, true, 0, 100, "a half-kilometre fast travel"))
			return true;

		if (!ExpectFare(500, 100, false, 0, 50, "a half-kilometre bus trip"))
			return true;

		if (!ExpectFare(999, 100, true, 0, 100, "a fast travel one metre short of the floor"))
			return true;

		// Exactly on the floor the two verbs agree, and they keep agreeing past it: the floor is a
		// minimum, not a flat rate.
		if (!ExpectFare(1000, 100, true, 0, 100, "a fast travel of exactly one kilometre"))
			return true;

		if (!ExpectFare(1000, 100, false, 0, 100, "a bus trip of exactly one kilometre"))
			return true;

		if (!ExpectFare(2500, 100, true, 0, 250, "a two-and-a-half kilometre fast travel"))
			return true;

		if (!ExpectFare(2500, 100, false, 0, 250, "a two-and-a-half kilometre bus trip"))
			return true;

		if (!ExpectFare(12000, 25, false, 0, 300, "a bus trip right across the map"))
			return true;

		// --- THE RECRUIT MULTIPLIER is exactly (1 + N), on both verbs.
		if (!ExpectFare(2000, 50, true, 1, 200, "a two-kilometre fast travel with one recruit"))
			return true;

		if (!ExpectFare(2000, 50, true, 2, 300, "a two-kilometre fast travel with two recruits"))
			return true;

		if (!ExpectFare(2000, 50, false, 5, 600, "a two-kilometre bus trip with five recruits"))
			return true;

		// The floor is applied before the multiplier, so a squad on a short hop pays N+1 floors.
		if (!ExpectFare(0, 100, true, 3, 400, "a zero-distance fast travel with three recruits"))
			return true;

		if (!ExpectFare(0, 100, false, 3, 0, "a zero-distance bus trip with three recruits"))
			return true;

		// Stated as the relation rather than as literals, over a spread of counts, because "(1 + N)
		// solo fares" is the contract the panel's label promises the player.
		if (!ExpectMultiplierRelation(3400, 37, true))
			return true;

		if (!ExpectMultiplierRelation(3400, 37, false))
			return true;

		// --- ROUNDING is Math.Round of the kilometre term, applied before the multiplier.
		if (!ExpectRoundingMatches(1234, 7, false))
			return true;

		if (!ExpectRoundingMatches(1234, 7, true))
			return true;

		// A term landing exactly on .5, which is the only place a different rounding rule would show.
		if (!ExpectRoundingMatches(1500, 1, false))
			return true;

		if (!ExpectRoundingMatches(2500, 3, false))
			return true;

		// Rounding happens once, before the multiplier: the party fare is exactly N+1 times the
		// rounded solo fare, not the rounding of N+1 times the raw term.
		int soloRounded = OVT_FastTravelService.ComputeFare(1500, 1, false, 0);
		int partyOfThree = OVT_FastTravelService.ComputeFare(1500, 1, false, 2);
		if (partyOfThree != soloRounded * 3)
		{
			SetFailure("A party of three paid %1 for a trip whose solo fare is %2; rounding must happen before the recruit multiplier, giving %3",
				partyOfThree.ToString(), soloRounded.ToString(), (soloRounded * 3).ToString());
			return true;
		}

		// --- DEGENERATE INPUTS never produce a negative charge or a bogus one.
		if (!ExpectFare(5000, 0, true, 4, 0, "a trip whose unit price is zero"))
			return true;

		if (!ExpectFare(5000, -10, true, 0, 0, "a trip whose unit price is negative"))
			return true;

		if (!ExpectFare(-500, 100, false, 0, 0, "a bus trip with a negative distance"))
			return true;

		if (!ExpectFare(-500, 100, true, 0, 100, "a fast travel with a negative distance"))
			return true;

		if (!ExpectFare(2000, 50, true, -3, 100, "a trip with a negative recruit count"))
			return true;

		Print("Travel fares: VerbUsesKmFloor puts the 1 km floor on fast travel and not on buses, a 0 m trip costs one unit with the floor and nothing without, the recruit multiplier is exactly (1 + N), and rounding is Math.Round applied before the multiplier");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one fare, naming the case in the failure message.
	//! \param[in] distMeters Distance to the destination.
	//! \param[in] unitPrice Price per kilometre.
	//! \param[in] applyKmFloor True for fast travel, false for a bus.
	//! \param[in] recruitCount Recruits travelling too.
	//! \param[in] expected The fare the model specifies.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectFare(float distMeters, int unitPrice, bool applyKmFloor, int recruitCount, int expected, string label)
	{
		int actual = OVT_FastTravelService.ComputeFare(distMeters, unitPrice, applyKmFloor, recruitCount);

		if (actual == expected)
			return true;

		// SCR_AutotestCaseBase.SetResultFailure takes at most three substitution parameters, so the
		// trip description is concatenated into the reason rather than passed as one.
		string trip = distMeters.ToString() + " m at " + unitPrice.ToString() + "/km, floor "
			+ applyKmFloor.ToString() + ", " + recruitCount.ToString() + " recruits";

		SetFailure("%1 (%2) cost %3, expected " + expected.ToString(), label, trip, actual.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that the fare for N recruits is exactly (1 + N) solo fares, over a spread of N.
	//! \param[in] distMeters Distance to the destination.
	//! \param[in] unitPrice Price per kilometre.
	//! \param[in] applyKmFloor True for fast travel, false for a bus.
	//! \return True when every count matched; false after recording the failure.
	protected bool ExpectMultiplierRelation(float distMeters, int unitPrice, bool applyKmFloor)
	{
		int solo = OVT_FastTravelService.ComputeFare(distMeters, unitPrice, applyKmFloor, 0);

		array<int> counts = {1, 2, 3, 7};

		foreach (int count : counts)
		{
			int actual = OVT_FastTravelService.ComputeFare(distMeters, unitPrice, applyKmFloor, count);
			int expected = solo * (1 + count);

			if (actual == expected)
				continue;

			string trip = distMeters.ToString() + " m at " + unitPrice.ToString() + "/km, floor "
				+ applyKmFloor.ToString();

			SetFailure("A trip (%1) with %2 recruits cost %3; " + (1 + count).ToString()
				+ " solo fares of " + solo.ToString() + " is " + expected.ToString(),
				trip, count.ToString(), actual.ToString());

			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that a solo fare is Math.Round of the kilometre term, rather than a truncation of it.
	//! Only meaningful for distances at or above the floor, where the floor cannot mask the term.
	//! \param[in] distMeters Distance to the destination. Must be >= 1000 m.
	//! \param[in] unitPrice Price per kilometre.
	//! \param[in] applyKmFloor True for fast travel, false for a bus.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectRoundingMatches(float distMeters, int unitPrice, bool applyKmFloor)
	{
		int expected = Math.Round((distMeters / 1000) * unitPrice);
		int actual = OVT_FastTravelService.ComputeFare(distMeters, unitPrice, applyKmFloor, 0);

		if (actual == expected)
			return true;

		string trip = distMeters.ToString() + " m at " + unitPrice.ToString() + "/km, floor "
			+ applyKmFloor.ToString();

		SetFailure("A trip (%1) cost %2; Math.Round of the kilometre term is %3",
			trip, actual.ToString(), expected.ToString());

		return false;
	}
}
