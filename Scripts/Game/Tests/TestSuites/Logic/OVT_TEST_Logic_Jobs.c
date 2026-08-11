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
//!
//! ONE CASE HERE IS NOT A CONDITION CASE: ..._LegacyIndexMapping pins the frozen version 1 job
//! table. It belongs in this tier for the same reason the conditions do - the two functions it
//! covers are pure statics that read a literal array and nothing else, so they need no world, no
//! manager and no config list. It names no storage type and no persisted record class; see its own
//! header.
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
//! REGRESSION for BUG-005 (formerly a pinned bug) - a dealer on the X = 0 plane is still a dealer.
//!
//! The condition used to be `if (town.gunDealerPosition && town.gunDealerPosition[0] != 0)`, so a
//! dealer standing anywhere on the X = 0 plane read as "this town has no dealer", no matter how far
//! along Z or Y they were. Fixed 2026-08-03 to the zero-vector check the intent always was
//! (`gunDealerPosition != vector.Zero`); this case asserts the input the old check got wrong.
//!
//! Nothing broke on Everon, where no town sits on X = 0 - the bug would only ever bite a map with
//! a town near the west edge of the world.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Jobs_DealerCondition_XZeroPlaneIsStillADealer : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownHasDealerJobCondition condition = new OVT_TownHasDealerJobCondition();

		OVT_TownData town = OVT_TEST_LogicFixture.MakeTown(50, 0);

		// A perfectly valid dealer position 500 m along Z, with X at exactly zero - the input the
		// old axis-only check misread as "no dealer".
		town.gunDealerPosition = "0 0 500";
		if (!condition.ShouldStart(town, null, -1))
		{
			SetResultFailure("BUG-005 REGRESSED: OVT_TownHasDealerJobCondition reported no dealer at %1 - a set position with X exactly 0 must read as a dealer", town.gunDealerPosition.ToString());
			return true;
		}

		// And the same dealer off the X = 0 plane is unchanged.
		town.gunDealerPosition = "1 0 500";
		if (!condition.ShouldStart(town, null, -1))
		{
			SetResultFailure("OVT_TownHasDealerJobCondition reported no dealer at %1", town.gunDealerPosition.ToString());
			return true;
		}

		Print("Dealer condition: a set position is a dealer regardless of which axes are zero");

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

//------------------------------------------------------------------------------------------------
//! T1 - THE FROZEN VERSION 1 JOB TABLE, PINNED AGAINST THIS CASE'S OWN LITERALS.
//!
//! WHAT IT GUARDS. OVT_JobManagerSerializer.LegacyIdForIndex() and IsRetiredLegacyId() are how a
//! campaign saved before the stable-id migration is read: the payload names each job by its POSITION
//! in the twelve-entry job list that existed then, and those two functions are the only thing that
//! turns a position back into the job it meant. Corrupt one row of that table and every affected
//! saved job comes back attached to a DIFFERENT job - at a stage index that is still valid, paying
//! that other job's reward, with its lifetime counters capping the wrong thing. No error, no log
//! line, no crash. It is the one table in this feature whose corruption is undetectable at runtime,
//! which is exactly why it is worth a case of its own.
//!
//! WHY THE EXPECTED VALUES ARE WRITTEN OUT HERE RATHER THAN READ FROM THE TABLE. A test that asserts
//! LEGACY_V1_JOB_IDS agrees with LEGACY_V1_JOB_IDS proves nothing at all - it stays green through
//! any edit, including the one it exists to stop. The three literal arrays below are an INDEPENDENT
//! WITNESS, transcribed from Prefabs/GameMode/OVT_OverthrowGameMode.et:26-48 (the m_aJobConfigs block
//! as it stood when version 1 payloads were written) and from the authoritative prose copy in
//! docs/features/new-player-experience/starter-jobs-retirement/context.md. Two independent copies of
//! history, and this case is the thing that notices when they stop agreeing.
//!
//! THEY ARE FROZEN TOO. This is history, not configuration. Adding a job to the game does not add a
//! row here and removing one does not remove a row: after the five retired jobs are deleted, all
//! twelve rows below are still exactly right about what a version 1 save meant. If a future reader
//! is here because this case went red after adding or deleting a job config, the correct fix is
//! almost certainly to put the TABLE back, not to update this case.
//!
//! TIER. World-free, per this file's header: both functions are pure statics over a literal array.
//! No manager, no game mode, no world, and no persistence-layer or persisted-record type is named.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Jobs_LegacyIndexMapping : SCR_AutotestCaseBase
{
	//! The twelve version 1 job indices, in order. Independent transcription - see the header.
	static const ref array<string> EXPECTED_V1_IDS = {
		"assassinate-traitor",    // 0
		"base-recon",             // 1
		"find-gun-dealer",        // 2
		"raise-support",          // 3
		"find-shop",              // 4
		"place-equipment-box",    // 5
		"recruit-a-civilian",     // 6
		"place-a-camp",           // 7
		"propaganda-run",         // 8
		"pirate-radio",           // 9
		"sabotage-radio-tower",   // 10
		"assassinate-officer"     // 11
	};

	//! The five version 1 jobs that left the game. A record naming one of these must be dropped.
	static const ref array<string> EXPECTED_RETIRED_IDS = {
		"find-gun-dealer",
		"find-shop",
		"place-equipment-box",
		"recruit-a-civilian",
		"place-a-camp"
	};

	//! The seven that stayed. A record naming one of these must be carried forward.
	static const ref array<string> EXPECTED_SURVIVING_IDS = {
		"assassinate-traitor",
		"base-recon",
		"raise-support",
		"propaganda-run",
		"pirate-radio",
		"sabotage-radio-tower",
		"assassinate-officer"
	};

	//! An id no job has ever carried, for the "unknown is not retired" edge.
	static const string UNKNOWN_ID = "not-a-job-that-ever-existed";

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// ---- 0. This case's own literals are internally consistent -------------------------------
		// A slip while transcribing the witness would weaken every assertion below it, silently, so
		// the witness is checked before it is used.
		string selfCheck = CheckOwnLiterals();
		if (selfCheck != "")
		{
			SetResultFailure(selfCheck);
			return true;
		}

		// ---- 1. Every one of the twelve indices resolves to the id it named -----------------------
		for (int index = 0; index < EXPECTED_V1_IDS.Count(); index++)
		{
			string actual = OVT_JobManagerSerializer.LegacyIdForIndex(index);
			if (actual != EXPECTED_V1_IDS[index])
			{
				SetResultFailure("The frozen version 1 job table is WRONG at index %1: it says '%2', but that index named '%3'. Every campaign saved before the stable-id migration would restore that job onto the wrong job - same stage index, different reward, wrong lifetime counters, and no error anywhere. The table is history and must never be edited: put it back.",
					index.ToString(), actual, EXPECTED_V1_IDS[index]);
				return true;
			}
		}

		// ---- 2. Out of range is an empty id, not a wrong one --------------------------------------
		// -1 and 12 are the two edges either side of the real table; the large values catch a bounds
		// check written against the wrong end.
		if (!AssertOutOfRange(-1))
			return true;

		if (!AssertOutOfRange(EXPECTED_V1_IDS.Count()))
			return true;

		if (!AssertOutOfRange(-999))
			return true;

		if (!AssertOutOfRange(999))
			return true;

		// ---- 3. Exactly the five are retired -------------------------------------------------------
		foreach (string retiredId : EXPECTED_RETIRED_IDS)
		{
			if (!OVT_JobManagerSerializer.IsRetiredLegacyId(retiredId))
			{
				SetResultFailure("'%1' is one of the five retired starter jobs but IsRetiredLegacyId() says it is not. Its saved board entries and lifetime counters would be carried forward instead of dropped, and it no longer exists.", retiredId);
				return true;
			}
		}

		// ---- 4. And none of the seven survivors is ------------------------------------------------
		foreach (string survivingId : EXPECTED_SURVIVING_IDS)
		{
			if (OVT_JobManagerSerializer.IsRetiredLegacyId(survivingId))
			{
				SetResultFailure("'%1' is a job that still exists but IsRetiredLegacyId() says it was retired. Every saved board entry and lifetime counter naming it would be DROPPED on the next load, with a log line claiming the job no longer exists.", survivingId);
				return true;
			}
		}

		// ---- 5. Neither an empty id nor an unknown one is retired ----------------------------------
		// An empty id is not an id at all, and an unknown id belongs to a job this table never knew -
		// dropping either is right, but reporting them as RETIRED would tell a bug reporter a story
		// about a starter job that has nothing to do with what actually happened.
		if (OVT_JobManagerSerializer.IsRetiredLegacyId(""))
		{
			SetResultFailure("IsRetiredLegacyId() reports the EMPTY id as retired. An empty id is not an id at all and must not be attributed to one of the five removed jobs.");
			return true;
		}

		if (OVT_JobManagerSerializer.IsRetiredLegacyId(UNKNOWN_ID))
		{
			SetResultFailure("IsRetiredLegacyId() reports the unknown id '%1' as retired. Only the five removed starter jobs may answer true.", UNKNOWN_ID);
			return true;
		}

		Print("Legacy v1 job table: all 12 indices map to their own ids, -1/12/±999 map to nothing, exactly the 5 retired ids are retired and none of the 7 survivors is");

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks that this case's three literal arrays agree with each other before they are trusted.
	//!
	//! The witness has to be right for the assertions built on it to mean anything: twelve entries,
	//! no duplicates, and every entry classified exactly once as retired or surviving.
	//! \return An empty string when the literals are consistent, otherwise the diagnostic to fail with.
	protected string CheckOwnLiterals()
	{
		if (EXPECTED_V1_IDS.Count() != 12)
			return string.Format("This case's own EXPECTED_V1_IDS has %1 entries, not 12. The version 1 job list had exactly twelve; fix the test before trusting it.", EXPECTED_V1_IDS.Count());

		if (EXPECTED_RETIRED_IDS.Count() + EXPECTED_SURVIVING_IDS.Count() != EXPECTED_V1_IDS.Count())
			return string.Format("This case's own literals disagree: %1 retired + %2 surviving is not %3 total.",
				EXPECTED_RETIRED_IDS.Count(), EXPECTED_SURVIVING_IDS.Count(), EXPECTED_V1_IDS.Count());

		for (int i = 0; i < EXPECTED_V1_IDS.Count(); i++)
		{
			string id = EXPECTED_V1_IDS[i];

			for (int j = i + 1; j < EXPECTED_V1_IDS.Count(); j++)
			{
				if (EXPECTED_V1_IDS[j] == id)
					return string.Format("This case's own EXPECTED_V1_IDS repeats '%1' at indices %2 and %3. Job ids are unique.", id, i.ToString(), j.ToString());
			}

			bool retired = Contains(EXPECTED_RETIRED_IDS, id);
			bool surviving = Contains(EXPECTED_SURVIVING_IDS, id);

			if (retired && surviving)
				return string.Format("This case's own literals list '%1' as BOTH retired and surviving.", id);

			if (!retired && !surviving)
				return string.Format("This case's own literals classify '%1' as neither retired nor surviving.", id);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that an index outside the twelve resolves to nothing.
	//! \param[in] index The out-of-range index to try.
	//! \return True when the index resolved to an empty id; false after failing the case.
	protected bool AssertOutOfRange(int index)
	{
		string actual = OVT_JobManagerSerializer.LegacyIdForIndex(index);
		if (actual == "")
			return true;

		SetResultFailure("Legacy index %1 is outside the twelve-entry version 1 job list but resolved to '%2'. An out-of-range index must resolve to nothing so the record is dropped and logged, never attached to a job it did not name.",
			index.ToString(), actual);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a literal array holds an id.
	//! \param[in] ids The array to search. Callers pass this case's own literals.
	//! \param[in] id The id to look for.
	//! \return True when the array holds the id.
	protected bool Contains(array<string> ids, string id)
	{
		foreach (string candidate : ids)
		{
			if (candidate == id)
				return true;
		}

		return false;
	}
}
