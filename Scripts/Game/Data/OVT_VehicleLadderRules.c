//! Pure spine of the occupying faction's vehicle escalation ladder: which rung is unlocked and
//! affordable for a given threat and a given purse. Every static here takes plain numbers only -
//! no manager, no controller, no live state of any kind - so each one is proven directly, function
//! by function, rather than through anything that has to be built and torn down.
class OVT_VehicleLadderRules
{
	//------------------------------------------------------------------------------------------------
	//! Stretches or compresses a rung's threat requirement by the campaign's difficulty scalar.
	//! \param[in] minThreat The rung's authored threat requirement.
	//! \param[in] scale The difficulty's threshold scalar. Zero or below is treated as one, so a
	//! misauthored difficulty preset never divides a rung's requirement away to nothing.
	//! \return The scaled requirement, never negative.
	static float ScaledThreshold(int minThreat, float scale)
	{
		float effectiveScale = scale;
		if (effectiveScale <= 0)
			effectiveScale = 1;

		return Math.Max(0, minThreat * effectiveScale);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a rung's requirement is met at the current threat.
	//! \param[in] minThreat The rung's authored threat requirement.
	//! \param[in] scale The difficulty's threshold scalar.
	//! \param[in] threat The live threat figure being compared against.
	//! \return True at or above the rung's scaled threshold, so a rung of zero is always unlocked,
	//! including at a threat of exactly zero.
	static bool RungUnlocked(int minThreat, float scale, float threat)
	{
		return threat >= ScaledThreshold(minThreat, scale);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a rung's price fits inside a purse.
	//! \param[in] cost The rung's authored price.
	//! \param[in] budget The purse to fit inside. A negative figure means unbounded - the caller
	//! has not been given a ceiling at all, as opposed to a ceiling of zero.
	//! \return True when the rung fits.
	static bool RungAffordable(int cost, int budget)
	{
		if (budget < 0)
			return true;

		return cost <= budget;
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the highest rung that is both unlocked and affordable.
	//! \param[in] minThreats One threat requirement per rung, parallel to costs.
	//! \param[in] costs One price per rung, parallel to minThreats.
	//! \param[in] scale The difficulty's threshold scalar.
	//! \param[in] threat The live threat figure.
	//! \param[in] budget The purse the pick must fit inside; negative is unbounded.
	//! \return The index of the qualifying rung with the highest requirement. Two rungs tied on the
	//! same requirement resolve to the lower index - author order wins a tie. -1 is a real answer,
	//! meaning nothing qualifies, and is not an error.
	static int PickRung(notnull array<int> minThreats, notnull array<int> costs, float scale, float threat, int budget)
	{
		int bestIndex = -1;
		int bestThreshold = -1;

		int count = minThreats.Count();
		for (int i = 0; i < count; i++)
		{
			int rungThreat = minThreats[i];
			int rungCost = costs[i];

			if (!RungUnlocked(rungThreat, scale, threat))
				continue;

			if (!RungAffordable(rungCost, budget))
				continue;

			if (rungThreat > bestThreshold)
			{
				bestThreshold = rungThreat;
				bestIndex = i;
			}
		}

		return bestIndex;
	}
}
