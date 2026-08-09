//------------------------------------------------------------------------------------------------
//! TIER C - the four economy-manager seams the shop UX rework is built on, against a live campaign.
//!
//! None of these can be asserted a tier down: GetNearestShop needs registered, streamed-in shop
//! entities; the category cache and the shop-type id sets need the entity catalog that
//! BuildResourceDatabase fills at world load, plus stock that only the campaign start produces.
//! The mapping RULE itself is pure and is asserted world-free in OVT_TEST_Logic_ShopUX - this case
//! deliberately does not restate it, it only proves the manager's caches were built from real data
//! and that the cached predicate still agrees with the uncached one.
//!
//! Four claims:
//!  1. GetNearestShop() at a registered shop's own origin returns that shop, at distance ~0. The
//!     lookup unions m_aAllShops with m_aGunDealers, and a gun dealer is only ever reachable through
//!     the second array (FilterShopEntities excludes dealers from the first).
//!  2. GetNearestShop() 5 km away with a 100 m limit returns null - the distance limit is honoured
//!     rather than "nearest wins regardless".
//!  3. GetItemCategory() answers something other than OTHER for at least one stocked resource, which
//!     is only possible if the cache was built and found arsenal data. A cache that silently built
//!     empty (or too early) would answer OTHER for everything and is the failure this catches.
//!  4. IsSoldAtShopCached(id, type) agrees with IsSoldAtShop(GetResource(id), type) on a SAMPLE of a
//!     shop's own stock. Sampling, not exhaustion: IsSoldAtShop is a full catalog scan per config
//!     rule per call, and this suite must finish before the town manager's 10 s modifier tick.
//!
//! READ-ONLY. This case mutates no campaign state, so it needs no restore path. It does prime two
//! lazily built manager caches, which is exactly what any first caller would do.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_ShopUXSeams : SCR_AutotestCaseBase
{
	//! Diagnostic backstop, not a retry budget: shop stock lands within ~604 ms of the campaign start.
	static const int MAX_POLLS = 600;

	//! How far away the out-of-range probe is placed, in metres. Far beyond any test-world shop.
	static const float FAR_AWAY_METRES = 5000;

	//! Radius given to the out-of-range probe, in metres.
	static const float FAR_AWAY_LIMIT_METRES = 100;

	//! Tolerance for "the shop is at its own origin", in metres.
	static const float SAME_PLACE_EPSILON = 1.0;

	//! How many stocked ids the IsSoldAtShopCached agreement check compares. A sample, deliberately.
	static const int AGREEMENT_SAMPLE_SIZE = 12;

	//! Polls spent waiting for a stocked shop to appear.
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetResultFailure("OVT_Global.GetEconomy() is null");
			return true;
		}

		array<RplId> shops = economy.GetAllShops();
		if (!shops || shops.Count() < 1)
		{
			SetResultFailure("No shops are registered with the economy manager, so none of these seams can be exercised");
			return true;
		}

		OVT_ShopComponent stockedShop = FindStockedShop(economy, shops);
		if (!stockedShop)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetResultFailure("No registered shop held any stock within %1 polls, so the category and sold-at caches have nothing real to answer about (%2 shop id(s) registered)",
					m_iPolls.ToString(), shops.Count().ToString());
				return true;
			}

			return false; // keep polling
		}

		string problem = CheckNearestShop(economy, stockedShop);
		if (problem != "")
		{
			SetResultFailure(problem);
			return true;
		}

		problem = CheckCategoryCache(economy, stockedShop);
		if (problem != "")
		{
			SetResultFailure(problem);
			return true;
		}

		problem = CheckSoldAtShopAgreement(economy, shops);
		if (problem != "")
		{
			SetResultFailure(problem);
			return true;
		}

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Claims 1 and 2 - the nearest-shop lookup finds a shop at its own origin and respects a radius.
	//! \param[in] economy The economy manager under test.
	//! \param[in] shop A registered shop resolved from the manager's own list.
	//! \return An empty string when both claims hold, or a description of the first failure.
	protected string CheckNearestShop(OVT_EconomyManagerComponent economy, OVT_ShopComponent shop)
	{
		IEntity owner = shop.GetOwner();
		if (!owner)
			return "The shop resolved from the manager's list has no owner entity, so it has no position to probe from";

		vector shopPos = owner.GetOrigin();

		OVT_ShopComponent found = economy.GetNearestShop(shopPos);
		if (!found)
			return string.Format("GetNearestShop() returned null at a registered shop's own origin %1", shopPos.ToString());

		IEntity foundOwner = found.GetOwner();
		if (!foundOwner)
			return "GetNearestShop() returned a shop component whose owner entity is null";

		float distance = vector.Distance(shopPos, foundOwner.GetOrigin());
		if (found != shop && distance > SAME_PLACE_EPSILON)
		{
			return string.Format("GetNearestShop() returned a different shop %1 m away from the origin it was asked about, expected the shop standing there",
				distance.ToString());
		}

		float offset = FAR_AWAY_METRES;
		float limit = FAR_AWAY_LIMIT_METRES;
		vector farAway = shopPos + Vector(offset, 0, 0);

		OVT_ShopComponent none = economy.GetNearestShop(farAway, limit);
		if (none)
		{
			IEntity noneOwner = none.GetOwner();
			float farDistance = -1;
			if (noneOwner)
				farDistance = vector.Distance(farAway, noneOwner.GetOrigin());

			return string.Format("GetNearestShop(pos + %1 m, maxDistance %2) returned a shop %3 m away, expected null - the distance limit is not being applied",
				offset.ToString(), limit.ToString(), farDistance.ToString());
		}

		PrintFormat("ShopUX seams: GetNearestShop resolved a shop at its own origin (%1 m) and returned null %2 m away with a %3 m limit",
			distance.ToString(), offset.ToString(), limit.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 3 - the id -> category cache was built from real arsenal data.
	//! Reads only ids the shop itself stocks, so the case asserts on state the campaign produced.
	//! \param[in] economy The economy manager under test.
	//! \param[in] shop A shop holding stock.
	//! \return An empty string when at least one stocked id is categorised, or a failure description.
	protected string CheckCategoryCache(OVT_EconomyManagerComponent economy, OVT_ShopComponent shop)
	{
		int categorised = 0;
		int inspected = 0;
		OVT_ShopCategory firstFound = OVT_ShopCategory.OTHER;

		for (int i = 0; i < shop.m_aInventory.Count(); i++)
		{
			int id = shop.m_aInventory.GetKey(i);
			if (!economy.IsValidResourceId(id))
				continue;

			inspected += 1;

			OVT_ShopCategory category = economy.GetItemCategory(id);
			if (category == OVT_ShopCategory.OTHER)
				continue;

			if (categorised == 0)
				firstFound = category;

			categorised += 1;
		}

		if (categorised < 1)
		{
			return string.Format("GetItemCategory() answered OTHER for all %1 stocked resource id(s) of a shop - the category cache is empty or was built before the entity catalog",
				inspected.ToString());
		}

		int firstFoundValue = firstFound;
		PrintFormat("ShopUX seams: %1 of %2 stocked ids categorised, first non-OTHER category %3",
			categorised.ToString(), inspected.ToString(), firstFoundValue.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 4 - the cached sold-at predicate agrees with the uncached one on a sample.
	//! Vehicle and procurement shops are skipped: their type has no inventory rules to compare.
	//! \param[in] economy The economy manager under test.
	//! \param[in] shops Every registered shop id.
	//! \return An empty string when every sampled id agreed, or a description of the first mismatch.
	protected string CheckSoldAtShopAgreement(OVT_EconomyManagerComponent economy, array<RplId> shops)
	{
		foreach (RplId shopId : shops)
		{
			OVT_ShopComponent shop = economy.GetShopByRplId(shopId);
			if (!shop || !shop.m_aInventory || shop.m_aInventory.Count() < 1)
				continue;

			if (shop.m_bProcurement || shop.m_ShopType == OVT_ShopType.SHOP_VEHICLE)
				continue;

			// IsSoldAtShop walks the config's rule list directly, so a shop type without a rule list
			// is not a comparable subject.
			OVT_ShopInventoryConfig config = economy.GetShopConfig(shop.m_ShopType);
			if (!config || !config.m_aInventoryItems || config.m_aInventoryItems.Count() < 1)
				continue;

			return CompareSoldAtShop(economy, shop);
		}

		return "No registered shop has a stocked inventory and a shop type with configured inventory rules, so the cached sold-at predicate could not be compared against the uncached one";
	}

	//------------------------------------------------------------------------------------------------
	//! Compares IsSoldAtShopCached against IsSoldAtShop for a sample of one shop's stocked ids.
	//! \param[in] economy The economy manager under test.
	//! \param[in] shop The shop whose stock supplies the sample and whose type is queried.
	//! \return An empty string when every sampled id agreed, or a description of the first mismatch.
	protected string CompareSoldAtShop(OVT_EconomyManagerComponent economy, OVT_ShopComponent shop)
	{
		int compared = 0;
		int soldHere = 0;

		for (int i = 0; i < shop.m_aInventory.Count(); i++)
		{
			if (compared >= AGREEMENT_SAMPLE_SIZE)
				break;

			int id = shop.m_aInventory.GetKey(i);
			if (!economy.IsValidResourceId(id))
				continue;

			ResourceName res = economy.GetResource(id);
			if (res == ResourceName.Empty)
				continue;

			bool cached = economy.IsSoldAtShopCached(id, shop.m_ShopType);
			bool uncached = economy.IsSoldAtShop(res, shop.m_ShopType);

			if (cached != uncached)
			{
				return string.Format("At shop type %1, IsSoldAtShopCached(id %2) returned %3 while IsSoldAtShop('%4') returned %5 - the cached id set does not match the configured rules",
					shop.m_ShopType.ToString(), id.ToString(), cached.ToString(), res, uncached.ToString());
			}

			compared += 1;
			if (cached)
				soldHere += 1;
		}

		if (compared < 1)
			return "No stocked id of the sampled shop resolved to a resource, so nothing was compared";

		if (soldHere < 1)
		{
			return string.Format("Both predicates answered false for all %1 sampled ids of a shop of type %2 that actually stocks them - agreement on 'nothing is sold here' proves nothing",
				compared.ToString(), shop.m_ShopType.ToString());
		}

		PrintFormat("ShopUX seams: IsSoldAtShopCached agreed with IsSoldAtShop on %1 sampled id(s) of a type-%2 shop, %3 of them sold there",
			compared.ToString(), shop.m_ShopType.ToString(), soldHere.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the first registered shop that resolves to a component and holds stock.
	//! \param[in] economy The economy manager holding the registrations.
	//! \param[in] shops Every registered shop id.
	//! \return A stocked shop component, or null while none is stocked yet.
	protected OVT_ShopComponent FindStockedShop(OVT_EconomyManagerComponent economy, array<RplId> shops)
	{
		foreach (RplId shopId : shops)
		{
			OVT_ShopComponent shop = economy.GetShopByRplId(shopId);
			if (!shop || !shop.m_aInventory)
				continue;

			if (shop.m_aInventory.Count() > 0)
				return shop;
		}

		return null;
	}
}
