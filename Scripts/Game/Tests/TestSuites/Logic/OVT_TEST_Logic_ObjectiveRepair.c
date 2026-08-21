//------------------------------------------------------------------------------------------------
//! TIER A - THE OCCUPYING FACTION'S REPAIR DETAIL, as arithmetic.
//!
//! Three pure rules: the repair DECISION (held, unopposed, a whole interval, paused not reset), the
//! INTERVAL (seconds to updates, campaign figure over authored fallback, floored at one) and the
//! QUOTA (same precedence, same floor).
//!
//! ⚠ THE PRECEDENCE IS SPLIT INTO PURE STATICS SO THIS TIER CAN REACH IT AT ALL - the live resolvers
//! read campaign settings this suite may not touch, and the precedence itself is two integers and a
//! comparison. The other half of the claim, that the module reads the CAMPAIGN'S numbers, is
//! OVT_TEST_Init_BaseRepair case D.
//!
//! ⚠ A NON-POSITIVE "campaign figure" MEANS "NOT LOADED, OR NOT AUTHORED", which is exactly the state
//! a hand-built module in a world with no campaign is in - so the fallback path has to work.
//!
//! Every subject is a bare `new` module or a static call. No world, no manager, no campaign, no
//! polling, no maxAttempts.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The repair clock runs only while the detail holds the base unopposed, pauses rather than resets,
//! fires repeatedly rather than once, and stops dead when the mission ends.
//!
//! ⚠ THE LATCH BELONGS TO THE MISSION, NOT TO A FIRING. Spent on the first completed interval it
//! would cap every mission at one structure whatever the campaign authored, with no error anywhere.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   A1. `if (aliveInside < 1) return false;` deleted. Fails on "an empty base must not advance the
//!       repair clock".
//!   A2. `if (enemyPresent) return false;` deleted. Fails on "a defended base must not advance the
//!       repair clock".
//!   A3. The interrupted branch changed to reset ticksLeft to its starting value. Fails on "an
//!       interruption must PAUSE the clock, not reset it".
//!   A4. `if (m_bMissionReported) return false;` deleted. Fails on "a finished mission must never
//!       repair again".
//!   A5. A PER-FIRING LATCH ADDED - `m_bMissionReported = true;` inserted before EvaluateRepair's
//!       final `return true;`. Fails on "a re-armed interval must be able to complete again".
//!   A6. The `if (ticksLeft > 0) ticksLeft = ticksLeft - 1;` guard changed to an unconditional
//!       decrement. Fails on "a zero interval must still cost one tick".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveRepair_ADecisionHoldsAndPauses : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = CheckDecision();
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective repair: the repair clock runs only while the base is held and unopposed, pauses rather than resets on an interruption, fires repeatedly rather than once, and stops dead once the mission has finished");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckDecision()
	{
		OVT_BaseRepairBehaviorDeploymentModule repair = new OVT_BaseRepairBehaviorDeploymentModule();

		int ticks = 3;

		// --- Nobody there yet: the detail is still walking in.
		if (repair.EvaluateRepair(0, false, ticks))
			return "an empty base must not complete a repair interval";

		if (ticks != 3)
			return string.Format("an empty base must not advance the repair clock: %1 tick(s) left, expected 3", ticks.ToString());

		// --- Contested. The detail is there, so is a player.
		if (repair.EvaluateRepair(4, true, ticks))
			return "a defended base must not complete a repair interval";

		if (ticks != 3)
			return string.Format("a defended base must not advance the repair clock: %1 tick(s) left, expected 3", ticks.ToString());

		// --- Held and unopposed: one tick.
		if (repair.EvaluateRepair(4, false, ticks))
			return "an interval with three ticks left must not complete on the first of them";

		if (ticks != 2)
			return string.Format("a held, unopposed tick must advance the clock by exactly one: %1 tick(s) left, expected 2", ticks.ToString());

		// --- Interrupted mid-interval. THE CLOCK MUST PAUSE, NOT RESET.
		if (repair.EvaluateRepair(0, false, ticks))
			return "the detail being wiped mid-interval must not complete it";

		if (ticks != 2)
			return string.Format("an interruption must PAUSE the clock, not reset it: %1 tick(s) left, expected 2", ticks.ToString());

		// --- Run it out.
		if (repair.EvaluateRepair(4, false, ticks))
			return "an interval with two ticks left must not complete on the first of them";

		if (!repair.EvaluateRepair(4, false, ticks))
			return "an interval whose last tick was served must complete";

		// --- ⚠ AND IT MUST BE ABLE TO FIRE AGAIN. A detail puts back several structures; the caller
		// re-arms the clock and the decision has no per-firing latch to spend.
		if (repair.HasMissionReported())
			return "one completed interval must not end the whole mission - a detail puts back the campaign's authored quota of structures, not one";

		ticks = 1;
		if (!repair.EvaluateRepair(4, false, ticks))
			return "a re-armed interval must be able to complete again - a per-firing latch would cap every mission at one structure";

		// --- A MISAUTHORED ZERO INTERVAL STILL COSTS ONE TICK, so nothing can come back on the update
		// the detail is registered, before it is anywhere near the base.
		ticks = 0;
		if (!repair.EvaluateRepair(4, false, ticks))
			return "a zero interval must still complete on a held, unopposed tick";

		if (ticks != 0)
			return string.Format("a zero interval must still cost one tick and stay at zero rather than going negative: %1 left", ticks.ToString());

		// --- Once the MISSION is over, nothing more is repaired.
		repair.AbortMission();

		if (!repair.HasMissionReported())
			return "the fixture could not put the module into its stopped state, so the next claim would pass vacuously";

		ticks = 1;
		if (repair.EvaluateRepair(4, false, ticks))
			return "a finished mission must never repair again";

		if (ticks != 1)
			return string.Format("a finished mission must not even advance its clock: %1 tick(s) left, expected 1", ticks.ToString());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! THE CAMPAIGN'S NUMBER BEATS THE MODULE'S OWN, and both answers have a floor of one.
//!
//! ⚠ BOTH FIGURES ARE AUTHORED TWICE - a module fallback for a world with no campaign, and a
//! difficulty preset - so a module reading its own attribute would make Easy and Insane repair at the
//! same rate with nothing to see.
//!
//! ⚠ THE INTERVAL FLOOR IS NOT PADDING. Integer division of a sub-update interval answers ZERO, and
//! a zero interval is a clock that is already expired.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   B1. `if (difficultySeconds > 0) seconds = difficultySeconds;` deleted from IntervalTicksFrom.
//!       Fails on "the campaign's interval must win".
//!   B2. `if (seconds < UPDATE_SECONDS) return 1;` deleted. Fails on "a sub-update interval must
//!       still be one update".
//!   B3. `if (count < 1) return 1;` deleted from StructuresPerMissionFrom. Fails on "a quota must be
//!       at least one".
//!   B4. The difficulty branch in StructuresPerMissionFrom changed to `>= 0`, which would let a
//!       preset authoring nothing (0) override a real fallback. Fails on "an unauthored campaign
//!       figure must leave the module's own fallback standing".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveRepair_BIntervalAndQuotaPrecedence : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = CheckInterval();
		if (failure == "")
			failure = CheckQuota();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective repair: the campaign's interval and quota beat the module's authored fallbacks, an unauthored campaign figure leaves the fallback standing, and both answers are floored at one");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckInterval()
	{
		int updateSeconds = OVT_BaseRepairBehaviorDeploymentModule.UPDATE_SECONDS;
		if (updateSeconds < 1)
			return string.Format("UPDATE_SECONDS is %1 - every interval below divides by it, and a non-positive value would make the whole ladder meaningless", updateSeconds.ToString());

		// --- The campaign wins. 120 s of a 10 s update is twelve updates; the fallback would be 300.
		int ticks = OVT_BaseRepairBehaviorDeploymentModule.IntervalTicksFrom(3000, 120);
		if (ticks != 120 / updateSeconds)
			return string.Format("the campaign's interval must win: got %1 update(s), expected %2",
				ticks.ToString(), (120 / updateSeconds).ToString());

		// --- Nothing authored by the campaign: the module's own number is the answer.
		ticks = OVT_BaseRepairBehaviorDeploymentModule.IntervalTicksFrom(200, 0);
		if (ticks != 200 / updateSeconds)
			return string.Format("an unauthored campaign interval must leave the module's own fallback standing: got %1 update(s), expected %2",
				ticks.ToString(), (200 / updateSeconds).ToString());

		// --- A negative campaign figure is "not authored", not "negative time".
		ticks = OVT_BaseRepairBehaviorDeploymentModule.IntervalTicksFrom(200, -50);
		if (ticks != 200 / updateSeconds)
			return string.Format("a negative campaign interval must read as unauthored rather than as a real value: got %1 update(s), expected %2",
				ticks.ToString(), (200 / updateSeconds).ToString());

		// --- 🔴 THE FLOOR. Anything shorter than one update is still one update.
		ticks = OVT_BaseRepairBehaviorDeploymentModule.IntervalTicksFrom(3000, 1);
		if (ticks != 1)
			return string.Format("a sub-update interval must still be one update: got %1 - zero would put a structure back on the update the detail is registered, before it is anywhere near the base",
				ticks.ToString());

		ticks = OVT_BaseRepairBehaviorDeploymentModule.IntervalTicksFrom(0, 0);
		if (ticks != 1)
			return string.Format("an interval of nothing at all must still be one update: got %1", ticks.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckQuota()
	{
		int quota = OVT_BaseRepairBehaviorDeploymentModule.StructuresPerMissionFrom(91, 3);
		if (quota != 3)
			return string.Format("the campaign's quota must win: got %1, expected 3", quota.ToString());

		quota = OVT_BaseRepairBehaviorDeploymentModule.StructuresPerMissionFrom(4, 0);
		if (quota != 4)
			return string.Format("an unauthored campaign figure must leave the module's own fallback standing: got %1, expected 4", quota.ToString());

		quota = OVT_BaseRepairBehaviorDeploymentModule.StructuresPerMissionFrom(4, -2);
		if (quota != 4)
			return string.Format("a negative campaign quota must read as unauthored rather than as a real value: got %1, expected 4", quota.ToString());

		// --- 🔴 THE FLOOR. A preset authored at zero reads as one per mission, not as none.
		quota = OVT_BaseRepairBehaviorDeploymentModule.StructuresPerMissionFrom(0, 0);
		if (quota != 1)
			return string.Format("a quota must be at least one: got %1", quota.ToString());

		return "";
	}
}
