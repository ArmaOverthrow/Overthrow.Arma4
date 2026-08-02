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
			SetResultFailure("No shops are registered with the economy manager, so none can be stocked");
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
			SetResultFailure("%1 shop id(s) are registered but none of them resolves to a shop component", shops.Count().ToString());
			return true;
		}

		if (stockedShops > 0)
		{
			PrintFormat("Shops initialised after %1 poll(s): %2 of %3 registered shops hold stock",
				m_iPolls.ToString(), stockedShops.ToString(), shops.Count().ToString());
			PrintFormat("Total stock entries across all shops: %1", stockEntries.ToString());
			SetResultSuccess();
			return true;
		}

		m_iPolls += 1;
		if (m_iPolls > MAX_POLLS)
		{
			SetResultFailure("No shop was stocked within %1 polls: %2 shop id(s) registered, %3 resolved to a component, none holds a single inventory entry",
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
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!economy || !towns || !config)
		{
			SetResultFailure("A manager needed by this case is null (economy, towns or config)");
			return true;
		}

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
		{
			SetResultFailure("OVT_Global.GetDifficulty() is null - the income formulas have no coefficients");
			return true;
		}

		array<ref OVT_TownData> townList = towns.GetTowns();
		if (!townList || townList.Count() < 1)
		{
			SetResultFailure("No towns are registered, so there is no income to calculate");
			return true;
		}

		OVT_TownData town = townList[0];
		if (!town)
		{
			SetResultFailure("Town 0 is null");
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
			SetResultFailure(problem);
			return true;
		}

		SetResultSuccess();
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
