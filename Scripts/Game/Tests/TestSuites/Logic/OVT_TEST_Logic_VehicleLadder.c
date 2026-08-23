//------------------------------------------------------------------------------------------------
//! TIER A - the vehicle escalation ladder's pure spine (OVT_VehicleLadderRules). Every subject is
//! a static function of plain numbers, per the tier rule in OVT_TEST_LogicSuite.c - including the
//! reviewer grep, which does not distinguish code from comments, so neither banned identifier
//! appears anywhere below, prose included.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! ScaledThreshold: the scale guard and the floor.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VehicleLadder_ScaledThreshold : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!Expect(100, 0, 100, "a scale of zero must be treated as one"))
			return true;

		if (!Expect(100, -5, 100, "a negative scale must be treated as one"))
			return true;

		if (!Expect(100, 2.0, 200, "scale 2.0 must double the requirement"))
			return true;

		if (!Expect(100, 0.25, 25, "scale 0.25 must quarter the requirement"))
			return true;

		if (!Expect(-50, 1, 0, "a negative requirement must floor at zero"))
			return true;

		Print("Vehicle ladder: ScaledThreshold guards a non-positive scale and floors at zero");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Expect(int minThreat, float scale, float expected, string claim)
	{
		float actual = OVT_VehicleLadderRules.ScaledThreshold(minThreat, scale);
		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		SetFailure(string.Format("%1: minThreat %2, scale %3 answered %4, expected %5",
			claim, minThreat.ToString(), scale.ToString(), actual.ToString()));

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! RungUnlocked: the boundary is inclusive, and a zero rung is always unlocked.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VehicleLadder_RungUnlocked : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_VehicleLadderRules.RungUnlocked(400, 1, 400))
		{
			SetFailure("a threat exactly at the scaled threshold must be unlocked - the comparison is >=, not >");
			return true;
		}

		if (OVT_VehicleLadderRules.RungUnlocked(400, 1, 399))
		{
			SetFailure("a threat one below the scaled threshold must not be unlocked");
			return true;
		}

		if (!OVT_VehicleLadderRules.RungUnlocked(0, 1, 0))
		{
			SetFailure("a rung of zero must be unlocked even at a threat of exactly zero");
			return true;
		}

		Print("Vehicle ladder: RungUnlocked is inclusive at the boundary and a zero rung is always unlocked");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! RungAffordable: a negative budget is unbounded, everything else is a plain comparison.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VehicleLadder_RungAffordable : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_VehicleLadderRules.RungAffordable(999999, -1))
		{
			SetFailure("a negative budget must be unbounded, whatever the cost");
			return true;
		}

		if (!OVT_VehicleLadderRules.RungAffordable(100, 100))
		{
			SetFailure("a cost exactly equal to the budget must be affordable");
			return true;
		}

		if (OVT_VehicleLadderRules.RungAffordable(101, 100))
		{
			SetFailure("a cost one over the budget must not be affordable");
			return true;
		}

		Print("Vehicle ladder: RungAffordable treats a negative budget as unbounded and everything else as a plain fit");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! PickRung: the picker itself - the top rung when it qualifies, the fallback when it does not,
//! the two "nothing qualifies" answers, and the tie-break.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VehicleLadder_PickRung : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<int> minThreats = {0, 400, 900};
		array<int> costs = {25, 70, 120};

		// --- The top rung, unlocked and affordable, wins outright.
		int topPick = OVT_VehicleLadderRules.PickRung(minThreats, costs, 1, 1000, -1);
		if (topPick != 2)
		{
			SetFailure(string.Format("a fully unlocked, fully affordable ladder must pick the top rung: answered %1, expected 2", topPick.ToString()));
			return true;
		}

		// --- The top rung is unlocked but too dear - the pick drops to the next one that fits.
		int middlePick = OVT_VehicleLadderRules.PickRung(minThreats, costs, 1, 1000, 100);
		if (middlePick != 1)
		{
			SetFailure(string.Format("a budget under the top rung's cost must drop to the next affordable rung: answered %1, expected 1", middlePick.ToString()));
			return true;
		}

		// --- Low threat only unlocks the bottom rung.
		int bottomPick = OVT_VehicleLadderRules.PickRung(minThreats, costs, 1, 200, -1);
		if (bottomPick != 0)
		{
			SetFailure(string.Format("a threat below every non-zero rung must pick the always-unlocked bottom rung: answered %1, expected 0", bottomPick.ToString()));
			return true;
		}

		// --- An empty ladder answers -1, a real answer and not an error.
		array<int> emptyThreats = {};
		array<int> emptyCosts = {};
		int emptyPick = OVT_VehicleLadderRules.PickRung(emptyThreats, emptyCosts, 1, 1000, -1);
		if (emptyPick != -1)
		{
			SetFailure(string.Format("an empty ladder must answer -1: answered %1", emptyPick.ToString()));
			return true;
		}

		// --- Every rung locked (threat too low for all of them) also answers -1.
		array<int> allLockedThreats = {100, 400, 900};
		array<int> allLockedCosts = {25, 70, 120};
		int lockedPick = OVT_VehicleLadderRules.PickRung(allLockedThreats, allLockedCosts, 1, 50, -1);
		if (lockedPick != -1)
		{
			SetFailure(string.Format("a threat below every rung's requirement must answer -1: answered %1", lockedPick.ToString()));
			return true;
		}

		// --- Two rungs tied on the same requirement resolve to the lower index.
		array<int> tiedThreats = {100, 100, 50};
		array<int> tiedCosts = {10, 10, 5};
		int tiedPick = OVT_VehicleLadderRules.PickRung(tiedThreats, tiedCosts, 1, 1000, -1);
		if (tiedPick != 0)
		{
			SetFailure(string.Format("a tie on the same requirement must resolve to the lower index: answered %1, expected 0", tiedPick.ToString()));
			return true;
		}

		Print("Vehicle ladder: PickRung picks the top affordable rung, falls back on budget and on threat, answers -1 on an empty or fully-locked ladder, and ties break to the lowest index");

		return true;
	}
}
