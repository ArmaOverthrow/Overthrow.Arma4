//------------------------------------------------------------------------------------------------
//! The fractional-charge accumulator that turns "2.08 dollars of fuel arrived" into whole dollars.
//!
//! Money is an int; the fuel price is a float and one 0.5 s refuel tick delivers a fraction of a
//! litre-price. Dropping the remainder every tick would make fuel nearly free at low flow rates;
//! rounding it up every tick would nearly double the price. So each tick's exact cost is accrued
//! against the player's persistent id, and only whole dollars are handed back to the caller to take.
//!
//! DELIBERATELY NEVER SETTLED AND NEVER PERSISTED - DO NOT "FIX" THIS.
//! The sub-dollar remainder simply waits at that pump for the player's next visit. It is not flushed
//! on disconnect, on action cancel, on station destruction or on save, and it is not written to disk.
//! That single decision (implementation.md D8) deletes the entire class of "settle on X" bugs -
//! disconnect mid-refuel, cancel mid-refuel, two players at one pump, station blown up mid-flow - at
//! a cost of at most $0.99 per player per pump, which is below the resolution of the currency and
//! well under the price of a single litre at most difficulties. Adding a settle path would trade a
//! rounding error nobody can observe for a money-mutation path with no fuel behind it.
//!
//! Keyed by persistent id, so concurrent refuellers at one pump can never cross-charge. One instance
//! lives on each modded station component.
//------------------------------------------------------------------------------------------------
class OVT_FuelChargeLedger : Managed
{
	//! Persistent id -> dollars accrued but not yet whole. Always in [0, 1).
	protected ref map<string, float> m_mPending;

	//------------------------------------------------------------------------------------------------
	//! Starts an empty ledger. One of these is created lazily per modded station component.
	void OVT_FuelChargeLedger()
	{
		m_mPending = new map<string, float>();
	}

	//------------------------------------------------------------------------------------------------
	//! Accrues the cost of one delivery and reports the whole dollars to take right now.
	//!
	//! The returned amount is what the caller should charge; whatever is left over stays pending. A
	//! zero-resolving price accrues nothing, so the kill switch cannot leave residue behind it.
	//! \param[in] key The payer's persistent id. An empty key is ignored (non-players refuel free).
	//! \param[in] litres Litres actually delivered by this tick.
	//! \param[in] pricePerLitre The configured price per litre.
	//! \return Whole dollars owed now. 0 when this tick did not push the pending amount over a dollar.
	int Accrue(string key, float litres, float pricePerLitre)
	{
		if (key == "")
			return 0;

		float cost = OVT_FuelPricing.ComputeCost(litres, pricePerLitre);
		if (cost <= 0)
			return 0;

		float pending = 0;
		if (m_mPending.Contains(key))
			pending = m_mPending.Get(key);

		pending = pending + cost;

		int whole = Math.Floor(pending);
		pending = pending - whole;

		m_mPending.Set(key, pending);

		return whole;
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets one payer's pending remainder, leaving every other payer untouched.
	//!
	//! Used when a charge was clamped by an empty wallet: the fuel already delivered was paid for as
	//! far as the player could pay, and carrying the shortfall forward would charge them again for it.
	//! \param[in] key The payer's persistent id.
	void Clear(string key)
	{
		if (!m_mPending.Contains(key))
			return;

		m_mPending.Remove(key);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] key The payer's persistent id.
	//! \return Dollars accrued but not yet charged for this payer, always in [0, 1). 0 when unknown.
	float GetPending(string key)
	{
		if (!m_mPending.Contains(key))
			return 0;

		return m_mPending.Get(key);
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many payers this ledger is holding a remainder for. Diagnostic.
	int GetTrackedCount()
	{
		return m_mPending.Count();
	}
}
