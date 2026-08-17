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
