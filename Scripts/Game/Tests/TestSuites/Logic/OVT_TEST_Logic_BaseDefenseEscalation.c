//------------------------------------------------------------------------------------------------
//! TIER A cases - OVT_DeploymentSelection, the pure arithmetic behind "what does this place buy
//! next?".
//!
//! This one subject carries the whole of the escalation contract. A place used to be able to hold
//! exactly ONE deployment config - the lowest-priority one it qualified for - and was then written
//! off for good, which is why nine per-concern base-defense configs could not exist. The rule that
//! replaces it is small enough to state in a sentence and silent enough when wrong to deserve the
//! cheapest tier in the tree: buy the lowest-priority config this place is still missing, break ties
//! by the order they were offered in, and answer nothing when it is missing none of them.
//!
//! WHAT BREAKS IF IT DRIFTS, and why none of it is visible from the outside:
//!   - lose the already-present filter and a base re-buys its first config on every pass, forever,
//!     draining the pool into deployments the 250 m dedup then refuses to create;
//!   - flip the tie comparison and three configs that share a priority acquire in the wrong order,
//!     which nobody can see because they all eventually arrive;
//!   - re-introduce a sentinel ceiling on the priority value and any config authored above it
//!     becomes unbuyable, with no error anywhere.
//!
//! WHAT IS NOT HERE. Whether a place is SUITABLE for a config - faction type, location kind, cost,
//! threat, chance, instance caps, condition modules - and whether it already holds one are questions
//! about live state, asserted in the Init tier. This file asserts only what is decided once those
//! answers are in.
//!
//! COMPILE PROOF: this file names OVT_DeploymentSelection, which nothing else in the test tree
//! names; tools/compile-check.sh resolves it, so these cases are genuinely in the compile set. No
//! retry attribute is used anywhere - this quality bar bans it outright.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! THE ACQUISITION LADDER: successive passes over one place acquire every config it qualifies for,
//! in priority order, one per pass, and then stop.
//!
//! This is the escalation contract stated as a walk. The seven entries below are the base-defense
//! ladder the migration ships, with three of them deliberately sharing priority 2 and the most
//! expensive one (priority 10) offered FIRST - so the case proves both halves at once: priority
//! beats the order they were offered in, and the order they were offered in breaks a tie between
//! equal priorities.
//!
//! The "and then stop" half is not a formality. It is what stops a fully fortified base being
//! re-evaluated into an endless stream of creation attempts that the dedup silently refuses.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, execution belongs to the phase's suite run): delete
//! the `if (IsAlreadyPresent(...)) continue;` line in SelectNextConfigIndex and the second rung goes
//! red (the ladder answers "Garrison Patrol" forever); change `priorities[i] < bestPriority` to
//! `<=` and the tie assertion goes red (the three priority-2 rungs come back in reverse order);
//! delete the `best == NOTHING_TO_BUY ||` clause and the FIRST rung goes red, because a
//! bestPriority initialised to 0 refuses every positive priority.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_BaseDefenseEscalation_AcquisitionLadder : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Offered in a deliberately unhelpful order: the most expensive concern first, the cheapest
		// second, and three concerns sharing priority 2 in the middle.
		array<string> names = {};
		names.Insert("Parked Vehicles");
		names.Insert("Garrison Patrol");
		names.Insert("Defense Positions");
		names.Insert("Tower Guards");
		names.Insert("Sniper Positions");
		names.Insert("Checkpoints");
		names.Insert("Fortifications");

		array<int> priorities = {};
		priorities.Insert(10);
		priorities.Insert(1);
		priorities.Insert(2);
		priorities.Insert(2);
		priorities.Insert(2);
		priorities.Insert(3);
		priorities.Insert(4);

		// What the place must acquire, in order, one per pass.
		array<string> expected = {};
		expected.Insert("Garrison Patrol");
		expected.Insert("Defense Positions");
		expected.Insert("Tower Guards");
		expected.Insert("Sniper Positions");
		expected.Insert("Checkpoints");
		expected.Insert("Fortifications");
		expected.Insert("Parked Vehicles");

		// The place starts with nothing and keeps what each pass gives it.
		array<string> present = {};

		for (int pass = 0; pass < expected.Count(); pass++)
		{
			int picked = OVT_DeploymentSelection.SelectNextConfigIndex(names, priorities, present);

			if (picked == OVT_DeploymentSelection.NOTHING_TO_BUY)
			{
				string missing = expected[pass];
				SetFailure("Pass %1 bought nothing, expected '%2' - a place stops fortifying while it is still missing concerns", pass.ToString(), missing);
				return true;
			}

			if (picked < 0 || picked >= names.Count())
			{
				SetFailure("Pass %1 answered index %2, which is not an index into a list of %3", pass.ToString(), picked.ToString(), names.Count().ToString());
				return true;
			}

			if (names[picked] != expected[pass])
			{
				string detail = names[picked] + ", expected " + expected[pass];
				SetFailure("Pass %1 bought %2 - priority is the ORDER OF ACQUISITION at one place, and this rung is out of order", pass.ToString(), detail);
				return true;
			}

			present.Insert(names[picked]);
		}

		// AND THEN STOP. Everything is here, so there is nothing left to buy.
		int exhausted = OVT_DeploymentSelection.SelectNextConfigIndex(names, priorities, present);
		if (exhausted != OVT_DeploymentSelection.NOTHING_TO_BUY)
		{
			SetFailure("A place holding all %1 configs still answered index %2 - a fully fortified base would be re-evaluated into creation attempts the dedup then refuses", names.Count().ToString(), exhausted.ToString());
			return true;
		}

		Print("The acquisition ladder buys the lowest-priority missing config each pass (priority beats offer order, offer order breaks ties) and stops once nothing is missing");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Partial states, absent sets and the inputs a caller can get wrong.
//!
//! A place is almost never empty when it is asked. It holds whatever previous passes bought, plus -
//! in a loaded campaign - whatever a save restored, and the set it is asked about may name configs
//! that are not on offer here at all. None of those may change the answer for the configs that ARE
//! on offer.
//!
//! THE SENTINEL CLAIM IS THE INTERESTING ONE. The selection this replaced started its comparison at
//! a hardcoded `bestPriority = 999`, so a config authored at priority 999 or above could never be
//! chosen by anything, ever, with no error and no log line. Priority is documented as 1-20 and the
//! shipped configs stay inside it, but "unbuyable above an undocumented ceiling" is not a rule
//! anybody wrote down, so it is pinned as gone.
//!
//! RAGGED INPUT IS REFUSED rather than clamped. The two lists are built side by side by one caller;
//! a mismatch is a programming error, and answering out of the first N entries would silently drop
//! configs off the end of the registry.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, execution belongs to the phase's suite run): change
//! the `names.Count() != priorities.Count()` guard to clamp to the shorter list and the ragged
//! assertion goes red; make IsAlreadyPresent answer true for a null set and the null-set assertion
//! goes red; re-introduce `int bestPriority = 999` with the loop's first-entry clause removed and
//! the high-priority assertion goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_BaseDefenseEscalation_PartialAndDefensiveInputs : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<string> names = {};
		names.Insert("Garrison Patrol");
		names.Insert("Checkpoints");

		array<int> priorities = {};
		priorities.Insert(1);
		priorities.Insert(3);

		// A NULL already-present set means "this place holds nothing" - the state every position is
		// in before its first deployment.
		int picked = OVT_DeploymentSelection.SelectNextConfigIndex(names, priorities, null);
		if (picked != 0)
		{
			SetFailure("With a null already-present set the pick was index %1, expected 0 ('Garrison Patrol') - a place that holds nothing must buy its cheapest priority", picked.ToString());
			return true;
		}

		// An EMPTY set is the same thing said differently.
		array<string> nothingHere = {};
		picked = OVT_DeploymentSelection.SelectNextConfigIndex(names, priorities, nothingHere);
		if (picked != 0)
		{
			SetFailure("With an empty already-present set the pick was index %1, expected 0", picked.ToString());
			return true;
		}

		// A set naming something that is not on offer here changes nothing.
		array<string> unrelated = {};
		unrelated.Insert("Town Patrol");
		picked = OVT_DeploymentSelection.SelectNextConfigIndex(names, priorities, unrelated);
		if (picked != 0)
		{
			SetFailure("A place already holding an unrelated config picked index %1, expected 0 - the filter is matching by something other than the name", picked.ToString());
			return true;
		}

		// PARTIAL: the cheapest one is here, so the next one is bought even though it costs a higher
		// priority number.
		array<string> partly = {};
		partly.Insert("Garrison Patrol");
		picked = OVT_DeploymentSelection.SelectNextConfigIndex(names, priorities, partly);
		if (picked != 1)
		{
			SetFailure("With the priority-1 config already here the pick was index %1, expected 1 ('Checkpoints') - this is the escalation itself", picked.ToString());
			return true;
		}

		// ALL PRESENT: nothing at all.
		array<string> everything = {};
		everything.Insert("Garrison Patrol");
		everything.Insert("Checkpoints");
		picked = OVT_DeploymentSelection.SelectNextConfigIndex(names, priorities, everything);
		if (picked != OVT_DeploymentSelection.NOTHING_TO_BUY)
		{
			SetFailure("A place holding both configs picked index %1, expected nothing", picked.ToString());
			return true;
		}

		// NOTHING ON OFFER is not an error either - it is every position that qualifies for no config.
		array<string> noNames = {};
		array<int> noPriorities = {};
		picked = OVT_DeploymentSelection.SelectNextConfigIndex(noNames, noPriorities, null);
		if (picked != OVT_DeploymentSelection.NOTHING_TO_BUY)
		{
			SetFailure("An empty offer answered index %1, expected nothing", picked.ToString());
			return true;
		}

		picked = OVT_DeploymentSelection.SelectNextConfigIndex(null, priorities, null);
		if (picked != OVT_DeploymentSelection.NOTHING_TO_BUY)
		{
			SetFailure("A null name list answered index %1, expected nothing", picked.ToString());
			return true;
		}

		picked = OVT_DeploymentSelection.SelectNextConfigIndex(names, null, null);
		if (picked != OVT_DeploymentSelection.NOTHING_TO_BUY)
		{
			SetFailure("A null priority list answered index %1, expected nothing", picked.ToString());
			return true;
		}

		// RAGGED: refused outright, never answered out of the shorter list.
		array<int> tooFew = {};
		tooFew.Insert(1);
		picked = OVT_DeploymentSelection.SelectNextConfigIndex(names, tooFew, null);
		if (picked != OVT_DeploymentSelection.NOTHING_TO_BUY)
		{
			SetFailure("Two names with one priority answered index %1, expected nothing - clamping to the shorter list silently drops configs off the end of the registry", picked.ToString());
			return true;
		}

		// PRIORITY HAS NO CEILING. The selection this replaced started at 999 and could never pick
		// anything at or above it.
		array<string> highNames = {};
		highNames.Insert("Very Low Priority Concern");

		array<int> highPriorities = {};
		highPriorities.Insert(1000);

		picked = OVT_DeploymentSelection.SelectNextConfigIndex(highNames, highPriorities, null);
		if (picked != 0)
		{
			SetFailure("The only config on offer, authored at priority 1000, answered index %1 - there is a hidden ceiling above which a config can never be bought", picked.ToString());
			return true;
		}

		// ...and 0 and negative priorities are ordinary values, not sentinels.
		array<string> edgeNames = {};
		edgeNames.Insert("Zero");
		edgeNames.Insert("Negative");

		array<int> edgePriorities = {};
		edgePriorities.Insert(0);
		edgePriorities.Insert(-5);

		picked = OVT_DeploymentSelection.SelectNextConfigIndex(edgeNames, edgePriorities, null);
		if (picked != 1)
		{
			SetFailure("Priorities 0 and -5 picked index %1, expected 1 ('Negative') - lower still has to win", picked.ToString());
			return true;
		}

		Print("Partial, empty and null already-present sets all behave; unrelated names are ignored; ragged input is refused; and priority has no hidden ceiling or sentinel");
		return true;
	}
}
