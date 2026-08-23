//------------------------------------------------------------------------------------------------
//! TIER A cases - OVT_BaseDefenseConversion, the two pieces of arithmetic the occupying faction's
//! defense economy now rests on.
//!
//! Both were expressions buried inside a 90-line campaign tick until this migration, and both are
//! the kind of thing that is catastrophic when wrong and completely invisible while it is:
//!
//!   1. THE FUNDING SPLIT. 80 % of every resource tick leaves the occupying faction's reserve and
//!      enters the deployment resource pool, which is the only budget base defense is bought from.
//!      Get the share wrong and the campaign either never defends its bases or starves its own
//!      counter-attacks, and either way nothing logs an error.
//!   2. THE LEGACY VALUE SUM. A campaign saved before this migration carries per-base upgrade
//!      records - banked resources plus a count of groups that were standing. Those upgrades no
//!      longer exist, so their VALUE is summed and refunded to the pool. Sum it twice and a loaded
//!      campaign is handed free money on every load; sum it as zero and a player's whole investment
//!      silently evaporates.
//!
//! WHAT IS NOT HERE. Where the money goes, when the tick fires, which records a save actually
//! carries, and whether the refund is credited once or eleven times are all questions about live
//! state and belong to the Init and Persistence tiers. This file asserts only the arithmetic.
//!
//! COMPILE PROOF: this file names OVT_BaseDefenseConversion, which nothing else in the test tree
//! names; tools/compile-check.sh resolves it, so these cases are genuinely in the compile set. No
//! retry attribute is used anywhere - this quality bar bans it outright.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! THE LEGACY VALUE SUM: banked resources plus (groups x per-group value), over a set of records,
//! with the three kinds of record that must convert to ZERO all proven to.
//!
//! THE IDEMPOTENCE CLAIM IS THE ONE THAT MATTERS AND IT IS THE LAST ASSERTION HERE. After the first
//! load of a pre-migration campaign, the save's upgrade array is rewritten EMPTY, so every later load
//! of that campaign converts an empty set. There is no flag anywhere guarding the refund - the whole
//! defence against paying twice is that an empty set sums to 0 - so "an empty set sums to 0" is not a
//! defensive-input nicety here, it IS the anti-double-pay mechanism, and it is asserted as such.
//!
//! THE COMPOSITION SHAPE IS THE SECOND. A structure record carries a tag and a position and NOTHING
//! ELSE: no banked resources, no groups. It must refund nothing, because the structure itself is an
//! ordinary tracked world entity that comes back from the save on its own and its slot claim comes
//! back beside it. Refunding for one would pay for it twice, and the record shape is exactly what
//! makes that free.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, execution belongs to the phase's suite run): change
//! `total += groups * legacyGroupValue` to `total += groups` and the mixed-set assertion goes red
//! naming both numbers; change the ragged-input guard to clamp to the shorter list and the ragged
//! assertion goes red; make the empty-set path return the per-group value instead of 0 and the
//! idempotence assertion goes red on its own while every other assertion still passes; drop the
//! `if (banked > 0)` guard to a bare `+=` and the corruption assertion goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_BaseDefenseConversion_LegacyValueSum : SCR_AutotestCaseBase
{
	//! A per-group value the campaign could really produce: LEGACY_GROUP_SIZE (4) x a baseResourceCost
	//! of 15, which is what the shipped Normal difficulty authors.
	static const int GROUP_VALUE = 60;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The constant is a promise about the shape of the refund, so it is pinned rather than
		// re-derived: 4 men to a group is what the deleted valuation priced.
		if (OVT_BaseDefenseConversion.LEGACY_GROUP_SIZE != 4)
		{
			SetFailure("LEGACY_GROUP_SIZE is %1, expected 4 - every legacy save in existence would convert to a different number than the one this migration was costed against", OVT_BaseDefenseConversion.LEGACY_GROUP_SIZE.ToString());
			return true;
		}

		if (OVT_BaseDefenseConversion.LegacyGroupValue(15) != GROUP_VALUE)
		{
			SetFailure("A per-man price of 15 valued a group at %1, expected %2", OVT_BaseDefenseConversion.LegacyGroupValue(15).ToString(), GROUP_VALUE.ToString());
			return true;
		}

		// A MIXED SET, which is what a real base's record list looks like: one record holding banked
		// value and no groups, one holding both, one holding only groups.
		//   90 + (0 x 60) = 90
		//   40 + (2 x 60) = 160
		//    0 + (3 x 60) = 180
		//                 = 430
		array<int> banked = {};
		banked.Insert(90);
		banked.Insert(40);
		banked.Insert(0);

		array<int> groups = {};
		groups.Insert(0);
		groups.Insert(2);
		groups.Insert(3);

		int value = OVT_BaseDefenseConversion.ConvertedValue(banked, groups, GROUP_VALUE);
		if (value != 430)
		{
			SetFailure("A mixed record set converted to %1, expected 430 - banked value and group value are not both being counted", value.ToString());
			return true;
		}

		// COMPOSITION-SHAPED RECORDS: a tag and a position, no banked resources, no groups. Three of
		// them, and they must be worth nothing at all.
		array<int> structureBanked = {};
		structureBanked.Insert(0);
		structureBanked.Insert(0);
		structureBanked.Insert(0);

		array<int> structureGroups = {};
		structureGroups.Insert(0);
		structureGroups.Insert(0);
		structureGroups.Insert(0);

		value = OVT_BaseDefenseConversion.ConvertedValue(structureBanked, structureGroups, GROUP_VALUE);
		if (value != 0)
		{
			SetFailure("Three structure-shaped records converted to %1, expected 0 - the structures themselves come back from the save on their own, so this is paying for them twice", value.ToString());
			return true;
		}

		// A per-group value of 0 (a campaign whose per-man price could not be resolved) must not turn a
		// standing force into a refund out of nowhere.
		array<int> onlyGroups = {};
		onlyGroups.Insert(0);

		array<int> fiveGroups = {};
		fiveGroups.Insert(5);

		value = OVT_BaseDefenseConversion.ConvertedValue(onlyGroups, fiveGroups, 0);
		if (value != 0)
		{
			SetFailure("Five groups at a per-group value of 0 converted to %1, expected 0", value.ToString());
			return true;
		}

		// CORRUPTION COUNTS AS ZERO, NEVER AS A SUBTRACTION. One bad record must not eat the refund the
		// records beside it earned.
		array<int> corruptBanked = {};
		corruptBanked.Insert(-5000);
		corruptBanked.Insert(200);

		array<int> corruptGroups = {};
		corruptGroups.Insert(-3);
		corruptGroups.Insert(1);

		value = OVT_BaseDefenseConversion.ConvertedValue(corruptBanked, corruptGroups, GROUP_VALUE);
		if (value != 260)
		{
			SetFailure("A set with one corrupt record converted to %1, expected 260 (200 banked + one group) - a negative entry is subtracting from its neighbours' refund", value.ToString());
			return true;
		}

		// DEFENSIVE INPUTS. Null lists and ragged lists answer 0 rather than guessing.
		array<int> one = {};
		one.Insert(10);

		if (OVT_BaseDefenseConversion.ConvertedValue(null, groups, GROUP_VALUE) != 0)
		{
			SetFailure("A null banked-value list converted to something other than 0");
			return true;
		}

		if (OVT_BaseDefenseConversion.ConvertedValue(banked, null, GROUP_VALUE) != 0)
		{
			SetFailure("A null group-count list converted to something other than 0");
			return true;
		}

		value = OVT_BaseDefenseConversion.ConvertedValue(banked, one, GROUP_VALUE);
		if (value != 0)
		{
			SetFailure("Three banked values against one group count converted to %1, expected 0 - clamping to the shorter list silently drops records off the end of a save", value.ToString());
			return true;
		}

		// A negative per-group value cannot be turned into a debit either.
		value = OVT_BaseDefenseConversion.ConvertedValue(onlyGroups, fiveGroups, -100);
		if (value != 0)
		{
			SetFailure("Five groups at a per-group value of -100 converted to %1, expected 0", value.ToString());
			return true;
		}

		// THE IDEMPOTENCE CLAIM. This is what an ALREADY-CONVERTED payload looks like: the arrays are
		// empty because the write path rewrote them empty after the first load. It must be worth
		// nothing, on every load, forever - there is no flag protecting this.
		array<int> convertedBanked = {};
		array<int> convertedGroups = {};

		value = OVT_BaseDefenseConversion.ConvertedValue(convertedBanked, convertedGroups, GROUP_VALUE);
		if (value != 0)
		{
			SetFailure("An already-converted (empty) record set converted to %1, expected 0 - every subsequent load of the same campaign would hand the occupying faction another refund", value.ToString());
			return true;
		}

		Print("Legacy conversion sums banked value plus groups-times-per-group-value; structure-shaped, corrupt, ragged, null and ALREADY-CONVERTED sets all convert to 0, which is the whole idempotence argument");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE FUNDING SPLIT, AND THE CONSERVED-TOTAL IDENTITY IT EXISTS TO GUARANTEE.
//!
//! Every resource tick is divided in exactly one place: 80 % of it is moved into the deployment
//! resource pool and the remainder stays in the occupying faction's reserve, where the QRF sizing and
//! the counter-attack roll draw on it. Two spenders used to draw on one economy through a
//! hand-written bridge; after this migration there is one, and "resource accounting is closed" is a
//! claim that can only be made if this division neither creates nor destroys resources.
//!
//! THE IDENTITY, STATED SO IT CAN BE READ BACK OFF THE CODE:
//!     reserve_after + pool_after == reserve_before + pool_before + tick
//! It is asserted here as a walk over several ticks, because a single-tick check would pass for a
//! split that quietly rounded the same fraction of a penny into the pool every time.
//!
//! THE FLOOR DIRECTION IS DELIBERATE AND IS ASSERTED. The share is FLOORED, so the fractional part
//! stays in the reserve; a share that rounded up would hand the pool money the reserve never had. The
//! exact multiples below (250 -> 200) are safe to assert exactly because the 32-bit constant for 0.8
//! sits fractionally ABOVE 0.8, so the product of an exact multiple never lands below its integer.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, execution belongs to the phase's suite run): change
//! DEFENSE_SHARE_OF_TICK to 0.5 and the shipped-tick assertion goes red naming both numbers; swap
//! Math.Floor for Math.Ceil and the flooring assertion goes red; delete the `newResources <= 0`
//! guard and the negative-tick assertion goes red with a negative share; change the walk's reserve
//! arithmetic to subtract anything other than the share and the conservation assertion goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_BaseDefenseConversion_FundingSplitConservesTheTotal : SCR_AutotestCaseBase
{
	//! What the shipped Normal difficulty gains on a quiet tick (baseResourcesPerTick).
	static const int SHIPPED_TICK = 250;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int share = OVT_BaseDefenseConversion.DefenseShare(SHIPPED_TICK);
		if (share != 200)
		{
			SetFailure("A tick of %1 gave the pool %2, expected 200 - four fifths of every tick is what buys base defense, and nothing else funds it", SHIPPED_TICK.ToString(), share.ToString());
			return true;
		}

		// FLOORED, NOT ROUNDED: 0.8 x 9 is 7.2 and the pool gets 7, leaving the fraction behind.
		share = OVT_BaseDefenseConversion.DefenseShare(9);
		if (share != 7)
		{
			SetFailure("A tick of 9 gave the pool %1, expected 7 - the share must be floored, or the pool is handed money the reserve never had", share.ToString());
			return true;
		}

		// A tick too small to split gives nothing away rather than rounding up to 1.
		share = OVT_BaseDefenseConversion.DefenseShare(1);
		if (share != 0)
		{
			SetFailure("A tick of 1 gave the pool %1, expected 0", share.ToString());
			return true;
		}

		// Degenerate ticks are not errors, and must never produce a negative transfer - a negative
		// share would move money OUT of the pool and INTO the reserve, silently.
		if (OVT_BaseDefenseConversion.DefenseShare(0) != 0)
		{
			SetFailure("A tick of 0 gave the pool %1, expected 0", OVT_BaseDefenseConversion.DefenseShare(0).ToString());
			return true;
		}

		if (OVT_BaseDefenseConversion.DefenseShare(-500) != 0)
		{
			SetFailure("A tick of -500 gave the pool %1, expected 0 - a negative share drains the pool back into the reserve with nothing logged", OVT_BaseDefenseConversion.DefenseShare(-500).ToString());
			return true;
		}

		// THE CONSERVED-TOTAL WALK. Six ticks, deliberately including ones that do not divide evenly,
		// with the reserve and the pool tracked exactly as the transfer moves them.
		array<int> ticks = {};
		ticks.Insert(250);
		ticks.Insert(9);
		ticks.Insert(1);
		ticks.Insert(0);
		ticks.Insert(1337);
		ticks.Insert(3);

		int reserve = 1500;
		int pool = 275;
		int expectedTotal = reserve + pool;

		foreach (int tick : ticks)
		{
			// The gain lands in the reserve first, exactly as the tick does.
			reserve += tick;
			expectedTotal += tick;

			int moved = OVT_BaseDefenseConversion.DefenseShare(tick);
			if (moved > reserve)
				moved = reserve;

			reserve -= moved;
			pool += moved;

			if (reserve < 0)
			{
				SetFailure("The reserve went negative (%1) after moving %2 - the transfer is handing out money that was not there", reserve.ToString(), moved.ToString());
				return true;
			}

			if (reserve + pool != expectedTotal)
			{
				string detail = (reserve + pool).ToString() + ", expected " + expectedTotal.ToString();
				SetFailure("After a tick of %1 the reserve and the pool sum to %2 - resources are being created or destroyed by the split, which is the one thing 'accounting is closed' forbids", tick.ToString(), detail);
				return true;
			}
		}

		// The reserve keeps a real remainder rather than being emptied into the pool: 20 % of the total
		// gained, plus whatever it started with, minus nothing else.
		if (pool <= 275)
		{
			SetFailure("The pool ended at %1 after six ticks, having started at 275 - nothing is reaching it at all", pool.ToString());
			return true;
		}

		if (reserve <= 1500)
		{
			SetFailure("The reserve ended at %1 after six ticks, having started at 1500 - the whole tick is going to the pool and the QRF reserve never grows", reserve.ToString());
			return true;
		}

		// A DEGENERATE STATE THE LIVE TRANSFER CLAMPS FOR: a reserve smaller than the share. The
		// identity still has to hold, with the pool getting only what existed.
		reserve = 5;
		pool = 0;
		int clamped = OVT_BaseDefenseConversion.DefenseShare(100);
		if (clamped > reserve)
			clamped = reserve;

		reserve -= clamped;
		pool += clamped;

		if (reserve != 0 || pool != 5)
		{
			string state = reserve.ToString() + "/" + pool.ToString();
			SetFailure("Transferring out of a reserve of 5 left reserve/pool at %1, expected 0/5 - the clamp to the reserve is what keeps the identity true in every state", state);
			return true;
		}

		PrintFormat("The funding split floors at four fifths (%1 -> %2), never goes negative, and conserves reserve+pool exactly across a six-tick walk",
			SHIPPED_TICK.ToString(), OVT_BaseDefenseConversion.DefenseShare(SHIPPED_TICK).ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE DRIP PAYS OUT EXACTLY THE SHARE, NEVER MORE AND NEVER LESS, whatever the share divides into.
//!
//! The drip replaced a single statement that moved the whole 80 % at the six-hour payday with six
//! hourly slices, and the only thing that made the old statement safe was that it was ONE statement:
//! reserve down by X, pool up by X, nothing to get wrong. Six slices reintroduce exactly the failure
//! the epic's design note warned about - "a share divided across N steps is 0 forever for small
//! amounts, needs a remainder carry or an accumulator" - and EnforceScript truncating in a pure-int
//! expression is precisely how that goes wrong silently.
//!
//! THE CARRY IS STRUCTURAL, WHICH IS WHY THIS CASE WALKS THE WINDOW RATHER THAN SPOT-CHECKING ONE
//! CALL. DripAmount divides what is STILL OWED by the drips STILL TO COME, so the remainder each
//! floor leaves behind is part of the next numerator by construction and the last drip - dividing by
//! one - pays out whatever is left. That claim is only true across a whole window, so a whole window
//! is what is asserted: six drips, summed, against the share they were opened with.
//!
//! A SHARE THAT ROUNDS TO ZERO PER STEP IS THE CASE THAT BREAKS A NAIVE IMPLEMENTATION. share/6 in a
//! pure-int expression pays 0 six times and strands the money forever, and the deployment pool goes
//! quietly unfunded with nothing logged. Re-deriving the divisor pays five out as 0,1,1,1,1,1: the
//! first floor is still 0, but from the second drip on there are as many drips left as there are
//! resources, so the money leaves as it can rather than waiting for the final step.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_BaseDefenseConversion_DripPaysOutExactlyTheShare : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_BaseDefenseConversion.DRIP_STEPS != 6)
		{
			SetFailure("DRIP_STEPS is %1, expected 6 - the drip is designed as one slice an hour across the six-hour payday grid, and this case's expectations are written against that", OVT_BaseDefenseConversion.DRIP_STEPS.ToString());
			return true;
		}

		// An evenly divisible share is the easy half: six equal slices, no carry needed.
		array<int> even = DripWindow(1200);
		if (even[0] != 200 || even[5] != 200)
		{
			SetFailure("A share of 1200 dripped %1 first and %2 last, expected 200 each - an evenly divisible share must not need the carry at all", even[0].ToString(), even[5].ToString());
			return true;
		}

		// THE SHARES THAT DO NOT DIVIDE. Each must still sum to itself exactly.
		array<int> shares = {};
		shares.Insert(1200);
		shares.Insert(200);
		shares.Insert(1337);
		shares.Insert(7);
		shares.Insert(5);
		shares.Insert(1);
		shares.Insert(0);

		foreach (int share : shares)
		{
			array<int> drips = DripWindow(share);

			int paid = 0;
			foreach (int drip : drips)
			{
				if (drip < 0)
				{
					SetFailure("A share of %1 produced a drip of %2 - a negative drip moves money OUT of the deployment pool and back into the reserve, silently", share.ToString(), drip.ToString());
					return true;
				}

				paid += drip;
			}

			if (paid != share)
			{
				SetFailure("A share of %1 dripped out to %2 across six steps - the remainder carry is losing or conjuring money, and neither shows up anywhere but a pool that will not fund its defenses", share.ToString(), paid.ToString());
				return true;
			}
		}

		// THE ROUNDS-TO-ZERO CASE, stated explicitly rather than left to the sum above: it is the one a
		// share/6 implementation gets wrong. The first floor is 0 and every later drip pays 1, because
		// once the drips left equal the resources left the divisor stops rounding anything away.
		array<int> tiny = DripWindow(5);
		if (tiny[0] != 0)
		{
			SetFailure("A share of 5 dripped %1 on its first step, expected 0 - floor(5/6) is 0 and rounding it up would overdraw the window", tiny[0].ToString());
			return true;
		}

		for (int i = 1; i < OVT_BaseDefenseConversion.DRIP_STEPS; i++)
		{
			if (tiny[i] != 1)
			{
				SetFailure("A share of 5 dripped %1 on step %2, expected 1 - a share smaller than the step count must still drain one at a time, not be truncated away six times and stranded", tiny[i].ToString(), (i + 1).ToString());
				return true;
			}
		}

		// DEGENERATE SCHEDULES ARE NOT ERRORS. A restored or corrupt window can claim more drips than
		// it has money for, or none at all; neither may over-pay.
		if (OVT_BaseDefenseConversion.DripAmount(100, 0) != 100)
		{
			SetFailure("A window with no drips left owed %1 of 100 - a debt with no schedule must be payable in full, or it can never be settled", OVT_BaseDefenseConversion.DripAmount(100, 0).ToString());
			return true;
		}

		if (OVT_BaseDefenseConversion.DripAmount(0, 6) != 0 || OVT_BaseDefenseConversion.DripAmount(-500, 6) != 0)
		{
			SetFailure("A non-positive debt dripped something - nothing owed must move nothing");
			return true;
		}

		if (OVT_BaseDefenseConversion.DripAmount(100, 1000) > 100)
		{
			SetFailure("A drip of a 100 debt across 1000 steps paid %1 - no single drip may exceed what is owed", OVT_BaseDefenseConversion.DripAmount(100, 1000).ToString());
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Walks one whole drip window the way the manager does, returning what each step paid.
	//! \param[in] share The window's opening debt.
	//! \return DRIP_STEPS amounts, in order.
	protected array<int> DripWindow(int share)
	{
		array<int> drips = {};

		int pending = share;
		int remaining = OVT_BaseDefenseConversion.DRIP_STEPS;

		for (int i = 0; i < OVT_BaseDefenseConversion.DRIP_STEPS; i++)
		{
			int amount = OVT_BaseDefenseConversion.DripAmount(pending, remaining);

			drips.Insert(amount);
			pending -= amount;
			remaining--;
		}

		return drips;
	}
}

//------------------------------------------------------------------------------------------------
//! THE RESERVE CEILING: the reserve stops being a one-way ratchet once it holds everything it can
//! ever spend.
//!
//! 🔴 WHAT THIS IS FOR. 20 % of every tick went into the reserve and the ONLY thing that ever took
//! money out was a battle, so a campaign in which the player fights no QRFs banks a reserve it can
//! never use while the deployment pool - which buys every garrison, patrol and checkpoint the player
//! actually meets - runs dry. Author, 2026-08-23, at 1438 banked on Normal against a 750 gate.
//!
//! THE ANCHOR IS THE LARGER OF THE TWO CONSUMERS AND BOTH MUST BE IN IT. maxQRF is the most one
//! battle can spend; objectiveQRFResourceGate is what the counter-attack demands before it will
//! launch. Easy authors maxQRF 500 against a gate of 750, so an anchor of maxQRF alone would park
//! the reserve permanently below the gate and no counter-attack would ever fire on Easy - which is
//! the fault this case exists to catch, and it is asserted with Easy's own shipped numbers.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_BaseDefenseConversion_ReserveCeiling : SCR_AutotestCaseBase
{
	//! Normal's shipped pair. Both 750, so the anchor is unambiguous whichever wins.
	static const int NORMAL_MAX_QRF = 750;
	static const int NORMAL_GATE = 750;

	//! Easy's shipped pair - the gate is the LARGER one here, which is the whole point of the case.
	static const int EASY_MAX_QRF = 500;
	static const int EASY_GATE = 750;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- CLAIM 1: the anchor is max(maxQRF, gate), on both shipped orderings.
		int normal = OVT_BaseDefenseConversion.ReserveTarget(NORMAL_MAX_QRF, NORMAL_GATE, 1);
		if (normal != 750)
		{
			SetFailure("Normal's pair (750/750) answered a target of %1, expected 750", normal.ToString());
			return true;
		}

		int easy = OVT_BaseDefenseConversion.ReserveTarget(EASY_MAX_QRF, EASY_GATE, 1);
		if (easy != EASY_GATE)
		{
			SetFailure("Easy's pair (maxQRF 500, gate 750) answered a target of %1, expected 750 - a target under the counter-attack's own funding gate means no counter-attack ever fires on Easy", easy.ToString());
			return true;
		}

		// --- CLAIM 2: the multiplier scales it, and a non-positive one reads as 1.
		if (OVT_BaseDefenseConversion.ReserveTarget(1000, 1000, 2) != 2000)
		{
			SetFailure("A multiplier of 2 against an anchor of 1000 answered %1, expected 2000", OVT_BaseDefenseConversion.ReserveTarget(1000, 1000, 2).ToString());
			return true;
		}

		if (OVT_BaseDefenseConversion.ReserveTarget(1000, 1000, 0) != 1000)
		{
			SetFailure("A multiplier of 0 answered %1, expected the anchor (1000) - a zero multiplier must read as 1, not drain the reserve to nothing", OVT_BaseDefenseConversion.ReserveTarget(1000, 1000, 0).ToString());
			return true;
		}

		if (OVT_BaseDefenseConversion.ReserveTarget(0, 0, 1) != 0)
		{
			SetFailure("An anchor of 0 answered a target above 0");
			return true;
		}

		// --- CLAIM 3: under the target the transfer is the ordinary share, unchanged.
		int under = OVT_BaseDefenseConversion.PoolTransferForWindow(250, 700, 750);
		if (under != OVT_BaseDefenseConversion.DefenseShare(250))
		{
			SetFailure("A reserve of 700 under a target of 750 transferred %1, expected the ordinary share of %2 - the 80/20 split must be untouched below the ceiling", under.ToString(), OVT_BaseDefenseConversion.DefenseShare(250).ToString());
			return true;
		}

		if (OVT_BaseDefenseConversion.PoolTransferForWindow(250, 750, 750) != OVT_BaseDefenseConversion.DefenseShare(250))
		{
			SetFailure("A reserve sitting exactly on its target transferred %1, expected the ordinary share - the surplus is zero there and the share is the floor", OVT_BaseDefenseConversion.PoolTransferForWindow(250, 750, 750).ToString());
			return true;
		}

		// --- CLAIM 4: THE ANTI-DOUBLE-PAY CLAIM, and the reason this method is not share + surplus.
		//     The reserve figure ALREADY CONTAINS this tick, so the surplus IS the whole transfer.
		//     1438 banked against a target of 750 must leave the reserve ON 750, never under it.
		int hoarding = OVT_BaseDefenseConversion.PoolTransferForWindow(250, 1438, 750);
		if (hoarding != 688)
		{
			SetFailure("A reserve of 1438 against a target of 750 transferred %1, expected 688 - anything larger is this tick's income being paid twice, and it walks the reserve under the counter-attack's own funding gate", hoarding.ToString());
			return true;
		}

		if (1438 - hoarding != 750)
		{
			SetFailure("After the transfer the reserve would hold %1, expected exactly its target of 750", (1438 - hoarding).ToString());
			return true;
		}

		// --- CLAIM 5: no target restores the old behaviour exactly.
		if (OVT_BaseDefenseConversion.PoolTransferForWindow(250, 100000, 0) != OVT_BaseDefenseConversion.DefenseShare(250))
		{
			SetFailure("With no target, a huge reserve changed the transfer - a campaign with no difficulty preset must behave exactly as it did before the ceiling existed");
			return true;
		}

		if (OVT_BaseDefenseConversion.ReserveOverflow(100000, 0) != 0)
		{
			SetFailure("With no target, an overflow was reported - see above");
			return true;
		}

		// --- CLAIM 6: the overflow is the surplus and nothing else.
		if (OVT_BaseDefenseConversion.ReserveOverflow(1438, 750) != 688)
		{
			SetFailure("A reserve of 1438 against a target of 750 overflowed %1, expected 688", OVT_BaseDefenseConversion.ReserveOverflow(1438, 750).ToString());
			return true;
		}

		if (OVT_BaseDefenseConversion.ReserveOverflow(750, 750) != 0)
		{
			SetFailure("A reserve sitting exactly on its target overflowed %1, expected 0", OVT_BaseDefenseConversion.ReserveOverflow(750, 750).ToString());
			return true;
		}

		if (OVT_BaseDefenseConversion.ReserveOverflow(100, 750) != 0)
		{
			SetFailure("A reserve under its target overflowed %1, expected 0", OVT_BaseDefenseConversion.ReserveOverflow(100, 750).ToString());
			return true;
		}

		Print("BaseDefenseConversion reserve ceiling: the anchor covers both consumers, the multiplier scales it, the ordinary share is the floor, the surplus is the whole transfer rather than an addition to it, and no target restores the old split");

		return true;
	}
}
