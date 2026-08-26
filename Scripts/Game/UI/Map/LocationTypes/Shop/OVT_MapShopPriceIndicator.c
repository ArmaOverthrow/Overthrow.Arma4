//! Relative price indicator for the shop and gun-dealer map panels.
//!
//! ================================================================================================
//! THE MODEL (implementation plan section 4.6). Normalising OVT_EconomyManagerComponent's
//! GetSellPriceAtOffset (:553-573) by base price gives exactly two independent terms:
//!
//!   price_i / base_i = 1 + 0.1 * (1 - townStock_i / townMaxStock_i) + 0.0001 * distToPort
//!                          \___ per item, bounded +/-10% ___/         \_ shop-wide, unbounded _/
//!
//! The per-item CARETS read the scarcity term only; the shop-level BADGE reads the remoteness term
//! only. Splitting by term rather than normalising against the shop's own mean is decision K6:
//! because the remoteness term is EXACTLY uniform across a shop, excluding it already solves the
//! "everything reads up at a remote shop" problem, and it preserves real information - a genuinely
//! scarce town honestly reads all-up instead of being flattened to neutral by its own average.
//!
//! IT IS A TOWN SIGNAL, NOT THIS SHOP'S PRICING WHIM (K7). Town stock sums across EVERY shop in the
//! town, so two shops in one town produce identical carets. That is a property of Overthrow's
//! economy, not a defect in this indicator, and the panel copy says "local supply" for that reason.
//!
//! NO PRICES, NO CURRENCY, NO PERCENTAGES, NO STOCK COUNTS ever leave this file. Every public
//! result is a band, a localization key or an imageset quad name. The shop menu remains the only
//! place with real numbers - that is the feature's defining constraint.
//! ================================================================================================

//------------------------------------------------------------------------------------------------
//! One caret band. NEUTRAL means "no caret" and, for a per-item row, "omit the row entirely" -
//! there is deliberately no neutral glyph in overthrow_priceicons.imageset.
//------------------------------------------------------------------------------------------------
enum OVT_MapPriceLevel
{
	DOWN_3,
	DOWN_2,
	DOWN_1,
	NEUTRAL,
	UP_1,
	UP_2,
	UP_3
}

//------------------------------------------------------------------------------------------------
//! The pure half of the indicator: banding maths over (townStock, townMaxStock) and (distToPort).
//!
//! WORLD-FREE AND MANAGER-FREE BY DESIGN. Everything here is a static function of its arguments,
//! which is what lets OVT_TEST_Logic_MapShopPriceBands assert it in the Logic tier without loading
//! a world. Keep it that way: the moment this class reads a manager, the only automatable part of
//! this feature stops being automatable.
//!
//! BAND BOUNDARIES ALWAYS RESOLVE TO THE LESS EXTREME BAND. +7.5 is two carets, not three; -2.5 is
//! neutral, not one caret down; a remoteness of exactly 5 hides the badge. One rule, applied
//! uniformly, so a reader never has to work out which side a boundary falls on.
//------------------------------------------------------------------------------------------------
class OVT_MapShopPriceBands
{
	//! Caret glyph set. White silhouettes on transparent - the map atlases are meant to be tinted,
	//! and the info panel background is near-black, so the row layout's authored white reads.
	static const ResourceName PRICE_ICON_IMAGESET = "{A5EA4C81F9A25690}UI/Imagesets/overthrow_priceicons.imageset";

	//! GetSellPriceAtOffset's scarcity coefficient (:567). Bounds the term to +/-10% of base price.
	static const float SCARCITY_COEFFICIENT = 0.1;

	//! GetSellPriceAtOffset's distance-to-port coefficient (:567), per metre.
	static const float REMOTENESS_COEFFICIENT = 0.0001;

	//! Caret band edges, in percent of base price.
	static const float SCARCITY_BAND_1 = 2.5;
	static const float SCARCITY_BAND_2 = 5.0;
	static const float SCARCITY_BAND_3 = 7.5;

	//! Badge band edges, in percent of base price. 5% is 500 m from a port.
	static const float REMOTENESS_BAND_1 = 5.0;
	static const float REMOTENESS_BAND_2 = 15.0;
	static const float REMOTENESS_BAND_3 = 30.0;

	//------------------------------------------------------------------------------------------------
	//! The scarcity term as a percentage of base price.
	//!
	//! Mirrors GetSellPriceAtOffset's own guard (:565): a max stock below 1 is clamped to 1 rather
	//! than dividing by zero. Stock can legitimately overshoot to twice max (TOWN_STOCK_BUY_CAP_MULTIPLIER,
	//! :629), so the result genuinely reaches -10 and real bargains exist.
	//! \param[in] townStock Total stock of this item across every shop in the town.
	//! \param[in] townMaxStock The town's theoretical capacity for this item.
	//! \return Roughly -10 (glutted) to +10 (sold out).
	static float GetScarcityPercent(int townStock, int townMaxStock)
	{
		int maxStock = townMaxStock;
		if (maxStock < 1)
			maxStock = 1;

		return 100.0 * SCARCITY_COEFFICIENT * (1.0 - ((float)townStock / maxStock));
	}

	//------------------------------------------------------------------------------------------------
	//! Bands a scarcity percentage.
	//! \param[in] scarcityPercent Output of GetScarcityPercent.
	//! \return The caret band. NEUTRAL means the row is omitted.
	static OVT_MapPriceLevel GetScarcityLevel(float scarcityPercent)
	{
		if (scarcityPercent > SCARCITY_BAND_3)
			return OVT_MapPriceLevel.UP_3;

		if (scarcityPercent > SCARCITY_BAND_2)
			return OVT_MapPriceLevel.UP_2;

		if (scarcityPercent > SCARCITY_BAND_1)
			return OVT_MapPriceLevel.UP_1;

		if (scarcityPercent < -SCARCITY_BAND_3)
			return OVT_MapPriceLevel.DOWN_3;

		if (scarcityPercent < -SCARCITY_BAND_2)
			return OVT_MapPriceLevel.DOWN_2;

		if (scarcityPercent < -SCARCITY_BAND_1)
			return OVT_MapPriceLevel.DOWN_1;

		return OVT_MapPriceLevel.NEUTRAL;
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience: stock straight to band.
	//! \param[in] townStock Total stock of this item across every shop in the town.
	//! \param[in] townMaxStock The town's theoretical capacity for this item.
	//! \return The caret band.
	static OVT_MapPriceLevel GetScarcityLevelForStock(int townStock, int townMaxStock)
	{
		return GetScarcityLevel(GetScarcityPercent(townStock, townMaxStock));
	}

	//------------------------------------------------------------------------------------------------
	//! The remoteness term as a percentage of base price.
	//! \param[in] distanceToPort Metres to the nearest registered port.
	//! \return 0.01 * distanceToPort. Meaningless for a negative distance - band it instead.
	static float GetRemotenessPercent(float distanceToPort)
	{
		return 100.0 * REMOTENESS_COEFFICIENT * distanceToPort;
	}

	//------------------------------------------------------------------------------------------------
	//! Bands a distance to the nearest port.
	//!
	//! A NEGATIVE DISTANCE HIDES THE BADGE (N7). DistanceToNearestPort returns -1 when no ports are
	//! registered at all (:846-858); banding that naively would render a confident "very cheap"
	//! badge built on the absence of data.
	//! \param[in] distanceToPort Metres to the nearest port, or negative when unknown.
	//! \return UP_1..UP_3, or NEUTRAL to hide the badge.
	static OVT_MapPriceLevel GetRemotenessLevel(float distanceToPort)
	{
		if (distanceToPort < 0)
			return OVT_MapPriceLevel.NEUTRAL;

		float remotenessPercent = GetRemotenessPercent(distanceToPort);

		if (remotenessPercent > REMOTENESS_BAND_3)
			return OVT_MapPriceLevel.UP_3;

		if (remotenessPercent > REMOTENESS_BAND_2)
			return OVT_MapPriceLevel.UP_2;

		if (remotenessPercent > REMOTENESS_BAND_1)
			return OVT_MapPriceLevel.UP_1;

		return OVT_MapPriceLevel.NEUTRAL;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] level The band.
	//! \return True when the band draws nothing.
	static bool IsNeutral(OVT_MapPriceLevel level)
	{
		return level == OVT_MapPriceLevel.NEUTRAL;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] level The band.
	//! \return True for the dearer half of the scale.
	static bool IsUp(OVT_MapPriceLevel level)
	{
		return level == OVT_MapPriceLevel.UP_1 || level == OVT_MapPriceLevel.UP_2 || level == OVT_MapPriceLevel.UP_3;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] level The band.
	//! \return True for the cheaper half of the scale.
	static bool IsDown(OVT_MapPriceLevel level)
	{
		return level == OVT_MapPriceLevel.DOWN_1 || level == OVT_MapPriceLevel.DOWN_2 || level == OVT_MapPriceLevel.DOWN_3;
	}

	//------------------------------------------------------------------------------------------------
	//! How many carets the band draws, ignoring direction.
	//! \param[in] level The band.
	//! \return 0 for NEUTRAL, otherwise 1, 2 or 3.
	static int GetMagnitude(OVT_MapPriceLevel level)
	{
		if (level == OVT_MapPriceLevel.UP_1 || level == OVT_MapPriceLevel.DOWN_1)
			return 1;

		if (level == OVT_MapPriceLevel.UP_2 || level == OVT_MapPriceLevel.DOWN_2)
			return 2;

		if (level == OVT_MapPriceLevel.UP_3 || level == OVT_MapPriceLevel.DOWN_3)
			return 3;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Quad name inside PRICE_ICON_IMAGESET.
	//! \param[in] level The band.
	//! \return "up_1".."up_3" / "down_1".."down_3", or "" for NEUTRAL (no neutral glyph exists).
	static string GetCaretIcon(OVT_MapPriceLevel level)
	{
		if (level == OVT_MapPriceLevel.UP_1)
			return "up_1";

		if (level == OVT_MapPriceLevel.UP_2)
			return "up_2";

		if (level == OVT_MapPriceLevel.UP_3)
			return "up_3";

		if (level == OVT_MapPriceLevel.DOWN_1)
			return "down_1";

		if (level == OVT_MapPriceLevel.DOWN_2)
			return "down_2";

		if (level == OVT_MapPriceLevel.DOWN_3)
			return "down_3";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	// GetDirectionKey() lived here and returned "#OVT-Map_Shop_Dearer"/"#OVT-Map_Shop_Cheaper" as the
	// text half of a dual affordance. Removed 2026-08-10 by user directive - the glyph is enough.
	//
	// F-6 still holds: its requirement is that up-versus-down not depend on COLOUR, and the glyph is a
	// directional SHAPE drawn in the same white as every other row icon, so direction survives with
	// colour removed entirely. What is lost is redundancy, not the signal.
	//
	// Both ids were cut from Language/localization_Overthrow.st the same day ({6A7E1C4D00000043} and
	// {6A7E1C4D00000044}); the user regenerates the six runtime exports. Do not reintroduce either key
	// without adding it back to the master first - a key absent from the exports renders raw on screen.
}

//------------------------------------------------------------------------------------------------
//! One row the panel may draw. Managed rather than a plain struct because the arrays that hold it
//! are ref arrays.
//------------------------------------------------------------------------------------------------
class OVT_MapShopPriceRow : Managed
{
	//! Resource id, and the key both caches are keyed on.
	int m_iResourceId;

	//! The scarcity term for this item, in percent of base price. Never rendered.
	float m_fScarcityPercent;

	//! Banded form of m_fScarcityPercent.
	OVT_MapPriceLevel m_eLevel;

	//! Only meaningful on gun-dealer rows.
	SCR_EArsenalItemType m_eWeaponType;

	//! Resolved lazily, and only for rows that survive selection (N8 - resolution loads and scans a
	//! prefab container, which is far too expensive to run for every id a shop stocks).
	string m_sDisplayName;
}

//------------------------------------------------------------------------------------------------
//! The world-touching half: reads replicated economy state and turns it into rows and a badge.
//!
//! LIFETIME. One instance per location type, held as a ref member. OVT_MapLocationType.Init() runs
//! on EVERY map open (map/location-types context.md gotcha 6) but the type objects themselves are
//! instantiated once from Configs/Map/OverthrowMap.conf, so the caches below survive across opens -
//! which is the whole point of having them.
//!
//! COMPUTED ONCE, AT PANEL-OPEN TIME (Q-7). Nothing here may be called from CanFastTravel or
//! ShouldShowLocation; both run per element on every zoom change.
//!
//! NEVER CALLS OVT_EconomyManagerComponent.GetTownStock (N6 / Q-4). That method calls
//! GetShopByRplId - which returns null on a Replication.FindItem miss - and then dereferences the
//! result unguarded, so a client with a shop still in flight takes a script error. GetShopByRplId
//! carries the same shape of hazard one level down (it dereferences rpl.GetEntity() unguarded), so
//! this file resolves shop components itself, from Replication.FindItem, with a guard at every step.
//------------------------------------------------------------------------------------------------
class OVT_MapShopPriceIndicator
{
	//! Resource id -> localized display name (N8). Mirrors OVT_ShopContext.m_mDisplayNames (:73).
	protected ref map<int, string> m_mDisplayNames = new map<int, string>();

	//! Resource id -> arsenal item type, for the four weapon kinds only. Null until first use.
	//! OVT_ShopCategory cannot answer this: it collapses rifle/pistol/sniper/MG/launcher into one
	//! WEAPONS bucket and checks mode before type (N16), and the economy manager caches only that
	//! coarser mapping. Built from FindInventoryItems, which is the only public window onto the
	//! entity catalog.
	protected ref map<int, SCR_EArsenalItemType> m_mWeaponTypes;

	//! The four kinds a gun dealer rolls one of each session (N13). Pistols are deliberately absent:
	//! EVERY dealer stocks EVERY pistol, so a pistol row differentiates nothing and would push the
	//! four rows that do differ off the panel.
	protected static const ref array<SCR_EArsenalItemType> SIGNATURE_WEAPON_TYPES = {
		SCR_EArsenalItemType.RIFLE,
		SCR_EArsenalItemType.SNIPER_RIFLE,
		SCR_EArsenalItemType.MACHINE_GUN,
		SCR_EArsenalItemType.ROCKET_LAUNCHER
	};

	//------------------------------------------------------------------------------------------------
	//! Resolves the town the SERVER would price against.
	//!
	//! GetSellPriceAtOffset uses GetNearestTown(pos) then GetTownID (:559-562) - deliberately NOT
	//! OVT_ShopComponent.m_iTownId. Using the shop's own field instead would make the panel disagree
	//! with the price the player is actually charged the moment the two ever diverge.
	//! \param[in] towns The town manager, may be null.
	//! \param[in] pos The shop's world position.
	//! \return The town id, or -1 when no town could be resolved.
	int ResolveTownId(OVT_TownManagerComponent towns, vector pos)
	{
		if (!towns)
			return -1;

		OVT_TownData town = towns.GetNearestTown(pos);
		if (!town)
			return -1;

		return towns.GetTownID(town);
	}

	//------------------------------------------------------------------------------------------------
	//! Every shop component in a town, skipping any whose replication has not resolved.
	//!
	//! Resolved ONCE per panel open and reused for every item, so the cost is one FindItem per shop
	//! rather than one per shop per item.
	//! \param[in] economy The economy manager, may be null.
	//! \param[in] townId Town id from ResolveTownId.
	//! \param[out] shops Receives the resolved components. Cleared first. Never null entries.
	void ResolveTownShops(OVT_EconomyManagerComponent economy, int townId, out array<OVT_ShopComponent> shops)
	{
		if (!shops)
			return;

		shops.Clear();

		if (!economy || townId < 0)
			return;

		if (!economy.m_mTownShops || !economy.m_mTownShops.Contains(townId))
			return;

		array<RplId> shopIds = economy.m_mTownShops[townId];
		if (!shopIds)
			return;

		foreach (RplId shopId : shopIds)
		{
			OVT_ShopComponent shop = ResolveShop(shopId);
			if (shop)
				shops.Insert(shop);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Null-safe RplId -> shop component. The deliberate replacement for GetShopByRplId.
	//! \param[in] shopId The shop's RplId.
	//! \return The component, or null at any broken link in the chain.
	OVT_ShopComponent ResolveShop(RplId shopId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		if (!rpl)
			return null;

		IEntity entity = rpl.GetEntity();
		if (!entity)
			return null;

		return OVT_ShopComponent.Cast(entity.FindComponent(OVT_ShopComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Total stock of an item across a town's shops. The null-safe stand-in for the economy manager's
	//! own town-stock sum, which this file deliberately never calls (N6 / Q-4).
	//! \param[in] shops Output of ResolveTownShops.
	//! \param[in] resourceId The item.
	//! \return The summed stock, or 0 when nothing is resolvable.
	int SumStockAcrossShops(array<OVT_ShopComponent> shops, int resourceId)
	{
		if (!shops)
			return 0;

		int stock = 0;

		foreach (OVT_ShopComponent shop : shops)
		{
			if (shop)
				stock += shop.GetStock(resourceId);
		}

		return stock;
	}

	//------------------------------------------------------------------------------------------------
	//! Distance to the nearest registered port, in metres.
	//!
	//! Mirrors OVT_EconomyManagerComponent.DistanceToNearestPort (:846-858) - same array, same
	//! minimum, same -1 for "no ports" - but guards the RplComponent lookup, which the manager's
	//! version does not. A port whose replication has not resolved is skipped rather than
	//! dereferenced, so a panel opened seconds after joining degrades to a missing badge (Q-3).
	//! \param[in] economy The economy manager, may be null.
	//! \param[in] pos The shop's world position.
	//! \return Metres to the nearest port, or -1 when there are none (N7 - hide the badge).
	float GetDistanceToPort(OVT_EconomyManagerComponent economy, vector pos)
	{
		if (!economy)
			return -1;

		array<RplId> ports = economy.GetAllPorts();
		if (!ports)
			return -1;

		float nearest = -1;

		foreach (RplId portId : ports)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(portId));
			if (!rpl)
				continue;

			IEntity port = rpl.GetEntity();
			if (!port)
				continue;

			float distance = vector.Distance(pos, port.GetOrigin());
			if (nearest < 0 || distance < nearest)
				nearest = distance;
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Bands the shop-wide remoteness term.
	//! \param[in] economy The economy manager, may be null.
	//! \param[in] pos The shop's world position.
	//! \return UP_1..UP_3, or NEUTRAL to hide the badge.
	OVT_MapPriceLevel GetRemotenessLevel(OVT_EconomyManagerComponent economy, vector pos)
	{
		return OVT_MapShopPriceBands.GetRemotenessLevel(GetDistanceToPort(economy, pos));
	}

	//------------------------------------------------------------------------------------------------
	//! Scores every item a shop currently has in stock.
	//!
	//! Display names are NOT resolved here - only the ids that survive SelectExtremes get a name,
	//! because resolution loads and scans a prefab container (N8) and a general store can carry a
	//! hundred ids.
	//! \param[in] economy The economy manager, may be null.
	//! \param[in] towns The town manager, may be null.
	//! \param[in] shop The shop being described, may be null.
	//! \param[in] pos The shop's world position.
	//! \param[out] rows Receives one row per stocked, non-vehicle item. Cleared first.
	//! \return The number of stocked items considered, which is 0 for a shop with nothing on it.
	int CollectScarcityRows(OVT_EconomyManagerComponent economy, OVT_TownManagerComponent towns, OVT_ShopComponent shop, vector pos, out array<ref OVT_MapShopPriceRow> rows)
	{
		if (!rows)
			return 0;

		rows.Clear();

		if (!economy || !shop || !shop.m_aInventory)
			return 0;

		int townId = ResolveTownId(towns, pos);

		array<OVT_ShopComponent> townShops = new array<OVT_ShopComponent>();
		ResolveTownShops(economy, townId, townShops);

		int stockedCount = 0;

		for (int i = 0; i < shop.m_aInventory.Count(); i++)
		{
			int resourceId = shop.m_aInventory.GetKey(i);

			// N15 - sold-out ids stay in m_aInventory at 0 so per-town heavy weapons restock rather
			// than re-roll. Listing them would advertise stock the shop does not have.
			if (shop.m_aInventory.GetElement(i) <= 0)
				continue;

			if (!economy.IsValidResourceId(resourceId))
				continue;

			// GetSellPriceAtOffset returns the flat base price for anything in m_aAllVehicles (:556),
			// BEFORE both adjustment terms, so a vehicle's caret would always be a lie about nothing.
			if (economy.IsVehicle(resourceId))
				continue;

			stockedCount++;

			// Without a town there is no scarcity term at all: GetSellPriceAtOffset skips the whole
			// adjustment when GetNearestTown returns nothing, so every item really is at base price.
			if (townId < 0)
				continue;

			int townStock = SumStockAcrossShops(townShops, resourceId);
			int townMaxStock = economy.GetTownMaxStock(townId, resourceId);

			OVT_MapShopPriceRow row = new OVT_MapShopPriceRow();
			row.m_iResourceId = resourceId;
			row.m_fScarcityPercent = OVT_MapShopPriceBands.GetScarcityPercent(townStock, townMaxStock);
			row.m_eLevel = OVT_MapShopPriceBands.GetScarcityLevel(row.m_fScarcityPercent);
			row.m_eWeaponType = SCR_EArsenalItemType.RIFLE;
			row.m_sDisplayName = "";

			rows.Insert(row);
		}

		return stockedCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the best bargains and the worst rip-offs, skipping the neutral band.
	//!
	//! Selection by repeated extremum rather than a sort: maxPerSide is 3, so this is O(6n) on an
	//! array that is already bounded by one shop's inventory, and it avoids depending on any
	//! particular array-sort behaviour.
	//!
	//! De-duplicates, so a shop with five non-neutral items cannot list one of them twice when the
	//! two halves overlap.
	//! \param[in] rows Output of CollectScarcityRows.
	//! \param[in] maxPerSide How many of each direction to take.
	//! \param[out] selected Receives cheapest-first then dearest-first. Cleared first.
	void SelectExtremes(array<ref OVT_MapShopPriceRow> rows, int maxPerSide, out array<ref OVT_MapShopPriceRow> selected)
	{
		if (!selected)
			return;

		selected.Clear();

		if (!rows)
			return;

		// Cheapest first: the most negative scarcity percentages.
		for (int down = 0; down < maxPerSide; down++)
		{
			OVT_MapShopPriceRow best = null;

			foreach (OVT_MapShopPriceRow row : rows)
			{
				if (!OVT_MapShopPriceBands.IsDown(row.m_eLevel))
					continue;

				if (selected.Contains(row))
					continue;

				if (!best || row.m_fScarcityPercent < best.m_fScarcityPercent)
					best = row;
			}

			if (!best)
				break;

			selected.Insert(best);
		}

		// Then dearest: the most positive.
		for (int up = 0; up < maxPerSide; up++)
		{
			OVT_MapShopPriceRow worst = null;

			foreach (OVT_MapShopPriceRow row : rows)
			{
				if (!OVT_MapShopPriceBands.IsUp(row.m_eLevel))
					continue;

				if (selected.Contains(row))
					continue;

				if (!worst || row.m_fScarcityPercent > worst.m_fScarcityPercent)
					worst = row;
			}

			if (!worst)
				break;

			selected.Insert(worst);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The four weapons THIS dealer rolled (implementation plan section 4.6b).
	//!
	//! Of everything a dealer carries, only a handful of ids are per-dealer picks - one RIFLE, one
	//! SNIPER_RIFLE, one MACHINE_GUN, one ROCKET_LAUNCHER, plus one WEAPON_VARIANTS roll each for
	//! rifle/sniper/MG (m_bSingleRandomItem, OVT_TownController.c:288-310). One row per kind is
	//! shown, so a dealer that rolled both a rifle and a rifle variant lists the first found. Every pistol, all ammunition, every attachment, throwable and
	//! explosive is identical at every dealer and is excluded, because a row that reads the same at
	//! every dealer answers nobody's "is it worth walking to this one?".
	//!
	//! A CATEGORY THE DEALER DID NOT ROLL IS ABSENT, never "None". Three rows is a correct answer.
	//! \param[in] economy The economy manager, may be null.
	//! \param[in] towns The town manager, may be null.
	//! \param[in] dealer The dealer's shop component, may be null.
	//! \param[in] pos The dealer's world position.
	//! \param[out] rows Receives up to one row per weapon kind, in SIGNATURE_WEAPON_TYPES order.
	void CollectSignatureWeapons(OVT_EconomyManagerComponent economy, OVT_TownManagerComponent towns, OVT_ShopComponent dealer, vector pos, out array<ref OVT_MapShopPriceRow> rows)
	{
		if (!rows)
			return;

		rows.Clear();

		if (!economy || !dealer || !dealer.m_aInventory)
			return;

		int townId = ResolveTownId(towns, pos);

		array<OVT_ShopComponent> townShops = new array<OVT_ShopComponent>();
		ResolveTownShops(economy, townId, townShops);

		// Pass one: the small residue of in-stock, WEAPONS-category ids. The category check is the
		// cheap one (it is already cached, :1789) and it is the check that removes magazines and
		// optics, which are catalogued under the type of the weapon they belong to (N16).
		array<int> weaponIds = new array<int>();

		for (int i = 0; i < dealer.m_aInventory.Count(); i++)
		{
			int resourceId = dealer.m_aInventory.GetKey(i);

			if (dealer.m_aInventory.GetElement(i) <= 0)
				continue;

			if (!economy.IsValidResourceId(resourceId))
				continue;

			if (economy.GetItemCategory(resourceId) != OVT_ShopCategory.WEAPONS)
				continue;

			weaponIds.Insert(resourceId);
		}

		if (weaponIds.IsEmpty())
			return;

		BuildWeaponTypeCache(economy);

		// Pass two: one row per signature kind, in a fixed order so the panel does not reshuffle
		// itself between two dealers that rolled the same kinds.
		foreach (SCR_EArsenalItemType weaponType : SIGNATURE_WEAPON_TYPES)
		{
			foreach (int resourceId : weaponIds)
			{
				if (!m_mWeaponTypes || !m_mWeaponTypes.Contains(resourceId))
					continue;

				if (m_mWeaponTypes[resourceId] != weaponType)
					continue;

				string displayName = ResolveDisplayName(economy, resourceId);

				// A row with no name is a blank line, and a blank line above a heading is worse than
				// an absent category. Skip it and let the next candidate of this kind try.
				if (displayName.IsEmpty())
					continue;

				OVT_MapShopPriceRow row = new OVT_MapShopPriceRow();
				row.m_iResourceId = resourceId;
				row.m_eWeaponType = weaponType;
				row.m_sDisplayName = displayName;

				if (townId < 0)
				{
					row.m_fScarcityPercent = 0;
					row.m_eLevel = OVT_MapPriceLevel.NEUTRAL;
				}
				else
				{
					int townStock = SumStockAcrossShops(townShops, resourceId);
					int townMaxStock = economy.GetTownMaxStock(townId, resourceId);
					row.m_fScarcityPercent = OVT_MapShopPriceBands.GetScarcityPercent(townStock, townMaxStock);
					row.m_eLevel = OVT_MapShopPriceBands.GetScarcityLevel(row.m_fScarcityPercent);
				}

				rows.Insert(row);
				break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the resource id -> arsenal type cache for the four signature kinds, once.
	//!
	//! FindInventoryItems is the only public window onto the entity catalog (m_aEntityCatalogEntries
	//! is protected), and it wildcards the mode when passed DEFAULT - so this deliberately sweeps in
	//! magazines and optics typed after their parent weapon. That is harmless: the caller has already
	//! reduced its candidates to the WEAPONS category, which is mode-correct, so the intersection of
	//! the two is exactly "real weapons of this kind".
	//! \param[in] economy The economy manager, may be null.
	protected void BuildWeaponTypeCache(OVT_EconomyManagerComponent economy)
	{
		if (m_mWeaponTypes)
			return;

		if (!economy)
			return;

		map<int, SCR_EArsenalItemType> cache = new map<int, SCR_EArsenalItemType>();

		foreach (SCR_EArsenalItemType weaponType : SIGNATURE_WEAPON_TYPES)
		{
			array<SCR_EntityCatalogEntry> entries = new array<SCR_EntityCatalogEntry>();
			economy.FindInventoryItems(weaponType, SCR_EArsenalItemMode.DEFAULT, "", entries);

			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				if (!entry)
					continue;

				ResourceName prefab = entry.GetPrefab();

				// GetInventoryId is a bare map index: an unregistered prefab silently resolves to id 0,
				// i.e. some other item entirely.
				if (!economy.IsRegisteredResource(prefab))
					continue;

				cache.Set(economy.GetInventoryId(prefab), weaponType);
			}
		}

		// Only cached once it is built from a populated catalog, so an early open cannot poison it.
		if (!cache.IsEmpty())
			m_mWeaponTypes = cache;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key naming a signature weapon's kind.
	//! \param[in] weaponType One of SIGNATURE_WEAPON_TYPES.
	//! \return An #OVT- key, or "" for anything outside the four kinds.
	static string GetWeaponTypeKey(SCR_EArsenalItemType weaponType)
	{
		if (weaponType == SCR_EArsenalItemType.RIFLE)
			return "#OVT-Map_GunDealer_Rifle";

		if (weaponType == SCR_EArsenalItemType.SNIPER_RIFLE)
			return "#OVT-Map_GunDealer_Sniper";

		if (weaponType == SCR_EArsenalItemType.MACHINE_GUN)
			return "#OVT-Map_GunDealer_MachineGun";

		if (weaponType == SCR_EArsenalItemType.ROCKET_LAUNCHER)
			return "#OVT-Map_GunDealer_Launcher";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Localized display name for a resource, memoised for the life of this indicator (N8).
	//!
	//! Mirrors OVT_ShopContext.ResolveDisplayName (:855-883), which documents why: UIInfo resolution
	//! loads and scans a prefab container and is far too expensive to repeat per row.
	//! \param[in] economy The economy manager, may be null.
	//! \param[in] resourceId The item.
	//! \return A non-empty name, falling back to the prefab path when nothing resolves.
	string ResolveDisplayName(OVT_EconomyManagerComponent economy, int resourceId)
	{
		if (m_mDisplayNames.Contains(resourceId))
			return m_mDisplayNames[resourceId];

		if (!economy || !economy.IsValidResourceId(resourceId))
			return "";

		ResourceName prefab = economy.GetResource(resourceId);
		if (prefab.IsEmpty())
			return "";

		string name = "";

		UIInfo info = OVT_PrefabUtils.GetItemUIInfo(prefab);
		if (info)
			name = info.GetName();

		if (name == "")
			name = prefab;

		string translated = WidgetManager.Translate(name);
		if (translated != "")
			name = translated;

		m_mDisplayNames[resourceId] = name;
		return name;
	}
}
