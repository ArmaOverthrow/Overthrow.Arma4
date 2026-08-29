//------------------------------------------------------------------------------------------------
//! TIER C - the field-repair wrench must be registered, priced $150, and eligible at exactly the
//! two intended shops (general stores, gun dealers) and no others.
//!
//! Why this cannot live a tier down: all three claims are about the ENTITY CATALOG, which only
//! exists after BuildResourceDatabase has run at world load, and about IsSoldAtShop, which walks
//! that same live catalog against the loaded shop configs. There is no pure function in this
//! feature to test at Logic tier - `tools/check-shop-coverage.py` already covers the static config
//! parse headlessly, so this case exists to prove the same facts hold against a REAL loaded
//! campaign, not to duplicate the parser.
//!
//! Three claims:
//!  A. The wrench (`RepairKit_` under EQUIPMENT/SUPPORT_STATION) is registered in the resource
//!     database and its configured price is exactly 150. This is the claim that catches the
//!     `hidden 1` trap directly: BuildResourceDatabase `continue`s before insertion for a hidden
//!     rule, so a regression here means the wrench silently vanishes from the whole economy, not
//!     just from a shop. Asserted independently of any shop rule.
//!  B. The wrench is ELIGIBLE at both SHOP_GENERAL and SHOP_GUNDEALER. Asserted through
//!     IsSoldAtShop (the eligibility predicate), never through a shop's rolled stock: stock is a
//!     random draw per shop, so an empty result would be ambiguous between "the rule excludes it"
//!     and "the dice did not pick it" - see OVT_TEST_Campaign_ShopCivilianStock for the same
//!     reasoning applied to the sibling BUG-098 case.
//!  C. No BUG-098 regression: the wrench is NOT eligible at SHOP_ELECTRONIC, and the sibling
//!     `RearmingKit_` item (deliberately left `hidden 1` in Phase 2) is not registered at all -
//!     it must stay entirely absent from the resource database, not merely unsold.
//!
//! READ-ONLY. Mutates no campaign state.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_FieldRepairEconomy : SCR_AutotestCaseBase
{
	//! Diagnostic backstop, not a retry budget: the catalog is built at world load, long before this.
	static const int MAX_POLLS = 600;

	//! Polls spent waiting for the entity catalog to answer.
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetFailure("OVT_Global.GetEconomy() is null");
			return true;
		}

		array<SCR_EntityCatalogEntry> entries();
		economy.FindInventoryItems(SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.SUPPORT_STATION, "RepairKit_", entries);

		if (entries.IsEmpty())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("No EQUIPMENT/SUPPORT_STATION entry matching 'RepairKit_' found in the entity catalog within %1 polls - the catalog is empty, so this case has no subject and would pass vacuously",
					m_iPolls.ToString());
				return true;
			}

			return false; // keep polling - the catalog is not built yet
		}

		ResourceName wrench = entries[0].GetPrefab();
		if (wrench == "")
		{
			SetFailure("The matched 'RepairKit_' catalog entry resolved to an empty prefab ResourceName");
			return true;
		}

		string problem = CheckRegisteredAndPriced(economy, wrench);
		if (problem != "")
		{
			SetFailure(problem);
			return true;
		}

		problem = CheckSoldAtBothIntendedShops(economy, wrench);
		if (problem != "")
		{
			SetFailure(problem);
			return true;
		}

		problem = CheckNoBug098Regression(economy, wrench);
		if (problem != "")
		{
			SetFailure(problem);
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Claim A - the wrench is registered in the resource database and priced exactly 150.
	//! \param[in] economy The economy manager under test.
	//! \param[in] wrench The wrench prefab resolved from the catalog.
	//! \return An empty string when both hold, or a description of the failure.
	protected string CheckRegisteredAndPriced(OVT_EconomyManagerComponent economy, ResourceName wrench)
	{
		if (!economy.IsRegisteredResource(wrench))
		{
			return string.Format("'%1' appears in the EQUIPMENT/SUPPORT_STATION catalog but IsRegisteredResource() is false - it is not in the resource database, consistent with a 'hidden 1' pricing rule dropping it before insertion",
				wrench);
		}

		int id = economy.GetInventoryId(wrench);
		int price = economy.GetPrice(id);

		if (price != 150)
		{
			return string.Format("'%1' is registered but its configured price is %2, expected 150",
				wrench, price.ToString());
		}

		PrintFormat("Field repair economy: '%1' registered, price %2", wrench, price.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Claim B - the wrench is eligible (not necessarily rolled) at both intended shops.
	//! \param[in] economy The economy manager under test.
	//! \param[in] wrench The wrench prefab resolved from the catalog.
	//! \return An empty string when both shops are eligible, or a description of the failure.
	protected string CheckSoldAtBothIntendedShops(OVT_EconomyManagerComponent economy, ResourceName wrench)
	{
		if (!economy.IsSoldAtShop(wrench, OVT_ShopType.SHOP_GENERAL))
		{
			return string.Format("'%1' is not eligible at SHOP_GENERAL - IsSoldAtShop() returned false", wrench);
		}

		if (!economy.IsSoldAtShop(wrench, OVT_ShopType.SHOP_GUNDEALER))
		{
			return string.Format("'%1' is not eligible at SHOP_GUNDEALER - IsSoldAtShop() returned false", wrench);
		}

		PrintFormat("Field repair economy: '%1' eligible at SHOP_GENERAL and SHOP_GUNDEALER", wrench);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Claim C - no BUG-098 regression. The wrench stays off SHOP_ELECTRONIC, and the sibling
	//! RearmingKit_ item stays entirely unregistered (hidden 1, not merely unsold anywhere).
	//! \param[in] economy The economy manager under test.
	//! \param[in] wrench The wrench prefab resolved from the catalog.
	//! \return An empty string when both hold, or a description of the failure.
	protected string CheckNoBug098Regression(OVT_EconomyManagerComponent economy, ResourceName wrench)
	{
		if (economy.IsSoldAtShop(wrench, OVT_ShopType.SHOP_ELECTRONIC))
		{
			return string.Format("'%1' is eligible at SHOP_ELECTRONIC - BUG-098's civilian-electronics exclusion has regressed for the repair kit", wrench);
		}

		array<SCR_EntityCatalogEntry> rearmingEntries();
		economy.FindInventoryItems(SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.SUPPORT_STATION, "RearmingKit_", rearmingEntries);

		foreach (SCR_EntityCatalogEntry entry : rearmingEntries)
		{
			if (!entry)
				continue;

			ResourceName res = entry.GetPrefab();
			if (res == "")
				continue;

			if (economy.IsRegisteredResource(res))
			{
				return string.Format("'%1' matches 'RearmingKit_' and IS registered in the resource database - it must stay 'hidden 1' and entirely absent",
					res);
			}
		}

		PrintFormat("Field repair economy: '%1' not eligible at SHOP_ELECTRONIC, no registered 'RearmingKit_' entry (%2 catalog entries found, all unregistered)",
			wrench, rearmingEntries.Count().ToString());

		return "";
	}
}
