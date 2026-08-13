//------------------------------------------------------------------------------------------------
//! TIER C - civilian shops do not deal in crew-served weapon parts (BUG-098).
//!
//! Why this cannot live a tier down: the claim is about the ENTITY CATALOG, which only exists after
//! BuildResourceDatabase has run at world load. The rule being asserted is a config rule, but what
//! it matches is vanilla data, and vanilla data is exactly what went wrong - Arma files M2/NSV gun
//! and tripod parts, mortar barrels, base plates and bipods, sandbags and repair/rearming/fuel kits
//! as m_eItemType EQUIPMENT in m_eItemMode SUPPORT_STATION. The electronics shop's rule asks for
//! EQUIPMENT with no m_sFind, and SCR_EArsenalItemMode.DEFAULT is treated as a WILDCARD rather than
//! as a value to match, so it swept every one of them onto a civilian shelf at civilian prices.
//!
//! Two claims, and the second is what stops the first from being satisfied by an empty shop:
//!  1. NO SUPPORT_STATION entry in the catalog is eligible at SHOP_ELECTRONIC. Asserted over every
//!     such entry, not a sample - the set is small (~31 across all factions) and IsSoldAtShop is
//!     only run per entry, not per rule per entry.
//!  2. At least one ordinary EQUIPMENT entry IS still eligible there. Deleting the rule, or
//!     narrowing it until the shop sells nothing, would satisfy claim 1 and is not the fix.
//!
//! Deliberately asserted through IsSoldAtShop (the ELIGIBILITY predicate) rather than through a
//! shop's rolled stock: stock is a random draw per shop, so an empty result would be ambiguous
//! between "the rule excludes it" and "the dice did not pick it". Eligibility also drives the sell
//! browser, which is the other half of the bug - a tripod could be SOLD to an electronics shop too.
//!
//! READ-ONLY. Mutates no campaign state.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_ShopCivilianStock : SCR_AutotestCaseBase
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

		// Mode is passed EXPLICITLY here, so this lookup is the wildcard-free path and is unaffected by
		// the m_bIncludeSupportStationItems filter under test. It is how the case gets its subjects.
		array<SCR_EntityCatalogEntry> deployables();
		economy.FindInventoryItems(SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.SUPPORT_STATION, "", deployables);

		if (deployables.IsEmpty())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("No EQUIPMENT/SUPPORT_STATION entries found in the entity catalog within %1 polls - the catalog is empty, so this case has no subjects and would pass vacuously",
					m_iPolls.ToString());
				return true;
			}

			return false; // keep polling - the catalog is not built yet
		}

		string problem = CheckNoDeployablesAtElectronics(economy, deployables);
		if (problem != "")
		{
			SetFailure(problem);
			return true;
		}

		problem = CheckElectronicsStillSellsSomething(economy);
		if (problem != "")
		{
			SetFailure(problem);
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 1 - no deployable part is eligible at a civilian electronics shop.
	//! \param[in] economy The economy manager under test.
	//! \param[in] deployables Every EQUIPMENT/SUPPORT_STATION entry in the catalog.
	//! \return An empty string when none is eligible, or a description of the first offender.
	protected string CheckNoDeployablesAtElectronics(OVT_EconomyManagerComponent economy, array<SCR_EntityCatalogEntry> deployables)
	{
		int checked = 0;

		foreach (SCR_EntityCatalogEntry entry : deployables)
		{
			if (!entry)
				continue;

			ResourceName res = entry.GetPrefab();
			if (res == "")
				continue;

			checked += 1;

			if (economy.IsSoldAtShop(res, OVT_ShopType.SHOP_ELECTRONIC))
			{
				return string.Format("'%1' is an EQUIPMENT/SUPPORT_STATION deployable part and IsSoldAtShop() says a civilian electronics shop deals in it (%2 entries checked) - the shop rule is sweeping crew-served parts in through the DEFAULT mode wildcard",
					res, checked.ToString());
			}
		}

		if (checked < 1)
			return "No EQUIPMENT/SUPPORT_STATION entry resolved to a prefab, so nothing was checked";

		PrintFormat("Civilian stock: %1 deployable part(s) checked, none eligible at SHOP_ELECTRONIC", checked.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 2 - the electronics rule still matches ordinary gear, so claim 1 was not won by deletion.
	//! \param[in] economy The economy manager under test.
	//! \return An empty string when at least one ordinary EQUIPMENT entry is eligible, or a failure.
	protected string CheckElectronicsStillSellsSomething(OVT_EconomyManagerComponent economy)
	{
		array<SCR_EntityCatalogEntry> equipment();
		economy.FindInventoryItems(SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.DEFAULT, "", equipment);

		int sold = 0;
		ResourceName firstSold;

		foreach (SCR_EntityCatalogEntry entry : equipment)
		{
			if (!entry)
				continue;

			ResourceName res = entry.GetPrefab();
			if (res == "")
				continue;

			if (!economy.IsSoldAtShop(res, OVT_ShopType.SHOP_ELECTRONIC))
				continue;

			if (sold == 0)
				firstSold = res;

			sold += 1;
		}

		if (sold < 1)
		{
			return string.Format("No EQUIPMENT entry at all is eligible at SHOP_ELECTRONIC out of %1 in the catalog - the deployable exclusion has emptied the shop instead of filtering it",
				equipment.Count().ToString());
		}

		PrintFormat("Civilian stock: %1 ordinary EQUIPMENT entry(ies) still eligible at SHOP_ELECTRONIC, first '%2'",
			sold.ToString(), firstSold);

		return "";
	}
}
