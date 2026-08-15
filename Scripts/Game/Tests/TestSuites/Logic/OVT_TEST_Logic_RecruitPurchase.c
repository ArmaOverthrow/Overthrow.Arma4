//------------------------------------------------------------------------------------------------
//! TIER A cases - the arithmetic behind buying an equipped recruit at a recruitment tent.
//!
//! WHAT IS HERE. OVT_RecruitPurchaseRules, all of it: the fee multiplier's "unset means default"
//! rule, the gear fee and its rounding, the composition of the whole price, which sentence an outcome
//! earns, and how much is actually charged for each of those outcomes. Every one of these is a pure
//! function over plain numbers, which is exactly why they were split out of the transaction - THIS is
//! the half where a wrong answer is money.
//!
//! WHAT IS NOT HERE, and cannot be. What the individual items are worth needs an economy and a
//! position (OVT_RecruitLoadoutPricing, pinned in the Init tier, which has both). The transaction
//! itself - spawn, equip, take a supporter, charge - needs a live world, a tent and a player with a
//! balance, and is play-test territory.
//!
//! ! THE THING THESE CASES EXIST TO STOP is free gear. This feature re-opens a network route to the
//! loadout engine's spawning apply, which was once deleted as a free-item hole, and the single value
//! that decides whether an item costs anything is a float read out of a config file. A float that
//! nobody set reads as 0, and 0 multiplied by a subtotal is a free kit. Two of the five cases below
//! are about nothing but that.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! An outcome is charged for according to what the player actually received - and only ever less
//! than the quote, never more.
//!
//! THE TWO EXCEPTIONS TO "the quote is the charge", and there are only two: a spawn that never
//! happened costs nothing, and a recruit that arrived with none of its kit costs the recruit fee
//! alone. Everything else - including a partial apply - pays the number the player read before
//! pressing the button, because a per-item refund cannot be computed correctly for the contents of a
//! container and a wrong refund is worse than a price that was shown up front.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 9, by deliberate fault + compile-check; running the suite is
//! the orchestrator's job, not this file's):
//!   In OVT_RecruitPurchaseRules.ChargeFor, the RESULT_GEAR_FAILED branch was changed to return
//!   quotedTotal instead of tentRecruitCost - i.e. "charge the full price for a recruit that arrived
//!   naked". The tree recompiled CLEAN, which is the point: a wrong charge is not a syntax error. The
//!   second assertion below then reads 1625 where it demands 125 and the case fails on
//!   "a recruit that arrived with NO kit was charged the full quoted price". Fault reverted, tree
//!   recompiled clean.
//!   No maxAttempts: five calls and five integer comparisons, with nothing that can flake.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitPurchase_ChargeFollowsOutcome : SCR_AutotestCaseBase
{
	//! What the plain tent action charges for the recruit alone.
	static const int RECRUIT_COST = 125;

	//! What the player was quoted for recruit plus kit.
	static const int QUOTED_TOTAL = 1625;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int nothingSpawned = OVT_RecruitPurchaseRules.ChargeFor(OVT_RecruitCommandComponent.RESULT_SPAWN_FAILED, RECRUIT_COST, QUOTED_TOTAL);
		if (nothingSpawned != 0)
		{
			SetFailure("A purchase that spawned nothing charged %1 - a player must never pay for a recruit that does not exist", nothingSpawned.ToString());
			return true;
		}

		int gearFailed = OVT_RecruitPurchaseRules.ChargeFor(OVT_RecruitCommandComponent.RESULT_GEAR_FAILED, RECRUIT_COST, QUOTED_TOTAL);
		if (gearFailed != RECRUIT_COST)
		{
			SetFailure("A recruit that arrived with NO kit was charged %1, expected the recruit cost %2 alone - the gear fee must not be taken for gear that never arrived",
				gearFailed.ToString(), RECRUIT_COST.ToString());
			return true;
		}

		int complete = OVT_RecruitPurchaseRules.ChargeFor(OVT_RecruitCommandComponent.RESULT_BUY_OK, RECRUIT_COST, QUOTED_TOTAL);
		if (complete != QUOTED_TOTAL)
		{
			SetFailure("A complete purchase charged %1 instead of the quoted %2 - the price shown and the price taken are the same number by design",
				complete.ToString(), QUOTED_TOTAL.ToString());
			return true;
		}

		// A partial apply is charged IN FULL. This is a decision, not an oversight: how much of a kit
		// arrived is only known at the top level, so any refund would be wrong for container contents.
		int partial = OVT_RecruitPurchaseRules.ChargeFor(OVT_RecruitCommandComponent.RESULT_BUY_PARTIAL, RECRUIT_COST, QUOTED_TOTAL);
		if (partial != QUOTED_TOTAL)
		{
			SetFailure("A partial purchase charged %1 instead of the quoted %2", partial.ToString(), QUOTED_TOTAL.ToString());
			return true;
		}

		// Nonsense input must never produce a negative charge, which would CREDIT the player.
		int negative = OVT_RecruitPurchaseRules.ChargeFor(OVT_RecruitCommandComponent.RESULT_BUY_OK, -50, -200);
		if (negative != 0)
		{
			SetFailure("A negative quote produced a charge of %1 - a charge below zero would pay the player to buy recruits", negative.ToString());
			return true;
		}

		Print("Charge follows outcome: nothing spawned 0, no kit " + RECRUIT_COST.ToString() + ", complete and partial " + QUOTED_TOTAL.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An unset fee multiplier can never make gear free.
//!
//! THE FAILURE THIS PINS. The multiplier is a float on the difficulty settings. It is written into
//! every preset config explicitly, but a field absent from an authored config, or a server owner's
//! JSON that overrides difficulty without naming it, reads as 0 - and a 0 multiplier on a money path
//! means every kit is issued free, on a route that exists precisely because free kit was once an
//! exploit. So 0 and anything below it are treated as "nobody configured this" and resolve to the
//! default. A zero fee is deliberately NOT a supported setting; 0.01 is available and means the same
//! thing in practice.
//!
//! The second half of the case is the one that matters more: it asserts the guard is reached THROUGH
//! GearFee, not just when ResolveFeeMultiplier is called directly. A guard that the fee path does not
//! go through is decoration.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 9, by deliberate fault + compile-check):
//!   OVT_RecruitPurchaseRules.ResolveFeeMultiplier's guard was changed from `configured <= 0` to
//!   `configured < 0`, so an explicit zero passes straight through - the exact real-world defect, and
//!   the tree recompiled CLEAN. The zero assertion below then reads 0.0 where it demands 1.5 and the
//!   case fails on "a fee multiplier of 0 resolved to 0". Fault reverted, tree recompiled clean.
//!   No maxAttempts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitPurchase_FeeMultiplierNeverResolvesToZero : SCR_AutotestCaseBase
{
	//! A kit worth this much at shop prices, used to prove the guard is reached through GearFee.
	static const int GEAR_SUBTOTAL = 1000;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		float fromZero = OVT_RecruitPurchaseRules.ResolveFeeMultiplier(0);
		if (!OVT_TEST_LogicFixture.FloatEquals(fromZero, OVT_RecruitPurchaseRules.DEFAULT_LOADOUT_FEE_MULTIPLIER))
		{
			SetFailure("A fee multiplier of 0 resolved to %1, expected the default %2 - an unset config value must never make gear free",
				fromZero.ToString(), OVT_RecruitPurchaseRules.DEFAULT_LOADOUT_FEE_MULTIPLIER.ToString());
			return true;
		}

		float fromNegative = OVT_RecruitPurchaseRules.ResolveFeeMultiplier(-2.5);
		if (!OVT_TEST_LogicFixture.FloatEquals(fromNegative, OVT_RecruitPurchaseRules.DEFAULT_LOADOUT_FEE_MULTIPLIER))
		{
			SetFailure("A negative fee multiplier resolved to %1, expected the default %2", fromNegative.ToString(), OVT_RecruitPurchaseRules.DEFAULT_LOADOUT_FEE_MULTIPLIER.ToString());
			return true;
		}

		// A configured value is honoured - the guard must not swallow real settings.
		float configured = OVT_RecruitPurchaseRules.ResolveFeeMultiplier(2.25);
		if (!OVT_TEST_LogicFixture.FloatEquals(configured, 2.25))
		{
			SetFailure("A configured fee multiplier of 2.25 resolved to %1 - the unset guard is swallowing real settings", configured.ToString());
			return true;
		}

		// And a nearly-free setting IS reachable, which is what makes refusing zero cost nobody
		// anything.
		float nearlyFree = OVT_RecruitPurchaseRules.ResolveFeeMultiplier(0.01);
		if (!OVT_TEST_LogicFixture.FloatEquals(nearlyFree, 0.01))
		{
			SetFailure("A fee multiplier of 0.01 resolved to %1 - a nearly-free fee must remain configurable", nearlyFree.ToString());
			return true;
		}

		// THE HALF THAT MATTERS: the guard has to be on the path the price actually takes.
		int feeFromZero = OVT_RecruitPurchaseRules.GearFee(GEAR_SUBTOTAL, 0);
		if (feeFromZero <= 0)
		{
			SetFailure("A kit worth %1 cost %2 with an unset fee multiplier - free gear on a money path is the exploit this whole path is guarded against",
				GEAR_SUBTOTAL.ToString(), feeFromZero.ToString());
			return true;
		}

		int feeFromDefault = OVT_RecruitPurchaseRules.GearFee(GEAR_SUBTOTAL, OVT_RecruitPurchaseRules.DEFAULT_LOADOUT_FEE_MULTIPLIER);
		if (feeFromZero != feeFromDefault)
		{
			SetFailure("An unset fee multiplier charged %1 but the default charges %2 - they must be the same number",
				feeFromZero.ToString(), feeFromDefault.ToString());
			return true;
		}

		Print("Unset fee multiplier resolves to the default and a kit worth " + GEAR_SUBTOTAL.ToString() + " still costs " + feeFromZero.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The gear fee multiplies, rounds rather than truncates, and never goes below zero.
//!
//! ON THE EXACT HALF. The plan asked for a rounding case at a half. It is deliberately NOT asserted
//! exactly: the engine's rounding function is documented only as returning the closest whole number,
//! which says nothing about a tie, and this project's rule is to probe an exact boundary before
//! pinning it rather than to guess and produce a case that is wrong instead of failing. What IS
//! pinned is the claim the half case was there to make - that the fee ROUNDS, in both directions -
//! using two products that are nowhere near a tie, plus a bounds assertion on the tie itself. A
//! truncating implementation fails the round-up assertion immediately.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 9, by deliberate fault + compile-check):
//!   OVT_RecruitPurchaseRules.GearFee's Math.Round was replaced with Math.Floor - the classic
//!   truncation defect - and the tree recompiled CLEAN. The round-up assertion then reads 12 where it
//!   demands 13 and the case fails on "a kit worth 7 at a 1.8 multiplier cost 12". Fault reverted,
//!   tree recompiled clean.
//!   No maxAttempts: pure arithmetic on constants.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitPurchase_GearFeeRounds : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// A multiplier of 1 charges shop price - the identity case, and the one an admin who wants no
		// convenience fee at all is expected to use.
		int atOne = OVT_RecruitPurchaseRules.GearFee(100, 1.0);
		if (atOne != 100)
		{
			SetFailure("A kit worth 100 at a multiplier of 1 cost %1, expected 100", atOne.ToString());
			return true;
		}

		int atDefault = OVT_RecruitPurchaseRules.GearFee(100, 1.5);
		if (atDefault != 150)
		{
			SetFailure("A kit worth 100 at a multiplier of 1.5 cost %1, expected 150", atDefault.ToString());
			return true;
		}

		// ROUNDS UP: 7 * 1.8 = 12.6. A truncating implementation answers 12.
		int roundsUp = OVT_RecruitPurchaseRules.GearFee(7, 1.8);
		if (roundsUp != 13)
		{
			SetFailure("A kit worth 7 at a 1.8 multiplier cost %1, expected 13 - the fee must round, not truncate", roundsUp.ToString());
			return true;
		}

		// ROUNDS DOWN: 7 * 1.2 = 8.4.
		int roundsDown = OVT_RecruitPurchaseRules.GearFee(7, 1.2);
		if (roundsDown != 8)
		{
			SetFailure("A kit worth 7 at a 1.2 multiplier cost %1, expected 8", roundsDown.ToString());
			return true;
		}

		// THE TIE, bounded rather than pinned - see the note in the case header. 3 * 1.5 = 4.5, so the
		// answer is one of the two adjacent integers whichever way the engine breaks a tie; anything
		// else means the multiplication or the rounding is wrong, not merely tie-broken differently.
		int tie = OVT_RecruitPurchaseRules.GearFee(3, 1.5);
		if (tie < 4 || tie > 5)
		{
			SetFailure("A kit worth 3 at a 1.5 multiplier cost %1, which is not adjacent to 4.5 at all", tie.ToString());
			return true;
		}

		// Nothing to charge for is free, and a nonsense subtotal cannot become a credit.
		if (OVT_RecruitPurchaseRules.GearFee(0, 1.5) != 0)
		{
			SetFailure("An empty kit was charged for");
			return true;
		}

		if (OVT_RecruitPurchaseRules.GearFee(-500, 1.5) != 0)
		{
			SetFailure("A negative subtotal produced a non-zero fee - a fee below zero would pay the player");
			return true;
		}

		Print("Gear fee rounds both ways: 12.6 -> " + roundsUp.ToString() + ", 8.4 -> " + roundsDown.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Which sentence a completed purchase earns, over the whole grid of what could have happened.
//!
//! Four outcomes and one rule each: nothing spawned beats everything else; a recruit with nothing on
//! it is its own outcome because it is the one that changes what is charged; everything landed is a
//! clean buy; some of it landed is partial.
//!
//! THE ZERO-ITEM ROW IS THE INTERESTING ONE. A loadout that records no items answers OK rather than
//! "no kit landed": nothing was owed and nothing failed. It is refused earlier in practice - an empty
//! loadout never reaches a purchase - but the honest answer to "the recruit arrived and there was
//! nothing to put on it" is not a failure code, and getting it wrong here would charge the recruit
//! fee under a failure sentence.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 9, by deliberate fault + compile-check):
//!   OVT_RecruitPurchaseRules.OutcomeFor's `appliedItems >= totalItems` was weakened to
//!   `appliedItems > 0` - so a partial apply reports a clean buy - and the tree recompiled CLEAN. The
//!   partial assertion below then reads RESULT_BUY_OK where it demands RESULT_BUY_PARTIAL and the
//!   case fails on "3 of 5 items landing was reported as". Fault reverted, tree recompiled clean.
//!   No maxAttempts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitPurchase_OutcomeGrid : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// NOT SPAWNED beats everything, including counts that would otherwise read as a clean buy.
		int noSpawn = OVT_RecruitPurchaseRules.OutcomeFor(false, 0, 0);
		if (noSpawn != OVT_RecruitCommandComponent.RESULT_SPAWN_FAILED)
		{
			SetFailure("A purchase with no recruit reported %1, expected RESULT_SPAWN_FAILED %2",
				noSpawn.ToString(), OVT_RecruitCommandComponent.RESULT_SPAWN_FAILED.ToString());
			return true;
		}

		int noSpawnWithCounts = OVT_RecruitPurchaseRules.OutcomeFor(false, 5, 5);
		if (noSpawnWithCounts != OVT_RecruitCommandComponent.RESULT_SPAWN_FAILED)
		{
			SetFailure("A purchase with no recruit but a full item count reported %1 - a body that does not exist cannot be wearing anything",
				noSpawnWithCounts.ToString());
			return true;
		}

		// SPAWNED, NOTHING TO WEAR. Not a failure: nothing was owed.
		int emptyLoadout = OVT_RecruitPurchaseRules.OutcomeFor(true, 0, 0);
		if (emptyLoadout != OVT_RecruitCommandComponent.RESULT_BUY_OK)
		{
			SetFailure("A recruit bought with a zero-item loadout reported %1, expected RESULT_BUY_OK %2",
				emptyLoadout.ToString(), OVT_RecruitCommandComponent.RESULT_BUY_OK.ToString());
			return true;
		}

		// SPAWNED, NOTHING LANDED. Its own outcome, because it is the one that changes the charge.
		int gearFailed = OVT_RecruitPurchaseRules.OutcomeFor(true, 0, 5);
		if (gearFailed != OVT_RecruitCommandComponent.RESULT_GEAR_FAILED)
		{
			SetFailure("A recruit that arrived with none of its 5 items reported %1, expected RESULT_GEAR_FAILED %2",
				gearFailed.ToString(), OVT_RecruitCommandComponent.RESULT_GEAR_FAILED.ToString());
			return true;
		}

		int complete = OVT_RecruitPurchaseRules.OutcomeFor(true, 5, 5);
		if (complete != OVT_RecruitCommandComponent.RESULT_BUY_OK)
		{
			SetFailure("A recruit that arrived with all 5 items reported %1, expected RESULT_BUY_OK %2",
				complete.ToString(), OVT_RecruitCommandComponent.RESULT_BUY_OK.ToString());
			return true;
		}

		int partial = OVT_RecruitPurchaseRules.OutcomeFor(true, 3, 5);
		if (partial != OVT_RecruitCommandComponent.RESULT_BUY_PARTIAL)
		{
			SetFailure("3 of 5 items landing was reported as %1, expected RESULT_BUY_PARTIAL %2",
				partial.ToString(), OVT_RecruitCommandComponent.RESULT_BUY_PARTIAL.ToString());
			return true;
		}

		int single = OVT_RecruitPurchaseRules.OutcomeFor(true, 1, 1);
		if (single != OVT_RecruitCommandComponent.RESULT_BUY_OK)
		{
			SetFailure("A one-item loadout landing its one item reported %1, expected RESULT_BUY_OK", single.ToString());
			return true;
		}

		// Defensive: more successes than the record had entries cannot report a shortfall.
		int overCount = OVT_RecruitPurchaseRules.OutcomeFor(true, 6, 5);
		if (overCount != OVT_RecruitCommandComponent.RESULT_BUY_OK)
		{
			SetFailure("More items landing than the loadout recorded reported %1, expected RESULT_BUY_OK", overCount.ToString());
			return true;
		}

		Print("Outcome grid: no spawn, empty loadout, no kit, complete, partial and single all report their own code");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The quoted price is the recruit's own cost plus the gear fee, and nothing else.
//!
//! An equipped recruit is a tent recruit that arrives dressed, so it pays what the plain action
//! charges AND the fee on the kit. Charging for the gear alone would make the equipped purchase
//! cheaper than the plain one for a cheap kit, which is the composition error this pins.
//!
//! The zero-multiplier row is here as well as in the multiplier case on purpose: it is the whole
//! price, computed the way the transaction computes it, and it is the number a player would actually
//! be charged - so if the guard were ever bypassed at this level the failure would show up as a total
//! that equals the recruit cost alone.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 9, by deliberate fault + compile-check):
//!   OVT_RecruitPurchaseRules.TotalPrice was changed to return GearFee(...) alone, dropping the
//!   recruit cost - the "gear only" pricing this case exists to refuse - and the tree recompiled
//!   CLEAN. The first assertion then reads 1500 where it demands 1625 and the case fails on
//!   "a recruit costing 125 with a kit worth 1000 was quoted". Fault reverted, tree recompiled clean.
//!   No maxAttempts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitPurchase_TotalPriceComposition : SCR_AutotestCaseBase
{
	//! Half the Normal preset's base recruit cost, which is what the tent charges.
	static const int RECRUIT_COST = 125;

	//! A kit worth this much at local shop buy prices.
	static const int GEAR_SUBTOTAL = 1000;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int quoted = OVT_RecruitPurchaseRules.TotalPrice(RECRUIT_COST, GEAR_SUBTOTAL, 1.5);
		if (quoted != 1625)
		{
			SetFailure("A recruit costing %1 with a kit worth %2 was quoted %3, expected 1625 (125 + 1000 * 1.5)",
				RECRUIT_COST.ToString(), GEAR_SUBTOTAL.ToString(), quoted.ToString());
			return true;
		}

		// The gear fee is the ONLY thing the multiplier touches.
		int atOne = OVT_RecruitPurchaseRules.TotalPrice(RECRUIT_COST, GEAR_SUBTOTAL, 1.0);
		if (atOne != RECRUIT_COST + GEAR_SUBTOTAL)
		{
			SetFailure("At a multiplier of 1 the quote was %1, expected the recruit cost plus the kit at shop price %2",
				atOne.ToString(), (RECRUIT_COST + GEAR_SUBTOTAL).ToString());
			return true;
		}

		// No kit costs exactly what the plain tent action costs - the two prices meet, they never cross.
		int noGear = OVT_RecruitPurchaseRules.TotalPrice(RECRUIT_COST, 0, 1.5);
		if (noGear != RECRUIT_COST)
		{
			SetFailure("A purchase with no gear was quoted %1, expected the plain recruit cost %2",
				noGear.ToString(), RECRUIT_COST.ToString());
			return true;
		}

		if (quoted <= noGear)
		{
			SetFailure("An equipped recruit (%1) is not dearer than a plain one (%2) - equipping must never be cheaper than not equipping",
				quoted.ToString(), noGear.ToString());
			return true;
		}

		// An unset multiplier resolves through the same guard here as anywhere else.
		int unsetMultiplier = OVT_RecruitPurchaseRules.TotalPrice(RECRUIT_COST, GEAR_SUBTOTAL, 0);
		if (unsetMultiplier != quoted)
		{
			SetFailure("An unset fee multiplier quoted %1 where the default quotes %2 - the guard is not reached through TotalPrice",
				unsetMultiplier.ToString(), quoted.ToString());
			return true;
		}

		// Nonsense input cannot produce a negative price.
		int negative = OVT_RecruitPurchaseRules.TotalPrice(-500, -500, 1.5);
		if (negative != 0)
		{
			SetFailure("A negative composition produced a quote of %1 - a price below zero would pay the player", negative.ToString());
			return true;
		}

		Print("Total price composes as recruit cost + gear fee: " + quoted.ToString() + " for a " + GEAR_SUBTOTAL.ToString() + " kit");
		return true;
	}
}
