//------------------------------------------------------------------------------------------------
//! What an equipped tent recruit COSTS, and what its purchase EARNED - as pure arithmetic over
//! plain values.
//!
//! PURE. Nothing here looks up a manager, reads the world, or dereferences an entity, and nothing
//! ever may: this is the half of the tent purchase the Logic tier can pin, and it is the half where
//! a wrong number is money. The world-bound half - what the individual items are worth at the shop
//! nearest the tent - lives in OVT_RecruitLoadoutPricing, which needs an economy manager and a
//! position and therefore cannot be tested world-free. The split is the same one this feature
//! already draws for clustering (OVT_RecruitInactiveGrouping) and status (OVT_RecruitStatus).
//!
//! THE ONE NUMBER RULE. The quote the player is shown, the funds check that decides whether the
//! purchase is allowed, and the amount actually taken are all produced by TotalPrice() on the SERVER.
//! There is no second implementation to drift from - a client cannot even attempt one, because
//! loadout JIP carries metadata only and every client's copy of a loadout has an EMPTY item list
//! (OVT_LoadoutManagerComponent.c:40, RplSave :2153-2180). See decisions D18/D19/Q12.
//!
//! WHY THIS FILE KNOWS THE RESULT_ CODES. OutcomeFor() returns one of OVT_RecruitCommandComponent's
//! RESULT_ constants rather than inventing a private outcome vocabulary that would then need
//! translating. Reading a `static const int` off a class is a compile-time constant - it constructs
//! nothing, touches no component instance and reaches no manager - so it costs this class none of its
//! purity, and it buys the Logic tier the ability to pin the ACTUAL code that crosses the wire
//! instead of a stand-in for it.
//------------------------------------------------------------------------------------------------
class OVT_RecruitPurchaseRules
{
	//! The convenience fee charged on top of what the kit would cost item-by-item at a shop, used
	//! whenever the difficulty config does not name one.
	//!
	//! 1.5 means "half again as much as buying it yourself" - the recruit arrives dressed, at the
	//! tent, with no shopping trip.
	static const float DEFAULT_LOADOUT_FEE_MULTIPLIER = 1.5;

	//------------------------------------------------------------------------------------------------
	//! The fee multiplier actually in force, given whatever the config holds.
	//!
	//! ! ZERO AND NEGATIVE ARE TREATED AS "UNSET" AND RESOLVE TO THE DEFAULT. A zero multiplier means
	//! FREE GEAR on a money path, which is the exact failure this whole phase exists to prevent
	//! (BUG-043 was a free-item endpoint), and an unset float field reads as 0 - so "nobody
	//! configured this" and "an admin wants gear to be free" would be the same value. Making zero
	//! unreachable costs an admin nothing (0.01 is available and means the same thing in practice) and
	//! removes the possibility entirely. This is a BACKSTOP, not the primary defence: the value is
	//! also written explicitly into every difficulty preset .conf, so neither has to be trusted alone.
	//! \param[in] configured The multiplier read from the difficulty settings.
	//! \return The multiplier to use.
	static float ResolveFeeMultiplier(float configured)
	{
		if (configured <= 0)
			return DEFAULT_LOADOUT_FEE_MULTIPLIER;

		return configured;
	}

	//------------------------------------------------------------------------------------------------
	//! What the kit costs the buyer, given what it is worth at the shop.
	//!
	//! The multiplication happens in floats and is rounded exactly once, here - callers add the
	//! (integer) recruit cost to the rounded result rather than rounding a mixed sum, so the gear
	//! half of the price is the same number whether it is quoted alone or as part of a total.
	//! \param[in] gearSubtotal Sum of the loadout's items at local shop buy price.
	//! \param[in] multiplier The configured fee multiplier (passed through ResolveFeeMultiplier).
	//! \return The amount charged for the gear, never negative.
	static int GearFee(int gearSubtotal, float multiplier)
	{
		if (gearSubtotal <= 0)
			return 0;

		return Math.Round(gearSubtotal * ResolveFeeMultiplier(multiplier));
	}

	//------------------------------------------------------------------------------------------------
	//! The whole price of an equipped tent recruit.
	//!
	//! An equipped purchase is A TENT RECRUIT THAT HAPPENS TO ARRIVE DRESSED (decision D23): it pays
	//! the ordinary tent recruit cost AND consumes a town supporter, exactly like the plain action,
	//! with the gear fee on top. Charging for the gear alone would make the equipped purchase cheaper
	//! than the plain one for a cheap kit, and skipping the supporter would make it a way around the
	//! town-support economy.
	//! \param[in] tentRecruitCost What the plain Recruit Civilian action charges.
	//! \param[in] gearSubtotal Sum of the loadout's items at local shop buy price.
	//! \param[in] multiplier The configured fee multiplier.
	//! \return The quoted total, never negative.
	static int TotalPrice(int tentRecruitCost, int gearSubtotal, float multiplier)
	{
		int total = tentRecruitCost + GearFee(gearSubtotal, multiplier);

		if (total < 0)
			return 0;

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Which sentence the purchase earned, from what actually landed on the body.
	//!
	//! "How much of the kit arrived" is a TOP-LEVEL number and cannot be anything else: the spawning
	//! apply counts successes per top-level item (OVT_LoadoutManagerComponent.c:457-471) while the
	//! nested-container walk returns void (:1603-1621). A rucksack that arrives with half its contents
	//! therefore counts as one success, which is why this grades an outcome rather than computing a
	//! refund - a per-item refund would be wrong for container contents, and a wrong refund is worse
	//! than a price the player was shown before pressing the button (decision D19).
	//!
	//! A zero-item loadout answers OK rather than "no gear landed": nothing was owed and nothing
	//! failed. In practice it never reaches here - a loadout that records no items is refused at quote
	//! time with RESULT_LOADOUT_EMPTY - but the honest answer for "the recruit arrived and there was
	//! nothing to put on it" is not a failure code.
	//! \param[in] spawned Whether a recruit body was created and owned at all.
	//! \param[in] appliedItems Top-level items the apply reported as landed.
	//! \param[in] totalItems Top-level items the loadout recorded.
	//! \return One of OVT_RecruitCommandComponent's RESULT_ codes.
	static int OutcomeFor(bool spawned, int appliedItems, int totalItems)
	{
		if (!spawned)
			return OVT_RecruitCommandComponent.RESULT_SPAWN_FAILED;

		if (totalItems <= 0)
			return OVT_RecruitCommandComponent.RESULT_BUY_OK;

		if (appliedItems <= 0)
			return OVT_RecruitCommandComponent.RESULT_GEAR_FAILED;

		if (appliedItems >= totalItems)
			return OVT_RecruitCommandComponent.RESULT_BUY_OK;

		return OVT_RecruitCommandComponent.RESULT_BUY_PARTIAL;
	}

	//------------------------------------------------------------------------------------------------
	//! What the player is actually charged, given the outcome.
	//!
	//! THE TWO EXCEPTIONS TO "QUOTE == CHARGE", AND THERE ARE ONLY TWO (decision D19):
	//!   - nothing spawned  -> nothing is charged. Paying for a recruit that does not exist is the one
	//!     outcome no amount of explanation makes acceptable.
	//!   - the recruit arrived but NOT ONE PIECE of the kit did -> only the ordinary recruit cost is
	//!     charged. The player got a recruit, so the recruit is paid for; they did not get the gear,
	//!     so the gear fee is not.
	//! Everything else - including a partial apply - pays the quoted total, which is the number the
	//! player read before committing.
	//! \param[in] outcomeCode The code OutcomeFor() returned.
	//! \param[in] tentRecruitCost What the plain Recruit Civilian action charges.
	//! \param[in] quotedTotal The price the player was shown.
	//! \return The amount to take, never negative.
	static int ChargeFor(int outcomeCode, int tentRecruitCost, int quotedTotal)
	{
		if (outcomeCode == OVT_RecruitCommandComponent.RESULT_SPAWN_FAILED)
			return 0;

		if (outcomeCode == OVT_RecruitCommandComponent.RESULT_GEAR_FAILED)
		{
			if (tentRecruitCost < 0)
				return 0;

			return tentRecruitCost;
		}

		if (quotedTotal < 0)
			return 0;

		return quotedTotal;
	}
}
