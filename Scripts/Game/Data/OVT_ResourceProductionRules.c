//------------------------------------------------------------------------------------------------
//! Every production-site decision that is a rule rather than a lookup, as pure statics.
//!
//! The callers do the lookups (the live price, the clock hour, the viewer's persistent id) and
//! hand the answers in, so the Logic tier can assert the decisions without a world behind them.
//------------------------------------------------------------------------------------------------
class OVT_ResourceProductionRules
{
	//! The unowned-site sale discount off the live import price. Not the port's 0.5 sell ratio.
	static const float SITE_SELL_RATIO = 0.8;

	//! A hard ceiling on a single Produce() call, so a corrupt or extreme sleep skip cannot overflow.
	static const int MAX_SKIP_HOURS = 720;

	//------------------------------------------------------------------------------------------------
	//! What an unowned site charges for its accumulated stock. One ratio off the LIVE import price -
	//! never the port's sell ratio, never the base or stored price.
	//! \param[in] livePrice The live import price for that resource.
	//! \param[in] ratio Fraction of it the site charges. SITE_SELL_RATIO in production.
	//! \return Dollars per unit; never below 1.
	static int SitePrice(int livePrice, float ratio)
	{
		return Math.Max(1, Math.Round(livePrice * ratio));
	}

	//------------------------------------------------------------------------------------------------
	//! What buying the site itself costs, difficulty-scaled. Evaluated identically on the client
	//! (the action's label) and the server (the charge), so the two can never disagree.
	//! \param[in] baseCost The site's authored base cost.
	//! \param[in] multiplier The difficulty's real-estate cost multiplier.
	//! \return Dollars; never below 1.
	static int BuyCost(int baseCost, float multiplier)
	{
		return Math.Max(1, Math.Round(baseCost * multiplier));
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a viewer may open a site's storage. The single access predicate - called by the
	//! client action's gate and the server's holder ladder, never re-implemented inline.
	//! \param[in] viewerId The viewer's persistent id. "" for an unresolved viewer.
	//! \param[in] owner "" unowned, "resistance", or a player persistent id.
	//! \param[in] isPrivate Meaningless while unowned.
	//! \return True when the viewer may open the storage.
	static bool MayAccessStore(string viewerId, string owner, bool isPrivate)
	{
		if (owner == "")
			return false;

		if (owner == "resistance")
			return true;

		if (!isPrivate)
			return true;

		if (viewerId == "")
			return false;

		return viewerId == owner;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a viewer may toggle a site's public/private flag.
	//! \param[in] viewerId The viewer's persistent id. "" for an unresolved viewer.
	//! \param[in] owner "" unowned, "resistance", or a player persistent id.
	//! \param[in] isOfficer Whether the viewer is a resistance officer.
	//! \return True when the viewer may toggle privacy.
	static bool MayTogglePrivacy(string viewerId, string owner, bool isOfficer)
	{
		if (owner == "")
			return false;

		if (owner == "resistance")
			return isOfficer;

		if (viewerId == "")
			return false;

		return viewerId == owner;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the site itself may be bought.
	//! \param[in] owner "" unowned, "resistance", or a player persistent id.
	//! \return True only while unowned.
	static bool MayBuySite(string owner)
	{
		return owner == "";
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a player may buy the site's accumulated stock. Same predicate as MayBuySite today,
	//! kept as a separate name because a later economy pass may let owners sell (requirement §23).
	//! \param[in] owner "" unowned, "resistance", or a player persistent id.
	//! \return True only while unowned.
	static bool MayBuyStock(string owner)
	{
		return owner == "";
	}

	//------------------------------------------------------------------------------------------------
	//! Whether production should run this tick. Every in-game hour, unlike price drift's four
	//! windows a day.
	//! \param[in] currentHour Hour of day from the clock.
	//! \param[in] latchedHour The hour production last ran; -1 before the first batch.
	//! \return True only on an hour that has not already produced.
	static bool ShouldProduce(int currentHour, int latchedHour)
	{
		return currentHour != latchedHour;
	}

	//------------------------------------------------------------------------------------------------
	//! The drip. A per-site fractional carry means a sub-1 rate still produces over time rather than
	//! never. hours is clamped to MAX_SKIP_HOURS so an extreme sleep skip cannot overflow.
	//! \param[in] unitsPerHour The site's authored production rate.
	//! \param[in] hours How many in-game hours to produce for.
	//! \param[in] carryIn The fractional remainder carried from the last batch.
	//! \param[out] carryOut The new fractional remainder, always in [0, 1).
	//! \return Whole units produced.
	static int Produce(float unitsPerHour, int hours, float carryIn, out float carryOut)
	{
		if (unitsPerHour <= 0 || hours <= 0)
		{
			carryOut = carryIn;
			return 0;
		}

		int clampedHours = hours;
		if (clampedHours > MAX_SKIP_HOURS)
			clampedHours = MAX_SKIP_HOURS;

		float total = carryIn + (unitsPerHour * clampedHours);
		int units = Math.Floor(total);
		carryOut = total - units;

		if (carryOut < 0)
			carryOut = 0;

		if (carryOut >= 1)
			carryOut = 0;

		return units;
	}

	//------------------------------------------------------------------------------------------------
	//! How many of the units produced actually fit. Whole units that do not fit are discarded, not
	//! banked - capacity is the throttle, not a queue.
	//! \param[in] units Whole units produced this batch.
	//! \param[in] freeLitres Free space in the site's store. Negative means unlimited.
	//! \param[in] litresPerUnit Litres one unit occupies.
	//! \return Units that fit; 0 when litresPerUnit is not positive.
	static int FitProduction(int units, int freeLitres, int litresPerUnit)
	{
		if (litresPerUnit <= 0)
			return 0;

		if (freeLitres < 0)
			return units;

		int fits = freeLitres / litresPerUnit;
		return Math.Min(units, fits);
	}

	//------------------------------------------------------------------------------------------------
	//! The map icon's colour state, driving GetIconColor with no branching in the map class.
	//! \param[in] viewerId The viewer's persistent id. "" for an unresolved viewer.
	//! \param[in] owner "" unowned, "resistance", or a player persistent id.
	//! \param[in] isPrivate Meaningless while unowned.
	//! \return 0 unowned, 1 owned-and-accessible, 2 owned-private-not-yours.
	static int ColourState(string viewerId, string owner, bool isPrivate)
	{
		if (owner == "")
			return 0;

		if (MayAccessStore(viewerId, owner, isPrivate))
			return 1;

		return 2;
	}
}
