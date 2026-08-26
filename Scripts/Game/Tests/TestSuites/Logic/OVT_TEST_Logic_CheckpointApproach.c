//------------------------------------------------------------------------------------------------
//! TIER A - the mobile checkpoint's approach arithmetic (OVT_CheckpointApproachRules). Every subject
//! is a static function of plain numbers, per the tier rule in OVT_TEST_LogicSuite.c - including the
//! reviewer grep, which does not distinguish code from comments, so no banned identifier appears
//! anywhere below, prose included.
//!
//! ⚠ THE ROAD TEST IS NOT ASSERTED HERE AND CANNOT BE. "Is there a road within N metres" is the one
//! part of the chooser that has to ask the map, so it lives in the module and is pinned at the
//! initialisation tier instead. What IS here is the part that goes wrong silently: the wraparound, the
//! band, and the rule that a relocation must not come back to the bearing it just left.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! NormaliseBearing and AngularSeparation: the wraparound, from both sides of it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_CheckpointApproach_Wraparound : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!ExpectBearing(0, 0, "zero must stay zero"))
			return true;

		if (!ExpectBearing(360, 0, "a full turn must fold back to zero, not report itself as a full turn away from it"))
			return true;

		if (!ExpectBearing(370, 10, "a bearing past a full turn must fold into the first turn"))
			return true;

		if (!ExpectBearing(-30, 330, "a negative bearing must fold to its positive equivalent"))
			return true;

		if (!ExpectBearing(-390, 330, "a bearing more than a full turn negative must fold too"))
			return true;

		// ⚠ THE SHORT WAY ROUND. 350 and 10 are twenty degrees apart, not three hundred and forty; a
		// separation that measured the long way would call every pair of bearings either side of north
		// "different approaches" and let a checkpoint oscillate across it forever.
		if (!ExpectSeparation(350, 10, 20, "a separation across north must be measured the short way round"))
			return true;

		if (!ExpectSeparation(10, 350, 20, "the separation must not depend on which bearing is named first"))
			return true;

		if (!ExpectSeparation(0, 180, 180, "opposite bearings are half a turn apart, which is the maximum"))
			return true;

		if (!ExpectSeparation(90, 90, 0, "a bearing is zero degrees from itself"))
			return true;

		Print("Checkpoint approach: bearings fold into one turn from both directions, and separation is always measured the short way round");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] degrees The bearing to fold.
	//! \param[in] expected What it must fold to.
	//! \param[in] claim What is being claimed.
	//! \return True when the claim held.
	protected bool ExpectBearing(float degrees, float expected, string claim)
	{
		float actual = OVT_CheckpointApproachRules.NormaliseBearing(degrees);
		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		SetFailure(string.Format("%1: %2 folded to %3, expected %4",
			claim, degrees.ToString(), actual.ToString(), expected.ToString()));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] first One bearing.
	//! \param[in] second The other.
	//! \param[in] expected The separation between them.
	//! \param[in] claim What is being claimed.
	//! \return True when the claim held.
	protected bool ExpectSeparation(float first, float second, float expected, string claim)
	{
		float actual = OVT_CheckpointApproachRules.AngularSeparation(first, second);
		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		SetFailure(string.Format("%1: answered %2, expected %3",
			claim, actual.ToString(), expected.ToString()));

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! BearingForSample, ClampToBand and BandDistance: the sampling ring and the distance band.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_CheckpointApproach_Band : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The sampling ring: twelve samples is one every thirty degrees, and the first is north.
		if (!ExpectFloat(OVT_CheckpointApproachRules.BearingForSample(0, 12), 0, "the first of twelve samples must be due north"))
			return true;

		if (!ExpectFloat(OVT_CheckpointApproachRules.BearingForSample(1, 12), 30, "twelve samples must be thirty degrees apart"))
			return true;

		if (!ExpectFloat(OVT_CheckpointApproachRules.BearingForSample(11, 12), 330, "the last of twelve samples must be thirty degrees short of a full turn"))
			return true;

		// ⚠ THE LAST SAMPLE MUST NOT BE THE FIRST ONE AGAIN. A divisor of count minus one would put
		// sample eleven back on north and waste a twelfth of the ring.
		if (OVT_TEST_LogicFixture.FloatEquals(OVT_CheckpointApproachRules.BearingForSample(11, 12), 0))
		{
			SetFailure("the last sample folded back onto the first - a full turn divided by twelve must not put sample eleven on north");
			return true;
		}

		if (OVT_CheckpointApproachRules.BearingForSample(0, 0) >= 0)
		{
			SetFailure("dividing a turn into zero samples must answer the no-bearing sentinel, not a real bearing");
			return true;
		}

		// --- The band.
		if (!ExpectFloat(OVT_CheckpointApproachRules.ClampToBand(50, 150, 300), 150, "a distance below the band must clamp to its near edge"))
			return true;

		if (!ExpectFloat(OVT_CheckpointApproachRules.ClampToBand(900, 150, 300), 300, "a distance above the band must clamp to its far edge"))
			return true;

		if (!ExpectFloat(OVT_CheckpointApproachRules.ClampToBand(200, 150, 300), 200, "a distance inside the band must be left alone"))
			return true;

		// ⚠ AN INVERTED BAND IS AN AUTHORING MISTAKE, NOT A CRASH. A config with the two ends the wrong
		// way round still has to answer a distance inside them rather than clamping everything to the
		// smaller number - which would put every checkpoint at one fixed range.
		if (!ExpectFloat(OVT_CheckpointApproachRules.ClampToBand(200, 300, 150), 200, "a band authored the wrong way round must still admit a distance between its ends"))
			return true;

		if (!ExpectFloat(OVT_CheckpointApproachRules.ClampToBand(-10, -50, -20), 0, "a wholly negative band must clamp to zero rather than answering a negative distance"))
			return true;

		// --- The roll across the band.
		if (!ExpectFloat(OVT_CheckpointApproachRules.BandDistance(150, 300, 0), 150, "a roll of zero must sit on the near edge of the band"))
			return true;

		if (!ExpectFloat(OVT_CheckpointApproachRules.BandDistance(150, 300, 1), 300, "a roll of one must sit on the far edge of the band"))
			return true;

		if (!ExpectFloat(OVT_CheckpointApproachRules.BandDistance(150, 300, 0.5), 225, "a roll of a half must sit in the middle of the band"))
			return true;

		if (!ExpectFloat(OVT_CheckpointApproachRules.BandDistance(150, 300, 4), 300, "a roll past one must clamp rather than run off the end of the band"))
			return true;

		if (!ExpectFloat(OVT_CheckpointApproachRules.BandDistance(200, 200, 0.5), 200, "a band with no width must answer its own single value"))
			return true;

		Print("Checkpoint approach: the sampling ring divides a full turn without repeating itself, and the band clamps from both ends, either way round, and rolls across");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] actual What was answered.
	//! \param[in] expected What was claimed.
	//! \param[in] claim What is being claimed.
	//! \return True when the claim held.
	protected bool ExpectFloat(float actual, float expected, string claim)
	{
		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		SetFailure(string.Format("%1: answered %2, expected %3", claim, actual.ToString(), expected.ToString()));

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! PickCandidate: the roll-to-index conversion, and the two ends nobody tests until they break.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_CheckpointApproach_Pick : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_CheckpointApproachRules.PickCandidate(0, 0.5) != -1)
		{
			SetFailure("picking from an empty list must answer -1, which is a real answer and not an error");
			return true;
		}

		if (OVT_CheckpointApproachRules.PickCandidate(4, 0) != 0)
		{
			SetFailure("a roll of zero must take the first candidate");
			return true;
		}

		// 🔴 THE OFF-BY-ONE THIS FUNCTION EXISTS FOR. A roll of exactly one multiplied by the count is
		// the count, which is one past the end of the list; unclamped it reads a candidate that is not
		// there.
		int top = OVT_CheckpointApproachRules.PickCandidate(4, 1);
		if (top != 3)
		{
			SetFailure(string.Format("a roll of exactly one must take the LAST candidate and not run one past the end: answered %1, expected 3", top.ToString()));
			return true;
		}

		if (OVT_CheckpointApproachRules.PickCandidate(4, 0.5) != 2)
		{
			SetFailure("a roll of a half of four candidates must take the third");
			return true;
		}

		if (OVT_CheckpointApproachRules.PickCandidate(4, -3) != 0)
		{
			SetFailure("a roll below zero must clamp to the first candidate rather than answering a negative index");
			return true;
		}

		if (OVT_CheckpointApproachRules.PickCandidate(1, 0.99) != 0)
		{
			SetFailure("a single candidate must be picked for every roll");
			return true;
		}

		Print("Checkpoint approach: PickCandidate answers -1 on an empty list, clamps both ends of the roll, and never indexes past the last candidate");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! ChooseBearingIndex: THE RULE THAT STOPS A TWO-ROAD TOWN OSCILLATING.
//!
//! ⚠ THE INDEX IS INTO THE CALLER'S OWN LIST, and that is asserted deliberately. The caller keeps two
//! parallel lists beside the bearings - the road point and its heading - so an index into a filtered
//! copy would park the checkpoint on somebody else's road while reporting the right bearing. The
//! survivors are therefore collected as indices rather than by removing from the input, which is also
//! why the tree's own swap-with-the-last array removal never appears in this path.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_CheckpointApproach_Exclusion : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<float> bearings = {0, 90, 180, 270};

		// --- Nothing used yet: every candidate is available, and the roll decides.
		int first = OVT_CheckpointApproachRules.ChooseBearingIndex(bearings, OVT_CheckpointApproachRules.NO_PREVIOUS_BEARING, 60, 0);
		if (first != 0)
		{
			SetFailure(string.Format("with no bearing used yet, a roll of zero must take the first candidate: answered %1, expected 0", first.ToString()));
			return true;
		}

		int last = OVT_CheckpointApproachRules.ChooseBearingIndex(bearings, OVT_CheckpointApproachRules.NO_PREVIOUS_BEARING, 60, 1);
		if (last != 3)
		{
			SetFailure(string.Format("with no bearing used yet, a roll of one must take the last candidate: answered %1, expected 3", last.ToString()));
			return true;
		}

		// --- Having just left 0, the roll of zero must NOT take index 0 again: the first survivor is 90.
		int rotated = OVT_CheckpointApproachRules.ChooseBearingIndex(bearings, 0, 60, 0);
		if (rotated != 1)
		{
			SetFailure(string.Format("a relocation from bearing 0 must skip it: answered index %1, expected 1 (bearing 90)", rotated.ToString()));
			return true;
		}

		// --- And the index still points into the CALLER'S list, not into the filtered survivors: with
		//     bearing 0 excluded there are three survivors, and a roll of one must land on index 3.
		int rotatedTop = OVT_CheckpointApproachRules.ChooseBearingIndex(bearings, 0, 60, 1);
		if (rotatedTop != 3)
		{
			SetFailure(string.Format("the answer must be an index into the caller's own list: with one candidate excluded, a roll of one answered %1 instead of 3 - a checkpoint parked on the road belonging to a different bearing", rotatedTop.ToString()));
			return true;
		}

		// --- A near miss is still the same approach. 30 degrees from the bearing just left is inside
		//     the separation, so it is excluded along with the exact one.
		array<float> nearMiss = {30, 200};
		int avoidedNearMiss = OVT_CheckpointApproachRules.ChooseBearingIndex(nearMiss, 0, 60, 0);
		if (avoidedNearMiss != 1)
		{
			SetFailure(string.Format("a candidate 30 degrees from the bearing just left is the same road out of town and must be excluded: answered %1, expected 1", avoidedNearMiss.ToString()));
			return true;
		}

		// 🔴 ONE USABLE APPROACH MUST REFUSE, NOT FALL BACK ONTO ITSELF. A relocation that answered the
		// bearing it is already parked on would announce a move that never happened, restart the clock
		// and do it again forever.
		array<float> onlyOne = {0};
		if (OVT_CheckpointApproachRules.ChooseBearingIndex(onlyOne, 0, 60, 0) != -1)
		{
			SetFailure("with the only candidate excluded, the chooser must refuse - a relocation onto the spot already occupied is not a relocation");
			return true;
		}

		array<float> none = {};
		if (OVT_CheckpointApproachRules.ChooseBearingIndex(none, OVT_CheckpointApproachRules.NO_PREVIOUS_BEARING, 60, 0) != -1)
		{
			SetFailure("an empty candidate list must answer -1");
			return true;
		}

		// --- A non-positive separation switches the rule off, which is authored behaviour.
		if (OVT_CheckpointApproachRules.ChooseBearingIndex(onlyOne, 0, 0, 0) != 0)
		{
			SetFailure("a separation of zero must switch the exclusion off rather than excluding everything");
			return true;
		}

		// --- Across north: 350 is only 20 degrees from 10 and must be excluded when 10 was just left.
		array<float> acrossNorth = {350, 120};
		int acrossPick = OVT_CheckpointApproachRules.ChooseBearingIndex(acrossNorth, 10, 60, 0);
		if (acrossPick != 1)
		{
			SetFailure(string.Format("a candidate 20 degrees away ACROSS north must be excluded: answered %1, expected 1", acrossPick.ToString()));
			return true;
		}

		Print("Checkpoint approach: the chooser excludes the bearing just left and everything within the separation of it, answers an index into the caller's own list, and refuses rather than relocating onto itself");

		return true;
	}
}
