//------------------------------------------------------------------------------------------------
//! TIER A cases - the objective RESERVE FLOOR: the arithmetic that stops the occupying faction's
//! routine garrisoning outbidding its own strategic operation.
//!
//! Every subject here is a static function of plain integers and strings. Nothing in this file
//! resolves a manager, a controller or a world, and nothing needs to. See OVT_TEST_LogicSuite.c for
//! the tier rule and the house rules - including the reviewer grep over this directory, which does
//! not distinguish code from comments, so neither banned identifier appears anywhere below, prose
//! included.
//!
//! WHY THIS FILE EXISTS. occupying/counter-attacks D18 put a subtraction and a clamp inside
//! OVT_DeploymentManagerComponent.EvaluateFactionDeployments() - the pass that decides where every
//! routine deployment in the campaign is created and that thirteen shipped configs depend on. The
//! whole safety argument for that edit is three claims about the functions below:
//!
//!   1. WITH NOTHING RESERVED, SPENDING IS UNCHANGED. The evaluator's budget is the pool it always
//!      was, and no faction in a campaign that never starts an objective is ever floored.
//!   2. THE ANSWER IS NEVER NEGATIVE. The caller subtracts the answer from a real pool, so a
//!      negative answer would CREDIT the faction - a hole in the "every resource leaves the pool
//!      exactly once" identity (G5) rather than a tuning error. A reserve larger than the pool is
//!      not an edge case, it is the NORMAL state of a faction saving up for an operation.
//!   3. THE FLOOR ACTUALLY BINDS. A gate that answers "yes, spend it" whatever is reserved compiles,
//!      logs, persists and does nothing at all, which is indistinguishable from the defect it was
//!      written to fix.
//!
//! ⚠ ONE THING IS DELIBERATELY NOT CLAIMED, BECAUSE IT IS PROVABLY UNOBSERVABLE, and it is worth
//! saying because the sibling anchor arithmetic DOES claim it. SpendableResources() returns early on
//! a zero reserve rather than falling through to `pool - reserve`, and no case can tell those two
//! apart: integer subtraction of an exact zero is exact, unlike the float arithmetic the anchor's
//! identity claim has to defend itself against. The early return is there so that "unchanged" is a
//! FACT about the code path rather than an argument about arithmetic; what the rows below actually
//! pin is the neighbouring claim that a NEGATIVE reserve does not INFLATE the budget, which is the
//! same guard and is observable.
//!
//! ALSO NOT HERE, AND ON PURPOSE: that the evaluator writes the debited POOL back to the resource map
//! rather than the spendable figure, that the floor governs the routine pass and nothing else, and
//! that the objective machine drops it on every teardown path. Those are claims about live components
//! rather than about arithmetic, so they are pinned one tier up (OVT_TEST_Init_ObjectiveReserve.c).
//! The first of them is the most dangerous line in the whole change - writing back the wrong integer
//! destroys the reserved resources outright - so it is named here so nobody reads this file as the
//! whole story.
//!
//! CAN-FAIL PROOFS. Running a suite is the orchestrator's job, not an implementation agent's
//! (.claude/test-policy.md), so each proof below is a fault that was injected into the subject one at
//! a time and compiled - every one exited tools/compile-check.sh with 0, which is the point: none of
//! these are syntax errors and nothing else in the tree would stop them reaching players. The subject
//! was restored and re-compiled clean afterwards.
//!
//! No maxAttempts anywhere: every subject is a pure function with no clock, no RNG and no world, and
//! cannot flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! A faction nobody has earmarked anything for may spend its whole pool, and one that has been
//! earmarked for may spend the remainder - never less than nothing, never more than it holds.
//!
//! THE FIRST HALF IS THE REGRESSION PROPERTY OF THE WHOLE CHANGE, and it is asserted with `==`
//! rather than with a tolerance because there is nothing to tolerate: these are integers, and "the
//! evaluator's budget is the pool it always was" is either exactly true or it is a behaviour change.
//!
//! ⚠ THE NEGATIVE-RESERVE ROWS ARE NOT PARANOIA, THEY ARE THE ONLY OBSERVABLE HALF OF THE GUARD. See
//! the file header: a missing `if (reserve <= NO_RESERVE)` is invisible for a reserve of exactly
//! zero and turns a negative one into a BONUS - a faction whose earmark had underflowed, or whose
//! operation had been mis-priced below zero, would be handed a budget larger than its pool and would
//! spend money it does not have. That is the same class of fault as the negative clamp, pointing the
//! other way.
//!
//! ⚠ THE OVER-RESERVED ROWS ARE THE ORDINARY CASE, NOT THE CORNER. The floor exists precisely so a
//! faction can accumulate toward an operation it cannot yet afford, so "reserve exceeds pool" is what
//! the campaign looks like for the entire time the mechanism is doing its job. Nothing about it may
//! read as an error, and above all it may not come back negative.
//!
//! CAN-FAIL, two faults, injected and compiled separately because the case makes two independent
//! claims and one fault must not stand in for the other. Both exited tools/compile-check.sh 0:
//!   L1. DROP THE RESERVE GUARD - remove `if (reserve <= NO_RESERVE) return pool;` from
//!       SpendableResources(). Compiled clean (exit 0). Every zero-reserve row still passes, which is
//!       exactly the point of the header's honesty note; the case then fails on the negative row:
//!       "a nonsense earmark must never INFLATE the budget: pool 240 with reserve -60 answered 300,
//!       expected 240".
//!   L2. DROP THE NEGATIVE CLAMP - remove `if (spendable < 0) return 0;`. Compiled clean (exit 0).
//!       The case then fails on
//!       "a reserve larger than the pool must read as 'spend nothing', never as a credit: pool 20
//!       with reserve 120 answered -100, expected 0".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveReserveFloor_NothingReservedSpendsEverything : SCR_AutotestCaseBase
{
	//! An arbitrary but non-round pool, chosen so a fault that returns a constant, a zero or a
	//! rounded value cannot coincidentally match it.
	protected const int POOL = 240;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: nothing earmarked means the whole pool, which is what every faction in every
		//     campaign that never starts an objective is in.
		if (!ExpectSpendable(POOL, OVT_DeploymentSelection.NO_RESERVE, POOL, "a faction nobody has earmarked anything for must be able to spend its whole pool"))
			return true;

		if (!ExpectSpendable(0, OVT_DeploymentSelection.NO_RESERVE, 0, "an empty pool with nothing earmarked must read as an empty pool"))
			return true;

		// --- ...INCLUDING when the earmark is nonsense. This is the only row that reaches the reserve
		//     guard at all - see the file header for why a zero reserve cannot.
		if (!ExpectSpendable(POOL, -60, POOL, "a nonsense earmark must never INFLATE the budget"))
			return true;

		// --- Claim 2: an earmark takes exactly itself out of the budget and no more.
		if (!ExpectSpendable(POOL, 100, POOL - 100, "an earmark must withhold exactly its own price"))
			return true;

		if (!ExpectSpendable(POOL, POOL, 0, "an earmark equal to the pool must leave nothing to spend and must not overshoot"))
			return true;

		// --- Claim 3: over-reserved is the NORMAL state of a faction saving up, and must never credit.
		if (!ExpectSpendable(20, 120, 0, "a reserve larger than the pool must read as 'spend nothing', never as a credit"))
			return true;

		if (!ExpectSpendable(0, 120, 0, "an empty pool under an earmark must read as zero, not as a debt"))
			return true;

		Print("Objective reserve floor: nothing earmarked spends the whole pool, an earmark withholds exactly its own price, and an over-reserved pool reads as zero rather than as a credit");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one spendable-budget row.
	//! \param[in] pool What the faction holds.
	//! \param[in] reserve What is earmarked out of it.
	//! \param[in] expected What the evaluator must be allowed to spend.
	//! \param[in] claim What this row is claiming, for the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectSpendable(int pool, int reserve, int expected, string claim)
	{
		int actual = OVT_DeploymentSelection.SpendableResources(pool, reserve);
		if (actual == expected)
			return true;

		SetFailure(string.Format("%1: pool %2 with reserve %3 answered %4, expected %5",
			claim, pool.ToString(), reserve.ToString(), actual.ToString(), expected.ToString()));

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The gate the routine evaluator asks: this purchase is allowed only if it leaves the earmark
//! standing.
//!
//! WHY THIS IS A SEPARATE CASE FROM THE ARITHMETIC ABOVE. The budget function could be perfect and
//! the gate could still ignore it - an inline `pool >= cost` at the call site, a comparison against
//! the wrong local, a `>` where a `>=` belongs. This case asks the decision rather than the number,
//! and the row that matters is the one where the purchase is affordable OUT OF THE POOL and refused
//! OUT OF THE BUDGET. That row is the entire feature.
//!
//! ⚠ THE BOUNDARY IS CLAIMED IN BOTH DIRECTIONS, and it is worth a row each way because the whole
//! rule is an inequality. A purchase that spends the budget down to exactly the earmark is ALLOWED -
//! the floor is a floor, not a cushion, and a faction that could never spend its last spendable
//! resource would sit on money it is meant to be using. One more than that is refused.
//!
//! CAN-FAIL, two faults, both exited tools/compile-check.sh 0:
//!   L3. IGNORE THE EARMARK - replace MayEvaluatorSpend()'s body with `return pool >= cost;`, which
//!       is exactly the code that shipped before D18 and exactly the defect. Compiled clean (exit 0).
//!       Every unreserved row still passes; the case fails on
//!       "a purchase the pool could cover must still be refused when it would eat the earmark: pool
//!       170, reserve 120, cost 51 was allowed".
//!   L4. TIGHTEN THE BOUNDARY - change the `>=` to `>`. Compiled clean (exit 0). The case fails on
//!       "spending the budget down to exactly the earmark must be allowed - the floor is a floor,
//!       not a cushion: pool 170, reserve 120, cost 50 was refused".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveReserveFloor_SpendIsRefusedWhenItWouldEatTheReserve : SCR_AutotestCaseBase
{
	//! A pool that has just been credited, and an earmark that predates the credit. The numbers are
	//! the play-tested shape of the defect: a 120-cost strategic operation against routine work that
	//! would otherwise take the lot.
	protected const int POOL = 170;
	protected const int RESERVE = 120;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- With nothing earmarked the gate is the pool, exactly as it was before this existed.
		if (!ExpectAllowed(POOL, OVT_DeploymentSelection.NO_RESERVE, POOL, "a faction nobody has earmarked anything for must be able to spend down to nothing"))
			return true;

		if (!ExpectRefused(POOL, OVT_DeploymentSelection.NO_RESERVE, POOL + 1, "no earmark does not mean no limit - the pool is still a limit"))
			return true;

		// --- THE ROW THE WHOLE FEATURE IS. Affordable out of the pool, refused out of the budget.
		if (!ExpectRefused(POOL, RESERVE, 51, "a purchase the pool could cover must still be refused when it would eat the earmark"))
			return true;

		// --- The boundary, both ways.
		if (!ExpectAllowed(POOL, RESERVE, 50, "spending the budget down to exactly the earmark must be allowed - the floor is a floor, not a cushion"))
			return true;

		if (!ExpectAllowed(POOL, RESERVE, 1, "an earmark must not stop ordinary spending out of the remainder"))
			return true;

		// --- A faction saving up buys nothing at all, including nothing free-of-charge-shaped.
		if (!ExpectRefused(20, RESERVE, 1, "a faction whose earmark exceeds its pool must buy nothing routine at all"))
			return true;

		if (!ExpectAllowed(20, RESERVE, 0, "a purchase that costs nothing is not a spend and must never be refused by a floor"))
			return true;

		Print("Objective reserve floor: the routine evaluator may spend the remainder down to exactly the earmark and not one resource further, and an earmark bigger than the pool refuses everything that costs anything");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that one purchase is allowed.
	//! \param[in] pool What the faction holds.
	//! \param[in] reserve What is earmarked out of it.
	//! \param[in] cost What the evaluator wants to buy.
	//! \param[in] claim What this row is claiming, for the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectAllowed(int pool, int reserve, int cost, string claim)
	{
		if (OVT_DeploymentSelection.MayEvaluatorSpend(pool, reserve, cost))
			return true;

		SetFailure(string.Format("%1: pool %2, reserve %3, cost %4 was refused",
			claim, pool.ToString(), reserve.ToString(), cost.ToString()));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that one purchase is refused.
	//! \param[in] pool What the faction holds.
	//! \param[in] reserve What is earmarked out of it.
	//! \param[in] cost What the evaluator wants to buy.
	//! \param[in] claim What this row is claiming, for the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectRefused(int pool, int reserve, int cost, string claim)
	{
		if (!OVT_DeploymentSelection.MayEvaluatorSpend(pool, reserve, cost))
			return true;

		SetFailure(string.Format("%1: pool %2, reserve %3, cost %4 was allowed",
			claim, pool.ToString(), reserve.ToString(), cost.ToString()));

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Two earmarks are the same entry - and therefore the second one is silent - only when the
//! operation AND the price both match.
//!
//! WHY A LOG KEY IS WORTH A CASE. A reserve floor is invisible from the outside: nothing errors, no
//! deployment fails, the faction simply buys less than it could. The one line that explains it is
//! latched, because the owner of the earmark re-asserts it every ten real seconds for as long as it
//! is short - and a latch keyed too coarsely does not quieten a log, it HIDES THE SECOND FACT BEHIND
//! THE FIRST. That is not a hypothetical: the sibling refusal ledger in the objective machine was
//! keyed on the objective, and a play-test on 2026-08-19 spent five real minutes with a forward-base
//! phase visibly stuck and no line to explain it, because the same objective had said it was short of
//! money twelve in-game hours earlier about a completely different operation.
//!
//! ⚠ THE PRICE IS HALF THE KEY, NOT DECORATION. The same operation can be re-priced between one
//! campaign and the next, and a ramp's ladder rung changes what is being saved up for while the
//! config NAME stays put for the whole objective. An earmark that went from 100 to 300 is news.
//!
//! CAN-FAIL, one fault, exited tools/compile-check.sh 0:
//!   L5. DROP THE PRICE COMPARISON - replace IsSameReserve()'s body with `return operationA ==
//!       operationB;`. Compiled clean (exit 0). The case fails on
//!       "the same operation at a different price is news and must not be silenced by the first".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveReserveFloor_TheLatchKeyIsTheOperationAndThePrice : SCR_AutotestCaseBase
{
	protected const string FORWARD_BASE = "Objective Forward Base";
	protected const string SABOTAGE = "Objective Sabotage";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- An identical repeat is the same entry. This is the branch that runs six times a real
		//     minute for as long as a faction is saving up, and it must stay silent.
		if (!OVT_DeploymentSelection.IsSameReserve(FORWARD_BASE, 120, FORWARD_BASE, 120))
		{
			SetFailure("an identical repeat must be the same entry, or the log gets one line every ten real seconds for as long as the faction is saving up");
			return true;
		}

		// --- A different operation is a different entry, whatever it costs.
		if (OVT_DeploymentSelection.IsSameReserve(FORWARD_BASE, 120, SABOTAGE, 120))
		{
			SetFailure("two different operations at the same price must be different entries, or the second one is hidden behind the first");
			return true;
		}

		// --- ...and so is the same operation at a different price.
		if (OVT_DeploymentSelection.IsSameReserve(FORWARD_BASE, 120, FORWARD_BASE, 300))
		{
			SetFailure("the same operation at a different price is news and must not be silenced by the first");
			return true;
		}

		// --- An unnamed earmark is a legitimate value and must not collapse onto a named one.
		if (OVT_DeploymentSelection.IsSameReserve("", 120, FORWARD_BASE, 120))
		{
			SetFailure("an unnamed earmark and a named one at the same price must be different entries");
			return true;
		}

		if (!OVT_DeploymentSelection.IsSameReserve("", 120, "", 120))
		{
			SetFailure("two unnamed earmarks at the same price must be the same entry - the key is the pair, not the presence of a name");
			return true;
		}

		Print("Objective reserve floor: the announcement latch is keyed on the operation AND its price, so an identical repeat is silent and a re-priced or different earmark is heard");

		return true;
	}
}
