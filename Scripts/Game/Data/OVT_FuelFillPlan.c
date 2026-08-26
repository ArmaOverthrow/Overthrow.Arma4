//------------------------------------------------------------------------------------------------
//! What one press of the fast "Fill" action will actually do: how many litres move, and how many
//! whole dollars that costs.
//!
//! A two-field answer rather than two out-parameters because the two numbers are only ever correct
//! TOGETHER. The litres are clamped by the money and the money is derived from the litres, so a
//! caller that recomputed one of them from the other - or read a stale one next to a fresh one -
//! would charge for fuel it did not deliver or deliver fuel it did not charge for. Both halves come
//! out of OVT_FuelPricing.ComputeFillPlan in one call and neither is meaningful alone.
//!
//! IT IS A PLAN, NOT A RECEIPT. The client builds one to draw the action label ("Fill Tank ($37)")
//! and to decide whether the action is worth offering; the SERVER builds its own from its own
//! numbers and honours that one. They can legitimately differ - the source may have been drained,
//! or the player's money spent, in the second between the label being drawn and the request landing
//! - and when they do, the server's plan is the one that happens. Nothing here is authority.
//------------------------------------------------------------------------------------------------
class OVT_FuelFillPlan
{
	//! Litres to move from the source into the target. Never negative, never more than the target can
	//! hold, the source can give, or the money can buy.
	float m_fLitres;

	//! Whole dollars to charge for those litres. Always 0 at a free source, always within the balance
	//! the plan was computed against, never negative.
	int m_iCost;

	//------------------------------------------------------------------------------------------------
	//! Whether this plan would move any fuel at all.
	//! \return True when there are litres to transfer.
	bool HasFuel()
	{
		return m_fLitres > 0;
	}
}
