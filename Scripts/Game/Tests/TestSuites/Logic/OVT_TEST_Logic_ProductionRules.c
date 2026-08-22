//------------------------------------------------------------------------------------------------
//! TIER A cases - the pure production-site rules.
//!
//! OVT_ResourceProductionRules takes every lookup as an argument (the live price, the clock hour,
//! the viewer's persistent id), so each case hands in literals and asserts one decision. Nothing
//! here resolves a manager, a game mode or a controller - the rules are asserted with no world
//! behind them.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! SitePrice is 80% of the live price, floored at 1 - never 0, even for a live price of 0 or 1.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_SitePriceIsEightyPercentFloorOne : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int normal = OVT_ResourceProductionRules.SitePrice(100, 0.8);
		if (normal != 80)
		{
			SetFailure("SitePrice(100, 0.8) is %1, expected 80", normal.ToString());
			return true;
		}

		int small = OVT_ResourceProductionRules.SitePrice(1, 0.8);
		if (small != 1)
		{
			SetFailure("SitePrice(1, 0.8) is %1, expected 1 - round(0.8) floored", small.ToString());
			return true;
		}

		int zero = OVT_ResourceProductionRules.SitePrice(0, 0.8);
		if (zero != 1)
		{
			SetFailure("SitePrice(0, 0.8) is %1, expected the floor 1 - SitePrice must never return 0", zero.ToString());
			return true;
		}

		int large = OVT_ResourceProductionRules.SitePrice(1000, 0.8);
		if (large != 800)
		{
			SetFailure("SitePrice(1000, 0.8) is %1, expected 800", large.ToString());
			return true;
		}

		Print("Site price: 80% of live, floored at 1, never 0");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The site's discount is its own ratio (0.8), not the port's export ratio (0.5). Wiring the wrong
//! ratio in would produce identical numbers at every live price, which this case would catch.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_SitePriceIsNotTheSellRatio : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<int> liveTable = new array<int>();
		liveTable.Insert(10);
		liveTable.Insert(40);
		liveTable.Insert(120);
		liveTable.Insert(999);

		for (int i = 0; i < liveTable.Count(); i++)
		{
			int live = liveTable[i];
			int atSiteRatio = OVT_ResourceProductionRules.SitePrice(live, OVT_ResourceProductionRules.SITE_SELL_RATIO);
			int atPortRatio = OVT_ResourceProductionRules.SitePrice(live, 0.5);

			if (atSiteRatio == atPortRatio)
			{
				SetFailure("At live price %1 the site ratio and the port ratio (0.5) produced the same answer %2 - they must differ", live.ToString(), atSiteRatio.ToString());
				return true;
			}
		}

		if (OVT_ResourceProductionRules.SITE_SELL_RATIO != 0.8)
		{
			SetFailure("SITE_SELL_RATIO is %1, expected 0.8", OVT_ResourceProductionRules.SITE_SELL_RATIO.ToString());
			return true;
		}

		Print("Site price: uses its own 0.8 ratio, distinct from the port's 0.5 sell ratio at every price checked");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! BuyCost scales by the difficulty multiplier at all five shipped presets, and never floors to 0.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_BuyCostScalesAndFloors : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	bool AssertCost(int baseCost, float multiplier, int expected)
	{
		int cost = OVT_ResourceProductionRules.BuyCost(baseCost, multiplier);
		if (cost != expected)
		{
			string request = "BuyCost(" + baseCost.ToString() + ", " + multiplier.ToString() + ")";
			SetFailure("%1 is %2, expected %3", request, cost.ToString(), expected.ToString());
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Easy 0.4 / default 0.5 / Hard 0.7 / Extreme 1.5 / Insane 2, against an 8000 base cost.
		if (!AssertCost(8000, 0.4, 3200))
			return true;

		if (!AssertCost(8000, 0.5, 4000))
			return true;

		if (!AssertCost(8000, 0.7, 5600))
			return true;

		if (!AssertCost(8000, 1.5, 12000))
			return true;

		if (!AssertCost(8000, 2.0, 16000))
			return true;

		int floored = OVT_ResourceProductionRules.BuyCost(1, 0.01);
		if (floored != 1)
		{
			SetFailure("BuyCost(1, 0.01) is %1, expected the floor 1", floored.ToString());
			return true;
		}

		Print("Buy cost: scales by all five shipped difficulty multipliers and never floors to 0");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An unowned site refuses every viewer, including an unresolved (empty) one.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_AccessUnownedIsRefused : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_ResourceProductionRules.MayAccessStore("player1", "", false))
		{
			SetFailure("MayAccessStore('player1', '', false) admitted a viewer to an unowned site");
			return true;
		}

		if (OVT_ResourceProductionRules.MayAccessStore("player1", "", true))
		{
			SetFailure("MayAccessStore('player1', '', true) admitted a viewer to an unowned site");
			return true;
		}

		if (OVT_ResourceProductionRules.MayAccessStore("", "", false))
		{
			SetFailure("MayAccessStore('', '', false) admitted an unresolved viewer to an unowned site");
			return true;
		}

		Print("Access: an unowned site refuses every viewer");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The owner always passes, private or public - ownership is checked before privacy, not after.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_AccessOwnerAlwaysAllowed : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_ResourceProductionRules.MayAccessStore("owner1", "owner1", true))
		{
			SetFailure("MayAccessStore('owner1', 'owner1', true) refused the owner on a private site");
			return true;
		}

		if (!OVT_ResourceProductionRules.MayAccessStore("owner1", "owner1", false))
		{
			SetFailure("MayAccessStore('owner1', 'owner1', false) refused the owner on a public site");
			return true;
		}

		Print("Access: the owner always passes, private or public");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A public (non-private) owned site admits a different persistent id.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_AccessPublicAllowsStrangers : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_ResourceProductionRules.MayAccessStore("stranger", "owner1", false))
		{
			SetFailure("MayAccessStore('stranger', 'owner1', false) refused a stranger on a public site");
			return true;
		}

		if (!OVT_ResourceProductionRules.MayAccessStore("", "owner1", false))
		{
			SetFailure("MayAccessStore('', 'owner1', false) refused an unresolved viewer on a public site");
			return true;
		}

		Print("Access: a public owned site admits any viewer");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A private owned site refuses a different persistent id, and an empty viewer id never matches an
//! empty owner (the special-cased unowned refusal must not be reachable through a plain == compare).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_AccessPrivateRefusesStrangers : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_ResourceProductionRules.MayAccessStore("stranger", "owner1", true))
		{
			SetFailure("MayAccessStore('stranger', 'owner1', true) admitted a stranger on a private site");
			return true;
		}

		if (OVT_ResourceProductionRules.MayAccessStore("", "owner1", true))
		{
			SetFailure("MayAccessStore('', 'owner1', true) admitted an unresolved viewer on a private site");
			return true;
		}

		// owner "" is refused by the unowned guard, never by an accidental "" == "" match.
		if (OVT_ResourceProductionRules.MayAccessStore("", "", true))
		{
			SetFailure("MayAccessStore('', '', true) is true - an empty viewer id must never match an empty owner");
			return true;
		}

		Print("Access: a private owned site refuses strangers, and an empty viewer never matches an empty owner");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A resistance-owned site admits everyone, whatever the privacy flag says.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_AccessResistanceAllowsEveryone : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_ResourceProductionRules.MayAccessStore("anyone", "resistance", true))
		{
			SetFailure("MayAccessStore('anyone', 'resistance', true) refused a viewer on a resistance-owned private site");
			return true;
		}

		if (!OVT_ResourceProductionRules.MayAccessStore("", "resistance", false))
		{
			SetFailure("MayAccessStore('', 'resistance', false) refused an unresolved viewer on a resistance-owned site");
			return true;
		}

		Print("Access: a resistance-owned site admits everyone regardless of privacy");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Privacy may be toggled by the owner, by an officer on a resistance-owned site, and by nobody
//! else - including on an unowned site, which has no privacy to toggle.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_PrivacyTogglingIsOwnerOrOfficer : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_ResourceProductionRules.MayTogglePrivacy("owner1", "owner1", false))
		{
			SetFailure("MayTogglePrivacy('owner1', 'owner1', false) refused the owner");
			return true;
		}

		if (OVT_ResourceProductionRules.MayTogglePrivacy("stranger", "owner1", false))
		{
			SetFailure("MayTogglePrivacy('stranger', 'owner1', false) admitted a non-owner, non-officer stranger");
			return true;
		}

		if (!OVT_ResourceProductionRules.MayTogglePrivacy("officer1", "resistance", true))
		{
			SetFailure("MayTogglePrivacy('officer1', 'resistance', true) refused an officer on a resistance-owned site");
			return true;
		}

		if (OVT_ResourceProductionRules.MayTogglePrivacy("officer1", "resistance", false))
		{
			SetFailure("MayTogglePrivacy('officer1', 'resistance', false) admitted a non-officer on a resistance-owned site");
			return true;
		}

		if (OVT_ResourceProductionRules.MayTogglePrivacy("anyone", "", true))
		{
			SetFailure("MayTogglePrivacy('anyone', '', true) admitted a toggle on an unowned site");
			return true;
		}

		Print("Privacy toggle: owner or officer-on-resistance only, never on an unowned site");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A sub-1 rate accumulates a fraction rather than producing nothing forever. Each call is
//! independent from a fresh carry of 0, walking the hours parameter 1, 2, 3, 4 at 0.5/hour.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_ProduceAccumulatesFraction : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		float carryOut;

		int oneHour = OVT_ResourceProductionRules.Produce(0.5, 1, 0, carryOut);
		if (oneHour != 0)
		{
			SetFailure("Produce(0.5, 1, 0, out) returned %1 units, expected 0", oneHour.ToString());
			return true;
		}

		if (Math.AbsFloat(carryOut - 0.5) > 0.0001)
		{
			SetFailure("Produce(0.5, 1, 0, out) left carryOut at %1, expected 0.5", carryOut.ToString());
			return true;
		}

		int twoHours = OVT_ResourceProductionRules.Produce(0.5, 2, 0, carryOut);
		if (twoHours != 1)
		{
			SetFailure("Produce(0.5, 2, 0, out) returned %1 units, expected 1", twoHours.ToString());
			return true;
		}

		if (Math.AbsFloat(carryOut) > 0.0001)
		{
			SetFailure("Produce(0.5, 2, 0, out) left carryOut at %1, expected 0", carryOut.ToString());
			return true;
		}

		int threeHours = OVT_ResourceProductionRules.Produce(0.5, 3, 0, carryOut);
		if (threeHours != 1)
		{
			SetFailure("Produce(0.5, 3, 0, out) returned %1 units, expected 1", threeHours.ToString());
			return true;
		}

		if (Math.AbsFloat(carryOut - 0.5) > 0.0001)
		{
			SetFailure("Produce(0.5, 3, 0, out) left carryOut at %1, expected 0.5", carryOut.ToString());
			return true;
		}

		int fourHours = OVT_ResourceProductionRules.Produce(0.5, 4, 0, carryOut);
		if (fourHours != 2)
		{
			SetFailure("Produce(0.5, 4, 0, out) returned %1 units, expected 2", fourHours.ToString());
			return true;
		}

		Print("Produce: 0.5/hour over 1, 2, 3, 4 hours yields 0, 1, 1, 2 with the carry tracked");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A non-positive rate or a non-positive hour count produces nothing and leaves the carry
//! untouched - never re-derived from a zero-length window.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_ProduceIsZeroForNonPositive : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	bool AssertNoOp(float unitsPerHour, int hours)
	{
		float carryOut;
		int units = OVT_ResourceProductionRules.Produce(unitsPerHour, hours, 0.3, carryOut);

		if (units != 0)
		{
			SetFailure("Produce(%1, %2, 0.3, out) returned %3 units, expected 0", unitsPerHour.ToString(), hours.ToString(), units.ToString());
			return false;
		}

		if (Math.AbsFloat(carryOut - 0.3) > 0.0001)
		{
			SetFailure("Produce(%1, %2, 0.3, out) left carryOut at %3, expected the untouched 0.3", unitsPerHour.ToString(), hours.ToString(), carryOut.ToString());
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!AssertNoOp(0, 5))
			return true;

		if (!AssertNoOp(-1, 5))
			return true;

		if (!AssertNoOp(2, 0))
			return true;

		if (!AssertNoOp(2, -5))
			return true;

		Print("Produce: a non-positive rate or hour count yields 0 units with the carry untouched");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A huge skip (a corrupt or extreme sleep call) is clamped to MAX_SKIP_HOURS rather than
//! overflowing into a nonsense unit count.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_ProduceClampsHugeSkips : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		float carryOut;
		int units = OVT_ResourceProductionRules.Produce(1, 100000, 0, carryOut);

		if (units != OVT_ResourceProductionRules.MAX_SKIP_HOURS)
		{
			SetFailure("Produce(1, 100000, 0, out) returned %1 units, expected the clamped %2", units.ToString(), OVT_ResourceProductionRules.MAX_SKIP_HOURS.ToString());
			return true;
		}

		if (Math.AbsFloat(carryOut) > 0.0001)
		{
			SetFailure("Produce(1, 100000, 0, out) left carryOut at %1, expected 0", carryOut.ToString());
			return true;
		}

		// A fractional rate against the same huge skip still returns a sane, bounded int.
		float fractionalCarry;
		int fractionalUnits = OVT_ResourceProductionRules.Produce(0.05, 1000000, 0, fractionalCarry);
		int expectedFractional = Math.Floor(0.05 * OVT_ResourceProductionRules.MAX_SKIP_HOURS);
		if (fractionalUnits != expectedFractional)
		{
			SetFailure("Produce(0.05, 1000000, 0, out) returned %1 units, expected %2 from the clamped window", fractionalUnits.ToString(), expectedFractional.ToString());
			return true;
		}

		Print("Produce: a 720-hour clamp holds even against a 100,000-hour or 1,000,000-hour skip");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The carry never leaves [0, 1), threaded sequentially across many small batches - the shape a
//! real hourly tick uses.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_ProduceCarryStaysInUnitRange : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		float carry = 0;

		for (int hour = 0; hour < 50; hour++)
		{
			float carryOut;
			OVT_ResourceProductionRules.Produce(0.37, 1, carry, carryOut);
			carry = carryOut;

			if (carry < 0)
			{
				SetFailure("After hour %1 carry is %2, expected >= 0", hour.ToString(), carry.ToString());
				return true;
			}

			if (carry >= 1)
			{
				SetFailure("After hour %1 carry is %2, expected < 1", hour.ToString(), carry.ToString());
				return true;
			}
		}

		Print("Produce: the fractional carry stays inside [0, 1) across 50 sequential hourly batches");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Free space throttles production: nothing fits when the store is full, a partial gap fits only
//! whole units, and a negative free-litres value means unlimited (a site with no capacity cap).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_FitProductionPausesWhenFull : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int full = OVT_ResourceProductionRules.FitProduction(10, 0, 5);
		if (full != 0)
		{
			SetFailure("FitProduction(10, 0, 5) is %1, expected 0 - a full store fits nothing", full.ToString());
			return true;
		}

		int partial = OVT_ResourceProductionRules.FitProduction(10, 12, 5);
		if (partial != 2)
		{
			SetFailure("FitProduction(10, 12, 5) is %1, expected 2 - a 12-litre gap fits two whole 5-litre units", partial.ToString());
			return true;
		}

		int unlimited = OVT_ResourceProductionRules.FitProduction(10, -1, 5);
		if (unlimited != 10)
		{
			SetFailure("FitProduction(10, -1, 5) is %1, expected all 10 - negative free litres means unlimited", unlimited.ToString());
			return true;
		}

		int exact = OVT_ResourceProductionRules.FitProduction(4, 20, 5);
		if (exact != 4)
		{
			SetFailure("FitProduction(4, 20, 5) is %1, expected 4 - an exact fit is not discarded", exact.ToString());
			return true;
		}

		Print("Fit production: a full store fits nothing, a partial gap fits whole units only, negative free means unlimited");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A non-positive litres-per-unit fits nothing, rather than dividing by zero or by a negative.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_FitProductionRejectsBadLitres : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int zero = OVT_ResourceProductionRules.FitProduction(10, 100, 0);
		if (zero != 0)
		{
			SetFailure("FitProduction(10, 100, 0) is %1, expected 0", zero.ToString());
			return true;
		}

		int negative = OVT_ResourceProductionRules.FitProduction(10, 100, -3);
		if (negative != 0)
		{
			SetFailure("FitProduction(10, 100, -3) is %1, expected 0", negative.ToString());
			return true;
		}

		Print("Fit production: a non-positive litres-per-unit always fits 0");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Exactly one production batch per in-game hour: a second call inside the same hour is a no-op,
//! and the check survives the midnight rollover (hour 23 back to hour 0).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_ShouldProduceOncePerHour : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_ResourceProductionRules.ShouldProduce(5, -1))
		{
			SetFailure("ShouldProduce(5, -1) refused the first batch at Init");
			return true;
		}

		if (OVT_ResourceProductionRules.ShouldProduce(5, 5))
		{
			SetFailure("ShouldProduce(5, 5) is true - a second call inside the same hour must be a no-op");
			return true;
		}

		if (!OVT_ResourceProductionRules.ShouldProduce(6, 5))
		{
			SetFailure("ShouldProduce(6, 5) is false - the next hour must produce");
			return true;
		}

		if (!OVT_ResourceProductionRules.ShouldProduce(0, 23))
		{
			SetFailure("ShouldProduce(0, 23) is false - hour 0 following a latch of 23 must still produce (the midnight rollover)");
			return true;
		}

		Print("Should produce: exactly once per hour, including across the midnight rollover");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Three colour states, no fewer: unowned, owned-and-accessible (yours, public, or resistance),
//! and owned-private-not-yours.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_ColourStateHasThreeAnswers : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int unowned = OVT_ResourceProductionRules.ColourState("me", "", false);
		if (unowned != 0)
		{
			SetFailure("ColourState('me', '', false) is %1, expected 0 (unowned)", unowned.ToString());
			return true;
		}

		int yours = OVT_ResourceProductionRules.ColourState("me", "me", true);
		if (yours != 1)
		{
			SetFailure("ColourState('me', 'me', true) is %1, expected 1 (accessible - yours)", yours.ToString());
			return true;
		}

		int publicSite = OVT_ResourceProductionRules.ColourState("stranger", "me", false);
		if (publicSite != 1)
		{
			SetFailure("ColourState('stranger', 'me', false) is %1, expected 1 (accessible - public)", publicSite.ToString());
			return true;
		}

		int resistance = OVT_ResourceProductionRules.ColourState("anyone", "resistance", true);
		if (resistance != 1)
		{
			SetFailure("ColourState('anyone', 'resistance', true) is %1, expected 1 (accessible - resistance)", resistance.ToString());
			return true;
		}

		int privateNotYours = OVT_ResourceProductionRules.ColourState("stranger", "me", true);
		if (privateNotYours != 2)
		{
			SetFailure("ColourState('stranger', 'me', true) is %1, expected 2 (private, not yours)", privateNotYours.ToString());
			return true;
		}

		Print("Colour state: exactly three answers - unowned, accessible, private-not-yours");

		return true;
	}
}
