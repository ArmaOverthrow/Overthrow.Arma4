//------------------------------------------------------------------------------------------------
//! Base-defense FUNDING and legacy-save CONVERSION arithmetic - PURE, SYSTEM-FREE.
//!
//! ===========================================================================================
//! HARD RULE: NOTHING IN THIS FILE MAY TOUCH A SYSTEM, THE GAME MODE OR ANY LIVE STATE.
//! ===========================================================================================
//!
//! Two numbers decide whether the occupying faction can defend itself, and both used to be
//! expressions buried inside a 90-line tick:
//!
//!   1. HOW MUCH OF A RESOURCE TICK BUYS DEFENSE. 80 % of every tick leaves the occupying
//!      faction's reserve and enters the deployment pool, which is the ONLY thing base defense,
//!      town patrols, tower garrisons and vehicle patrols are bought out of. The other 20 % stays
//!      behind as the QRF and counter-attack reserve.
//!   2. WHAT A PRE-MIGRATION SAVE'S BASE UPGRADES ARE WORTH. A legacy save carries, per base, a
//!      list of upgrade records holding banked resources and a count of groups that were standing.
//!      Those upgrades no longer exist, so their VALUE is summed and refunded to the pool and the
//!      deployment evaluator re-establishes defense from it.
//!
//! They live here, as pure functions of their arguments, so the cheapest test tier can assert them
//! without a world, a manager, a campaign or a save file - the same shape and for the same reason as
//! OVT_DeploymentSelection.
//!
//! WHY THE RECORDS ARE PASSED AS PARALLEL PLAIN ARRAYS rather than as save records: reading a save
//! record is exactly the kind of live state this file may not see, and a persisted payload class
//! named in here would drag the persistence layer into the pure tier. The caller unpacks.
//------------------------------------------------------------------------------------------------
class OVT_BaseDefenseConversion
{
	//! The share of every resource tick that leaves the reserve to buy defense. The remaining 20 % is
	//! what the QRF sizing (OVT_QRFControllerComponent) and the counter-attack roll draw on, and both
	//! are deliberately excluded from this migration.
	static const float DEFENSE_SHARE_OF_TICK = 0.8;

	//! How many slices one six-hour defense share is paid to the deployment pool in. Six slices on a
	//! six-hour payday grid is one an hour, which is the whole point: the pool is the CONTENDED
	//! resource, so smoothing its arrival is what removes the faction's burst-spending rhythm without
	//! touching income, its hour latches, or the sleep replay's granularity.
	static const int DRIP_STEPS = 6;

	//! Boundary spacing of the drip, for OVT_SleepSchedule.IsIntervalBoundary().
	static const int DRIP_INTERVAL_HOURS = 1;

	//! Exclusive upper bound of the minute-of-hour a drip is rolled onto, so a payday is never
	//! followed by six transfers on the exact hour.
	static const int DRIP_MINUTE_SPREAD = 60;

	//! The average number of men a legacy base-upgrade group was worth, used to value the groups a
	//! pre-migration save recorded as standing.
	//!
	//! APPROXIMATION BY DESIGN - VALUE-PARITY, NOT ENTITY-IDENTITY. The legacy valuation priced a live
	//! group at (agents * baseResourceCost) and the shipped group compositions run 2-6 men, so 4 is
	//! their middle. The requirement being satisfied is that a loaded campaign gets back the VALUE it
	//! had invested, not that the same men come back; nothing anywhere reconstructs a legacy group.
	static const int LEGACY_GROUP_SIZE = 4;

	//------------------------------------------------------------------------------------------------
	//! How much of one resource tick goes to the deployment pool.
	//!
	//! FLOORED, and floored the same way the per-base split it replaced was: the fractional part stays
	//! in the reserve rather than being conjured into the pool, so the two halves always sum to no
	//! more than the tick.
	//! \param[in] newResources The resources gained this tick. A negative or zero tick buys nothing.
	//! \return The pool's share, never negative.
	static int DefenseShare(int newResources)
	{
		if (newResources <= 0)
			return 0;

		int share = Math.Floor((float)newResources * DEFENSE_SHARE_OF_TICK);
		if (share < 0)
			return 0;

		return share;
	}

	//------------------------------------------------------------------------------------------------
	//! HOW MUCH RESERVE IS WORTH KEEPING - everything above this is dead money.
	//!
	//! The reserve has exactly two consumers and both are already difficulty-scaled, so this invents no
	//! new number: `maxQRF` is the most one battle can ever spend, and `objectiveQRFResourceGate` is what
	//! the objective director demands before it will launch the counter-attack. Holding the LARGER of the
	//! two funds both; holding more than that funds nothing at all.
	//!
	//! ⚠ THE GATE MUST BE PART OF THIS. Draining to `maxQRF` alone would, on Easy (maxQRF 500,
	//! gate 750), park the reserve permanently below the counter-attack's own funding gate and the
	//! campaign would never see one.
	//! \param[in] maxQRF The difficulty's maxQRF.
	//! \param[in] counterAttackGate The difficulty's objectiveQRFResourceGate.
	//! \param[in] multiplier The difficulty's reserveTargetMultiplier. Non-positive reads as 1, matching
	//!            how OVT_VehicleLadderRules treats its own scale.
	//! \return The reserve to hold, never negative.
	static int ReserveTarget(int maxQRF, int counterAttackGate, float multiplier)
	{
		int anchor = maxQRF;
		if (counterAttackGate > anchor)
			anchor = counterAttackGate;

		if (anchor <= 0)
			return 0;

		if (multiplier <= 0)
			multiplier = 1;

		int target = Math.Round((float)anchor * multiplier);
		if (target < 0)
			return 0;

		return target;
	}

	//------------------------------------------------------------------------------------------------
	//! WHAT THE POOL IS OWED FOR ONE WINDOW - the ordinary share, or the surplus, whichever is larger.
	//!
	//! Two rules, and the MAX of them is what makes both hold at once:
	//!   1. THE POOL NEVER GETS LESS THAN IT USED TO. DefenseShare() is the floor, so a reserve sitting
	//!      on or under its target behaves exactly as the campaign always has.
	//!   2. THE RESERVE IS NOT A ONE-WAY RATCHET. When it holds more than it can ever spend, the
	//!      SURPLUS is what crosses - which lands the reserve exactly on its target rather than
	//!      somewhere below it.
	//!
	//! ⚠ THE SURPLUS IS THE WHOLE TRANSFER, NOT AN ADDITION TO THE SHARE. The reserve figure passed in
	//! ALREADY CONTAINS this tick's income, so `share + surplus` would hand the pool this tick's money
	//! twice and walk the reserve under its own target - and under the counter-attack's funding gate
	//! with it. This is the fault this method was rewritten to remove; do not re-add the two.
	//!
	//! THE STEADY STATE IS THE RESERVE PINNED EXACTLY ON ITS TARGET, and it is worth stating because it
	//! is not obvious from the max(): once the reserve is on the target, the next tick's income makes
	//! the surplus equal to the WHOLE tick, which always beats four fifths of it - so rule 2 wins every
	//! window from then on and hands the pool the entire tick, leaving the reserve back on the target.
	//! Below the target rule 1 wins, the reserve keeps its usual fifth, and it climbs until it arrives.
	//! There is no oscillation in either direction.
	//! \param[in] newResources The resources gained this tick.
	//! \param[in] reserve The reserve AFTER this tick's income and after the previous window was flushed.
	//! \param[in] reserveTarget What ReserveTarget() answered. Zero or below keeps the ordinary split.
	//! \return What to move to the pool this window, never negative.
	static int PoolTransferForWindow(int newResources, int reserve, int reserveTarget)
	{
		int share = DefenseShare(newResources);

		int surplus = ReserveOverflow(reserve, reserveTarget);
		if (surplus > share)
			return surplus;

		return share;
	}

	//------------------------------------------------------------------------------------------------
	//! What the reserve is holding that it can never spend.
	//! \param[in] reserve The reserve after this tick's income.
	//! \param[in] reserveTarget What ReserveTarget() answered. Zero or below overflows nothing.
	//! \return The surplus, never negative.
	static int ReserveOverflow(int reserve, int reserveTarget)
	{
		if (reserveTarget <= 0)
			return 0;

		if (reserve <= reserveTarget)
			return 0;

		return reserve - reserveTarget;
	}

	//------------------------------------------------------------------------------------------------
	//! What one group recorded in a legacy base-upgrade payload is worth in deployment resources.
	//! \param[in] baseResourceCost The campaign difficulty's baseResourceCost - the per-man price the
	//!            legacy valuation used.
	//! \return The per-group refund value, never negative.
	static int LegacyGroupValue(int baseResourceCost)
	{
		if (baseResourceCost <= 0)
			return 0;

		return LEGACY_GROUP_SIZE * baseResourceCost;
	}

	//------------------------------------------------------------------------------------------------
	//! The total value of a set of legacy base-upgrade records.
	//!
	//! sum(banked resources) + sum(group counts) * legacyGroupValue.
	//!
	//! THREE KINDS OF RECORD CONVERT TO ZERO, AND EACH ON PURPOSE:
	//!   - a composition or checkpoint record (a tag and a position, no banked resources, no groups):
	//!     the STRUCTURE is an ordinary tracked world entity that comes back from the save on its own
	//!     and its slot claim comes back beside it, so refunding for it would pay for it twice;
	//!   - an upgrade class that never wrote a record at all (parked vehicles, specops, town patrol
	//!     all serialised to null), which simply is not in the list;
	//!   - AN ALREADY-CONVERTED SET. After the first load the write path stores an EMPTY upgrade
	//!     array, so every later pass sums zero. That is the whole idempotence argument - it is
	//!     STRUCTURAL, there is no flag anywhere, and it is why this function must answer 0 for an
	//!     empty input rather than treating it as an error.
	//!
	//! RAGGED INPUT IS REFUSED OUTRIGHT rather than clamped to the shorter list, matching
	//! OVT_DeploymentSelection: the two lists are built side by side by one caller and a mismatch is a
	//! programming error, not a save that needs rescuing.
	//!
	//! A NEGATIVE ENTRY COUNTS AS ZERO rather than subtracting. Corruption in one record must not eat
	//! another base's refund.
	//! \param[in] bankedResources Each record's banked resource value, in record order. May be null.
	//! \param[in] groupCounts How many groups each of those records held, same order, same length.
	//! \param[in] legacyGroupValue What one group is worth - see LegacyGroupValue().
	//! \return The value to credit, never negative.
	static int ConvertedValue(array<int> bankedResources, array<int> groupCounts, int legacyGroupValue)
	{
		if (!bankedResources || !groupCounts)
			return 0;

		if (bankedResources.Count() != groupCounts.Count())
			return 0;

		if (legacyGroupValue < 0)
			legacyGroupValue = 0;

		int total = 0;
		int count = bankedResources.Count();

		for (int i = 0; i < count; i++)
		{
			int banked = bankedResources[i];
			if (banked > 0)
				total += banked;

			int groups = groupCounts[i];
			if (groups > 0)
				total += groups * legacyGroupValue;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! How much of a still-owed defense share the NEXT drip pays.
	//!
	//! REMAINDER-CARRYING BY CONSTRUCTION, which is why it divides the amount STILL OWED by the drips
	//! STILL TO COME rather than the original share by DRIP_STEPS. A fixed share/6 truncates six times
	//! and strands the remainder; this floor re-derives itself every step, so whatever the last floor
	//! left behind is simply part of the next numerator, and the final drip - dividing by 1 - pays out
	//! everything that is left. Nothing needs an accumulator and nothing can be stranded.
	//!
	//! A SHARE SMALLER THAN THE STEP COUNT PAYS ZERO ONLY UNTIL THE DIVISOR CATCHES UP, which is a
	//! consequence of re-deriving it rather than a rule of its own: five owed over six drips is
	//! 0,1,1,1,1,1 - the first floor is 0, and from the second drip on there are as many drips left as
	//! there are resources, so it pays out one at a time instead of being held to the end.
	//! \param[in] pendingTotal What the pool is still owed from this window. Non-positive pays nothing.
	//! \param[in] dripsRemaining How many drips are left including this one. One means "pay it all".
	//! \return The amount to move now, never negative and never more than pendingTotal.
	static int DripAmount(int pendingTotal, int dripsRemaining)
	{
		if (pendingTotal <= 0)
			return 0;

		if (dripsRemaining <= 1)
			return pendingTotal;

		int amount = Math.Floor((float)pendingTotal / dripsRemaining);
		if (amount < 0)
			return 0;

		if (amount > pendingTotal)
			return pendingTotal;

		return amount;
	}
}
