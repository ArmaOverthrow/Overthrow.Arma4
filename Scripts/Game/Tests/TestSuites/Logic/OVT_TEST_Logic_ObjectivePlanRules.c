//------------------------------------------------------------------------------------------------
//! TIER A cases - the objective PLAN arithmetic: which authored number wins, which plan wins, when a
//! phase advances, when an objective is abandoned, and where a persisted phase name lands.
//!
//! Every subject here is a static function of plain numbers and strings. Nothing in this file
//! resolves a manager, a controller or a world, and nothing needs to. See OVT_TEST_LogicSuite.c for
//! the tier rule and the house rules - including the reviewer grep over this directory, which does
//! not distinguish code from comments, so neither banned identifier appears anywhere below, prose
//! included.
//!
//! WHY THIS FILE EXISTS. occupying/objectives Phase 2 moved the occupying faction's doctrine out of a
//! 5,201-line component and into authored data. Four of the decisions that came with it are pure
//! arithmetic, and all four are the kind that fail SILENTLY:
//!
//!   1. A SENTINEL THAT LEAKS IS A FOREVER. Every module attribute with a difficulty twin defaults to
//!      -1 meaning "use the campaign's setting". If ResolveWithDifficulty() ever hands that -1 back
//!      out, it becomes a countdown, a cap or a radius of MINUS ONE somewhere downstream - which
//!      reads as "never", "nothing" or "no reach" depending on who got it, and logs nothing.
//!   2. A PRIORITY THAT INVERTS IS A PLAN THAT NEVER RUNS. The objective convention is a float
//!      multiplier where HIGHER wins; the deployment convention next door is an int where LOWER wins.
//!      Two conventions in one tree is a real cost and these rows are what stops it becoming a bug.
//!   3. THE TWO EMPTY ANSWERS ARE OPPOSITE AND BOTH ARE LOAD-BEARING. A phase with no conditions must
//!      not be BLOCKED; a phase with no aborts must not ABORT. Getting either backwards produces a
//!      machine that either never advances or never runs, and neither says anything in a log.
//!   4. AN UNKNOWN PHASE NAME MUST NOT BE PHASE ZERO. The save payload carries phase NAMES precisely
//!      so a renamed phase can be detected; answering 0 for an unknown one would adopt the first
//!      phase of a plan instead, which is the exact silent-adoption the names were introduced to end.
//!
//! ALSO NOT HERE, AND ON PURPOSE: that the RUNNER refuses to advance on an empty condition set even
//! though AllConditionsMet() answers true for one. That is a claim about a live component - the
//! distinction is deliberate and is documented at RunObjectiveConditionModules() - so it is pinned one
//! tier up. This file pins only what the arithmetic says.
//!
//! CAN-FAIL PROOFS. Running a suite is the orchestrator's job, not an implementation agent's
//! (.claude/test-policy.md), so each proof below is a fault that was injected into the subject one at
//! a time and compiled - every one exited tools/compile-check.sh with 0, which is the point: none of
//! these are syntax errors and nothing else in the tree would stop them reaching players. The subject
//! was restored and re-compiled clean afterwards.
//!
//! No maxAttempts anywhere: every subject is a pure function with no clock, no randomness and no
//! world, and cannot flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! An authored attribute beats the difficulty setting; the sentinel defers to it; and the sentinel
//! never leaks out the other side.
//!
//! ⚠ ZERO IS AN AUTHORED VALUE, NOT AN UNSET ONE, and that row is the sharpest in the case. The
//! obvious implementation - "if the authored value is falsy, use difficulty" - passes every other row
//! here and quietly makes it impossible to author a cadence of zero, a cap of zero or a radius of
//! zero. The sentinel is NEGATIVE precisely so that zero can mean zero.
//!
//! ⚠ THE ABSURD ROW ASSERTS THAT NOTHING IS CLAMPED, which is a claim about scope rather than about
//! safety: "absurd" is domain knowledge this function does not have, and a shared clamp would be
//! wrong for every second caller (20 000 is nonsense for a concurrency cap and ordinary for a radius
//! in metres). Where a range genuinely exists it is clamped by the caller that knows it.
//!
//! CAN-FAIL, injected and compiled separately, both exit 0:
//!   P1. TREAT ZERO AS UNSET - change `if (authored > USE_DIFFICULTY)` to `if (authored > 0)`.
//!       Compiled clean. The case then fails on "an authored ZERO is a value, not an absence: authored
//!       0 against difficulty 60 answered 60, expected 0".
//!   P2. LET THE SENTINEL ESCAPE - return `authored` unconditionally. Compiled clean. The case then
//!       fails on "the sentinel must never reach a caller as a number: authored -1 against difficulty
//!       60 answered -1, expected 60".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectivePlanRules_DifficultyFallbackHonoursTheAuthoredValue : SCR_AutotestCaseBase
{
	//! The campaign's harassment interval on Normal, in in-game minutes. A real shipped figure, so a
	//! fault that returns a constant cannot coincidentally match every row.
	protected const int DIFFICULTY_VALUE = 60;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: the sentinel defers, and never leaks.
		if (!ExpectResolved(OVT_ObjectivePlanRules.USE_DIFFICULTY, DIFFICULTY_VALUE, DIFFICULTY_VALUE, "the sentinel must never reach a caller as a number"))
			return true;

		// A mis-keyed -2 is a sentinel too: nobody ever means "minus two ticks".
		if (!ExpectResolved(-2, DIFFICULTY_VALUE, DIFFICULTY_VALUE, "any negative authored value is the sentinel, because a mis-keyed -2 is never an intent"))
			return true;

		// --- Claim 2: ZERO IS A VALUE. See the header - this is the row the obvious fault passes.
		if (!ExpectResolved(0, DIFFICULTY_VALUE, 0, "an authored ZERO is a value, not an absence"))
			return true;

		// --- Claim 3: an authored number wins outright, high or low.
		if (!ExpectResolved(45, DIFFICULTY_VALUE, 45, "an authored value must beat the difficulty setting"))
			return true;

		if (!ExpectResolved(1, DIFFICULTY_VALUE, 1, "an authored value of one must beat the difficulty setting like any other"))
			return true;

		// --- Claim 4: nothing is clamped. An absurd authored value is the author's business.
		if (!ExpectResolved(20000, DIFFICULTY_VALUE, 20000, "an absurd authored value is honoured verbatim - this function has no idea what unit it is in"))
			return true;

		// --- Claim 5: an unusable difficulty value under the sentinel answers a NUMBER, not a sentinel.
		if (!ExpectResolved(OVT_ObjectivePlanRules.USE_DIFFICULTY, -1, OVT_ObjectivePlanRules.NO_VALUE, "a sentinel deferring to an unusable setting must still answer a number"))
			return true;

		if (!ExpectResolved(OVT_ObjectivePlanRules.USE_DIFFICULTY, 0, 0, "a difficulty setting of zero is a setting, and is deferred to like any other"))
			return true;

		Print("Objective plan rules: the sentinel defers and never leaks, an authored zero is a value, an absurd value is honoured, and an unusable pair still answers a number");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one difficulty-resolution row.
	//! \param[in] authored The module attribute.
	//! \param[in] difficultyValue The campaign's twin setting.
	//! \param[in] expected What the machine must use.
	//! \param[in] claim What this row is claiming, for the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectResolved(int authored, int difficultyValue, int expected, string claim)
	{
		int actual = OVT_ObjectivePlanRules.ResolveWithDifficulty(authored, difficultyValue);
		if (actual == expected)
			return true;

		SetFailure(string.Format("%1: authored %2 against difficulty %3 answered %4, expected %5",
			claim, authored.ToString(), difficultyValue.ToString(), actual.ToString(), expected.ToString()));

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! A plan's rank is its selector score times its priority, higher wins, and neither half may go
//! negative.
//!
//! ⚠ THE NEGATIVE ROWS ARE THE ONLY OBSERVABLE HALF OF THE CONVENTION GUARD. A multiplier is not
//! merely tuning here: a negative one applied to a positive score would sort a bad plan ABOVE a good
//! one, and a negative score under a negative multiplier would sort it above everything. Both are
//! mis-keys rather than intents, and both are floored rather than rejected, because a plan that
//! silently never runs is worse than one that runs at rank zero and can be seen doing it.
//!
//! ⚠ ZERO IS A SUPPORTED AUTHORING GESTURE, NOT AN EDGE CASE. "Ship this doctrine but do not run it"
//! is exactly what a mod author wants while building a third plan, and it is why the multiplier is
//! not clamped to a minimum of one.
//!
//! CAN-FAIL, injected and compiled separately, both exit 0:
//!   P3. DROP THE PRIORITY FLOOR - remove `if (priority < 0) priority = 0;`. Compiled clean. The case
//!       then fails on "a mis-keyed negative multiplier must never out-rank a real plan: score 80 at
//!       priority -2 answered -160, expected 0".
//!   P4. ADD INSTEAD OF MULTIPLY - return `score + priority`. Compiled clean. The case then fails on
//!       the very first row: "a plan with no opinion must be scored on its selector alone: score 80 at
//!       priority 1 answered 81, expected 80".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectivePlanRules_PlanScoreMultipliesAndNeverGoesNegative : SCR_AutotestCaseBase
{
	//! A plausible selector score. Not round, so a fault returning a constant cannot match every row.
	protected const float SCORE = 80;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: the default multiplier changes nothing, which is what every plan that authors
		//     no opinion must behave as.
		if (!ExpectScore(SCORE, OVT_ObjectivePlanRules.DEFAULT_PRIORITY, SCORE, "a plan with no opinion must be scored on its selector alone"))
			return true;

		// --- Claim 2: higher wins, and it is a MULTIPLIER.
		if (!ExpectScore(SCORE, 2, 160, "a priority of two must double the plan's rank"))
			return true;

		if (!ExpectScore(SCORE, 0.5, 40, "a fractional priority must halve the plan's rank, not floor it away"))
			return true;

		// --- Claim 3: zero excludes, deliberately.
		if (!ExpectScore(SCORE, 0, 0, "a priority of zero must exclude the plan outright - that is a supported authoring gesture"))
			return true;

		// --- Claim 4: nothing may come back negative, from either half.
		if (!ExpectScore(SCORE, -2, 0, "a mis-keyed negative multiplier must never out-rank a real plan"))
			return true;

		if (!ExpectScore(-50, 2, 0, "a negative selector score must read as no score, never as a rank"))
			return true;

		if (!ExpectScore(-50, -2, 0, "two negatives must not multiply into a winning rank"))
			return true;

		Print("Objective plan rules: rank is score times priority, one changes nothing, zero excludes, and no combination of negatives can out-rank a real plan");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one plan-rank row. Floats compare with the tier's epsilon, never with ==.
	//! \param[in] score The selector's score.
	//! \param[in] priority The plan's multiplier.
	//! \param[in] expected The rank.
	//! \param[in] claim What this row is claiming, for the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectScore(float score, float priority, float expected, string claim)
	{
		float actual = OVT_ObjectivePlanRules.ResolvePlanScore(score, priority);
		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		SetFailure(string.Format("%1: score %2 at priority %3 answered %4, expected %5",
			claim, score.ToString(), priority.ToString(), actual.ToString(), expected.ToString()));

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The highest-ranked ELIGIBLE plan wins, ties go to the authored order, and nothing eligible selects
//! nothing.
//!
//! ⚠ "NOTHING" IS -1 AND NOT 0, AND THAT IS THE WHOLE POINT OF THE ROW THAT ASSERTS IT. Zero is a real
//! plan index. A selector that answered 0 for "no plan was eligible" would commit the campaign to the
//! first plan in the registry every time nothing qualified - which in an early campaign, where the
//! resistance holds nothing worth attacking, is every single in-game minute.
//!
//! ⚠ THE TIE ROW IS A PREDICTABILITY CLAIM, NOT A TIDINESS ONE. This feature's whole reason for
//! existing is that the occupying faction's intent should be READABLE, and there is no jitter anywhere
//! in the selection path. A strict comparison is what makes the authored order of the registry the
//! answer to a tie rather than whatever the comparison happened to do.
//!
//! CAN-FAIL, injected and compiled separately, both exit 0:
//!   P5. IGNORE ELIGIBILITY - remove `if (!eligible[i]) continue;`. Compiled clean. The case then
//!       fails on "an ineligible plan must not win however it scored: expected 2, got 1".
//!   P6. LET TIES GO TO THE LATER ENTRY - change `scores[i] <= bestScore` to `scores[i] < bestScore`.
//!       Compiled clean. The case then fails on "a tie must go to the earlier entry, because the
//!       authored order is the tie-break: expected 0, got 2".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectivePlanRules_SelectBestPlanIndexPicksTheHighestAndTiesByOrder : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<float> scores = new array<float>();
		array<bool> eligible = new array<bool>();

		// --- Claim 1: the highest eligible score wins.
		Row(scores, eligible, 10, true);
		Row(scores, eligible, 90, true);
		Row(scores, eligible, 50, true);

		if (!Expect(scores, eligible, 1, "the highest-ranked plan must win"))
			return true;

		// --- Claim 2: an ineligible plan is invisible however it scored.
		eligible[1] = false;

		if (!Expect(scores, eligible, 2, "an ineligible plan must not win however it scored"))
			return true;

		// --- Claim 3: ties go to the earlier entry. Three equal scores, all eligible.
		scores.Clear();
		eligible.Clear();
		Row(scores, eligible, 42, true);
		Row(scores, eligible, 41, true);
		Row(scores, eligible, 42, true);

		if (!Expect(scores, eligible, 0, "a tie must go to the earlier entry, because the authored order is the tie-break"))
			return true;

		// --- Claim 4: nothing eligible selects NOTHING, and nothing is -1.
		eligible[0] = false;
		eligible[1] = false;
		eligible[2] = false;

		if (!Expect(scores, eligible, OVT_ObjectivePlanRules.NOTHING_TO_SELECT, "a registry with nothing eligible must select NOTHING, never plan zero"))
			return true;

		// --- Claim 5: an empty registry selects nothing, and so does a mismatched pair of arrays.
		scores.Clear();
		eligible.Clear();

		if (!Expect(scores, eligible, OVT_ObjectivePlanRules.NOTHING_TO_SELECT, "an empty registry must select NOTHING, never plan zero"))
			return true;

		Row(scores, eligible, 42, true);
		eligible.Clear();

		if (!Expect(scores, eligible, OVT_ObjectivePlanRules.NOTHING_TO_SELECT, "parallel arrays of different lengths must select nothing rather than read past the shorter one"))
			return true;

		// --- Claim 6: a zero rank is still a rank when it is the only eligible one. A plan excluded by
		//     a zero multiplier is excluded by ELIGIBILITY, never by arithmetic.
		scores.Clear();
		eligible.Clear();
		Row(scores, eligible, 0, true);

		if (!Expect(scores, eligible, 0, "a lone eligible plan must win even at rank zero"))
			return true;

		Print("Objective plan rules: the highest eligible rank wins, ineligible plans are invisible, ties go to the authored order, and nothing eligible answers NOTHING rather than plan zero");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one plan to the parallel arrays.
	//! \param[in] scores The score array.
	//! \param[in] eligible The eligibility array.
	//! \param[in] score The plan's rank.
	//! \param[in] isEligible Whether faction, cap and chance let it compete.
	protected void Row(notnull array<float> scores, notnull array<bool> eligible, float score, bool isEligible)
	{
		scores.Insert(score);
		eligible.Insert(isEligible);
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one selection row.
	//! \param[in] scores The score array.
	//! \param[in] eligible The eligibility array.
	//! \param[in] expected The index that must win.
	//! \param[in] claim What this row is claiming, for the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool Expect(notnull array<float> scores, notnull array<bool> eligible, int expected, string claim)
	{
		int actual = OVT_ObjectivePlanRules.SelectBestPlanIndex(scores, eligible);
		if (actual == expected)
			return true;

		SetFailure(string.Format("%1: expected %2, got %3", claim, expected.ToString(), actual.ToString()));

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The two empty answers are OPPOSITE, and both of them are right.
//!
//! 🔴 THIS IS THE CASE THAT EXISTS BECAUSE THE TWO FOLDS LOOK IDENTICAL AND ARE NOT. An AND over an
//! empty set is true; an OR over an empty set is false. Written as two three-line loops next to each
//! other, they are trivially easy to make symmetrical - and symmetrical is wrong whichever way it is
//! made:
//!   BOTH FALSE  a phase with no conditions can never advance. The plan wedges at its first phase and
//!               the only symptom is that the occupying faction stops attacking.
//!   BOTH TRUE   a phase with no aborts abandons its objective on its first tick. The campaign churns
//!               through targets forever and never builds toward one.
//! Neither failure logs anything. That is why the two claims are asserted in ONE case that names both.
//!
//! CAN-FAIL, injected and compiled separately, both exit 0:
//!   P7. MAKE THE AND EMPTY-FALSE - add `if (results.IsEmpty()) return false;` to AllConditionsMet().
//!       Compiled clean. The case then fails on its very first row: "an empty condition set must not
//!       BLOCK a phase: AllConditionsMet answered false, expected true".
//!   P8. MAKE THE OR EMPTY-TRUE - add `if (results.IsEmpty()) return true;` to AnyAbort(). Compiled
//!       clean. The case then fails on its second row: "an empty abort set must not ABORT an objective:
//!       AnyAbort answered true, expected false".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectivePlanRules_EmptyConditionsPassAndEmptyAbortsDoNot : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<bool> results = new array<bool>();

		// --- Claim 1: the two empty answers, side by side, opposite.
		if (!ExpectConditions(results, true, "an empty condition set must not BLOCK a phase"))
			return true;

		if (!ExpectAbort(results, false, "an empty abort set must not ABORT an objective"))
			return true;

		// --- Claim 2: the AND actually ANDs.
		results.Insert(true);
		if (!ExpectConditions(results, true, "one satisfied condition must satisfy the set"))
			return true;

		results.Insert(false);
		if (!ExpectConditions(results, false, "one unsatisfied condition must block the whole set"))
			return true;

		results.Clear();
		results.Insert(false);
		results.Insert(true);
		if (!ExpectConditions(results, false, "the unsatisfied condition must block wherever it sits in the order"))
			return true;

		// --- Claim 3: the OR actually ORs.
		results.Clear();
		results.Insert(false);
		results.Insert(false);
		if (!ExpectAbort(results, false, "no abort firing must not abort"))
			return true;

		results.Insert(true);
		if (!ExpectAbort(results, true, "one abort firing must abort the objective"))
			return true;

		results.Clear();
		results.Insert(true);
		results.Insert(false);
		if (!ExpectAbort(results, true, "the firing abort must count wherever it sits in the order"))
			return true;

		// --- Claim 4: a null set behaves like an empty one, in both directions. The runner builds these
		//     arrays per tick and a phase with no modules of a role produces one.
		if (!ExpectConditions(null, true, "a null condition set must read as an empty one, not as a block"))
			return true;

		if (!ExpectAbort(null, false, "a null abort set must read as an empty one, not as an abort"))
			return true;

		Print("Objective plan rules: conditions AND with an empty set passing, aborts OR with an empty set refusing - the two empty answers are opposite and both are load-bearing");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one condition-fold row.
	//! \param[in] results One answer per condition module.
	//! \param[in] expected What the fold must answer.
	//! \param[in] claim What this row is claiming, for the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectConditions(array<bool> results, bool expected, string claim)
	{
		bool actual = OVT_ObjectivePlanRules.AllConditionsMet(results);
		if (actual == expected)
			return true;

		SetFailure(string.Format("%1: AllConditionsMet answered %2, expected %3", claim, actual.ToString(), expected.ToString()));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one abort-fold row.
	//! \param[in] results One answer per abort module.
	//! \param[in] expected What the fold must answer.
	//! \param[in] claim What this row is claiming, for the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectAbort(array<bool> results, bool expected, string claim)
	{
		bool actual = OVT_ObjectivePlanRules.AnyAbort(results);
		if (actual == expected)
			return true;

		SetFailure(string.Format("%1: AnyAbort answered %2, expected %3", claim, actual.ToString(), expected.ToString()));

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! A persisted phase name becomes an index, and a name this build has never shipped becomes -1.
//!
//! 🔴 THE -1 IS THE ENTIRE POINT OF PUTTING NAMES IN THE SAVE PAYLOAD. The record used to carry an
//! enum integer, which meant a build that renumbered or removed a phase re-labelled every saved
//! objective silently - and the only defence was a header comment telling every future reader never
//! to renumber. Names replace that comment with a MECHANISM, and the mechanism is this function
//! answering "I have never heard of that" rather than answering "phase zero".
//!
//! ⚠ AN EMPTY NAME MATCHES NOTHING, EVEN AGAINST A LIST THAT CONTAINS AN EMPTY ENTRY. A phase with no
//! name is a config fault the registry's validator refuses outright; letting an unset field match one
//! would restore an objective into a phase nobody authored.
//!
//! CAN-FAIL, injected and compiled separately, both exit 0:
//!   P9.  RETURN ZERO FOR AN UNKNOWN NAME - change the final `return NO_PHASE_INDEX;` to `return 0;`.
//!        Compiled clean. The case then fails on "a phase name this build has never shipped must
//!        answer NOTHING, never phase zero: 'Bombardment' answered 0, expected -1".
//!   P10. DROP THE EMPTY-NAME GUARD - remove `|| name == ""` from the entry test. Compiled clean. The
//!        case then fails on "an unset phase name must match nothing, even against a list with an
//!        empty entry: '' answered 1, expected -1".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectivePlanRules_PhaseIndexOfAnswersMinusOneForAnUnknownName : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The three phase names both shipped plans author, in their authored order.
		array<string> names = new array<string>();
		names.Insert("Harassment");
		names.Insert("ForwardBase");
		names.Insert("CounterAttack");

		// --- Claim 1: every authored name resolves to its own index.
		if (!Expect(names, "Harassment", 0, "the first phase must resolve to index zero"))
			return true;

		if (!Expect(names, "ForwardBase", 1, "the middle phase must resolve to its own index"))
			return true;

		if (!Expect(names, "CounterAttack", 2, "the last phase must resolve to its own index"))
			return true;

		// --- Claim 2: a name this build has never shipped answers NOTHING.
		if (!Expect(names, "Bombardment", OVT_ObjectivePlanRules.NO_PHASE_INDEX, "a phase name this build has never shipped must answer NOTHING, never phase zero"))
			return true;

		// --- Claim 3: matching is exact. A phase name is an identifier an author types twice.
		if (!Expect(names, "harassment", OVT_ObjectivePlanRules.NO_PHASE_INDEX, "phase names are case-sensitive, or two phases differing only in case would both resolve"))
			return true;

		if (!Expect(names, "Harassment ", OVT_ObjectivePlanRules.NO_PHASE_INDEX, "a trailing space is a different name, because nothing here trims"))
			return true;

		// --- Claim 4: an empty name matches nothing, even against a list that has an empty entry.
		array<string> withUnnamed = new array<string>();
		withUnnamed.Insert("Harassment");
		withUnnamed.Insert("");
		withUnnamed.Insert("CounterAttack");

		if (!Expect(withUnnamed, "", OVT_ObjectivePlanRules.NO_PHASE_INDEX, "an unset phase name must match nothing, even against a list with an empty entry"))
			return true;

		// A named phase AFTER the unnamed one still resolves to its real index: a null or unnamed entry
		// occupies its slot rather than shifting everything after it one place to the left.
		if (!Expect(withUnnamed, "CounterAttack", 2, "an unnamed phase must occupy its index rather than shifting the phases after it"))
			return true;

		// --- Claim 5: an empty plan and a null list both answer NOTHING.
		array<string> empty = new array<string>();

		if (!Expect(empty, "Harassment", OVT_ObjectivePlanRules.NO_PHASE_INDEX, "a plan with no phases must answer NOTHING for every name"))
			return true;

		if (!Expect(null, "Harassment", OVT_ObjectivePlanRules.NO_PHASE_INDEX, "a null phase list must answer NOTHING rather than throwing"))
			return true;

		Print("Objective plan rules: an authored phase name resolves to its own index, and an unknown, mis-cased, padded, empty or absent one answers NOTHING rather than phase zero");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one phase-lookup row.
	//! \param[in] names The plan's phase names, in authored order.
	//! \param[in] name The name to find.
	//! \param[in] expected The index it must resolve to.
	//! \param[in] claim What this row is claiming, for the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool Expect(array<string> names, string name, int expected, string claim)
	{
		int actual = OVT_ObjectivePlanRules.PhaseIndexOf(names, name);
		if (actual == expected)
			return true;

		SetFailure(string.Format("%1: '%2' answered %3, expected %4", claim, name, actual.ToString(), expected.ToString()));

		return false;
	}
}
