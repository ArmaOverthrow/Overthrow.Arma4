//------------------------------------------------------------------------------------------------
//! What a litre of fuel costs, what a delivery costs, and what the next refuel tick will cost.
//!
//! Pure arithmetic, held apart from the modded vanilla fuel station on purpose: the SAME numbers
//! decide the price suffix drawn on the client's Refuel action, whether the client greys that action
//! out because it cannot afford the next tick, and how many dollars the server takes after fuel has
//! actually moved. Three callers on two machines, one rule set - which is the only way the client's
//! offer and the server's authority cannot drift apart. It is also the only reason any of the fuel
//! fee is testable at all; the engine fuel stack is not.
//!
//! THE KILL SWITCH LIVES HERE. A fuelPricePerLitre of 0 (or anything negative, from a mis-authored
//! preset or a client that failed to read the config stream) resolves to 0 everywhere, which makes
//! every cost 0 and every tick estimate 0 - i.e. exactly vanilla behaviour, the safest possible
//! failure mode (implementation.md D9).
//------------------------------------------------------------------------------------------------
class OVT_FuelPricing
{
	//! Minimum charge for a tick that delivers fuel at a non-zero price. A 0.5 s tick at a low price
	//! can round to nothing, and a free-in-practice pump is worse than a cheap one - so any tick that
	//! is charged at all is charged at least a dollar. Never applied at price 0; see EstimateTickCost.
	static const int MIN_TICK_COST = 1;

	//------------------------------------------------------------------------------------------------
	//! Normalises a configured price into one that can be used for money.
	//!
	//! Negative prices are clamped to 0 rather than refunding fuel. Everything else in this class
	//! goes through here, so no caller has to remember the clamp.
	//! \param[in] raw The price per litre as configured or replicated.
	//! \return The price to charge with; never negative.
	static float ResolvePrice(float raw)
	{
		if (raw < 0)
			return 0;

		return raw;
	}

	//------------------------------------------------------------------------------------------------
	//! The exact (fractional) cost of a quantity of fuel.
	//!
	//! Fractional by design: a 0.5 s tick delivers a couple of litres and the remainder is carried by
	//! OVT_FuelChargeLedger rather than being rounded away on every tick.
	//! \param[in] litres Litres actually delivered.
	//! \param[in] pricePerLitre The configured price per litre.
	//! \return Dollars owed, as a float. 0 when the price resolves to 0 or nothing was delivered.
	static float ComputeCost(float litres, float pricePerLitre)
	{
		float price = ResolvePrice(pricePerLitre);
		if (price == 0)
			return 0;

		if (litres <= 0)
			return 0;

		return litres * price;
	}

	//------------------------------------------------------------------------------------------------
	//! An upper bound on what the NEXT refuel tick will cost, for the affordability gate.
	//!
	//! Rounded UP and floored at MIN_TICK_COST, because the gate must refuse a player who cannot pay
	//! for the tick they are about to receive - being conservative here leaves a player holding a few
	//! unspendable dollars, which is cheaper than letting a charge overdraw (implementation.md R10).
	//! At price 0 the answer is 0, never the floor: free fuel must never gate.
	//! \param[in] litresThisTick The station's output for one tick, in litres.
	//! \param[in] pricePerLitre The configured price per litre.
	//! \return Whole dollars the player must have to be allowed one more tick.
	static int EstimateTickCost(float litresThisTick, float pricePerLitre)
	{
		float price = ResolvePrice(pricePerLitre);
		if (price == 0)
			return 0;

		float cost = ComputeCost(litresThisTick, price);
		int rounded = Math.Ceil(cost);

		if (rounded < MIN_TICK_COST)
			return MIN_TICK_COST;

		return rounded;
	}

	//------------------------------------------------------------------------------------------------
	//! Everything the fast "Fill" action needs to know before it moves a drop: how many litres will
	//! actually be transferred, and what that costs in whole dollars.
	//!
	//! THE LITRES ARE THE MINIMUM OF THREE INDEPENDENT LIMITS - what the target can still hold, what
	//! the source still has, and what the player can pay for. Any one of them can bite on its own, and
	//! the money one has to bite by REDUCING THE FUEL rather than by refusing the fill: the whole point
	//! of the amendment is that a broke player still gets the fuel their money buys, and is told the
	//! real price before they hold the action (implementation.md A1).
	//!
	//! WHY THE CHARGE CAN NEVER EXCEED THE BALANCE. Two branches, and both are bounded by construction:
	//!  - the fuel is affordable in full, i.e. litres x price <= balance. balance is an INT, so the
	//!    smallest integer at or above litres x price is still <= balance, and rounding UP is therefore
	//!    safe. It also stops a 0.4 L top-up at a live price from being free, the same reason
	//!    EstimateTickCost floors at a dollar.
	//!  - the fuel is NOT affordable in full, so the player spends everything they have and receives
	//!    exactly balance / price litres. The cost is then the balance itself - equal to it, never past
	//!    it - and the litres are strictly fewer than they asked for.
	//! Either way the caller may take the returned cost without re-checking, which matters because
	//! TakePlayerMoneyPersistentId clamps at zero and would hide an overdraw as a successful charge.
	//!
	//! A FREE SOURCE NEVER CONSULTS THE MONEY AT ALL. Price 0 (a fuel truck, a jerrycan, the Fuel
	//! Depot, or the fuelPricePerLitre kill switch) returns the full litres with a cost of 0, so a
	//! player with nothing in their pocket can still empty a truck into their car.
	//! \param[in] litresNeeded How much the target can still take. Zero or negative means "nothing to
	//!            do" and produces an empty plan.
	//! \param[in] sourceAvailable How much the source can still give. Zero or negative likewise.
	//! \param[in] balance The payer's money, in whole dollars. Ignored entirely when the price is 0.
	//! \param[in] pricePerLitre Dollars per litre at this source; 0 (or negative) means free.
	//! \return A plan whose litres and cost are consistent with each other. Never null.
	static OVT_FuelFillPlan ComputeFillPlan(float litresNeeded, float sourceAvailable, int balance, float pricePerLitre)
	{
		OVT_FuelFillPlan plan = new OVT_FuelFillPlan();

		float litres = litresNeeded;
		if (sourceAvailable < litres)
			litres = sourceAvailable;

		// Nothing to move: an empty plan, whatever the price or the balance says.
		if (litres <= 0)
			return plan;

		float price = ResolvePrice(pricePerLitre);
		if (price == 0)
		{
			plan.m_fLitres = litres;
			return plan;
		}

		// Paid fuel with no money buys no fuel. The action reports this as a reason rather than
		// offering a fill that would move nothing.
		if (balance <= 0)
			return plan;

		float fullCost = litres * price;
		if (fullCost <= balance)
		{
			plan.m_fLitres = litres;
			plan.m_iCost = Math.Ceil(fullCost);
			return plan;
		}

		// Money is the binding limit: spend all of it and take only the fuel it bought. The division is
		// written through a float local on purpose - an int/int expression would truncate.
		float affordableLitres = balance;
		affordableLitres = affordableLitres / price;

		plan.m_fLitres = affordableLitres;
		plan.m_iCost = balance;

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! The price as it appears in the action label, without trailing-zero noise.
	//!
	//! "1" rather than "1.00", "1.5" rather than "1.50" - the label is already busy with the fill
	//! percentage and this sits inside it.
	//! \param[in] price The price per litre to draw.
	//! \return The formatted number, with no currency symbol and no unit.
	static string FormatPricePerLitre(float price)
	{
		return SCR_FormatHelper.FloatToStringNoZeroDecimalEndings(ResolvePrice(price), 2);
	}
}
