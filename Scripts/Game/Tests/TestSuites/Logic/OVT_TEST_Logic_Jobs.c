//------------------------------------------------------------------------------------------------
//! TIER A cases - the pure job conditions.
//!
//! A job condition decides whether a job may start in a town. The three covered here are pure: they
//! read only the town record they are handed, so they can be exercised against a hand-built town
//! with no manager, no controller and no world. The conditions that resolve a player entity or ask
//! a manager for the nearest town are NOT in this tier and are not covered by this feature.
//!
//! THE `new` TRAP APPLIES HARDEST HERE. [Attribute()] defvalues are applied by the config loader,
//! not by `new`, and two of these conditions use a NEGATIVE sentinel for "unset" or a multiplier
//! whose neutral value is 1. A hand-built condition therefore starts with min/max = 0 (which is a
//! real constraint, not "unset") and with every chance factor = 0 (which zeroes the chance
//! entirely). Every case below sets every field explicitly for exactly that reason.
//!
//! ShouldStart() takes a base record and a player id as well as the town; both are unused by these
//! three conditions and are passed as null / -1.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! OVT_TownSupportJobCondition - minimum and maximum town support, and the "unset" sentinel.
//!
//! The condition compares OVT_TownData.SupportPercentage() against m_iMinSupport and m_iMaxSupport,
//! and treats any value BELOW ZERO as "no constraint" (`if (m_iMinSupport > -1 && ...)`). Both
//! bounds are INCLUSIVE: support exactly equal to the minimum or the maximum passes.
//!
//! The two towns used here sit at the ends of the range - 50 supporters of 50 civilians reads as
//! 100, and 0 of 50 reads as 0 - so that the case asserts the CONDITION's comparisons rather than
//! re-asserting the percentage maths that
//! OVT_TEST_Logic_Town_SupportPercentage_Boundaries already covers.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Jobs_TownSupportCondition_MinMaxAndUnset : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownData fullSupportTown = OVT_TEST_LogicFixture.MakeTown(50, 50);   // reads as 100
		OVT_TownData noSupportTown = OVT_TEST_LogicFixture.MakeTown(50, 0);      // reads as 0

		// Both bounds unset: the condition never blocks.
		OVT_TownSupportJobCondition unset = new OVT_TownSupportJobCondition();
		unset.m_iMinSupport = -1;
		unset.m_iMaxSupport = -1;

		if (!unset.ShouldStart(fullSupportTown, null, -1))
		{
			SetResultFailure("An unconstrained OVT_TownSupportJobCondition refused a town at 100 support");
			return true;
		}

		if (!unset.ShouldStart(noSupportTown, null, -1))
		{
			SetResultFailure("An unconstrained OVT_TownSupportJobCondition refused a town at 0 support");
			return true;
		}

		// Minimum only.
		OVT_TownSupportJobCondition minimum = new OVT_TownSupportJobCondition();
		minimum.m_iMinSupport = 50;
		minimum.m_iMaxSupport = -1;

		if (!minimum.ShouldStart(fullSupportTown, null, -1))
		{
			SetResultFailure("m_iMinSupport 50 refused a town at 100 support");
			return true;
		}

		if (minimum.ShouldStart(noSupportTown, null, -1))
		{
			SetResultFailure("m_iMinSupport 50 accepted a town at 0 support");
			return true;
		}

		// Maximum only.
		OVT_TownSupportJobCondition maximum = new OVT_TownSupportJobCondition();
		maximum.m_iMinSupport = -1;
		maximum.m_iMaxSupport = 50;

		if (!maximum.ShouldStart(noSupportTown, null, -1))
		{
			SetResultFailure("m_iMaxSupport 50 refused a town at 0 support");
			return true;
		}

		if (maximum.ShouldStart(fullSupportTown, null, -1))
		{
			SetResultFailure("m_iMaxSupport 50 accepted a town at 100 support");
			return true;
		}

		// Both bounds, both inclusive at their own edge.
		OVT_TownSupportJobCondition window = new OVT_TownSupportJobCondition();
		window.m_iMinSupport = 0;
		window.m_iMaxSupport = 100;

		if (!window.ShouldStart(noSupportTown, null, -1))
		{
			SetResultFailure("A [0, 100] window refused a town sitting exactly on its minimum");
			return true;
		}

		if (!window.ShouldStart(fullSupportTown, null, -1))
		{
			SetResultFailure("A [0, 100] window refused a town sitting exactly on its maximum");
			return true;
		}

		Print("Town support condition: unset passes both, min blocks the low town, max blocks the high town, bounds are inclusive");

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TownHasDealerJobCondition - a town has a gun dealer once its dealer position is set.
//!
//! The dealer position starts at the zero vector on a fresh town record and is written by the town
//! controller when it spawns a dealer at campaign start, so the condition is really asking "has
//! this town been given a dealer yet".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Jobs_DealerCondition_SetAndUnset : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownHasDealerJobCondition condition = new OVT_TownHasDealerJobCondition();

		OVT_TownData town = OVT_TEST_LogicFixture.MakeTown(50, 0);

		if (condition.ShouldStart(town, null, -1))
		{
			SetResultFailure("OVT_TownHasDealerJobCondition reported a dealer in a town whose gunDealerPosition is the zero vector");
			return true;
		}

		town.gunDealerPosition = "150 0 250";
		if (!condition.ShouldStart(town, null, -1))
		{
			SetResultFailure("OVT_TownHasDealerJobCondition reported no dealer in a town whose gunDealerPosition is %1", town.gunDealerPosition.ToString());
			return true;
		}

		Print("Dealer condition: zero vector -> no dealer, real position -> dealer");

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! PINS A SUSPECTED BUG - OVT_TownHasDealerJobCondition only inspects the X axis.
//!
//! The condition is `if (town.gunDealerPosition && town.gunDealerPosition[0] != 0)`, so a dealer
//! standing anywhere on the X = 0 plane reads as "this town has no dealer", no matter how far along
//! Z or Y they are. The intended test is plainly "is the position set", which the zero-vector check
//! alone does not express.
//!
//! Nothing breaks on Everon, where no town sits on X = 0, and the test world's town is at X = 208 -
//! which is exactly why this would never be noticed until a map put a town near the west edge of
//! the world.
//!
//! Asserts the CURRENT behaviour on purpose; fixing the condition must turn this case red. Logged
//! in findings.md under "Bugs found (log only)".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Jobs_DealerCondition_PinsAxisOnlyCheckBug : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownHasDealerJobCondition condition = new OVT_TownHasDealerJobCondition();

		OVT_TownData town = OVT_TEST_LogicFixture.MakeTown(50, 0);

		// A perfectly valid dealer position 500 m along Z, with X at exactly zero.
		town.gunDealerPosition = "0 0 500";
		if (condition.ShouldStart(town, null, -1))
		{
			SetResultFailure("PINNED BUG CHANGED: OVT_TownHasDealerJobCondition now reports a dealer at %1. The pin recorded that a position with X exactly 0 reads as 'no dealer'. If the condition was fixed, update this case and findings.md", town.gunDealerPosition.ToString());
			return true;
		}

		// A dealer with a non-zero X is found, confirming the axis is the only difference.
		town.gunDealerPosition = "1 0 500";
		if (!condition.ShouldStart(town, null, -1))
		{
			SetResultFailure("OVT_TownHasDealerJobCondition reported no dealer at %1 - the pinned difference is supposed to be the X axis alone", town.gunDealerPosition.ToString());
			return true;
		}

		Print("PINNED: dealer at X=0 reads as absent, the same dealer at X=1 reads as present");

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_RandomJobCondition at its DETERMINISTIC edges only.
//!
//! The condition rolls s_AIRandomGenerator.RandFloatXY(0, 100) against a chance that has first been
//! multiplied by a factor for each of low population, low stability and low support. Two edges are
//! deterministic and are the ones covered:
//!   - an effective chance of 0 can never be beaten, because the roll is never negative;
//!   - an effective chance ABOVE the roll's range always wins.
//!
//! The intermediate chances are genuinely random and are NOT covered - that is a property of the
//! subject, not a gap in the suite, and the framework's per-case retry attribute is banned by this
//! feature's quality bar.
//!
//! Note on the 100 edge: the plan names m_fChance 100 as the "always" edge, and it is asserted
//! here. Because the engine does not document whether RandFloatXY's upper bound is inclusive, the
//! case ALSO asserts a chance beyond the roll's range, which is always true regardless of that
//! semantic. If the 100 assertion ever fails while the 200 assertion passes, the bound is inclusive
//! and the plan's edge - not this suite - needs revisiting.
//!
//! The three factor assertions double as the clearest demonstration of the `new` trap: a factor of
//! 0 is what a hand-built condition starts with, and it silently zeroes the whole chance.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Jobs_RandomCondition_DeterministicEdges : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// Population 100, stability 100, support 100 % - no factor applies to this town, so the
		// effective chance is exactly m_fChance.
		OVT_TownData healthyTown = OVT_TEST_LogicFixture.MakeTown(100, 100, 100);

		OVT_RandomJobCondition never = MakeCondition(0, 1, 1, 1);
		if (never.ShouldStart(healthyTown, null, -1))
		{
			SetResultFailure("OVT_RandomJobCondition with m_fChance 0 started a job - the roll is never negative, so this can never be beaten");
			return true;
		}

		OVT_RandomJobCondition always = MakeCondition(100, 1, 1, 1);
		if (!always.ShouldStart(healthyTown, null, -1))
		{
			SetResultFailure("OVT_RandomJobCondition with m_fChance 100 refused to start a job");
			return true;
		}

		OVT_RandomJobCondition beyond = MakeCondition(200, 1, 1, 1);
		if (!beyond.ShouldStart(healthyTown, null, -1))
		{
			SetResultFailure("OVT_RandomJobCondition with a chance beyond the roll's range refused to start a job");
			return true;
		}

		// Low population multiplies the chance by m_fLowPopulationFactor. Zero it and a certainty
		// becomes an impossibility.
		OVT_TownData smallTown = OVT_TEST_LogicFixture.MakeTown(10, 10, 100);
		OVT_RandomJobCondition populationGated = MakeCondition(200, 0, 1, 1);
		if (populationGated.ShouldStart(smallTown, null, -1))
		{
			SetResultFailure("A zero low-population factor did not suppress a certain job in a town of 10 civilians");
			return true;
		}

		if (!populationGated.ShouldStart(healthyTown, null, -1))
		{
			SetResultFailure("The low-population factor was applied to a town of 100 civilians, which is not below the 50 threshold");
			return true;
		}

		// Low stability multiplies the chance by m_fLowStabilityFactor.
		OVT_TownData unstableTown = OVT_TEST_LogicFixture.MakeTown(100, 100, 40);
		OVT_RandomJobCondition stabilityGated = MakeCondition(200, 1, 0, 1);
		if (stabilityGated.ShouldStart(unstableTown, null, -1))
		{
			SetResultFailure("A zero low-stability factor did not suppress a certain job in a town at 40 stability");
			return true;
		}

		// Low support multiplies the chance by m_fLowSupportFactor.
		OVT_TownData unsupportiveTown = OVT_TEST_LogicFixture.MakeTown(100, 0, 100);
		OVT_RandomJobCondition supportGated = MakeCondition(200, 1, 1, 0);
		if (supportGated.ShouldStart(unsupportiveTown, null, -1))
		{
			SetResultFailure("A zero low-support factor did not suppress a certain job in a town at 0 support");
			return true;
		}

		Print("Random condition: chance 0 never starts, chance 100 and 200 always start, each low-X factor gates its own condition");

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a random job condition with every field set explicitly - `new` applies no attribute
	//! defvalues, so an unset factor would be 0 and would zero the chance.
	//! \param[in] chance Base chance, as a percentage.
	//! \param[in] populationFactor Multiplier applied when the town has fewer than 50 civilians.
	//! \param[in] stabilityFactor Multiplier applied when the town is below 50 stability.
	//! \param[in] supportFactor Multiplier applied when the town reads below 50 support.
	//! \return The condition.
	protected OVT_RandomJobCondition MakeCondition(float chance, float populationFactor, float stabilityFactor, float supportFactor)
	{
		OVT_RandomJobCondition condition = new OVT_RandomJobCondition();
		condition.m_fChance = chance;
		condition.m_fLowPopulationFactor = populationFactor;
		condition.m_fLowStabilityFactor = stabilityFactor;
		condition.m_fLowSupportFactor = supportFactor;
		return condition;
	}
}
