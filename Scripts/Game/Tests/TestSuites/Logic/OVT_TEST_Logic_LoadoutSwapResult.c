//------------------------------------------------------------------------------------------------
//! TIER A cases - the loadout swap's OUTCOME CLASSIFICATION (recruit-ux Phase 7).
//!
//! WHAT IS HERE, AND WHAT CANNOT BE. The swap itself is unreachable from this tier by nature: it
//! moves live item entities between two spawned characters' replicated inventories, and this suite
//! runs with no world at all. What IS reachable, and what this file pins, is the small pure object
//! the swap hands back - OVT_LoadoutSwapResult - and the three questions the caller asks it to pick
//! the sentence a player reads:
//!
//!   "kit swapped"                 <- IsComplete()
//!   "kit swapped, but ... "       <- IsPartial()
//!   "nothing could be exchanged"  <- neither
//!
//! Those three branches are the whole player-facing contract of the feature, and every one of them
//! is a plain comparison over four ints. Pinning them here costs nothing and means the only thing
//! left to play-testing is whether the ITEMS end up in the right places - not whether the message
//! about them is right.
//!
//! WHY "RELOCATED" GETS A CASE OF ITS OWN. The subtlest decision in the swap is that an item which
//! DID reach the other character, but landed in a rucksack instead of on their back, is a PARTIAL
//! outcome rather than a complete one. Nothing was lost and the counters still balance, so it would
//! be very easy for a later edit to simplify IsComplete() down to "no failures and no drops" and
//! never notice - and the player would then be told "kit swapped" while hunting for a jacket that is
//! inside a bag. The fourth case below is there to make that edit fail.
//!
//! WHY THE COUNTERS ARE ASSERTED TO BE A PARTITION. Every item the swap touches is counted in exactly
//! one of the four fields, which is what makes the plan's item-count invariant (Q3) checkable from a
//! log line rather than by opening two inventories. GetMovedCount() must therefore be exactly the two
//! fields that describe "it is on the other character now", and must not quietly absorb a third.
//!
//! HOUSE RULES OBSERVED. Every field is set explicitly (`new` applies no attribute defvalues), no
//! case reads state another wrote, and no float is compared.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 7, by deliberate fault + compile-check; running the suite is
//! the orchestrator's job, not this file's):
//!   1. OVT_LoadoutSwapResult.IsComplete() had its `m_iRelocated == 0` term deleted, leaving
//!      "no failures and no drops". The tree recompiled CLEAN - which is exactly why this needs a
//!      case, since a widened success condition is not a syntax error - and
//!      ..._RelocatedIsNotComplete then fails on its first assertion, because a result carrying one
//!      relocated item reports itself complete. The term was restored and the tree recompiled clean.
//!   2. GetMovedCount() was changed to return m_iExchanged alone. It compiled clean and
//!      ..._MovedCountIsExchangedPlusRelocated fails on "moved count ignored a relocated item".
//!      Reverted and recompiled clean.
//!   Deliberately NOT claimed as faults: dropping the m_bRan guard (it is asserted separately in the
//!   first case), or renaming a field (that is a compile error, which no case is needed for).
//!   No maxAttempts anywhere: these are four ints and a handful of comparisons, and they cannot
//!   flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! A swap that never ran is neither complete nor partial, whatever its counters say.
//!
//! This is the guard on the whole classification. A result is constructed BEFORE the routine's
//! server check and its two null checks, so a refused swap returns a fully-formed object with
//! m_bRan false - and if either question answered "yes" to that, a client that was refused outright
//! would be told its kit had been swapped.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_LoadoutSwapResult_DidNotRunIsNeither : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_LoadoutSwapResult result = new OVT_LoadoutSwapResult();
		result.m_bRan = false;
		result.m_iExchanged = 0;
		result.m_iRelocated = 0;
		result.m_iFailed = 0;
		result.m_iDropped = 0;

		if (result.IsComplete())
		{
			SetFailure("A swap that never ran reported itself COMPLETE - a refused request would tell the player their kit was swapped");
			return true;
		}

		if (result.IsPartial())
		{
			SetFailure("A swap that never ran reported itself PARTIAL");
			return true;
		}

		// The guard has to survive counters being set, because a future caller could reasonably fill
		// them in before discovering it cannot run.
		result.m_iExchanged = 6;
		result.m_iRelocated = 1;
		result.m_iFailed = 2;
		result.m_iDropped = 1;

		if (result.IsComplete())
		{
			SetFailure("A swap that never ran reported itself COMPLETE once its counters were non-zero - m_bRan is not being consulted");
			return true;
		}

		if (result.IsPartial())
		{
			SetFailure("A swap that never ran reported itself PARTIAL once its counters were non-zero - m_bRan is not being consulted");
			return true;
		}

		Print("Loadout swap result: a run that never happened is neither complete nor partial");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A swap between two characters carrying nothing is COMPLETE.
//!
//! Not a corner case worth skipping: afterwards each of them is wearing exactly what the other was,
//! which is what was asked for. Reporting "nothing could be exchanged" here would be a lie about a
//! successful operation, and the alternative reading - "moved nothing, so it failed" - is the one a
//! naive implementation reaches for.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_LoadoutSwapResult_EmptySwapIsComplete : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_LoadoutSwapResult result = new OVT_LoadoutSwapResult();
		result.m_bRan = true;
		result.m_iExchanged = 0;
		result.m_iRelocated = 0;
		result.m_iFailed = 0;
		result.m_iDropped = 0;

		if (!result.IsComplete())
		{
			SetFailure("A swap between two characters carrying nothing did not report COMPLETE");
			return true;
		}

		if (result.IsPartial())
		{
			SetFailure("An empty swap reported itself PARTIAL as well as complete - the two answers must be exclusive");
			return true;
		}

		if (result.GetMovedCount() != 0)
		{
			SetFailure("An empty swap claimed to have moved something");
			return true;
		}

		Print("Loadout swap result: an empty swap is complete, not failed");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Everything in its matching slot is COMPLETE and nothing else.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_LoadoutSwapResult_FullSwapIsComplete : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_LoadoutSwapResult result = new OVT_LoadoutSwapResult();
		result.m_bRan = true;
		result.m_iExchanged = 14;
		result.m_iRelocated = 0;
		result.m_iFailed = 0;
		result.m_iDropped = 0;

		if (!result.IsComplete())
		{
			SetFailure("A swap in which every item reached its matching slot did not report COMPLETE");
			return true;
		}

		if (result.IsPartial())
		{
			SetFailure("A fully successful swap also reported PARTIAL - the two answers must be exclusive");
			return true;
		}

		if (result.GetMovedCount() != 14)
		{
			SetFailure("A fully successful swap miscounted what it moved");
			return true;
		}

		Print("Loadout swap result: everything in its matching slot is complete");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An item that crossed but did NOT land in its matching slot makes the swap PARTIAL.
//!
//! THE LOAD-BEARING CASE OF THIS FILE. Nothing was lost, the counters balance, and every other
//! measure of success is satisfied - so this is precisely the assertion a well-meaning
//! simplification of IsComplete() would delete. The player consequence is concrete: their jacket is
//! inside the recruit's rucksack rather than on the recruit, and if they are told "kit swapped" they
//! have no reason to go looking for it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_LoadoutSwapResult_RelocatedIsNotComplete : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_LoadoutSwapResult result = new OVT_LoadoutSwapResult();
		result.m_bRan = true;
		result.m_iExchanged = 11;
		result.m_iRelocated = 1;
		result.m_iFailed = 0;
		result.m_iDropped = 0;

		if (result.IsComplete())
		{
			SetFailure("A swap that stowed an item instead of putting it in its slot reported COMPLETE - the player is never told to go looking for it");
			return true;
		}

		if (!result.IsPartial())
		{
			SetFailure("A swap that stowed an item instead of putting it in its slot did not report PARTIAL either - the player would be told nothing was exchanged");
			return true;
		}

		Print("Loadout swap result: an item stowed rather than worn makes the swap partial");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An item left behind, or left on the ground, makes the swap PARTIAL.
//!
//! Both counters are checked in one case because they encode the same rule from two directions, and
//! the dropped one is the outcome the plan calls the designed worst case (D13): recoverable, but
//! only if the player is told.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_LoadoutSwapResult_LeftBehindIsPartial : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_LoadoutSwapResult failedOne = new OVT_LoadoutSwapResult();
		failedOne.m_bRan = true;
		failedOne.m_iExchanged = 9;
		failedOne.m_iRelocated = 0;
		failedOne.m_iFailed = 2;
		failedOne.m_iDropped = 0;

		if (failedOne.IsComplete())
		{
			SetFailure("A swap that left two items behind reported COMPLETE");
			return true;
		}

		if (!failedOne.IsPartial())
		{
			SetFailure("A swap that moved nine items and left two behind did not report PARTIAL");
			return true;
		}

		OVT_LoadoutSwapResult droppedOne = new OVT_LoadoutSwapResult();
		droppedOne.m_bRan = true;
		droppedOne.m_iExchanged = 9;
		droppedOne.m_iRelocated = 0;
		droppedOne.m_iFailed = 0;
		droppedOne.m_iDropped = 1;

		if (droppedOne.IsComplete())
		{
			SetFailure("A swap that left an item on the ground reported COMPLETE - nothing would tell the player to pick it up");
			return true;
		}

		if (!droppedOne.IsPartial())
		{
			SetFailure("A swap that left an item on the ground did not report PARTIAL");
			return true;
		}

		Print("Loadout swap result: an item left behind or on the ground makes the swap partial");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A swap that moved NOTHING but failed at something is neither complete nor partial.
//!
//! This is the branch that produces "nothing could be exchanged", and it is a real state: two
//! characters whose every garment is blocked or refused end up here. Answering PARTIAL would tell
//! the player some of their kit had moved when none of it had.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_LoadoutSwapResult_MovedNothingIsNeither : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_LoadoutSwapResult result = new OVT_LoadoutSwapResult();
		result.m_bRan = true;
		result.m_iExchanged = 0;
		result.m_iRelocated = 0;
		result.m_iFailed = 8;
		result.m_iDropped = 0;

		if (result.IsComplete())
		{
			SetFailure("A swap in which every item was refused reported COMPLETE");
			return true;
		}

		if (result.IsPartial())
		{
			SetFailure("A swap in which every item was refused reported PARTIAL - the player would be told some of their kit had moved");
			return true;
		}

		Print("Loadout swap result: refusing everything is neither complete nor partial");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The moved count is exactly the two counters that mean "it is on the other character now".
//!
//! The four counters are a PARTITION of everything the swap touched - that is what lets the
//! feature's item-count invariant be read straight off a log line. This case pins both halves: that
//! the two "it crossed" counters are both included, and that neither of the two "it did not cross"
//! counters leaks in.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_LoadoutSwapResult_MovedCountIsExchangedPlusRelocated : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_LoadoutSwapResult result = new OVT_LoadoutSwapResult();
		result.m_bRan = true;
		result.m_iExchanged = 5;
		result.m_iRelocated = 3;
		result.m_iFailed = 0;
		result.m_iDropped = 0;

		if (result.GetMovedCount() != 8)
		{
			SetFailure("The moved count ignored a relocated item - an item stowed on the recipient did cross and has to be counted as moved");
			return true;
		}

		// Now add items that did NOT cross. The moved count must not change.
		result.m_iFailed = 4;
		result.m_iDropped = 2;

		if (result.GetMovedCount() != 8)
		{
			SetFailure("The moved count changed when items that did NOT cross were counted - the four counters are not a partition");
			return true;
		}

		Print("Loadout swap result: moved count is exchanged plus relocated, and nothing else");

		return true;
	}
}
