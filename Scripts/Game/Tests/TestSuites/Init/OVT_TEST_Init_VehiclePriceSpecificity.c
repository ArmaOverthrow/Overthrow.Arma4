//------------------------------------------------------------------------------------------------
//! TIER B - an armed variant keeps its OWN price config, and a broader entry cannot take it over.
//!
//! THE BUG. vehiclePrices.conf matches on a substring of the prefab path, and BuildResourceDatabase
//! used to let the LAST matching entry win. The file is authored most-specific-first (and sorted by
//! descending cost), so every armed variant was silently overwritten by a broader entry further down:
//!
//!   M151A2_M2HB.et      matched "M151A2_M2HB" (12500, illegal) then "M151A2"  (2400, legal)
//!   M1025_armed_M2HB.et matched "M1025_armed" (25998, illegal) then "M998"    (3500, legal)
//!   UAZ469_PKM.et       matched "UAZ469_PKM"  (25000, illegal) then "UAZ469"  (1500, legal)
//!
//! The visible symptom was machine-gun jeeps for sale in civilian vehicle shops at civilian prices,
//! because the civilian shop filter is exactly the legality flag written by that loop. Two things
//! were wrong and both are asserted here: the match rule (most specific wins now, whatever the order
//! of the list) and "M998", which is a FOLDER name that M1025 and M997 also live in.
//!
//! WHY THIS TIER. The resource database is built in OVT_EconomyManagerComponent.AfterInit at world
//! load, so it exists without a started campaign. Nothing here mutates it - the case only reads back
//! what the loop decided.
//!
//! PRICE IS ASSERTED, NOT ONLY LEGALITY. A fix that flipped the illegal flag without fixing which
//! config won would leave the armed jeep illegal AND priced at 1500, so legality alone would go green
//! on a half-fix. And the "at least one legal vehicle" assertion at the end is what stops the whole
//! case from being satisfied by marking every vehicle in the game illegal.
//!
//! SUBJECTS ARE FOUND BY SUBSTRING, NOT BY GUID: the reference extraction of the base game carries no
//! .meta files, so a vanilla prefab's ResourceName GUID cannot be written down here. Each subject is
//! required to match at least one registered vehicle - an absent subject fails loudly rather than
//! passing vacuously, which is correct: the shop cannot stock what was never registered, and all four
//! of these were seen on a civilian shop shelf when the bug was reported.
//!
//! PROVEN ABLE TO FAIL: the bestMatch comparison in BuildResourceDatabase was deleted - restoring
//! last-match-wins, the exact defect - and the tree recompiled CLEAN, which is the point: a rule that
//! resolves to the wrong config is not a syntax error. The case then went red on
//! "{F6B23D17D5067C11}...M151A2_M2HB.et is legal, so a civilian vehicle shop can stock it". Fault
//! reverted, tree recompiled clean, case green again in the Fast group.
//! No polling, no timing, no async step anywhere in the case - no maxAttempts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_VehiclePriceSpecificity : SCR_AutotestCaseBase
{
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

		// Each subject is a search string from vehiclePrices.conf paired with the cost that entry
		// carries. All four are armed variants and all four are illegal, i.e. off the civilian shelf.
		if (!AssertArmed(economy, "M151A2_M2HB", 12500)) return true;
		if (!AssertArmed(economy, "M1025_armed", 25998)) return true;
		if (!AssertArmed(economy, "UAZ469_PKM", 25000)) return true;
		if (!AssertArmed(economy, "UAZ469_UK59", 25000)) return true;

		// The counter-assertion. Without it, illegal = true for every vehicle would pass everything
		// above and would empty every civilian vehicle shop in the game.
		array<ResourceName> all();
		economy.FindVehicles("", all);

		int legal = 0;
		foreach (ResourceName res : all)
		{
			if (economy.IsLegalVehicle(economy.GetInventoryId(res)))
				legal += 1;
		}

		if (legal == 0)
		{
			SetFailure("Not one of the %1 registered vehicles is legal - civilian vehicle shops would have nothing to sell",
				all.Count().ToString());
			return true;
		}

		PrintFormat("Vehicle price specificity: 4 armed variants keep their own config, %1 of %2 registered vehicles are legal",
			legal.ToString(), all.Count().ToString());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts every registered vehicle matching a search string is illegal and priced at its own
	//! config's cost.
	//! \param[in] economy The live economy manager.
	//! \param[in] find The m_sFind string of the vehiclePrices.conf entry that should have won.
	//! \param[in] expectedCost The cost that entry carries.
	//! \return True if the assertion held; false after calling SetFailure.
	protected bool AssertArmed(OVT_EconomyManagerComponent economy, string find, int expectedCost)
	{
		array<ResourceName> matches();
		economy.FindVehicles(find, matches);

		if (matches.IsEmpty())
		{
			SetFailure("No registered vehicle matches '%1' - this subject has nothing to assert against and the case would pass vacuously",
				find);
			return false;
		}

		foreach (ResourceName res : matches)
		{
			int id = economy.GetInventoryId(res);

			if (economy.IsLegalVehicle(id))
			{
				SetFailure("%1 is legal, so a civilian vehicle shop can stock it - a broader price config took over the '%2' entry",
					res, find);
				return false;
			}

			int cost = economy.GetPrice(id);
			if (cost != expectedCost)
			{
				SetFailure("%1 is priced at %2, expected %3 - it is being priced by some other config entry",
					res, cost.ToString(), expectedCost.ToString());
				return false;
			}
		}

		return true;
	}
}


//------------------------------------------------------------------------------------------------
//! NO REGISTERED VEHICLE MAY BE SOLD AT THE UNPRICED DEFAULT.
//!
//! OVT_VehiclePriceConfig.cost defaults to 50000, so a vehicle that matches NO entry in
//! vehiclePrices.conf is quietly listed at that price rather than refused. That is not a theoretical
//! hole: `m_sFind "M1025.et"` was written with the extension to stop it swallowing "M1025_armed",
//! and it also excluded M1025_MERDC.et, which then sold for 50000 - $40,000 at an owned base, beside
//! an identical M1025 at $9,600 (reported 2026-08-26).
//!
//! ⚠ THE RUNTIME ALREADY KNOWS. BuildResourceDatabase Prints "Default price being set for: ..." on
//! exactly this condition - the information was there all along, in a log nobody reads. This case is
//! that Print promoted to a gate.
//!
//! ⚠ IT ASSERTS THE PRICE, NOT THE MATCH, because that is what a player sees. A vehicle deliberately
//! authored at 50000 would trip it; the fix then is to say so with its own entry, which is also the
//! honest thing for the config to record.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_VehiclePriceHasNoUnpricedFallthrough : SCR_AutotestCaseBase
{
	//! OVT_VehiclePriceConfig.cost's [Attribute] default - the value a vehicle gets when nothing matched.
	static const int UNPRICED_DEFAULT = 50000;

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

		array<ResourceName> all();
		economy.FindVehicles("", all);

		if (all.IsEmpty())
		{
			SetFailure("No vehicles are registered at all - this case would pass vacuously");
			return true;
		}

		string offenders = "";
		int count = 0;

		foreach (ResourceName res : all)
		{
			if (economy.GetPrice(economy.GetInventoryId(res)) != UNPRICED_DEFAULT)
				continue;

			count += 1;
			if (count <= 5)
				offenders = offenders + res + " ";
		}

		if (count > 0)
		{
			SetFailure("%1 registered vehicle(s) match no vehiclePrices.conf entry and are on sale at the %2 default: %3",
				count.ToString(), UNPRICED_DEFAULT.ToString(), offenders);
			return true;
		}

		PrintFormat("All %1 registered vehicles are priced by a real config entry, none at the %2 default",
			all.Count().ToString(), UNPRICED_DEFAULT.ToString());

		return true;
	}
}
