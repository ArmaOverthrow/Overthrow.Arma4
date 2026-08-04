//------------------------------------------------------------------------------------------------
//! Whether a shop buys from players, at what rate, and why an individual row cannot be sold.
//!
//! Pure predicates over plain values, held apart from the shop component on purpose: the SAME rules
//! decide whether the menu shows a Sell tab, whether a card is greyed out, whether the vehicle trunk
//! action appears, and whether the server honours a sell request. Four callers, one rule set - which
//! is the only way the client's offer and the server's authority cannot drift apart.
//!
//! Shop type is taken as an int because that is how OVT_ShopComponent.m_ShopType is declared; the
//! values compared against are OVT_ShopType members.
//------------------------------------------------------------------------------------------------
class OVT_ShopSellRules
{
	//! Reason keys. One per blocked case, so the details pane can say which rule stopped the sale.
	static const string REASON_EQUIPPED = "#OVT-SellBlocked_Equipped";
	static const string REASON_SHOP_DOES_NOT_BUY = "#OVT-SellBlocked_NotBoughtHere";
	static const string REASON_NO_VALUE = "#OVT-SellBlocked_NoValue";

	//------------------------------------------------------------------------------------------------
	//! Whether this shop has a sell path at all.
	//!
	//! Vehicle shops and procurement shops (garages, helipads) sell vehicles and buy nothing back.
	//! A gun dealer buys only while the difficulty config gives it a non-zero sell multiplier, so
	//! setting that multiplier to zero disables the whole gun-dealer sell path - menu toggle and
	//! trunk action alike - from one config value.
	//! \param[in] shopType OVT_ShopType value of the shop.
	//! \param[in] isProcurement The shop's m_bProcurement flag.
	//! \param[in] gunDealerMultiplier Difficulty config's gun dealer sell price multiplier.
	//! \return True when players can sell here.
	static bool ShopBuysFromPlayers(int shopType, bool isProcurement, float gunDealerMultiplier)
	{
		if (isProcurement)
			return false;

		if (shopType == OVT_ShopType.SHOP_VEHICLE)
			return false;

		if (shopType == OVT_ShopType.SHOP_GUNDEALER)
			return gunDealerMultiplier > 0;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The factor applied to an item's base sell price at this shop.
	//!
	//! Today only gun dealers modify the price; everywhere else pays the base rate. The server
	//! recomputes the credited amount through this same call, so the multiplier stops being the
	//! UI-only decoration it is on the legacy path.
	//! \param[in] shopType OVT_ShopType value of the shop.
	//! \param[in] gunDealerMultiplier Difficulty config's gun dealer sell price multiplier.
	//! \return The multiplier to apply to the base sell price.
	static float GetSellMultiplier(int shopType, float gunDealerMultiplier)
	{
		if (shopType == OVT_ShopType.SHOP_GUNDEALER)
			return gunDealerMultiplier;

		return 1.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one specific item may be sold.
	//!
	//! Takes the equipped classification as a boolean so this stays a pure decision: producing that
	//! boolean correctly is the scanner's job, deciding what it means is this one's.
	//! \param[in] isEquipped True when the item is worn, held, slung or holstered.
	//! \param[in] shopBuys Result of ShopBuysFromPlayers() for this shop, plus this shop's
	//! per-item IsSoldAtShop answer where the caller has one.
	//! \param[in] unitPrice The already-multiplied unit sell price.
	//! \return True when the item can be sold here for money.
	static bool CanSellItem(bool isEquipped, bool shopBuys, int unitPrice)
	{
		if (isEquipped)
			return false;

		if (!shopBuys)
			return false;

		if (unitPrice <= 0)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Names the rule that blocked a sale, for the greyed-out card's reason line.
	//!
	//! Checked in the same order as CanSellItem() so the reason always matches the rule that actually
	//! fired, and each blocked case gets its own key rather than a shared "cannot sell".
	//! \param[in] isEquipped True when the item is worn, held, slung or holstered.
	//! \param[in] shopBuys Whether this shop buys this item.
	//! \param[in] unitPrice The already-multiplied unit sell price.
	//! \return An #OVT- key, or an empty string when the item is sellable.
	static string GetBlockReasonKey(bool isEquipped, bool shopBuys, int unitPrice)
	{
		if (isEquipped)
			return REASON_EQUIPPED;

		if (!shopBuys)
			return REASON_SHOP_DOES_NOT_BUY;

		if (unitPrice <= 0)
			return REASON_NO_VALUE;

		return "";
	}
}
