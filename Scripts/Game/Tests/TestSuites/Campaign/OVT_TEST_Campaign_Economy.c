//------------------------------------------------------------------------------------------------
//! TIER C cases - the economy after the campaign has started.
//!
//! Both cases here need a RUNNING campaign and cannot be moved down a tier: shop inventory is
//! filled by OVT_EconomyManagerComponent.PostGameStart(), and the income calculators read town
//! state that only the campaign start establishes.
//!
//! Economy anchors are read from the difficulty config at runtime, never hardcoded. The test world
//! runs on the 'Test World' preset (100000 starting cash, 200 starting resources) and inherits the
//! rest of its economy values from OVT_DifficultySettings' own attribute defaults; hardcoding
//! either would make a deliberate config change look like a regression.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Shops are stocked by the campaign start.
//!
//! Shop ENTITIES are found at world load; their INVENTORIES are filled one frame after
//! DoStartGame() by OVT_EconomyManagerComponent.PostGameStart() -> InitShopInventory(). Measured on
//! 1.7.0.54: 5 shops registered, 0 stock entries at the moment the start returns, 286 entries
//! within 604 ms (findings.md 1.4). This case therefore polls, and asserts on ">= 1 shop with
//! stock" rather than on a stock count - a magic number here would be a content assertion, not a
//! behaviour assertion.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_Economy_ShopsInitialise : SCR_AutotestCaseBase
{
	//! Diagnostic backstop, not a retry budget: stock is expected inside ~604 ms.
	static const int MAX_POLLS = 600;

	//! Polls spent waiting for the first stocked shop.
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

		array<RplId> shops = economy.GetAllShops();
		if (!shops || shops.Count() < 1)
		{
			SetFailure("No shops are registered with the economy manager, so none can be stocked");
			return true;
		}

		int stockedShops = 0;
		int stockEntries = 0;
		int resolvedShops = 0;

		foreach (RplId shopId : shops)
		{
			OVT_ShopComponent shop = economy.GetShopByRplId(shopId);
			if (!shop)
				continue;

			resolvedShops += 1;

			if (!shop.m_aInventory)
				continue;

			if (shop.m_aInventory.Count() > 0)
			{
				stockedShops += 1;
				stockEntries += shop.m_aInventory.Count();
			}
		}

		if (resolvedShops < 1)
		{
			SetFailure("%1 shop id(s) are registered but none of them resolves to a shop component", shops.Count().ToString());
			return true;
		}

		if (stockedShops > 0)
		{
			PrintFormat("Shops initialised after %1 poll(s): %2 of %3 registered shops hold stock",
				m_iPolls.ToString(), stockedShops.ToString(), shops.Count().ToString());
			PrintFormat("Total stock entries across all shops: %1", stockEntries.ToString());
			return true;
		}

		m_iPolls += 1;
		if (m_iPolls > MAX_POLLS)
		{
			SetFailure("No shop was stocked within %1 polls: %2 shop id(s) registered, %3 resolved to a component, none holds a single inventory entry",
				m_iPolls.ToString(), shops.Count().ToString(), resolvedShops.ToString());
			return true;
		}

		return false; // keep polling
	}
}

//------------------------------------------------------------------------------------------------
//! Tax and donation income match the town state the case sets up.
//!
//! The two calculators are the campaign's whole passive economy, and both are pure functions of
//! town state plus the difficulty config:
//!   tax      = taxIncome * population * (stability / 100), summed over towns NOT held by the
//!              occupying faction;
//!   donation = donationIncome * supporters, doubled while stability is above 75, summed over all
//!              towns.
//!
//! The case drives the town through three states and checks the income at each: occupied with no
//! supporters (both incomes zero), liberated (tax appears), and liberated with supporters
//! (donations appear). Expected values are DERIVED from the live town records and the difficulty
//! config, so a config or content change moves both sides rather than silently invalidating the
//! case.
//!
//! Three claims are independent of that derivation, and they are the ones that would survive a
//! wholesale rewrite of the formulas:
//!   - an occupied town is not taxed at all, no matter how big or stable it is;
//!   - liberating a populated town produces strictly positive tax income;
//!   - donation income is LINEAR in supporters - the same increment of supporters adds the same
//!     income twice in a row.
//!
//! The case restores the town's faction and support before it reports, on the failure path as well
//! as the success path, so a red run cannot cascade into the rest of the suite.
//!
//! NOT ASSERTED: the "stability above 75 doubles donations" branch. Lowering stability below 75
//! needs several stability modifiers whose effects come from a content config, which would make the
//! case a content assertion. The branch is honoured in the derived expectation, which does read the
//! town's live stability.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_Economy_IncomeMatchesTownState : SCR_AutotestCaseBase
{
	//! Supporters added per step. Deliberately not 1, so a formula that ignores the count still
	//! fails, and small enough that the town cannot run out of civilians.
	static const int SUPPORT_DELTA = 8;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!economy || !towns || !config)
		{
			SetFailure("A manager needed by this case is null (economy, towns or config)");
			return true;
		}

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
		{
			SetFailure("OVT_Global.GetDifficulty() is null - the income formulas have no coefficients");
			return true;
		}

		array<ref OVT_TownData> townList = towns.GetTowns();
		if (!townList || townList.Count() < 1)
		{
			SetFailure("No towns are registered, so there is no income to calculate");
			return true;
		}

		OVT_TownData town = townList[0];
		if (!town)
		{
			SetFailure("Town 0 is null");
			return true;
		}

		int originalFaction = town.faction;
		int originalSupport = town.support;

		string problem = RunIncomeChecks(economy, towns, config, difficulty, town);

		// Always restore, on both paths, before reporting.
		towns.ResetSupport(town);
		if (originalSupport > 0)
			towns.AddSupport(town.location, originalSupport);

		towns.ChangeTownControl(town, originalFaction);

		if (problem != "")
		{
			SetFailure(problem);
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Drives the town through its three states and checks the income at each.
	//! Returns rather than asserting so that the caller can restore town state on every path.
	//! \param[in] economy The economy manager under test.
	//! \param[in] towns The town manager used to mutate town state.
	//! \param[in] config The Overthrow config, for the faction indices.
	//! \param[in] difficulty The difficulty preset supplying the income coefficients.
	//! \param[in] town The town this case mutates.
	//! \return An empty string when every check passed, or a description of the first failure.
	protected string RunIncomeChecks(OVT_EconomyManagerComponent economy, OVT_TownManagerComponent towns, OVT_OverthrowConfigComponent config, OVT_DifficultySettings difficulty, OVT_TownData town)
	{
		int occupyingFaction = config.GetOccupyingFactionIndex();
		int playerFaction = config.GetPlayerFactionIndex();

		if (occupyingFaction < 0 || playerFaction < 0 || occupyingFaction == playerFaction)
		{
			return string.Format("Faction indices are unusable: occupying %1, player %2", occupyingFaction, playerFaction);
		}

		if (town.faction != occupyingFaction)
		{
			return string.Format("The campaign start left town 0 under faction %1, expected the occupying faction %2", town.faction, occupyingFaction);
		}

		if (town.population < 1)
		{
			return string.Format("Town 0 has population %1, so no income assertion would mean anything", town.population);
		}

		// State 1 - occupied, no supporters.
		towns.ResetSupport(town);

		int taxWhileOccupied = economy.GetTaxIncome();
		if (taxWhileOccupied != 0)
		{
			return string.Format("GetTaxIncome() returned %1 while every town is held by the occupying faction, expected 0", taxWhileOccupied);
		}

		int donationWithoutSupport = economy.GetDonationIncome();
		if (donationWithoutSupport != 0)
		{
			return string.Format("GetDonationIncome() returned %1 with no supporters anywhere, expected 0", donationWithoutSupport);
		}

		// State 2 - liberated, still no supporters. Tax appears, donations do not.
		towns.ChangeTownControl(town, playerFaction);

		int expectedTax = ExpectedTaxIncome(towns, occupyingFaction, difficulty);
		int taxWhenLiberated = economy.GetTaxIncome();
		if (taxWhenLiberated != expectedTax)
		{
			return string.Format("GetTaxIncome() returned %1 for a liberated town, expected %2 (taxIncome %3 x population %4 x stability/100)",
				taxWhenLiberated, expectedTax, difficulty.taxIncome, town.population);
		}

		if (taxWhenLiberated <= 0)
		{
			return string.Format("Liberating a town of %1 civilians at %2 stability produced no tax income at all", town.population, town.stability);
		}

		if (economy.GetDonationIncome() != 0)
		{
			return string.Format("GetDonationIncome() returned %1 for a liberated town with no supporters, expected 0", economy.GetDonationIncome());
		}

		// State 3 - liberated, with supporters. Donations appear and scale with the supporters.
		towns.AddSupport(town.location, SUPPORT_DELTA);

		int expectedDonation = ExpectedDonationIncome(towns, difficulty);
		int donationWithSupport = economy.GetDonationIncome();
		if (donationWithSupport != expectedDonation)
		{
			return string.Format("GetDonationIncome() returned %1 with %2 supporters, expected %3 (donationIncome %4 per supporter, doubled above 75 stability)",
				donationWithSupport, town.support, expectedDonation, difficulty.donationIncome);
		}

		if (donationWithSupport <= donationWithoutSupport)
		{
			return string.Format("Adding %1 supporters did not raise donation income: it was %2 and is now %3",
				SUPPORT_DELTA, donationWithoutSupport, donationWithSupport);
		}

		// Linearity: the same increment of supporters must add the same income again.
		towns.AddSupport(town.location, SUPPORT_DELTA);

		int donationWithDoubleSupport = economy.GetDonationIncome();
		int firstIncrement = donationWithSupport - donationWithoutSupport;
		int secondIncrement = donationWithDoubleSupport - donationWithSupport;
		if (firstIncrement != secondIncrement)
		{
			return string.Format("Donation income is not linear in supporters: the first %1 supporters added %2, the next %1 added %3",
				SUPPORT_DELTA, firstIncrement, secondIncrement);
		}

		// Tax does not move with supporters - it is a function of population and stability only.
		if (economy.GetTaxIncome() != taxWhenLiberated)
		{
			return string.Format("GetTaxIncome() moved from %1 to %2 when supporters were added, and it is supposed to depend only on population and stability",
				taxWhenLiberated, economy.GetTaxIncome());
		}

		// State 4 - stability below its maximum.
		//
		// This is the assertion that actually distinguishes a FRACTIONAL stability factor from a
		// truncated one, and it is the reason this case does more than restate a formula: at 100
		// stability the two behave identically, which is exactly why the plan's suspicion about
		// this expression could not be settled by reading it (findings.md, Phase 5). Stability is
		// lowered through the only public seam that moves it - a stability modifier - and restored
		// immediately afterwards.
		OVT_TownModifierSystem stabilitySystem = towns.GetModifierSystem(OVT_TownStabilityModifierSystem);
		if (!stabilitySystem || !stabilitySystem.m_Config || !stabilitySystem.m_Config.m_aModifiers)
		{
			return "The town manager has no stability modifier system with a loaded config, so stability cannot be lowered";
		}

		int modifierIndex = FindNegativeModifierIndex(stabilitySystem);
		if (modifierIndex < 0)
		{
			return "No stability modifier has a negative base effect, so stability cannot be moved down from its maximum";
		}

		int townId = towns.GetTownID(town);
		int stabilityBefore = town.stability;

		if (!towns.TryAddStabilityModifier(townId, modifierIndex))
		{
			return string.Format("TryAddStabilityModifier(%1) refused to add a modifier to town %2", modifierIndex, townId);
		}

		string stabilityProblem = CheckTaxAtLoweredStability(economy, towns, occupyingFaction, difficulty, town, stabilityBefore, taxWhenLiberated);
		towns.RemoveStabilityModifier(townId, modifierIndex);

		if (stabilityProblem != "")
			return stabilityProblem;

		PrintFormat("Income: occupied tax %1, liberated tax %2, donations with %3 supporters",
			taxWhileOccupied.ToString(), taxWhenLiberated.ToString(), town.support.ToString());
		PrintFormat("Income: donations 0 -> %1 -> %2, increment %3 per step",
			donationWithSupport.ToString(), donationWithDoubleSupport.ToString(), firstIncrement.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Checks tax income against a town whose stability has just been lowered below its maximum.
	//! Split out so the caller can remove the stability modifier on every path.
	//! \param[in] economy The economy manager under test.
	//! \param[in] towns The town manager holding the records.
	//! \param[in] occupyingFaction Faction index whose towns are tax-exempt.
	//! \param[in] difficulty The difficulty preset supplying taxIncome.
	//! \param[in] town The town whose stability was lowered.
	//! \param[in] stabilityBefore Stability before the modifier was added.
	//! \param[in] taxAtFullStability Tax income measured before the modifier was added.
	//! \return An empty string when every check passed, or a description of the first failure.
	protected string CheckTaxAtLoweredStability(OVT_EconomyManagerComponent economy, OVT_TownManagerComponent towns, int occupyingFaction, OVT_DifficultySettings difficulty, OVT_TownData town, int stabilityBefore, int taxAtFullStability)
	{
		if (town.stability >= stabilityBefore)
		{
			return string.Format("A negative stability modifier did not lower stability: it was %1 and is %2", stabilityBefore, town.stability);
		}

		if (town.stability <= 0)
		{
			return string.Format("Stability fell to %1, which would make the tax assertion meaningless", town.stability);
		}

		int expectedScaledTax = ExpectedTaxIncome(towns, occupyingFaction, difficulty);
		int scaledTax = economy.GetTaxIncome();
		if (scaledTax != expectedScaledTax)
		{
			return string.Format("GetTaxIncome() returned %1 at %2 stability, expected %3", scaledTax, town.stability, expectedScaledTax);
		}

		if (scaledTax >= taxAtFullStability)
		{
			return string.Format("Lowering stability from %1 to %2 did not lower tax income: it was %3 and is %4",
				stabilityBefore, town.stability, taxAtFullStability, scaledTax);
		}

		// The claim that a truncated stability factor could not satisfy: below full stability the
		// town still pays, proportionally, rather than dropping straight to nothing.
		if (scaledTax <= 0)
		{
			return string.Format("Tax income collapsed to %1 at %2 stability - the stability factor is being truncated to zero rather than scaled",
				scaledTax, town.stability);
		}

		PrintFormat("Income: tax at %1 stability is %2, down from %3 at full stability",
			town.stability.ToString(), scaledTax.ToString(), taxAtFullStability.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the first stability modifier with a negative base effect. Stability starts at its
	//! configured maximum, so only a negative modifier can move it observably.
	//! \param[in] system The stability modifier system holding the config.
	//! \return Index into the config's modifier list, or -1 when none has a negative effect.
	protected int FindNegativeModifierIndex(OVT_TownModifierSystem system)
	{
		foreach (int i, OVT_ModifierConfig config : system.m_Config.m_aModifiers)
		{
			if (config && config.baseEffect < 0)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Recomputes the expected tax income from live town records and the difficulty config, using
	//! the documented formula: towns held by the occupying faction are exempt, and the rest pay
	//! taxIncome per civilian, scaled by stability.
	//! \param[in] towns The town manager holding the records.
	//! \param[in] occupyingFaction Faction index whose towns are exempt.
	//! \param[in] difficulty The difficulty preset supplying taxIncome.
	//! \return The expected total.
	protected int ExpectedTaxIncome(OVT_TownManagerComponent towns, int occupyingFaction, OVT_DifficultySettings difficulty)
	{
		int expected = 0;

		foreach (OVT_TownData town : towns.GetTowns())
		{
			if (!town)
				continue;

			if (town.faction == occupyingFaction)
				continue;

			float stabilityFactor = town.stability / 100;
			expected += (int)Math.Round(difficulty.taxIncome * town.population * stabilityFactor);
		}

		return expected;
	}

	//------------------------------------------------------------------------------------------------
	//! Recomputes the expected donation income from live town records and the difficulty config:
	//! donationIncome per supporter, doubled while the town is above 75 stability.
	//! \param[in] towns The town manager holding the records.
	//! \param[in] difficulty The difficulty preset supplying donationIncome.
	//! \return The expected total.
	protected int ExpectedDonationIncome(OVT_TownManagerComponent towns, OVT_DifficultySettings difficulty)
	{
		int expected = 0;

		foreach (OVT_TownData town : towns.GetTowns())
		{
			if (!town)
				continue;

			int increase = difficulty.donationIncome * town.support;
			if (town.stability > 75)
				increase *= 2;

			expected += increase;
		}

		return expected;
	}
}

//------------------------------------------------------------------------------------------------
//! The town-stock scarcity term in GetSellPrice() is a gradient, not a step (pins BUG-105).
//!
//! The price model is documented as continuous: the emptier the town, the more a shop pays, up to
//! +10% of base. Before the fix, stock_level / max_stock was INTEGER division, so every stock
//! level strictly between empty and full priced identically (+10%) and the term only ever moved
//! at the ceiling.
//!
//! The case seeds a synthetic resource into one registered shop at three town-stock levels -
//! nearly empty, half full, nearly full - and asserts the sell price at the shop's own position
//! is STRICTLY DECREASING across them. Under integer division all three reads are equal, which is
//! exactly how this case was proven red. The port-distance term is identical across the three
//! reads (same position), so it cancels out of the comparisons.
//!
//! The synthetic id follows OVT_TEST_Init_Economy_PriceAndDemandSeams' convention (far outside
//! m_aResources, so no real item's price is disturbed). The shop's inventory entry is removed on
//! every path, so no later case can see phantom stock.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_Economy_ScarcityPriceGradient : SCR_AutotestCaseBase
{
	//! Synthetic resource id, far outside m_aResources (same convention as the Init price case).
	static const int PROBE_ITEM_ID = 900101;

	//! Base price seeded through SetPrice(). Large enough that the gap between any two of the
	//! three probe points is a double-digit number of dollars, far clear of Math.Round noise.
	static const int PROBE_PRICE = 1000;

	//! Demand seeded through SetDemand() so GetTownMaxStock is comfortably above the probe
	//! points for any populated test-world town.
	static const int PROBE_DEMAND = 20;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!economy || !towns)
		{
			SetFailure("A manager needed by this case is null (economy or towns)");
			return true;
		}

		economy.SetPrice(PROBE_ITEM_ID, PROBE_PRICE);
		economy.SetDemand(PROBE_ITEM_ID, PROBE_DEMAND);

		// GetSellPrice resolves the town by POSITION while GetTownStock reads it by REGISTRATION,
		// so the case needs a shop for which the two agree. The write-1-read-1 probe below proves
		// agreement through the same seams the price read will use.
		OVT_ShopComponent shop;
		int townId = -1;
		vector shopPos;

		array<RplId> shops = economy.GetAllShops();
		if (shops)
		{
			foreach (RplId shopId : shops)
			{
				OVT_ShopComponent candidate = economy.GetShopByRplId(shopId);
				if (!candidate || !candidate.m_aInventory || !candidate.GetOwner())
					continue;

				vector pos = candidate.GetOwner().GetOrigin();
				OVT_TownData nearest = towns.GetNearestTown(pos);
				if (!nearest)
					continue;

				int nearestId = towns.GetTownID(nearest);

				candidate.m_aInventory[PROBE_ITEM_ID] = 1;
				bool agrees = economy.GetTownStock(nearestId, PROBE_ITEM_ID) == 1;
				candidate.m_aInventory.Remove(PROBE_ITEM_ID);

				if (!agrees)
					continue;

				shop = candidate;
				townId = nearestId;
				shopPos = pos;
				break;
			}
		}

		if (!shop)
		{
			SetFailure("No registered shop is visible to its own nearest town's stock count, so the scarcity term cannot be probed");
			return true;
		}

		int maxStock = economy.GetTownMaxStock(townId, PROBE_ITEM_ID);
		if (maxStock < 4)
		{
			SetFailure(string.Format("GetTownMaxStock() is %1 for the probe item at demand %2, too small to hold three distinct interior stock levels", maxStock, PROBE_DEMAND));
			return true;
		}

		string problem = CheckGradient(economy, shop, townId, shopPos, maxStock);

		// Always remove the synthetic stock, on both paths.
		shop.m_aInventory.Remove(PROBE_ITEM_ID);

		if (problem != "")
		{
			SetFailure(problem);
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the sell price at three interior stock levels and checks it strictly decreases.
	//! Returns rather than asserting so the caller can clean up the seeded stock on every path.
	//! \param[in] economy The economy manager under test.
	//! \param[in] shop The shop holding the synthetic stock.
	//! \param[in] townId The town both the stock count and the price read resolve to.
	//! \param[in] shopPos The shop's position, passed to GetSellPrice.
	//! \param[in] maxStock The town's max stock for the probe item.
	//! \return An empty string when the gradient holds, or a description of the first failure.
	protected string CheckGradient(OVT_EconomyManagerComponent economy, OVT_ShopComponent shop, int townId, vector shopPos, int maxStock)
	{
		int nearlyEmpty = 1;
		int halfFull = maxStock / 2;
		int nearlyFull = maxStock - 1;

		shop.m_aInventory[PROBE_ITEM_ID] = nearlyEmpty;
		int priceNearlyEmpty = economy.GetSellPrice(PROBE_ITEM_ID, shopPos);

		shop.m_aInventory[PROBE_ITEM_ID] = halfFull;
		int priceHalfFull = economy.GetSellPrice(PROBE_ITEM_ID, shopPos);

		shop.m_aInventory[PROBE_ITEM_ID] = nearlyFull;
		int priceNearlyFull = economy.GetSellPrice(PROBE_ITEM_ID, shopPos);

		if (priceNearlyEmpty <= priceHalfFull)
		{
			return string.Format("Sell price did not fall as the town filled from %1 to %2 of %3 stock: %4 then %5 - the scarcity term is a step, not a gradient",
				nearlyEmpty, halfFull, maxStock, priceNearlyEmpty, priceHalfFull);
		}

		if (priceHalfFull <= priceNearlyFull)
		{
			return string.Format("Sell price did not fall as the town filled from %1 to %2 of %3 stock: %4 then %5 - the scarcity term is a step, not a gradient",
				halfFull, nearlyFull, maxStock, priceHalfFull, priceNearlyFull);
		}

		Print(string.Format("Scarcity gradient at max stock %1: price %2 -> %3 -> %4 across stock %5/%6/%7",
			maxStock, priceNearlyEmpty, priceHalfFull, priceNearlyFull, nearlyEmpty, halfFull, nearlyFull));

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! GetSellPriceAtOffset prices hypothetical stock identically to physical stock (pins BUG-117).
//!
//! The offset seam is what lets ExecuteSell price a bulk sale marginally: unit i of a resource is
//! priced with offset i, so a dump rides the scarcity curve down instead of collecting the
//! pre-sale price for every unit. The seam's whole contract is that "current stock s, offset k"
//! and "current stock s+k, offset 0" are THE SAME PRICE - this case asserts exactly that, plus
//! that offset 0 is the plain GetSellPrice.
//!
//! Shop selection, the synthetic id convention and the cleanup discipline are the same as
//! OVT_TEST_Campaign_Economy_ScarcityPriceGradient's.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_Economy_SellPriceStockOffset : SCR_AutotestCaseBase
{
	//! Synthetic resource id, far outside m_aResources.
	static const int PROBE_ITEM_ID = 900102;

	//! Base price seeded through SetPrice().
	static const int PROBE_PRICE = 1000;

	//! Demand seeded through SetDemand() so GetTownMaxStock leaves room for a wide offset.
	static const int PROBE_DEMAND = 20;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!economy || !towns)
		{
			SetFailure("A manager needed by this case is null (economy or towns)");
			return true;
		}

		economy.SetPrice(PROBE_ITEM_ID, PROBE_PRICE);
		economy.SetDemand(PROBE_ITEM_ID, PROBE_DEMAND);

		OVT_ShopComponent shop;
		int townId = -1;
		vector shopPos;

		array<RplId> shops = economy.GetAllShops();
		if (shops)
		{
			foreach (RplId shopId : shops)
			{
				OVT_ShopComponent candidate = economy.GetShopByRplId(shopId);
				if (!candidate || !candidate.m_aInventory || !candidate.GetOwner())
					continue;

				vector pos = candidate.GetOwner().GetOrigin();
				OVT_TownData nearest = towns.GetNearestTown(pos);
				if (!nearest)
					continue;

				int nearestId = towns.GetTownID(nearest);

				candidate.m_aInventory[PROBE_ITEM_ID] = 1;
				bool agrees = economy.GetTownStock(nearestId, PROBE_ITEM_ID) == 1;
				candidate.m_aInventory.Remove(PROBE_ITEM_ID);

				if (!agrees)
					continue;

				shop = candidate;
				townId = nearestId;
				shopPos = pos;
				break;
			}
		}

		if (!shop)
		{
			SetFailure("No registered shop is visible to its own nearest town's stock count, so the offset seam cannot be probed");
			return true;
		}

		int maxStock = economy.GetTownMaxStock(townId, PROBE_ITEM_ID);
		if (maxStock < 4)
		{
			SetFailure(string.Format("GetTownMaxStock() is %1 for the probe item at demand %2, too small to leave room for an offset", maxStock, PROBE_DEMAND));
			return true;
		}

		int low = 1;
		int high = maxStock - 1;

		// Read at low physical stock: the plain price, the offset-0 price and the price offset up
		// to the high level.
		shop.m_aInventory[PROBE_ITEM_ID] = low;
		int plainAtLow = economy.GetSellPrice(PROBE_ITEM_ID, shopPos);
		int offsetZeroAtLow = economy.GetSellPriceAtOffset(PROBE_ITEM_ID, shopPos, 0);
		int offsetToHigh = economy.GetSellPriceAtOffset(PROBE_ITEM_ID, shopPos, high - low);

		// Read at high physical stock: the plain price the offset read must have predicted.
		shop.m_aInventory[PROBE_ITEM_ID] = high;
		int plainAtHigh = economy.GetSellPrice(PROBE_ITEM_ID, shopPos);

		// Always remove the synthetic stock before asserting.
		shop.m_aInventory.Remove(PROBE_ITEM_ID);

		if (offsetZeroAtLow != plainAtLow)
		{
			SetFailure(string.Format("Offset 0 priced %1 but the plain sell price is %2 - the two paths have drifted", offsetZeroAtLow, plainAtLow));
			return true;
		}

		if (offsetToHigh != plainAtHigh)
		{
			SetFailure(string.Format("Offset %1 over stock %2 priced %3, but physically stocking %4 prices %5 - hypothetical and physical stock disagree",
				high - low, low, offsetToHigh, high, plainAtHigh));
			return true;
		}

		if (offsetToHigh >= plainAtLow)
		{
			SetFailure(string.Format("Offsetting stock from %1 to %2 did not lower the price (%3 -> %4), so a bulk sale would not ride the scarcity curve",
				low, high, plainAtLow, offsetToHigh));
			return true;
		}

		Print(string.Format("Offset seam at max stock %1: plain %2 at stock %3, offset(%4) %5 == plain %6 at stock %7",
			maxStock, plainAtLow, low, high - low, offsetToHigh, plainAtHigh, high));

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The town buy cap flips exactly at the configured multiple of max stock, and units accepted
//! earlier in the same bulk sale count against it (pins BUG-117's knob 4).
//!
//! CanTownAbsorbStock is the gate both ExecuteSell (per unit, with the collated count as
//! extraUnits) and the sell browser's grey-out (extra 0) stand on. Three claims:
//!   - one unit below the cap the town still absorbs;
//!   - at the cap it refuses;
//!   - extraUnits count: with physical stock at max, an extra of max-1 is still absorbed and an
//!     extra of max is refused - so a single Sell All cannot blow through the cap before the
//!     stock broadcast lands.
//! The expected boundary is DERIVED from TOWN_STOCK_BUY_CAP_MULTIPLIER, so retuning the cap moves
//! both sides rather than silently invalidating the case.
//!
//! Shop selection, the synthetic id convention and the cleanup discipline are the same as the two
//! cases above.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_Economy_TownBuyCap : SCR_AutotestCaseBase
{
	//! Synthetic resource id, far outside m_aResources.
	static const int PROBE_ITEM_ID = 900103;

	//! Demand seeded through SetDemand() so GetTownMaxStock is comfortably large.
	static const int PROBE_DEMAND = 20;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!economy || !towns)
		{
			SetFailure("A manager needed by this case is null (economy or towns)");
			return true;
		}

		economy.SetDemand(PROBE_ITEM_ID, PROBE_DEMAND);

		OVT_ShopComponent shop;
		int townId = -1;

		array<RplId> shops = economy.GetAllShops();
		if (shops)
		{
			foreach (RplId shopId : shops)
			{
				OVT_ShopComponent candidate = economy.GetShopByRplId(shopId);
				if (!candidate || !candidate.m_aInventory || !candidate.GetOwner())
					continue;

				OVT_TownData nearest = towns.GetNearestTown(candidate.GetOwner().GetOrigin());
				if (!nearest)
					continue;

				int nearestId = towns.GetTownID(nearest);

				candidate.m_aInventory[PROBE_ITEM_ID] = 1;
				bool agrees = economy.GetTownStock(nearestId, PROBE_ITEM_ID) == 1;
				candidate.m_aInventory.Remove(PROBE_ITEM_ID);

				if (!agrees)
					continue;

				shop = candidate;
				townId = nearestId;
				break;
			}
		}

		if (!shop)
		{
			SetFailure("No registered shop is visible to its own nearest town's stock count, so the buy cap cannot be probed");
			return true;
		}

		int maxStock = economy.GetTownMaxStock(townId, PROBE_ITEM_ID);
		if (maxStock < 2)
		{
			SetFailure(string.Format("GetTownMaxStock() is %1 for the probe item at demand %2, too small to place stock below the cap", maxStock, PROBE_DEMAND));
			return true;
		}

		int cap = OVT_EconomyManagerComponent.TOWN_STOCK_BUY_CAP_MULTIPLIER * maxStock;

		// One below the cap: still absorbed.
		shop.m_aInventory[PROBE_ITEM_ID] = cap - 1;
		bool absorbsBelowCap = economy.CanTownAbsorbStock(townId, PROBE_ITEM_ID, 0);

		// At the cap: refused.
		shop.m_aInventory[PROBE_ITEM_ID] = cap;
		bool absorbsAtCap = economy.CanTownAbsorbStock(townId, PROBE_ITEM_ID, 0);

		// extraUnits count against the cap exactly like physical stock.
		shop.m_aInventory[PROBE_ITEM_ID] = maxStock;
		bool absorbsWithExtraBelow = economy.CanTownAbsorbStock(townId, PROBE_ITEM_ID, cap - maxStock - 1);
		bool absorbsWithExtraAt = economy.CanTownAbsorbStock(townId, PROBE_ITEM_ID, cap - maxStock);

		// Always remove the synthetic stock before asserting.
		shop.m_aInventory.Remove(PROBE_ITEM_ID);

		if (!absorbsBelowCap)
		{
			SetFailure(string.Format("Town refused a unit at stock %1 with the cap at %2 - the cap fires a unit early", cap - 1, cap));
			return true;
		}

		if (absorbsAtCap)
		{
			SetFailure(string.Format("Town absorbed a unit at stock %1 with the cap at %2 - the cap never fires", cap, cap));
			return true;
		}

		if (!absorbsWithExtraBelow)
		{
			SetFailure(string.Format("Town refused with stock %1 and %2 extra units against a cap of %3 - extras are over-counted", maxStock, cap - maxStock - 1, cap));
			return true;
		}

		if (absorbsWithExtraAt)
		{
			SetFailure(string.Format("Town absorbed with stock %1 and %2 extra units against a cap of %3 - extras are not counted, so one Sell All can blow through the cap", maxStock, cap - maxStock, cap));
			return true;
		}

		Print(string.Format("Buy cap at %1 (max stock %2 x %3): boundary and extra-unit accounting both flip where expected",
			cap, maxStock, OVT_EconomyManagerComponent.TOWN_STOCK_BUY_CAP_MULTIPLIER));

		return true;
	}
}
