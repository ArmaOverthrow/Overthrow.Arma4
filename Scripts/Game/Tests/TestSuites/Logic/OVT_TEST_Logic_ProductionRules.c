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

//------------------------------------------------------------------------------------------------
//! The hand-built table and cart the SITE_BUY composition cases share.
//!
//! The four shipped resources with their real litres and base prices, built by hand so the money a
//! SITE_BUY cart costs is asserted against a table rather than against a live catalogue.
//------------------------------------------------------------------------------------------------
class OVT_TEST_ProductionCartFixture
{
	static const string TIMBER = "timber";
	static const string CEMENT = "cement";
	static const string STEEL = "steel";
	static const string HARDWARE = "hardware";

	//------------------------------------------------------------------------------------------------
	//! \return A table matching the shipped resources.conf: litres are m³ × 1000.
	static OVT_ResourceDefs MakeDefs()
	{
		OVT_ResourceDefs defs = new OVT_ResourceDefs();

		defs.AddDef(TIMBER, 100, 25, 40, 1, 0);
		defs.AddDef(CEMENT, 50, 50, 60, 1, 0);
		defs.AddDef(STEEL, 40, 90, 120, 1, 0);
		defs.AddDef(HARDWARE, 20, 10, 200, 1, 0);

		return defs;
	}

	//------------------------------------------------------------------------------------------------
	//! One cart line, every field set explicitly.
	//! \param[in] resIndex Definition index.
	//! \param[in] quantity How many units.
	//! \return The line.
	static OVT_ResourceCartLine MakeLine(int resIndex, int quantity)
	{
		OVT_ResourceCartLine line = new OVT_ResourceCartLine();
		line.m_iResIndex = resIndex;
		line.m_iQuantity = quantity;

		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! What the server's commit loop sums for a SITE_BUY: the site price of the LIVE price at each
	//! line's OWN index, times that line's OWN quantity.
	//! \param[in] cart The cart lines.
	//! \param[in] livePrices Live import price per definition index.
	//! \return The money total.
	static int CartTotal(array<ref OVT_ResourceCartLine> cart, array<int> livePrices)
	{
		int total = 0;

		foreach (OVT_ResourceCartLine line : cart)
		{
			int unit = OVT_ResourceProductionRules.SitePrice(livePrices[line.m_iResIndex], OVT_ResourceProductionRules.SITE_SELL_RATIO);
			total = total + (unit * line.m_iQuantity);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! What the same cart weighs in litres, from the same table.
	//! \param[in] defs The definition table.
	//! \param[in] cart The cart lines.
	//! \return The litre total.
	static int CartLitres(OVT_ResourceDefs defs, array<ref OVT_ResourceCartLine> cart)
	{
		int litres = 0;

		foreach (OVT_ResourceCartLine line : cart)
		{
			litres = litres + (defs.LitresAt(line.m_iResIndex) * line.m_iQuantity);
		}

		return litres;
	}
}

//------------------------------------------------------------------------------------------------
//! A SITE_BUY cart costs the sum of each line's own site price times its own quantity, and weighs
//! the sum of each line's own litres. Both are keyed on the LINE's index, so a rule that priced the
//! whole cart at one resource's rate would total differently.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_SitePriceComposesOverACart : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ProductionCartFixture.MakeDefs();

		// Deliberately NOT the base prices in the table. The site quotes the LIVE import price, so a
		// case built on the base price would pass against a rule that read the wrong one.
		array<int> live = new array<int>();
		live.Insert(50);
		live.Insert(75);
		live.Insert(150);
		live.Insert(200);

		array<ref OVT_ResourceCartLine> cart = new array<ref OVT_ResourceCartLine>();
		cart.Insert(OVT_TEST_ProductionCartFixture.MakeLine(0, 20));
		cart.Insert(OVT_TEST_ProductionCartFixture.MakeLine(2, 5));
		cart.Insert(OVT_TEST_ProductionCartFixture.MakeLine(1, 3));

		foreach (OVT_ResourceCartLine line : cart)
		{
			if (defs.IdAt(line.m_iResIndex) == "")
			{
				SetFailure("The fixture cart names definition index %1, which the table does not hold", line.m_iResIndex.ToString());
				return true;
			}
		}

		// SitePrice(50) 40 × 20, SitePrice(150) 120 × 5, SitePrice(75) 60 × 3.
		int total = OVT_TEST_ProductionCartFixture.CartTotal(cart, live);
		if (total != 1580)
		{
			SetFailure("A 20 timber / 5 steel / 3 cement cart totalled %1, expected 1580 (40x20 + 120x5 + 60x3)", total.ToString());
			return true;
		}

		// 100 × 20 + 40 × 5 + 50 × 3.
		int litres = OVT_TEST_ProductionCartFixture.CartLitres(defs, cart);
		if (litres != 2350)
		{
			SetFailure("The same cart weighed %1 litres, expected 2350 (100x20 + 40x5 + 50x3)", litres.ToString());
			return true;
		}

		// The same three quantities against different resources. A flat per-cart rate would tie.
		array<ref OVT_ResourceCartLine> swapped = new array<ref OVT_ResourceCartLine>();
		swapped.Insert(OVT_TEST_ProductionCartFixture.MakeLine(0, 5));
		swapped.Insert(OVT_TEST_ProductionCartFixture.MakeLine(2, 20));
		swapped.Insert(OVT_TEST_ProductionCartFixture.MakeLine(1, 3));

		int swappedTotal = OVT_TEST_ProductionCartFixture.CartTotal(swapped, live);
		if (swappedTotal == total)
		{
			SetFailure("Moving the quantities between resources left the total at %1 - the price is not being read at each line's own index", swappedTotal.ToString());
			return true;
		}

		if (swappedTotal != 2780)
		{
			SetFailure("The swapped cart totalled %1, expected 2780 (40x5 + 120x20 + 60x3)", swappedTotal.ToString());
			return true;
		}

		Print("Site cart: priced and weighed per line, at each line's own definition index");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The worst cart the server can legally be asked to price - every line at MAX_LINE_QUANTITY, at a
//! live price far above anything a shipped resource can drift to - stays a POSITIVE int.
//!
//! This is R6's headroom. PlayerHasMoney() accepts a negative amount and TakePlayerMoney() of a
//! negative PAYS the player, so a wrapped total is an exploit that prints money. Raising
//! MAX_LINE_QUANTITY or the cart-line cap re-derives this case rather than quietly reopening it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_SiteCartHasHeadroomAtTheShippedBounds : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The m_iMaxCartLines attribute default on OVT_ResourceRequestComponent. An attribute cannot be
		// read without a world, so it is restated here and the case fails loudly if the maths stops
		// fitting - which is the only thing this number is used for.
		int maxLines = 16;

		int qty = OVT_ResourceRequestComponent.MAX_LINE_QUANTITY;
		if (qty <= 0)
		{
			SetFailure("MAX_LINE_QUANTITY is %1 - the per-line bound is the first of the three money bounds and cannot be zero or negative", qty.ToString());
			return true;
		}

		// 25× the dearest price any shipped resource can reach: base 200, price band max 2.0.
		int unit = OVT_ResourceProductionRules.SitePrice(10000, OVT_ResourceProductionRules.SITE_SELL_RATIO);
		if (unit != 8000)
		{
			SetFailure("SitePrice(10000, 0.8) is %1, expected 8000", unit.ToString());
			return true;
		}

		int total = 0;

		for (int i = 0; i < maxLines; i++)
		{
			total = total + (unit * qty);

			if (total <= 0)
			{
				SetFailure("The worst legal SITE_BUY cart wrapped to %1 at line %2 - a negative total passes PlayerHasMoney and pays the player", total.ToString(), i.ToString());
				return true;
			}
		}

		int expected = maxLines * unit * qty;
		if (total != expected)
		{
			SetFailure("The bounded cart totalled %1, expected %2", total.ToString(), expected.ToString());
			return true;
		}

		// The dearest per-unit price the bounds can carry without wrapping.
		int ceiling = int.MAX / (maxLines * qty);
		if (unit > ceiling)
		{
			SetFailure("A unit price of %1 is above the %2 the bounds can carry - the cart total would wrap before the tripwire could read it", unit.ToString(), ceiling.ToString());
			return true;
		}

		Print("Site cart: 16 lines at MAX_LINE_QUANTITY stay positive with headroom to spare");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A site undercuts the port it competes with, and still sells for MORE than a port pays - so
//! buying stock at a site and exporting it at a port always loses money.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ProdRules_SitePriceUndercutsThePortPerResource : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ProductionCartFixture.MakeDefs();
		if (defs.Count() != 4)
		{
			SetFailure("The fixture table holds %1 definitions, expected 4", defs.Count().ToString());
			return true;
		}

		for (int i = 0; i < defs.Count(); i++)
		{
			string id = defs.IdAt(i);
			int live = defs.BasePriceAt(i);

			int site = OVT_ResourceProductionRules.SitePrice(live, OVT_ResourceProductionRules.SITE_SELL_RATIO);
			if (site < 1)
			{
				SetFailure("'%1' priced at %2 - a site price is never below 1", id, site.ToString());
				return true;
			}

			if (site >= live)
			{
				SetFailure("'%1' costs %2 at a site against %3 at a port - a site must undercut the port it competes with", id, site.ToString(), live.ToString());
				return true;
			}

			int portPays = OVT_ResourceRules.SellPrice(live, 0.5);
			if (site <= portPays)
			{
				SetFailure("'%1' costs %2 at a site and a port pays %3 for it - buying at a site and exporting would be free money", id, site.ToString(), portPays.ToString());
				return true;
			}
		}

		Print("Site price: undercuts the port on every shipped resource, and never below what a port pays");

		return true;
	}
}
