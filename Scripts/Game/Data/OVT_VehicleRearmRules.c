//------------------------------------------------------------------------------------------------
//! Every vehicle rearm decision that is a rule rather than a lookup, as pure statics.
//!
//! Callers resolve prefabs and counts (the muzzle, the ledger, the difficulty price) and hand the
//! answers in, so the Logic tier can assert the decisions without a world behind them.
//------------------------------------------------------------------------------------------------
class OVT_VehicleRearmRules
{
	//------------------------------------------------------------------------------------------------
	//! Resolves the ammo prefab a rearmable weapon eats.
	//! \param[in] muzzleDefault BaseMuzzleComponent.GetDefaultMagazineOrProjectileName() for a gun.
	//! \param[in] loadedPrefab The weapon's currently loaded magazine prefab, as a fallback.
	//! \param[in] rocketPrefab A rocket pod's default rocket prefab, as a fallback.
	//! \return The first non-empty argument, in that order; "" when all three are empty.
	static string ResolveAmmoPrefab(string muzzleDefault, string loadedPrefab, string rocketPrefab)
	{
		if (muzzleDefault != "")
			return muzzleDefault;

		if (loadedPrefab != "")
			return loadedPrefab;

		if (rocketPrefab != "")
			return rocketPrefab;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Prorates the money price of a rearm by the fraction of units the ledgers could not cover.
	//!
	//! An int/int division here would truncate before the rounding matters, so the divisor is cast
	//! to float explicitly. Clamped to a minimum of 1 whenever there is an uncovered unit at all, so
	//! a rounding-down never gives ammunition away free.
	//! \param[in] fullCost What the rearm costs today, wholly unbought.
	//! \param[in] uncoveredUnits Units the ledgers could not cover.
	//! \param[in] totalUnits Total units the rearm needs.
	//! \return 0 when totalUnits <= 0 or uncoveredUnits <= 0; fullCost when uncoveredUnits >= totalUnits;
	//! otherwise fullCost scaled by the uncovered fraction, floored at 1.
	static int ProratedCost(int fullCost, int uncoveredUnits, int totalUnits)
	{
		if (totalUnits <= 0)
			return 0;

		if (uncoveredUnits <= 0)
			return 0;

		if (uncoveredUnits >= totalUnits)
			return fullCost;

		int cost = Math.Round(fullCost * uncoveredUnits / (float)totalUnits);

		if (cost < 1)
			cost = 1;

		return cost;
	}
}
