//------------------------------------------------------------------------------------------------
//! What it costs to repair a ruined structure. Pure arithmetic - no world, no config, no manager.
//!
//! ONE ROUNDING, NOT TWO (implementation.md D11). OVT_OverthrowConfigComponent.GetBuildableCost()
//! already rounds the difficulty-multiplied build price; halving THAT and rounding again drifts by up
//! to half a dollar and makes the client's label and the server's charge two different functions of
//! the same inputs. This takes the RAW authored m_iCost and both multipliers, so the label the player
//! reads and the money the server takes come from one expression evaluated twice.
//!
//! MONEY ONLY, DELIBERATELY. When the logistics epic can deliver materials, the change is confined to
//! OVT_ResistanceFactionManager.RepairStructure() - this class stays a money function and must not be
//! widened for a hypothetical resource consumer (implementation.md §3.12).
//------------------------------------------------------------------------------------------------
class OVT_RepairPricing
{
	//------------------------------------------------------------------------------------------------
	//! The price of putting a ruined structure back, in dollars.
	//!
	//! Negative multipliers clamp to 0 rather than paying the player, the same guard rail
	//! OVT_FuelPricing.ResolvePrice applies to a mis-authored preset. A structure is never repaired at
	//! a negative price and never at a fractional one.
	//! \param[in] baseCost The buildable's raw authored m_iCost.
	//! \param[in] buildMultiplier OVT_DifficultySettings.buildableCostMultiplier.
	//! \param[in] repairMultiplier OVT_DifficultySettings.repairCostMultiplier.
	//! \return Dollars owed; never negative.
	static int RepairCost(int baseCost, float buildMultiplier, float repairMultiplier)
	{
		if (baseCost <= 0)
			return 0;

		float build = buildMultiplier;
		if (build < 0)
			build = 0;

		float repair = repairMultiplier;
		if (repair < 0)
			repair = 0;

		return Math.Round(baseCost * build * repair);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a structure can be priced for repair at all.
	//!
	//! UNKNOWN_STRUCTURE_COST is deliberately huge rather than zero (its own header says why), so a
	//! structure no config entry claims would otherwise quote a million-dollar repair instead of
	//! refusing. This is the guard that turns that into "not repairable".
	//! \param[in] baseCost The cost answered by OVT_ResistanceFactionManager.GetStructureCost().
	//! \return False for an unpriced structure or a non-positive cost.
	static bool IsRepairable(int baseCost)
	{
		if (baseCost <= 0)
			return false;

		return baseCost != OVT_ResistanceFactionManager.UNKNOWN_STRUCTURE_COST;
	}
}
