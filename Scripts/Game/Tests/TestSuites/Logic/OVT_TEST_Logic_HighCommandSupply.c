//------------------------------------------------------------------------------------------------
//! TIER A cases - OVT_HighCommandRules.AggregateResourceNeeds(), the rearm tick's needed-magazine
//! derivation (implementation.md Phase 10, T10.1). World-free by construction; see the suite
//! header for the tier rule.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Three deficient magazines of the same resource must aggregate to one entry counting 3, not
//! three separate entries or a count stuck at 1.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandSupply_AggregateSumsDuplicates : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<string> resourcesNeeded = {"mag_556", "mag_556", "mag_556"};

		map<string, int> needed;
		int distinct = OVT_HighCommandRules.AggregateResourceNeeds(resourcesNeeded, needed);

		if (distinct != 1 || !needed.Contains("mag_556") || needed.Get("mag_556") != 3)
		{
			SetFailure("Three deficient 'mag_556' magazines aggregated to %1 distinct resource(s) needing %2 - expected 1 resource needing 3", distinct.ToString(), needed.Get("mag_556").ToString());
			return true;
		}

		Print("High Command supply: three deficient magazines of the same resource sum to a need of 3");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Two different resources must be tracked independently, never merged into one bucket.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandSupply_AggregateSeparatesResources : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<string> resourcesNeeded = {"mag_556", "mag_762", "mag_556", "mag_762", "mag_762"};

		map<string, int> needed;
		int distinct = OVT_HighCommandRules.AggregateResourceNeeds(resourcesNeeded, needed);

		if (distinct != 2 || needed.Get("mag_556") != 2 || needed.Get("mag_762") != 3)
		{
			SetFailure("Five deficient magazines across two resources aggregated to %1 distinct resource(s) (mag_556=%2, mag_762=%3) - expected 2 resources needing 2 and 3", distinct.ToString(), needed.Get("mag_556").ToString(), needed.Get("mag_762").ToString());
			return true;
		}

		Print("High Command supply: two resources aggregate independently, never merged into one bucket");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An empty resource name - a muzzle whose default magazine could not be named - contributes
//! nothing rather than an unresolvable entry the warehouse would be asked to source.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandSupply_AggregateSkipsEmptyResource : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<string> resourcesNeeded = {"mag_556", "", "mag_556", ""};

		map<string, int> needed;
		int distinct = OVT_HighCommandRules.AggregateResourceNeeds(resourcesNeeded, needed);

		if (distinct != 1 || needed.Contains("") || needed.Get("mag_556") != 2)
		{
			SetFailure("Two empty resource names among two real ones aggregated to %1 distinct resource(s) needing %2, with an empty key present=%3 - expected exactly 1 resource ('mag_556') needing 2 and no empty key", distinct.ToString(), needed.Get("mag_556").ToString(), needed.Contains("").ToString());
			return true;
		}

		Print("High Command supply: an unresolvable (empty) resource name is skipped, never a bogus entry");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An empty input produces an empty, non-null map rather than leaving the out-parameter untouched.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandSupply_AggregateEmptyInputIsEmptyOutput : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<string> resourcesNeeded = {};

		map<string, int> needed;
		int distinct = OVT_HighCommandRules.AggregateResourceNeeds(resourcesNeeded, needed);

		if (!needed || distinct != 0 || needed.Count() != 0)
		{
			SetFailure("An empty deficiency list aggregated to %1 distinct resource(s) in a map of size %2 - expected an allocated, empty map", distinct.ToString(), needed.Count().ToString());
			return true;
		}

		Print("High Command supply: no deficient magazines aggregates to an empty, allocated map");
		return true;
	}
}
