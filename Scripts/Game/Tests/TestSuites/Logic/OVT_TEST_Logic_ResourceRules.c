//------------------------------------------------------------------------------------------------
//! TIER A cases - the packed wire form and the pure resource rules.
//!
//! OVT_ResourceRules takes every lookup as an argument (the clock hour, the threat, the port
//! fraction, the squared distances), so each case hands in literals and asserts one decision.
//! Nothing is constructed but a definition table, a ledger and plain arrays.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The hand-built tables these cases share. One resource is deliberately non-importable and one is
//! deliberately illegal - no shipped resource is either, and D20 is only true if a test says so.
//------------------------------------------------------------------------------------------------
class OVT_TEST_ResourceRulesFixture
{
	static const string TIMBER = "timber";
	static const string CEMENT = "cement";
	static const string STEEL = "steel";
	static const string RUBBLE = "rubble";
	static const string CONTRABAND = "contraband";

	//------------------------------------------------------------------------------------------------
	//! Three ordinary resources, one non-importable, one illegal.
	//! \return A fully populated table.
	static OVT_ResourceDefs MakeDefs()
	{
		OVT_ResourceDefs defs = new OVT_ResourceDefs();

		defs.AddDef(TIMBER, 100, 25, 40, 1, 0);
		defs.AddDef(CEMENT, 50, 50, 60, 1, 0);
		defs.AddDef(STEEL, 40, 90, 120, 1, 0);
		defs.AddDef(RUBBLE, 10, 5, 5, 0, 0);
		defs.AddDef(CONTRABAND, 10, 5, 900, 1, 1);

		return defs;
	}

	//------------------------------------------------------------------------------------------------
	//! One amount record, every field set explicitly.
	//! \param[in] id Stable resource id.
	//! \param[in] qty Units.
	//! \return The record.
	static OVT_ResourceAmount MakeAmount(string id, int qty)
	{
		OVT_ResourceAmount amount = new OVT_ResourceAmount();
		amount.m_sId = id;
		amount.m_iQuantity = qty;

		return amount;
	}
}

//------------------------------------------------------------------------------------------------
//! Encode -> Decode reproduces every line, and an empty ledger encodes as "". The packed string is
//! the ENTIRE replicated surface of a holder, so a dropped separator is a dropped cargo.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_PackRoundTrips : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceRulesFixture.MakeDefs();
		OVT_ResourceLedger source = new OVT_ResourceLedger();

		string empty = OVT_ResourcePack.Encode(source, defs);
		if (empty != "")
		{
			SetFailure("An empty ledger encoded as '%1', expected an empty string", empty);
			return true;
		}

		source.Add(OVT_TEST_ResourceRulesFixture.TIMBER, 12, defs, -1);
		source.Add(OVT_TEST_ResourceRulesFixture.CEMENT, 7, defs, -1);
		source.Add(OVT_TEST_ResourceRulesFixture.STEEL, 3, defs, -1);

		string packed = OVT_ResourcePack.Encode(source, defs);
		if (packed == "")
		{
			SetFailure("A three-line ledger encoded as an empty string");
			return true;
		}

		OVT_ResourceLedger mirror = new OVT_ResourceLedger();
		if (!OVT_ResourcePack.Decode(packed, defs, mirror))
		{
			SetFailure("Decode() rejected the string Encode() produced: '%1'", packed);
			return true;
		}

		if (mirror.LineCount() != 3)
		{
			SetFailure("The decoded mirror has %1 lines, expected 3 (packed: '%2')", mirror.LineCount().ToString(), packed);
			return true;
		}

		if (mirror.Count(OVT_TEST_ResourceRulesFixture.TIMBER) != 12)
		{
			SetFailure("The decoded mirror holds %1 timber, expected 12 (packed: '%2')", mirror.Count(OVT_TEST_ResourceRulesFixture.TIMBER).ToString(), packed);
			return true;
		}

		if (mirror.Count(OVT_TEST_ResourceRulesFixture.CEMENT) != 7)
		{
			SetFailure("The decoded mirror holds %1 cement, expected 7 (packed: '%2')", mirror.Count(OVT_TEST_ResourceRulesFixture.CEMENT).ToString(), packed);
			return true;
		}

		if (mirror.Count(OVT_TEST_ResourceRulesFixture.STEEL) != 3)
		{
			SetFailure("The decoded mirror holds %1 steel, expected 3 (packed: '%2')", mirror.Count(OVT_TEST_ResourceRulesFixture.STEEL).ToString(), packed);
			return true;
		}

		if (mirror.TotalLitres(defs) != source.TotalLitres(defs))
		{
			SetFailure("The decoded mirror holds %1 litres, expected the source's %2", mirror.TotalLitres(defs).ToString(), source.TotalLitres(defs).ToString());
			return true;
		}

		// An empty payload is a legal state: it clears the mirror rather than failing.
		if (!OVT_ResourcePack.Decode("", defs, mirror))
		{
			SetFailure("Decode('') failed, expected an empty payload to clear the mirror and succeed");
			return true;
		}

		if (mirror.LineCount() != 0)
		{
			SetFailure("Decode('') left %1 lines, expected the mirror cleared", mirror.LineCount().ToString());
			return true;
		}

		Print("Resource pack: encode/decode round-trips every line and an empty ledger packs as an empty string");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A malformed payload returns false and leaves the target UNTOUCHED. Half-applying a corrupt
//! RplProp would leave a client's cargo readout lying rather than stale.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_PackRejectsMalformed : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceRulesFixture.MakeDefs();
		OVT_ResourceLedger target = new OVT_ResourceLedger();
		target.Add(OVT_TEST_ResourceRulesFixture.TIMBER, 5, defs, -1);

		array<string> bad = new array<string>();
		bad.Insert("garbage");
		bad.Insert("0");
		bad.Insert("0:x");
		bad.Insert("x:1");
		bad.Insert("0:1|");
		bad.Insert("0:1|1");
		bad.Insert("99:1");
		bad.Insert("-1:1");
		bad.Insert("0:0");
		bad.Insert("0:1:2");

		for (int i = 0; i < bad.Count(); i++)
		{
			if (OVT_ResourcePack.Decode(bad[i], defs, target))
			{
				SetFailure("Decode() accepted the malformed payload '%1'", bad[i]);
				return true;
			}

			if (target.Count(OVT_TEST_ResourceRulesFixture.TIMBER) != 5)
			{
				SetFailure("The malformed payload '%1' mutated the target: timber is now %2, expected the original 5", bad[i], target.Count(OVT_TEST_ResourceRulesFixture.TIMBER).ToString());
				return true;
			}

			if (target.LineCount() != 1)
			{
				SetFailure("The malformed payload '%1' left %2 lines, expected the original 1", bad[i], target.LineCount().ToString());
				return true;
			}
		}

		// The same target still accepts a well-formed payload afterwards.
		if (!OVT_ResourcePack.Decode("1:4", defs, target))
		{
			SetFailure("Decode() rejected a well-formed payload after a run of malformed ones");
			return true;
		}

		if (target.Count(OVT_TEST_ResourceRulesFixture.CEMENT) != 4)
		{
			SetFailure("The well-formed payload decoded %1 cement, expected 4", target.Count(OVT_TEST_ResourceRulesFixture.CEMENT).ToString());
			return true;
		}

		Print("Resource pack: every malformed payload is refused and the target is left standing");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The walk is clamped at BOTH band edges, and never below 1. An unclamped walk compounds every six
//! hours and a resource ends the campaign free or unaffordable.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_DriftClampsAtBothEdges : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// base 100, band 0.5 - 2.0, step fraction 0.08, volatility 1.
		int high = OVT_ResourceRules.DriftStep(100, 100, 50, 0, 1, 0.08, 0.5, 2.0);
		if (high != 200)
		{
			SetFailure("A huge positive roll walked to %1, expected the upper edge 200", high.ToString());
			return true;
		}

		int low = OVT_ResourceRules.DriftStep(100, 100, -50, 0, 1, 0.08, 0.5, 2.0);
		if (low != 50)
		{
			SetFailure("A huge negative roll walked to %1, expected the lower edge 50", low.ToString());
			return true;
		}

		// An ordinary roll moves and stays inside the band.
		int ordinary = OVT_ResourceRules.DriftStep(100, 100, 1, 0, 1, 0.08, 0.5, 2.0);
		if (ordinary != 108)
		{
			SetFailure("A full positive roll walked to %1, expected 108", ordinary.ToString());
			return true;
		}

		// A band whose lower edge rounds to zero still cannot produce a free resource.
		int floored = OVT_ResourceRules.DriftStep(1, 1, -50, 0, 1, 0.08, 0.0, 2.0);
		if (floored != 1)
		{
			SetFailure("A band with a zero lower edge produced a price of %1, expected the floor 1", floored.ToString());
			return true;
		}

		// Pressure is clamped by the same edges as the roll.
		int pressured = OVT_ResourceRules.DriftStep(100, 190, 1, 1, 1, 0.08, 0.5, 2.0);
		if (pressured != 200)
		{
			SetFailure("A pressured step from 190 landed on %1, expected the upper edge 200", pressured.ToString());
			return true;
		}

		Print("Resource drift: clamped at both band edges and floored at 1");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Volatility scales the STEP, never the band. Doubling it doubles the movement and leaves both
//! clamp edges exactly where they were.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_DriftVolatilityScalesStepNotBand : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int single = OVT_ResourceRules.DriftStep(100, 100, 1, 0, 1, 0.08, 0.5, 2.0);
		int doubled = OVT_ResourceRules.DriftStep(100, 100, 1, 0, 2, 0.08, 0.5, 2.0);

		int singleStep = single - 100;
		int doubledStep = doubled - 100;

		if (singleStep != 8)
		{
			SetFailure("Volatility 1 moved the price by %1, expected 8", singleStep.ToString());
			return true;
		}

		if (doubledStep != (singleStep * 2))
		{
			SetFailure("Volatility 2 moved the price by %1, expected twice the volatility-1 step of %2", doubledStep.ToString(), singleStep.ToString());
			return true;
		}

		// The edges are identical whatever the volatility.
		int highSingle = OVT_ResourceRules.DriftStep(100, 100, 50, 0, 1, 0.08, 0.5, 2.0);
		int highDoubled = OVT_ResourceRules.DriftStep(100, 100, 50, 0, 2, 0.08, 0.5, 2.0);
		if (highSingle != highDoubled)
		{
			SetFailure("The upper edge moved with volatility: %1 at 1, %2 at 2", highSingle.ToString(), highDoubled.ToString());
			return true;
		}

		int lowSingle = OVT_ResourceRules.DriftStep(100, 100, -50, 0, 1, 0.08, 0.5, 2.0);
		int lowDoubled = OVT_ResourceRules.DriftStep(100, 100, -50, 0, 2, 0.08, 0.5, 2.0);
		if (lowSingle != lowDoubled)
		{
			SetFailure("The lower edge moved with volatility: %1 at 1, %2 at 2", lowSingle.ToString(), lowDoubled.ToString());
			return true;
		}

		if (highDoubled != 200 || lowDoubled != 50)
		{
			SetFailure("At volatility 2 the edges are %1 and %2, expected 200 and 50", highDoubled.ToString(), lowDoubled.ToString());
			return true;
		}

		Print("Resource drift: volatility scales the step and leaves both band edges untouched");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The difficulty multiplier is applied at READ time, after the band clamp, so on a hard difficulty
//! the live price may legitimately exceed base x bandMax. Applying it before the clamp would eat
//! the whole difficulty setting.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_DriftMultiplierAppliesAfterClamp : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The stored price is already at the upper edge of a 100-base, 2.0-max band.
		int stored = OVT_ResourceRules.DriftStep(100, 100, 50, 0, 1, 0.08, 0.5, 2.0);
		if (stored != 200)
		{
			SetFailure("The stored price is %1, expected the clamped upper edge 200", stored.ToString());
			return true;
		}

		int live = OVT_ResourceRules.ApplyLevelMultiplier(stored, 1.5);
		if (live != 300)
		{
			SetFailure("The live price is %1, expected round(200 x 1.5) = 300", live.ToString());
			return true;
		}

		int bandCeiling = 200;
		if (live <= bandCeiling)
		{
			SetFailure("The live price %1 did not exceed the band ceiling %2 - the multiplier was applied before the clamp", live.ToString(), bandCeiling.ToString());
			return true;
		}

		// A multiplier of 1 is the identity, and the floor still holds under a tiny one.
		if (OVT_ResourceRules.ApplyLevelMultiplier(stored, 1) != stored)
		{
			SetFailure("A multiplier of 1 changed the price to %1", OVT_ResourceRules.ApplyLevelMultiplier(stored, 1).ToString());
			return true;
		}

		int floored = OVT_ResourceRules.ApplyLevelMultiplier(1, 0.01);
		if (floored != 1)
		{
			SetFailure("A tiny multiplier produced a live price of %1, expected the floor 1", floored.ToString());
			return true;
		}

		Print("Resource drift: the difficulty multiplier is a read-time transform applied after the clamp");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! One drift per six-hour window. The latch is the hour that was last drifted, so a tick that fires
//! every minute must walk the prices exactly once inside a window.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_DriftOneStepPerWindow : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_ResourceRules.ShouldDrift(6, -1))
		{
			SetFailure("The first tick inside the 06:00 window did not drift");
			return true;
		}

		// Latched on 6 now.
		if (OVT_ResourceRules.ShouldDrift(6, 6))
		{
			SetFailure("A second tick inside the same 06:00 window drifted again");
			return true;
		}

		if (!OVT_ResourceRules.ShouldDrift(12, 6))
		{
			SetFailure("The 12:00 window did not drift with the latch still on 06:00");
			return true;
		}

		if (OVT_ResourceRules.ShouldDrift(7, 6))
		{
			SetFailure("An hour that is not a drift window drifted");
			return true;
		}

		if (OVT_ResourceRules.ShouldDrift(23, -1))
		{
			SetFailure("23:00 drifted; the windows are 0, 6, 12 and 18 only");
			return true;
		}

		if (!OVT_ResourceRules.ShouldDrift(0, 18))
		{
			SetFailure("Midnight did not drift with the latch on 18:00");
			return true;
		}

		if (!OVT_ResourceRules.IsDriftWindowHour(18))
		{
			SetFailure("18:00 is not recognised as a drift window");
			return true;
		}

		Print("Resource drift: exactly one step per six-hour window, latched on the hour");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Threat pushes prices UP, resistance port control pushes them DOWN, and the term never leaves
//! [-1, 1]. A flipped control sign would reward the player for losing ports.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_WarPressureDirection : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		float calm = OVT_ResourceRules.WarPressure(0, 2000, 0);
		float busy = OVT_ResourceRules.WarPressure(1000, 2000, 0);
		float war = OVT_ResourceRules.WarPressure(4000, 2000, 0);

		if (busy <= calm)
		{
			SetFailure("Threat did not push pressure up: %1 at zero threat, %2 at 1000", calm.ToString(), busy.ToString());
			return true;
		}

		if (Math.AbsFloat(busy - 0.5) > 0.0001)
		{
			SetFailure("Half the reference threat gave a pressure of %1, expected 0.5", busy.ToString());
			return true;
		}

		if (Math.AbsFloat(war - 1) > 0.0001)
		{
			SetFailure("Twice the reference threat gave a pressure of %1, expected the clamped 1", war.ToString());
			return true;
		}

		float held = OVT_ResourceRules.WarPressure(4000, 2000, 1);
		if (held >= war)
		{
			SetFailure("Controlling every port did not push pressure down: %1 with no ports, %2 with all of them", war.ToString(), held.ToString());
			return true;
		}

		if (Math.AbsFloat(held - 0.5) > 0.0001)
		{
			SetFailure("Full port control at maximum threat gave a pressure of %1, expected 0.5", held.ToString());
			return true;
		}

		float peace = OVT_ResourceRules.WarPressure(0, 2000, 1);
		if (peace >= 0)
		{
			SetFailure("Full port control with no threat gave a pressure of %1, expected a negative term", peace.ToString());
			return true;
		}

		if (peace < -1 || war > 1)
		{
			SetFailure("Pressure left [-1, 1]: %1 and %2", peace.ToString(), war.ToString());
			return true;
		}

		// A zero reference must not divide by zero; the threat term simply drops out.
		float noReference = OVT_ResourceRules.WarPressure(4000, 0, 0);
		if (Math.AbsFloat(noReference) > 0.0001)
		{
			SetFailure("A zero threat reference gave a pressure of %1, expected 0", noReference.ToString());
			return true;
		}

		Print("Resource drift: threat raises pressure, port control lowers it, always inside [-1, 1]");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Sell price follows the LIVE price, not the stored one - there is no second walk and no second
//! difficulty transform. Pricing the sell off the stored price would leak the multiplier.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_SellFollowsLive : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int sell = OVT_ResourceRules.SellPrice(120, 0.5);
		if (sell != 60)
		{
			SetFailure("SellPrice(120, 0.5) is %1, expected 60", sell.ToString());
			return true;
		}

		int stored = 100;
		int live = OVT_ResourceRules.ApplyLevelMultiplier(stored, 1.5);
		int sellFromLive = OVT_ResourceRules.SellPrice(live, 0.5);
		int sellFromStored = OVT_ResourceRules.SellPrice(stored, 0.5);

		if (sellFromLive != 75)
		{
			SetFailure("The sell price off a live 150 is %1, expected 75", sellFromLive.ToString());
			return true;
		}

		if (sellFromLive == sellFromStored)
		{
			SetFailure("The sell price is the same off the live and the stored price (%1) - it must follow the live one", sellFromLive.ToString());
			return true;
		}

		int floored = OVT_ResourceRules.SellPrice(1, 0.1);
		if (floored != 1)
		{
			SetFailure("SellPrice(1, 0.1) is %1, expected the floor 1", floored.ToString());
			return true;
		}

		Print("Resource prices: sell is one ratio off the live price, floored at 1");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Merge selection: nearest within the radius wins, an exact distance tie goes to the LARGER pile,
//! and nothing outside the radius merges. Squared distances throughout - vector.Distance is not
//! correctly rounded, so a 1000 m boundary decided on it is a coin flip.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_MergeSelectsNearestThenLargest : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<float> sqDistances = new array<float>();
		sqDistances.Insert(100);
		sqDistances.Insert(25);
		sqDistances.Insert(25);
		sqDistances.Insert(400);

		array<int> litres = new array<int>();
		litres.Insert(500);
		litres.Insert(100);
		litres.Insert(300);
		litres.Insert(900);

		int target = OVT_ResourceRules.SelectMergeTarget(sqDistances, litres, 900);
		if (target != 2)
		{
			SetFailure("The merge target is index %1, expected 2 - nearest, and the larger of the two tied piles", target.ToString());
			return true;
		}

		// Nothing in range.
		target = OVT_ResourceRules.SelectMergeTarget(sqDistances, litres, 10);
		if (target != -1)
		{
			SetFailure("A radius that reaches nothing selected index %1, expected -1", target.ToString());
			return true;
		}

		// A pile exactly on the radius merges, and the decision is exact even at 1000 m, where an
		// unsquared distance comparison would be at the mercy of the rounding.
		array<float> boundary = new array<float>();
		boundary.Insert(1000000);

		array<int> boundaryLitres = new array<int>();
		boundaryLitres.Insert(50);

		target = OVT_ResourceRules.SelectMergeTarget(boundary, boundaryLitres, 1000000);
		if (target != 0)
		{
			SetFailure("A pile exactly on a 1000 m radius selected %1, expected 0", target.ToString());
			return true;
		}

		boundary.Set(0, 1000001);
		target = OVT_ResourceRules.SelectMergeTarget(boundary, boundaryLitres, 1000000);
		if (target != -1)
		{
			SetFailure("A pile one squared metre outside the radius selected %1, expected -1", target.ToString());
			return true;
		}

		// A full tie - same distance, same litres - keeps the first candidate.
		array<float> tied = new array<float>();
		tied.Insert(9);
		tied.Insert(9);

		array<int> tiedLitres = new array<int>();
		tiedLitres.Insert(200);
		tiedLitres.Insert(200);

		target = OVT_ResourceRules.SelectMergeTarget(tied, tiedLitres, 100);
		if (target != 0)
		{
			SetFailure("A full tie selected index %1, expected the first candidate 0", target.ToString());
			return true;
		}

		if (OVT_ResourceRules.SelectMergeTarget(null, litres, 900) != -1)
		{
			SetFailure("A null candidate list did not select -1");
			return true;
		}

		Print("Resource merge: nearest by squared distance, ties to the larger pile, -1 outside the radius");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A non-zero requirement never scales away to nothing, and a zero requirement stays zero. An
//! easy difficulty must make a building cheaper, never free.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_RequirementScaleNeverZero : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int scaled = OVT_ResourceRules.ScaleRequirement(1, 0.1);
		if (scaled != 1)
		{
			SetFailure("ScaleRequirement(1, 0.1) is %1, expected the floor 1 - a requirement never scales to free", scaled.ToString());
			return true;
		}

		scaled = OVT_ResourceRules.ScaleRequirement(0, 5);
		if (scaled != 0)
		{
			SetFailure("ScaleRequirement(0, 5) is %1, expected 0 - the floor applies only to a real requirement", scaled.ToString());
			return true;
		}

		scaled = OVT_ResourceRules.ScaleRequirement(40, 1.5);
		if (scaled != 60)
		{
			SetFailure("ScaleRequirement(40, 1.5) is %1, expected 60", scaled.ToString());
			return true;
		}

		scaled = OVT_ResourceRules.ScaleRequirement(40, 1);
		if (scaled != 40)
		{
			SetFailure("A multiplier of 1 scaled 40 to %1", scaled.ToString());
			return true;
		}

		scaled = OVT_ResourceRules.ScaleRequirement(-5, 2);
		if (scaled != 0)
		{
			SetFailure("A negative requirement scaled to %1, expected 0", scaled.ToString());
			return true;
		}

		Print("Resource requirements: scaling is rounded, floored at 1 for a real requirement, and 0 stays 0");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Satisfaction sums availability ACROSS piles, is exact at short-by-one, and names the first
//! shortfall. Summing only the nearest pile is the plausible bug: a supply radius holds several.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_RequirementSatisfied : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		need.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.TIMBER, 40));
		need.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.STEEL, 10));

		// Four piles: the timber is spread across three of them.
		array<ref OVT_ResourceAmount> have = new array<ref OVT_ResourceAmount>();
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.TIMBER, 20));
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.TIMBER, 15));
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.STEEL, 10));
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.TIMBER, 5));

		string shortId;
		if (!OVT_ResourceRules.IsSatisfied(need, have, shortId))
		{
			SetFailure("An exactly-met requirement was reported short on '%1' - availability must sum across every pile", shortId);
			return true;
		}

		if (shortId != "")
		{
			SetFailure("A satisfied requirement named '%1' as short, expected an empty id", shortId);
			return true;
		}

		// Short by exactly one unit of timber.
		have[3].m_iQuantity = 4;
		if (OVT_ResourceRules.IsSatisfied(need, have, shortId))
		{
			SetFailure("A requirement short by one unit was reported satisfied");
			return true;
		}

		if (shortId != OVT_TEST_ResourceRulesFixture.TIMBER)
		{
			SetFailure("The shortfall was named '%1', expected 'timber'", shortId);
			return true;
		}

		// Surplus is fine.
		have[3].m_iQuantity = 500;
		if (!OVT_ResourceRules.IsSatisfied(need, have, shortId))
		{
			SetFailure("A surplus was reported short on '%1'", shortId);
			return true;
		}

		// The FIRST shortfall in requirement order is the one that is named.
		have[2].m_iQuantity = 0;
		have[0].m_iQuantity = 0;
		have[1].m_iQuantity = 0;
		have[3].m_iQuantity = 0;
		if (OVT_ResourceRules.IsSatisfied(need, have, shortId))
		{
			SetFailure("An empty supply was reported satisfied");
			return true;
		}

		if (shortId != OVT_TEST_ResourceRulesFixture.TIMBER)
		{
			SetFailure("With everything short, '%1' was named, expected the first requirement 'timber'", shortId);
			return true;
		}

		// Nothing required is trivially satisfied.
		array<ref OVT_ResourceAmount> nothing = new array<ref OVT_ResourceAmount>();
		if (!OVT_ResourceRules.IsSatisfied(nothing, have, shortId))
		{
			SetFailure("An empty requirement list was reported short on '%1'", shortId);
			return true;
		}

		Print("Resource requirements: availability sums across piles, exact at short-by-one, first shortfall named");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Piles are drained nearest first, stably, and the order array is an ORDERED list: removing the
//! pile at index 1 must leave the rest in place. array.Remove() is swap-with-last in EnforceScript,
//! which a three-element fixture cannot catch - hence four piles.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_ConsumptionOrderIsNearestFirst : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<float> sqDistances = new array<float>();
		sqDistances.Insert(400);
		sqDistances.Insert(25);
		sqDistances.Insert(25);
		sqDistances.Insert(100);

		array<int> order = new array<int>();
		OVT_ResourceRules.SortPilesForConsumption(sqDistances, order);

		if (order.Count() != 4)
		{
			SetFailure("The drain order has %1 entries, expected 4", order.Count().ToString());
			return true;
		}

		if (order[0] != 1)
		{
			SetFailure("The first pile drained is index %1, expected 1 - nearest first, and stable on the tie with index 2", order[0].ToString());
			return true;
		}

		if (order[1] != 2)
		{
			SetFailure("The second pile drained is index %1, expected 2 - the tie must keep input order", order[1].ToString());
			return true;
		}

		if (order[2] != 3)
		{
			SetFailure("The third pile drained is index %1, expected 3", order[2].ToString());
			return true;
		}

		if (order[3] != 0)
		{
			SetFailure("The furthest pile is drained at position %1, expected index 0 to be last", order[3].ToString());
			return true;
		}

		// A pile emptied mid-drain leaves the order it was in.
		order.RemoveOrdered(1);
		if (order[0] != 1 || order[1] != 3 || order[2] != 0)
		{
			SetFailure("After removing the second entry the order is %1, %2, %3 - expected 1, 3, 0", order[0].ToString(), order[1].ToString(), order[2].ToString());
			return true;
		}

		// The buffer is cleared, not appended to, on reuse.
		OVT_ResourceRules.SortPilesForConsumption(sqDistances, order);
		if (order.Count() != 4)
		{
			SetFailure("A reused order buffer holds %1 entries, expected it cleared and refilled to 4", order.Count().ToString());
			return true;
		}

		Print("Resource consumption: a stable nearest-first drain order that survives an ordered removal");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The importable flag gates BUYING only. A non-importable resource is never listed on import (a
//! row whose only outcome is a no-op click is a bug) but may always be sold.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_ImportablePredicate : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceRulesFixture.MakeDefs();

		int timber = defs.IndexOf(OVT_TEST_ResourceRulesFixture.TIMBER);
		int rubble = defs.IndexOf(OVT_TEST_ResourceRulesFixture.RUBBLE);

		if (timber == -1 || rubble == -1)
		{
			SetFailure("The fixture table did not resolve its own ids: timber %1, rubble %2", timber.ToString(), rubble.ToString());
			return true;
		}

		if (!OVT_ResourceRules.MayImport(defs, timber))
		{
			SetFailure("An importable resource was refused on import");
			return true;
		}

		if (OVT_ResourceRules.MayImport(defs, rubble))
		{
			SetFailure("A non-importable resource was admitted on import");
			return true;
		}

		if (!OVT_ResourceRules.MayExport(defs, rubble))
		{
			SetFailure("A non-importable resource was refused on export - the flag gates buying only");
			return true;
		}

		if (!OVT_ResourceRules.MayExport(defs, timber))
		{
			SetFailure("An importable resource was refused on export");
			return true;
		}

		if (OVT_ResourceRules.MayImport(defs, 99))
		{
			SetFailure("An out-of-range definition index was admitted on import");
			return true;
		}

		if (OVT_ResourceRules.MayExport(defs, 99))
		{
			SetFailure("An out-of-range definition index was admitted on export");
			return true;
		}

		Print("Resource port gate: importable gates buying only, and an unknown index is refused both ways");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! D20 - the illegal gate ships built and unexercised, so this case is the only thing standing
//! between "deliberately inert" and "dead code somebody deletes". No shipped resource is illegal;
//! this table flags one by hand.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_IllegalPredicate : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceRulesFixture.MakeDefs();

		int contraband = defs.IndexOf(OVT_TEST_ResourceRulesFixture.CONTRABAND);
		int timber = defs.IndexOf(OVT_TEST_ResourceRulesFixture.TIMBER);

		if (contraband == -1)
		{
			SetFailure("The fixture table does not hold the illegal resource");
			return true;
		}

		if (!defs.IsIllegal(contraband))
		{
			SetFailure("The hand-flagged resource does not report as illegal");
			return true;
		}

		if (defs.IsIllegal(timber))
		{
			SetFailure("An ordinary resource reports as illegal");
			return true;
		}

		// No permission, no resistance port: refused.
		if (OVT_ResourceRules.IllegalGateOpen(defs, contraband, false, false))
		{
			SetFailure("An illegal resource was admitted with neither the permission nor a resistance-held port");
			return true;
		}

		// The smuggling permission alone opens it.
		if (!OVT_ResourceRules.IllegalGateOpen(defs, contraband, true, false))
		{
			SetFailure("An illegal resource was refused to a player holding the illegal-imports permission");
			return true;
		}

		// A resistance-controlled port alone opens it.
		if (!OVT_ResourceRules.IllegalGateOpen(defs, contraband, false, true))
		{
			SetFailure("An illegal resource was refused at a resistance-controlled port");
			return true;
		}

		// A legal resource passes with neither.
		if (!OVT_ResourceRules.IllegalGateOpen(defs, timber, false, false))
		{
			SetFailure("A legal resource was put through the illegal gate");
			return true;
		}

		Print("Resource illegal gate: refused without permission, admitted with permission or a resistance-held port");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The readout lists EVERY requirement with what is available against what is needed, and marks the
//! short one. Listing only the shortfalls would leave a player with no idea what the building costs.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_ReadoutNamesTheShortfall : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceRulesFixture.MakeDefs();

		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		need.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.TIMBER, 40));
		need.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.STEEL, 10));

		array<ref OVT_ResourceAmount> have = new array<ref OVT_ResourceAmount>();
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.TIMBER, 12));
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.STEEL, 10));

		string readout = OVT_ResourceRules.FormatReadout(need, have, defs);

		if (!readout.Contains("timber: 12 / 40"))
		{
			SetFailure("The readout does not carry the short requirement's numbers: '%1'", readout);
			return true;
		}

		if (!readout.Contains("(short 28)"))
		{
			SetFailure("The readout does not mark the shortfall: '%1'", readout);
			return true;
		}

		if (!readout.Contains("steel: 10 / 10"))
		{
			SetFailure("The readout dropped the satisfied requirement: '%1'", readout);
			return true;
		}

		if (readout.Contains("steel: 10 / 10 (short"))
		{
			SetFailure("The readout marked a satisfied requirement as short: '%1'", readout);
			return true;
		}

		// Every requirement, satisfied or not, is one line.
		array<string> lines = new array<string>();
		readout.Split("\n", lines, true);
		if (lines.Count() != 2)
		{
			SetFailure("The readout has %1 lines for 2 requirements: '%2'", lines.Count().ToString(), readout);
			return true;
		}

		// Nothing to build is an empty readout, not a stray line.
		array<ref OVT_ResourceAmount> nothing = new array<ref OVT_ResourceAmount>();
		if (OVT_ResourceRules.FormatReadout(nothing, have, defs) != "")
		{
			SetFailure("An empty requirement list produced a readout");
			return true;
		}

		Print("Resource readout: every requirement listed with have/need, and only the short one marked");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The THREE-STEP composition of §3.4, end to end, at the Hard preset's numbers.
//!
//! The three steps are asserted separately elsewhere; what only this case says is the ORDER they
//! compose in, and it says it with a negative control - the same three functions composed the other
//! way round produce a strictly smaller answer, because the band clamp eats the multiplier.
//!
//! 1. stored = the walked value, clamped to the band. 2. live = round(stored x levelMultiplier),
//! at READ time, AFTER the clamp, so on Hard it legitimately exceeds base x bandMax.
//! 3. sell = round(live x sellRatio) - one ratio off the LIVE price, never a second walk.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_DriftCompositionOrder : SCR_AutotestCaseBase
{
	//! The Hard preset's resourcePriceMultiplier.
	static const float HARD_LEVEL = 1.25;

	//! The manager's authored sell ratio.
	static const float SELL_RATIO = 0.5;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- STEP 1. Walked hard against the upper edge of a 100-base, 0.5-2.0 band.
		int stored = OVT_ResourceRules.DriftStep(100, 100, 50, 0, 1.5, 0.08, 0.5, 2.0);
		if (stored != 200)
		{
			SetFailure("Step 1 stored %1, expected the clamped upper edge 200", stored.ToString());
			return true;
		}

		// --- STEP 2. The level multiplier lands on the CLAMPED value.
		int live = OVT_ResourceRules.ApplyLevelMultiplier(stored, HARD_LEVEL);
		if (live != 250)
		{
			SetFailure("Step 2 quoted %1, expected round(200 x 1.25) = 250", live.ToString());
			return true;
		}

		// The negative control: the same functions in the other order. A zero-roll DriftStep is exactly
		// "clamp to the band and do not move", so this IS the multiplier applied before the clamp.
		int wrongOrder = OVT_ResourceRules.DriftStep(100, live, 0, 0, 0, 0.08, 0.5, 2.0);
		if (wrongOrder != 200)
		{
			SetFailure("Composed the other way round the price is %1, expected the band to eat it back to 200", wrongOrder.ToString());
			return true;
		}

		if (live <= wrongOrder)
		{
			SetFailure("The live price %1 is not above the clamp-last answer %2 - the multiplier is being applied before the band clamp, which eats the whole difficulty setting", live.ToString(), wrongOrder.ToString());
			return true;
		}

		// --- STEP 3. Sell follows the LIVE price, so it inherits the multiplier exactly once.
		int sell = OVT_ResourceRules.SellPrice(live, SELL_RATIO);
		if (sell != 125)
		{
			SetFailure("Step 3 pays %1, expected round(250 x 0.5) = 125", sell.ToString());
			return true;
		}

		if (sell == OVT_ResourceRules.SellPrice(stored, SELL_RATIO))
		{
			SetFailure("The sell price is the same off the live and the stored price (%1) - step 3 must compose on step 2's output", sell.ToString());
			return true;
		}

		// --- THE IDENTITY CONTROL. At a preset that authors no multiplier the chain is stored -> stored.
		int plainLive = OVT_ResourceRules.ApplyLevelMultiplier(stored, 1);
		int plainSell = OVT_ResourceRules.SellPrice(plainLive, SELL_RATIO);
		if (plainLive != 200 || plainSell != 100)
		{
			SetFailure("At a multiplier of 1 the chain gave %1 / %2, expected the stored 200 and a sell of 100", plainLive.ToString(), plainSell.ToString());
			return true;
		}

		Print("Resource prices: clamp, then multiply, then sell - and composing the other way round loses the difficulty setting to the band");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! FOUR drift windows a day and no more, and a tick that fires many times an hour still walks the
//! prices exactly once per window.
//!
//! DriftOneStepPerWindow spot-checks the latch; this walks two whole in-game days through it. The
//! failure it exists for is a fifth window (or a renumbered constant) added while tuning, which
//! doubles the drift rate silently - there is no symptom short of prices moving faster than the
//! design says.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_DriftWindowsAreTheFourQuarters : SCR_AutotestCaseBase
{
	//! How many times the tick fires inside one simulated hour.
	static const int TICKS_PER_HOUR = 3;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int windows = 0;
		for (int hour = 0; hour < 24; hour++)
		{
			if (OVT_ResourceRules.IsDriftWindowHour(hour))
				windows = windows + 1;
		}

		if (windows != 4)
		{
			SetFailure("%1 of the 24 hours are drift windows, expected 4 (00, 06, 12, 18)", windows.ToString());
			return true;
		}

		if (!OVT_ResourceRules.IsDriftWindowHour(0) || !OVT_ResourceRules.IsDriftWindowHour(6) || !OVT_ResourceRules.IsDriftWindowHour(12) || !OVT_ResourceRules.IsDriftWindowHour(18))
		{
			SetFailure("The four drift windows are not 00, 06, 12 and 18");
			return true;
		}

		// --- TWO WHOLE DAYS through the latch, at three ticks an hour. 144 ticks, 8 drifts.
		int latch = -1;
		int drifts = 0;

		for (int day = 0; day < 2; day++)
		{
			for (int hour = 0; hour < 24; hour++)
			{
				for (int tick = 0; tick < TICKS_PER_HOUR; tick++)
				{
					if (!OVT_ResourceRules.ShouldDrift(hour, latch))
						continue;

					latch = hour;
					drifts = drifts + 1;
				}
			}
		}

		if (drifts != 8)
		{
			SetFailure("Two in-game days of ticks drifted the prices %1 times, expected 8 - four per day, one per window", drifts.ToString());
			return true;
		}

		if (latch != 18)
		{
			SetFailure("The latch ended on hour %1, expected the last window of the day, 18", latch.ToString());
			return true;
		}

		Print("Resource drift: four windows a day, and 144 ticks across two days walk the prices exactly 8 times");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The band clamp is UNCONDITIONAL - it does not depend on the step being non-zero.
//!
//! Two things ride on that. A preset may author a volatility of 0 to freeze prices, and that must
//! freeze them where they are rather than anywhere else; and a stored price loaded from a save taken
//! under a different band (or a retuned resources.conf) is outside the band on the very first walk,
//! and has to be pulled back in even though nothing moved it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_DriftStepZeroStillClamps : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- VOLATILITY 0 FREEZES THE WALK, whatever the roll and the pressure say.
		int frozenUp = OVT_ResourceRules.DriftStep(100, 137, 50, 1, 0, 0.08, 0.5, 2.0);
		if (frozenUp != 137)
		{
			SetFailure("At volatility 0 a huge positive roll moved the price to %1, expected it to stay at 137", frozenUp.ToString());
			return true;
		}

		int frozenDown = OVT_ResourceRules.DriftStep(100, 137, -50, -1, 0, 0.08, 0.5, 2.0);
		if (frozenDown != 137)
		{
			SetFailure("At volatility 0 a huge negative roll moved the price to %1, expected it to stay at 137", frozenDown.ToString());
			return true;
		}

		// --- AN OUT-OF-BAND STORED PRICE IS PULLED IN even with nothing pushing it.
		int pulledDown = OVT_ResourceRules.DriftStep(100, 500, 0, 0, 0, 0.08, 0.5, 2.0);
		if (pulledDown != 200)
		{
			SetFailure("A stored price of 500 against a 100-base, 2.0-max band stayed at %1, expected to be pulled to the upper edge 200", pulledDown.ToString());
			return true;
		}

		int pulledUp = OVT_ResourceRules.DriftStep(100, 5, 0, 0, 0, 0.08, 0.5, 2.0);
		if (pulledUp != 50)
		{
			SetFailure("A stored price of 5 against a 100-base, 0.5-min band stayed at %1, expected to be pulled to the lower edge 50", pulledUp.ToString());
			return true;
		}

		// --- AND WITH A LIVE STEP the clamp still wins over the walk, not the other way round.
		int walkedAndPulled = OVT_ResourceRules.DriftStep(100, 500, -1, 0, 1, 0.08, 0.5, 2.0);
		if (walkedAndPulled != 200)
		{
			SetFailure("An out-of-band price walked down by 8 landed on %1, expected the upper edge 200", walkedAndPulled.ToString());
			return true;
		}

		Print("Resource drift: the band clamp is unconditional - volatility 0 freezes in place, and an out-of-band price is pulled back on the next walk");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The eight SHIPPED requirement quantities at the five shipped difficulty multipliers.
//!
//! The multiplier is the only thing between what buildables.conf authors and what a construction
//! site takes off the ground, and this is the arithmetic both the build card and the site's own
//! consumption run. It is pinned here because a difficulty .conf edit that quietly halves a cost
//! looks exactly like a working game until somebody counts crates.
//!
//! It also states the floor at the numbers that could reach it: no authored quantity, at any shipped
//! multiplier, ever scales to zero.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_RequirementScaleMatchesShippedTable : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Guard Tower timber 40, at Easy / Normal / Hard / Extreme / Insane.
		if (!AssertScaled(40, 0.8, 32))
			return true;

		if (!AssertScaled(40, 1.0, 40))
			return true;

		if (!AssertScaled(40, 1.5, 60))
			return true;

		if (!AssertScaled(40, 3.0, 120))
			return true;

		if (!AssertScaled(40, 4.0, 160))
			return true;

		// Guard Tower steel 10 - the smallest shipped quantity, and the one nearest the floor.
		if (!AssertScaled(10, 0.8, 8))
			return true;

		if (!AssertScaled(10, 1.5, 15))
			return true;

		// Helipad cement 60 / steel 20.
		if (!AssertScaled(60, 0.8, 48))
			return true;

		if (!AssertScaled(20, 1.5, 30))
			return true;

		// Garage cement 120 / hardware 30 - the largest and the smallest of the four.
		if (!AssertScaled(120, 3.0, 360))
			return true;

		if (!AssertScaled(30, 0.8, 24))
			return true;

		// THE FLOOR, at every shipped quantity and every shipped multiplier: a requirement never
		// scales away to free. A zero here would make a building that costs resources on paper cost
		// none in the world.
		array<int> quantities = new array<int>();
		quantities.Insert(40);
		quantities.Insert(10);
		quantities.Insert(60);
		quantities.Insert(20);
		quantities.Insert(120);
		quantities.Insert(30);

		array<float> multipliers = new array<float>();
		multipliers.Insert(0.8);
		multipliers.Insert(1.0);
		multipliers.Insert(1.5);
		multipliers.Insert(3.0);
		multipliers.Insert(4.0);

		foreach (int quantity : quantities)
		{
			foreach (float multiplier : multipliers)
			{
				if (OVT_ResourceRules.ScaleRequirement(quantity, multiplier) >= 1)
					continue;

				SetFailure("ScaleRequirement(%1, %2) scaled a shipped requirement to nothing", quantity.ToString(), multiplier.ToString());
				return true;
			}
		}

		// Zero in, zero out - an unauthored requirement is not a one-unit one.
		if (OVT_ResourceRules.ScaleRequirement(0, 4.0) != 0)
		{
			SetFailure("ScaleRequirement(0, 4) is %1, expected 0 - only a NON-zero requirement has a floor", OVT_ResourceRules.ScaleRequirement(0, 4.0).ToString());
			return true;
		}

		Print("Resource requirements: the eight shipped quantities scale as the difficulty table says, and none of them ever reaches zero");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] baseQty The authored quantity.
	//! \param[in] multiplier The difficulty multiplier.
	//! \param[in] expected What the scaled figure must be.
	//! \return True when it matched; false after reporting the failure.
	protected bool AssertScaled(int baseQty, float multiplier, int expected)
	{
		int actual = OVT_ResourceRules.ScaleRequirement(baseQty, multiplier);
		if (actual == expected)
			return true;

		SetFailure(string.Format("ScaleRequirement(%1, %2) is %3, expected %4", baseQty.ToString(), multiplier.ToString(), actual.ToString(), expected.ToString()));
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The drain order is decided on SQUARED distances, and it is deterministic.
//!
//! vector.Distance is not correctly rounded - it is off by an ULP at 1000 m and again at 2000 m - so
//! two piles the same distance from a site can compare unequal, and which one is drained first would
//! then depend on which side of the site they happen to sit. vector.DistanceSq is an exact product
//! sum of the components, which is why OVT_ResourceRequirements feeds it and never the other.
//!
//! This case builds the SAME squared distances the consumption sweep does, from real positions, and
//! asserts the resulting order twice: nearest first, stable on the tie, and identical on a re-run.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_ConsumptionOrderIsDeterministicOnSquaredDistances : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector site = "1000 0 1000";

		// Two piles EXACTLY equidistant on opposite sides, one nearer and one further.
		array<vector> piles = new array<vector>();
		piles.Insert("1012 0 1000");   // 12 m east  - the tie, first in input order
		piles.Insert("1000 0 988");    // 12 m south - the tie, second in input order
		piles.Insert("1004 0 1000");   // 4 m  - nearest
		piles.Insert("1000 0 1025");   // 25 m - furthest

		array<float> sqDistances = new array<float>();
		foreach (vector pile : piles)
		{
			sqDistances.Insert(vector.DistanceSq(site, pile));
		}

		// The tie is a tie ONLY because the comparison is squared. State it, so a change to
		// unsquared distances makes this case say why it broke.
		if (sqDistances[0] != sqDistances[1])
		{
			SetFailure("Two piles 12 m from the site on different axes gave squared distances %1 and %2 - vector.DistanceSq is no longer exact, so the drain order is a coin flip", sqDistances[0].ToString(), sqDistances[1].ToString());
			return true;
		}

		array<int> order = new array<int>();
		OVT_ResourceRules.SortPilesForConsumption(sqDistances, order);

		if (order.Count() != 4)
		{
			SetFailure("The drain order has %1 entries for 4 piles", order.Count().ToString());
			return true;
		}

		if (order[0] != 2)
		{
			SetFailure("The first pile drained is index %1, expected 2 - the 4 m pile is nearest", order[0].ToString());
			return true;
		}

		if (order[1] != 0 || order[2] != 1)
		{
			SetFailure("The two equidistant piles are drained in the order %1, %2 - expected 0 then 1, their input order, because the sort is stable on a tie", order[1].ToString(), order[2].ToString());
			return true;
		}

		if (order[3] != 3)
		{
			SetFailure("The furthest pile is at position 3 in the drain order as index %1, expected 3", order[3].ToString());
			return true;
		}

		// DETERMINISM: two sites over the same field consume the same piles in the same sequence, on
		// every machine and in every session. A drain order that varied would make a server and a
		// re-loaded save disagree about which pile survived.
		array<int> second = new array<int>();
		OVT_ResourceRules.SortPilesForConsumption(sqDistances, second);

		for (int i = 0; i < order.Count(); i++)
		{
			if (order[i] == second[i])
				continue;

			SetFailure("A second sort of the same distances put index %1 at position %2 instead of %3 - the drain order is not deterministic", second[i].ToString(), i.ToString(), order[i].ToString());
			return true;
		}

		Print("Resource consumption: the drain order is nearest-first on exact squared distances, stable on a tie and identical on a re-run");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The requirements readout renders DISPLAY NAMES when a renderer supplies them, and is unchanged
//! when nobody does.
//!
//! P1-d: the readout ships raw because the Logic tier cannot resolve a "#OVT-..." key. The two
//! trailing parameters let the construction site's dialog hand in already-translated strings without
//! moving one digit of the arithmetic - which is the whole point, and is what the first half of this
//! case pins.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_ReadoutRendersDisplayNames : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceRulesFixture.MakeDefs();

		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		need.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.TIMBER, 40));
		need.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.STEEL, 10));

		array<ref OVT_ResourceAmount> have = new array<ref OVT_ResourceAmount>();
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.TIMBER, 12));
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.STEEL, 10));

		// THE DEFAULT PATH IS THE OLD PATH, to the character. Anything else and every caller that
		// never asked for display names silently changes what it prints.
		string raw = OVT_ResourceRules.FormatReadout(need, have, defs);
		if (raw != "timber: 12 / 40 (short 28)\nsteel: 10 / 10")
		{
			SetFailure("The three-argument readout produced '%1' - the display parameters changed the default form, which is the one the rest of this suite asserts", raw);
			return true;
		}

		array<string> titles = new array<string>();
		titles.Insert("Timber");
		titles.Insert("Steel");

		string rendered = OVT_ResourceRules.FormatReadout(need, have, defs, titles, "need");

		if (!rendered.Contains("Timber: 12 / 40"))
		{
			SetFailure("The rendered readout does not use the supplied display name: '%1'", rendered);
			return true;
		}

		if (!rendered.Contains("(need 28)"))
		{
			SetFailure("The rendered readout does not use the supplied shortfall word: '%1'", rendered);
			return true;
		}

		if (rendered.Contains("timber"))
		{
			SetFailure("The rendered readout still carries the raw id beside the display name: '%1'", rendered);
			return true;
		}

		// The numbers are the SAME numbers. Only the words changed.
		if (!rendered.Contains("Steel: 10 / 10") || rendered.Contains("Steel: 10 / 10 (need"))
		{
			SetFailure("The rendered readout lost or mis-marked the satisfied requirement: '%1'", rendered);
			return true;
		}

		// A short titles array falls back per entry rather than dropping the line.
		array<string> partial = new array<string>();
		partial.Insert("Timber");

		string mixed = OVT_ResourceRules.FormatReadout(need, have, defs, partial, "short");
		if (!mixed.Contains("Timber: 12 / 40") || !mixed.Contains("steel: 10 / 10"))
		{
			SetFailure("A titles array shorter than the requirement list dropped a line instead of falling back to the id: '%1'", mixed);
			return true;
		}

		Print("Resource readout: display names and the shortfall word are the renderer's, the numbers are not, and the default form is unchanged");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! "Need 28 more Timber" - the FIGURE in the build action's label.
//!
//! The action does not carry its own arithmetic: it names the first shortfall IsSatisfied() reports
//! and subtracts what AmountOf() finds from what AmountOf() needs. Both sides sum duplicates, which
//! is the only reason a requirement spread over two piles reads as one number.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_RequirementShortfallAmount : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		need.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.CEMENT, 120));
		need.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.TIMBER, 60));

		// Two piles' worth of cement, one pile of timber. Availability must SUM.
		array<ref OVT_ResourceAmount> have = new array<ref OVT_ResourceAmount>();
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.CEMENT, 44));
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.TIMBER, 60));
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.CEMENT, 31));

		string shortId;
		if (OVT_ResourceRules.IsSatisfied(need, have, shortId))
		{
			SetFailure("A site 45 cement short reported satisfied");
			return true;
		}

		if (shortId != OVT_TEST_ResourceRulesFixture.CEMENT)
		{
			SetFailure("The first shortfall is '%1', expected cement - the label would name the wrong resource", shortId);
			return true;
		}

		int missing = OVT_ResourceRules.AmountOf(need, shortId) - OVT_ResourceRules.AmountOf(have, shortId);
		if (missing != 45)
		{
			SetFailure("The label would read 'need %1 more cement', expected 45 - 120 needed against 44 + 31 spread over two piles", missing.ToString());
			return true;
		}

		// One more crate and the whole thing is satisfied, with no shortfall named.
		have.Insert(OVT_TEST_ResourceRulesFixture.MakeAmount(OVT_TEST_ResourceRulesFixture.CEMENT, 45));
		if (!OVT_ResourceRules.IsSatisfied(need, have, shortId))
		{
			SetFailure("An exactly-met requirement spread over three piles reported short on '%1'", shortId);
			return true;
		}

		if (shortId != "")
		{
			SetFailure("A satisfied requirement still named '%1' as short", shortId);
			return true;
		}

		Print("Resource requirements: the shortfall figure the build action shows sums across every pile, and reaches zero exactly when the site is buildable");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The town-control gate: outside the town anything goes, inside it the town must be yours.
//!
//! WHY IT IS A PURE STATIC AT ALL. The rule is enforced twice - once in the build menu so the player
//! reads a reason, and once on the server so a client that skips the menu is still refused. Two
//! copies of one comparison is exactly the shape that drifts, so both sides call this and only this,
//! and this tier is where the comparison is checked.
//!
//! SQUARED DISTANCES. vector.Distance is not correctly rounded, and the boundary case below sits
//! exactly ON the range: an unsquared comparison would decide it on an ULP. The case asserts the
//! boundary belongs to the OUTSIDE (the build is allowed), which is the shipped `dist < range`
//! convention read the other way round - a position exactly at the range is not in the town.
//!
//! PROVEN ABLE TO FAIL: change `sqDistance >= sqRange` to `>` and the boundary assertion reports an
//! enemy town refusing a build that is not in it; drop the distance test entirely and the "far away"
//! assertion fires instead.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_TownControlGate : SCR_AutotestCaseBase
{
	static const int RESISTANCE = 3;
	static const int OCCUPIER = 5;

	//! 400 m, the shipped town build range, kept as its square throughout.
	static const float RANGE = 400;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		float sqRange = RANGE * RANGE;
		float inside = 100 * 100;
		float outside = 500 * 500;

		if (!OVT_ResourceRules.TownControlAllowsBuild(RESISTANCE, RESISTANCE, inside, sqRange))
		{
			SetFailure("A build inside a town the resistance holds was refused - the warehouse could never be built in a town at all");
			return true;
		}

		if (OVT_ResourceRules.TownControlAllowsBuild(OCCUPIER, RESISTANCE, inside, sqRange))
		{
			SetFailure("A build inside a town the OCCUPIER holds was allowed. The gate is the only thing stopping a player putting a warehouse in enemy territory, on both sides of the wire.");
			return true;
		}

		if (!OVT_ResourceRules.TownControlAllowsBuild(OCCUPIER, RESISTANCE, outside, sqRange))
		{
			SetFailure("A build 500 m from an enemy town, well outside its 400 m build range, was refused. The gate must only speak about positions that are actually in a town - otherwise every base build near an enemy town breaks.");
			return true;
		}

		// Exactly on the range. The shipped town branch treats `dist < range` as inside, so the
		// boundary is outside and the build is allowed.
		if (!OVT_ResourceRules.TownControlAllowsBuild(OCCUPIER, RESISTANCE, sqRange, sqRange))
		{
			SetFailure("A build exactly on an enemy town's build range was refused. The shipped branch counts that position as OUTSIDE the town, so the client would offer it and the server would refuse it.");
			return true;
		}

		Print("Town control gate: an enemy town refuses only the positions inside it, and the range boundary is decided on squared distances");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Repairing a ruin costs a share of what building it cost - never free, and never more than the
//! build. The two clamps are the contract: a difficulty preset can make a repair cheap, but a
//! resource-costing building can never be put back for nothing, and a mis-authored multiplier above
//! 1 can never make repairing dearer than building.
//!
//! PROVEN ABLE TO FAIL: drop the `repair < 1` floor - red on the 0.5-of-1 case; drop the
//! `repair > scaledQty` ceiling - red on the 1.5 case.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceRules_RepairRequirementIsAShareOfTheBuild : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The shipped ladder: half on Easy/Normal, three quarters on Hard, full at Extreme/Insane.
		int repair = OVT_ResourceRules.RepairRequirement(100, 0.5);
		if (repair != 50)
		{
			SetFailure("RepairRequirement(100, 0.5) is %1, expected 50", repair.ToString());
			return true;
		}

		repair = OVT_ResourceRules.RepairRequirement(100, 0.75);
		if (repair != 75)
		{
			SetFailure("RepairRequirement(100, 0.75) is %1, expected 75", repair.ToString());
			return true;
		}

		repair = OVT_ResourceRules.RepairRequirement(100, 1);
		if (repair != 100)
		{
			SetFailure("At full difficulty a repair costs %1 of a 100 build, expected all of it", repair.ToString());
			return true;
		}

		// The floor: a single-unit requirement at half still costs one.
		repair = OVT_ResourceRules.RepairRequirement(1, 0.5);
		if (repair != 1)
		{
			SetFailure("RepairRequirement(1, 0.5) is %1, expected the floor 1 - a repair is never free", repair.ToString());
			return true;
		}

		// The ceiling: a preset authored above 1 must not make a repair dearer than the build.
		repair = OVT_ResourceRules.RepairRequirement(40, 1.5);
		if (repair != 40)
		{
			SetFailure("RepairRequirement(40, 1.5) is %1, expected the build requirement 40 as a ceiling", repair.ToString());
			return true;
		}

		// A negative multiplier clamps to zero and then meets the floor, the same guard rail
		// OVT_RepairPricing.RepairCost applies to a mis-authored preset.
		repair = OVT_ResourceRules.RepairRequirement(40, -1);
		if (repair != 1)
		{
			SetFailure("A negative multiplier gave %1, expected the floor 1", repair.ToString());
			return true;
		}

		// No requirement stays no requirement: a money-only buildable repairs for money alone.
		repair = OVT_ResourceRules.RepairRequirement(0, 0.5);
		if (repair != 0)
		{
			SetFailure("RepairRequirement(0, 0.5) is %1, expected 0 - a money-only buildable gains no material cost", repair.ToString());
			return true;
		}

		repair = OVT_ResourceRules.RepairRequirement(-5, 0.5);
		if (repair != 0)
		{
			SetFailure("A negative requirement gave %1, expected 0", repair.ToString());
			return true;
		}

		Print("Repair requirements: a share of the build, floored at 1, capped at the build, and 0 stays 0");

		return true;
	}
}
