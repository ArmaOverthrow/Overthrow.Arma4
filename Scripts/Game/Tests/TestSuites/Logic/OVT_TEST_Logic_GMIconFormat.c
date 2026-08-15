//------------------------------------------------------------------------------------------------
//! TIER A case - the Game Master selection readout's display formatting.
//!
//! OVT_GMIconFormat is a class of pure statics and it is the entirety of that readout's automatable
//! surface: everything else it does is a text write into a widget inside the Game Master editor,
//! which no suite can open.
//!
//! WHAT IT PINS:
//!  - THE DIVISION GUARD. A settlement with a population of zero is a normal state before the
//!    campaign has counted one, and it is the single input that could make the support percentage
//!    throw. It must read "0%";
//!  - THE DIVISION ITSELF. 248 of 400 is 62%, and an implementation that divides two integers before
//!    scaling gets 0 - a readout that would claim no support at all for a town two thirds of the way
//!    to flipping;
//!  - THE EXTREMES. 0 reads "0%" and a full count reads "100%", so neither end is off by one;
//!  - THE POPULATION PAIR. Both numbers are shown, because either alone hides whether a settlement is
//!    growing or emptying;
//!  - READABILITY OF A CLASS NAME. A shared prefix is stripped and the remaining words are separated,
//!    and - the part that matters more - a name the formatter does not recognise passes through
//!    UNCHANGED rather than becoming an empty row. A reader who sees an unexpected class name learns
//!    something; a reader who sees a blank learns nothing;
//!  - THE ABSENT-INDEX SENTINEL. Several group origins carry -1 because no index means anything for
//!    them. A line reading "QRF -1" is a leaked sentinel, and this pins that it never renders.
//!
//! The expected strings below are stated by the display specification, never obtained by calling the
//! subject to find out what it does.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_GMIconFormat : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- SUPPORT PERCENTAGE, and the guard that keeps it from dividing by zero.
		if (!ExpectSupport(248, 400, "62%", "a little under two thirds"))
			return true;

		if (!ExpectSupport(0, 0, "0%", "nothing counted yet - must never divide by zero"))
			return true;

		if (!ExpectSupport(0, 400, "0%", "no support at all"))
			return true;

		if (!ExpectSupport(400, 400, "100%", "complete support"))
			return true;

		// --- POPULATION. Both halves, in that order.
		if (!ExpectPopulation(380, 400, "380 / 400", "growing towards its target"))
			return true;

		// --- UPGRADE READABILITY.
		if (!ExpectUpgradeType("OVT_BaseUpgradeSniperPosition", "Sniper Position", "the prefix goes and the words separate"))
			return true;

		if (!ExpectUpgradeType("SomethingElseEntirely", "SomethingElseEntirely", "an unrecognised name passes through rather than becoming empty"))
			return true;

		// --- ORIGIN WITH AN INDEX. Both the kind and the number have to survive into the line.
		string indexed = OVT_GMIconFormat.FormatOrigin(OVT_EGroupOrigin.BASE_PATROL, 3, "OVT_BaseUpgradePatrols");
		string patrolName = OVT_GMIconFormat.FormatOriginType(OVT_EGroupOrigin.BASE_PATROL);

		if (!indexed.Contains(patrolName))
		{
			SetFailure("an indexed origin formatted as '%1', which does not name its kind ('%2')", indexed, patrolName);
			return true;
		}

		if (!indexed.Contains("3"))
		{
			SetFailure("an indexed origin formatted as '%1', which does not carry its index (3)", indexed);
			return true;
		}

		// --- ORIGIN WITHOUT ONE. The reason stands alone and the sentinel never reaches a reader.
		string unindexed = OVT_GMIconFormat.FormatOrigin(OVT_EGroupOrigin.QRF, -1, "counter-attack");

		if (unindexed != "counter-attack")
		{
			SetFailure("an origin with no index formatted as '%1', expected the reason alone ('counter-attack')", unindexed);
			return true;
		}

		if (unindexed.Contains("-1"))
		{
			SetFailure("an origin with no index formatted as '%1' - the -1 sentinel must never reach a reader", unindexed);
			return true;
		}

		// --- PURITY. The same input gives the same answer, interleaved with a different query so a
		// cached-last-answer implementation cannot pass.
		string firstCall = OVT_GMIconFormat.FormatSupport(248, 400);
		OVT_GMIconFormat.FormatSupport(1, 3);
		string secondCall = OVT_GMIconFormat.FormatSupport(248, 400);
		if (firstCall != secondCall)
		{
			SetFailure("248 of 400 formatted as '%1' and then as '%2'; the formatter holds no state and must be repeatable",
				firstCall, secondCall);
			return true;
		}

		Print("GM icon formatting: support divides in floating point and guards a zero denominator, population shows both halves, an unknown upgrade class name passes through unchanged, and an absent origin index never renders as -1");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one formatted support percentage, naming the case on failure.
	//! \param[in] support Support points handed to the formatter.
	//! \param[in] population Population they are measured against.
	//! \param[in] expected String the display specification requires.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectSupport(int support, int population, string expected, string label)
	{
		string actual = OVT_GMIconFormat.FormatSupport(support, population);

		if (actual == expected)
			return true;

		SetFailure("%1: support " + support.ToString() + " of " + population.ToString() + " formatted as '%2', expected '%3'",
			label, actual, expected);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one formatted population pair, naming the case on failure.
	//! \param[in] population Current population.
	//! \param[in] targetPopulation Population it is heading towards.
	//! \param[in] expected String the display specification requires.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectPopulation(int population, int targetPopulation, string expected, string label)
	{
		string actual = OVT_GMIconFormat.FormatPopulation(population, targetPopulation);

		if (actual == expected)
			return true;

		SetFailure("%1: population " + population.ToString() + " of " + targetPopulation.ToString() + " formatted as '%2', expected '%3'",
			label, actual, expected);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one formatted upgrade class name, naming the case on failure.
	//! \param[in] className Class name handed to the formatter.
	//! \param[in] expected String the display specification requires.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectUpgradeType(string className, string expected, string label)
	{
		string actual = OVT_GMIconFormat.FormatUpgradeType(className);

		if (actual == expected)
			return true;

		SetFailure("%1: '%2' formatted as '%3', expected '" + expected + "'",
			label, className, actual);

		return false;
	}
}
