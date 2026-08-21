//------------------------------------------------------------------------------------------------
//! Every resource decision that is a rule rather than a lookup, as pure statics.
//!
//! The callers do the lookups (the clock, the threat level, the port count, the world positions) and
//! hand the answers in, so the Logic tier can assert the decisions without a world behind them.
//------------------------------------------------------------------------------------------------
class OVT_ResourceRules
{
	//! The four hours at which prices may drift; one step per window.
	static const int DRIFT_HOUR_A = 0;
	static const int DRIFT_HOUR_B = 6;
	static const int DRIFT_HOUR_C = 12;
	static const int DRIFT_HOUR_D = 18;

	//------------------------------------------------------------------------------------------------
	//! One price walk step, clamped to the band around base.
	//!
	//! Volatility scales the STEP and never the band: the edges are base x bandMin / base x bandMax
	//! whatever the volatility is.
	//! \param[in] basePrice Config base price. The band and the step are both fractions of it.
	//! \param[in] current The stored (already clamped) price this window starts from.
	//! \param[in] roll Random walk term, normally in [-1, 1].
	//! \param[in] pressure War pressure term from WarPressure(), in [-1, 1].
	//! \param[in] volatility Difficulty volatility multiplier.
	//! \param[in] stepFraction Fraction of base one full roll moves.
	//! \param[in] bandMin Lower band edge as a fraction of base.
	//! \param[in] bandMax Upper band edge as a fraction of base.
	//! \return The next stored price; never below 1.
	static int DriftStep(int basePrice, int current, float roll, float pressure, float volatility, float stepFraction, float bandMin, float bandMax)
	{
		float delta = (roll + pressure) * stepFraction * volatility;
		int step = Math.Round(basePrice * delta);
		int next = current + step;

		int lo = Math.Round(basePrice * bandMin);
		int hi = Math.Round(basePrice * bandMax);

		if (next < lo)
			next = lo;

		if (next > hi)
			next = hi;

		if (next < 1)
			next = 1;

		return next;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an hour is one of the four drift windows.
	//! \param[in] hour Hour of day from the clock.
	//! \return True at 0, 6, 12 and 18.
	static bool IsDriftWindowHour(int hour)
	{
		if (hour == DRIFT_HOUR_A)
			return true;

		if (hour == DRIFT_HOUR_B)
			return true;

		if (hour == DRIFT_HOUR_C)
			return true;

		if (hour == DRIFT_HOUR_D)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the drift tick should walk the prices this call. One step per window, latched on the
	//! hour that was last drifted (the shipped m_iHourPaid* idiom).
	//! \param[in] hour Hour of day from the clock.
	//! \param[in] lastDriftedHour The latch; -1 before the first drift.
	//! \return True only on a window hour that is not already latched.
	static bool ShouldDrift(int hour, int lastDriftedHour)
	{
		if (!IsDriftWindowHour(hour))
			return false;

		return hour != lastDriftedHour;
	}

	//------------------------------------------------------------------------------------------------
	//! The live price a player sees, from the stored price. Applied at READ time, after the band
	//! clamp, so on a hard difficulty the live price may legitimately exceed base x bandMax.
	//! \param[in] storedPrice The clamped, walked price.
	//! \param[in] levelMultiplier The difficulty's resource price multiplier.
	//! \return The live price; never below 1.
	static int ApplyLevelMultiplier(int storedPrice, float levelMultiplier)
	{
		return Math.Max(1, Math.Round(storedPrice * levelMultiplier));
	}

	//------------------------------------------------------------------------------------------------
	//! What the port pays per unit. One ratio off the LIVE price - there is no second walk.
	//! \param[in] livePrice The price the player would pay to import.
	//! \param[in] sellRatio Fraction of it the port pays back.
	//! \return Dollars per unit; never below 1.
	static int SellPrice(int livePrice, float sellRatio)
	{
		return Math.Max(1, Math.Round(livePrice * sellRatio));
	}

	//------------------------------------------------------------------------------------------------
	//! The war-pressure term of the drift. Threat pushes prices UP, resistance port control pushes
	//! them DOWN.
	//! \param[in] threat The occupying faction's threat.
	//! \param[in] threatReference Threat that counts as maximum pressure. Zero or less yields no threat term.
	//! \param[in] controlledPortFraction Fraction of ports the resistance controls, in [0, 1].
	//! \return Pressure in [-1, 1].
	static float WarPressure(float threat, float threatReference, float controlledPortFraction)
	{
		float threatTerm = 0;
		if (threatReference > 0)
			threatTerm = Math.Clamp(threat / threatReference, 0, 1);

		return Math.Clamp(threatTerm - (0.5 * controlledPortFraction), -1, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the pile a new drop should merge into: nearest within the radius, ties to the larger
	//! pile, first entry on a full tie.
	//!
	//! SQUARED distances throughout - vector.Distance is not correctly rounded, so an exact-boundary
	//! decision taken on it is a coin flip.
	//! \param[in] sqDistances Squared distance from the drop to each candidate pile.
	//! \param[in] litres Litres each candidate holds, index-aligned with sqDistances.
	//! \param[in] radiusSq Squared merge radius; a candidate exactly on it merges.
	//! \return The winning index, or -1 when nothing is in range.
	static int SelectMergeTarget(array<float> sqDistances, array<int> litres, float radiusSq)
	{
		if (!sqDistances || !litres)
			return -1;

		int best = -1;
		float bestDistance = 0;
		int bestLitres = 0;

		int count = sqDistances.Count();
		if (litres.Count() < count)
			count = litres.Count();

		for (int i = 0; i < count; i++)
		{
			if (sqDistances[i] > radiusSq)
				continue;

			if (best == -1)
			{
				best = i;
				bestDistance = sqDistances[i];
				bestLitres = litres[i];
				continue;
			}

			if (sqDistances[i] < bestDistance)
			{
				best = i;
				bestDistance = sqDistances[i];
				bestLitres = litres[i];
				continue;
			}

			if (sqDistances[i] == bestDistance && litres[i] > bestLitres)
			{
				best = i;
				bestDistance = sqDistances[i];
				bestLitres = litres[i];
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Scales a config requirement by the difficulty multiplier. A non-zero requirement never scales
	//! away to nothing.
	//! \param[in] baseQty The config quantity.
	//! \param[in] multiplier The difficulty's buildable resource cost multiplier.
	//! \return The scaled quantity; 0 only when baseQty was 0 or less, otherwise at least 1.
	static int ScaleRequirement(int baseQty, float multiplier)
	{
		if (baseQty <= 0)
			return 0;

		int scaled = Math.Round(baseQty * multiplier);
		if (scaled < 1)
			scaled = 1;

		return scaled;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether every requirement is met by what is available.
	//! \param[in] need The scaled requirements.
	//! \param[in] have What is available nearby, summed across piles.
	//! \param[out] shortId The FIRST requirement that is short; "" when satisfied.
	//! \return True when nothing is short.
	static bool IsSatisfied(array<ref OVT_ResourceAmount> need, array<ref OVT_ResourceAmount> have, out string shortId)
	{
		shortId = "";

		if (!need)
			return true;

		for (int i = 0; i < need.Count(); i++)
		{
			OVT_ResourceAmount requirement = need[i];
			if (!requirement)
				continue;

			if (requirement.m_iQuantity <= 0)
				continue;

			if (AmountOf(have, requirement.m_sId) < requirement.m_iQuantity)
			{
				shortId = requirement.m_sId;
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How many units of one id an amount list holds. Sums duplicates.
	//! \param[in] amounts The list to search. Null is 0.
	//! \param[in] id Stable resource id.
	//! \return The summed quantity.
	static int AmountOf(array<ref OVT_ResourceAmount> amounts, string id)
	{
		if (!amounts)
			return 0;

		int total = 0;
		for (int i = 0; i < amounts.Count(); i++)
		{
			OVT_ResourceAmount amount = amounts[i];
			if (!amount)
				continue;

			if (amount.m_sId == id)
				total = total + amount.m_iQuantity;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! The order piles are drained in: nearest first, stable on ties (insertion sort, so equal
	//! distances keep their input order).
	//! \param[in] sqDistances Squared distance to each pile.
	//! \param[out] order Receives pile indices in drain order. Cleared first.
	static void SortPilesForConsumption(array<float> sqDistances, out array<int> order)
	{
		if (!order)
			order = new array<int>();

		order.Clear();

		if (!sqDistances)
			return;

		for (int i = 0; i < sqDistances.Count(); i++)
		{
			int slot = order.Count();
			for (int j = 0; j < order.Count(); j++)
			{
				if (sqDistances[i] < sqDistances[order[j]])
				{
					slot = j;
					break;
				}
			}

			order.InsertAt(i, slot);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The requirements readout: one line per requirement, marking each shortfall.
	//!
	//! THE TWO TRAILING PARAMETERS ARE THE DISPLAY LAYER'S, AND BOTH DEFAULT TO THE RAW FORM. Called
	//! with three arguments it produces exactly what it always produced - the stable id and the
	//! English word - which is what keeps this function assertable in the Logic tier, where a
	//! "#OVT-..." key resolves to nothing. A renderer passes already-translated strings instead, so
	//! the numbers and the shortfall marking stay pure and only the words come from the .st.
	//! \param[in] need The scaled requirements.
	//! \param[in] have What is available nearby.
	//! \param[in] defs The definition table; requirements it does not know are skipped.
	//! \param[in] titles Display names index-aligned with need. Null, short or empty entries fall
	//! back to the stable id.
	//! \param[in] shortWord The already-translated word marking a shortfall.
	//! \return A newline-separated readout; "" when there is nothing to list.
	static string FormatReadout(array<ref OVT_ResourceAmount> need, array<ref OVT_ResourceAmount> have, OVT_ResourceDefs defs, array<string> titles = null, string shortWord = "short")
	{
		if (!need || !defs)
			return "";

		string readout = "";
		for (int i = 0; i < need.Count(); i++)
		{
			OVT_ResourceAmount requirement = need[i];
			if (!requirement)
				continue;

			if (requirement.m_iQuantity <= 0)
				continue;

			if (!defs.Knows(requirement.m_sId))
				continue;

			int available = AmountOf(have, requirement.m_sId);

			string label = requirement.m_sId;
			if (titles && i < titles.Count() && titles[i] != "")
				label = titles[i];

			string line = label + ": " + available.ToString() + " / " + requirement.m_iQuantity.ToString();
			if (available < requirement.m_iQuantity)
			{
				int missing = requirement.m_iQuantity - available;
				line = line + " (" + shortWord + " " + missing.ToString() + ")";
			}

			if (readout != "")
				readout = readout + "\n";

			readout = readout + line;
		}

		return readout;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a resource may be BOUGHT at a port. A non-importable resource is never listed on
	//! import; it may still be sold.
	//! \param[in] defs The definition table.
	//! \param[in] index Definition index.
	//! \return True for a known, importable definition.
	static bool MayImport(OVT_ResourceDefs defs, int index)
	{
		if (!defs)
			return false;

		return defs.IsImportable(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a resource may be SOLD at a port. Anything the table knows may be sold, importable or
	//! not - that is what makes a non-importable resource worth hauling home.
	//! \param[in] defs The definition table.
	//! \param[in] index Definition index.
	//! \return True for a known definition.
	static bool MayExport(OVT_ResourceDefs defs, int index)
	{
		if (!defs)
			return false;

		return defs.IdAt(index) != "";
	}

	//------------------------------------------------------------------------------------------------
	//! The illegal gate (D20). A legal resource passes unconditionally; an illegal one needs the
	//! smuggling permission or a port the resistance holds.
	//! \param[in] defs The definition table.
	//! \param[in] index Definition index.
	//! \param[in] hasIllegalImportsPermission Whether the player has the IllegalImports permission.
	//! \param[in] resistanceControlsPort Whether the resistance controls the nearest port.
	//! \return True when the trade is allowed.
	static bool IllegalGateOpen(OVT_ResourceDefs defs, int index, bool hasIllegalImportsPermission, bool resistanceControlsPort)
	{
		if (!defs)
			return false;

		if (!defs.IsIllegal(index))
			return true;

		if (hasIllegalImportsPermission)
			return true;

		return resistanceControlsPort;
	}

	//------------------------------------------------------------------------------------------------
	//! The town-control gate for a town-buildable, run on BOTH sides of the wire: the client reason in
	//! OVT_BuildContext.CanBuild and the server re-check in OVT_ResistanceFactionManager.BuildItem
	//! call this same function with the same numbers, so the two can never disagree.
	//!
	//! SQUARED distances, because vector.Distance is not correctly rounded and this decides a refusal
	//! at a range boundary.
	//! \param[in] townFaction The nearest town's controlling faction index.
	//! \param[in] playerFaction The resistance faction index.
	//! \param[in] sqDistance Squared distance from the build position to the town centre.
	//! \param[in] sqRange Squared build range for that town's size.
	//! \return True when the position is outside the town, or the town is the player's.
	static bool TownControlAllowsBuild(int townFaction, int playerFaction, float sqDistance, float sqRange)
	{
		if (sqDistance >= sqRange)
			return true;

		return townFaction == playerFaction;
	}
}
