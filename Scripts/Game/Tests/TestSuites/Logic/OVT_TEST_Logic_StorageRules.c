//------------------------------------------------------------------------------------------------
//! TIER A cases - the storage rules' pure decisions.
//!
//! OVT_StorageRules takes every lookup as an argument (is it a vehicle, is it registered, what does
//! the shop charge, where is the holder), so each case here hands in literals and asserts one
//! branch. Nothing is constructed but a vector and a typename.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! AUTO capacity resolution, branch by branch. The truck and car branches are asserted with
//! DIFFERENT expected values (-1 vs the supplied default), so swapping them cannot pass.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageRules_AutoCapacity : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Not a vehicle: boxes and the warehouse building. Unlimited, whatever the other arguments say.
		int capacity = OVT_StorageRules.ResolveAutoCapacity(false, false, false, OVT_ParkingType.PARKING_CAR, 300);
		if (capacity != -1)
		{
			SetFailure("A non-vehicle holder resolved to %1, expected -1 (unlimited)", capacity.ToString());
			return true;
		}

		// A vehicle the economy does not know gets NOTHING - the caller logs the error.
		capacity = OVT_StorageRules.ResolveAutoCapacity(true, false, true, OVT_ParkingType.PARKING_TRUCK, 300);
		if (capacity != 0)
		{
			SetFailure("An unregistered vehicle resolved to %1, expected 0", capacity.ToString());
			return true;
		}

		// Illegal or armed, however it parks.
		capacity = OVT_StorageRules.ResolveAutoCapacity(true, true, false, OVT_ParkingType.PARKING_TRUCK, 300);
		if (capacity != 0)
		{
			SetFailure("An illegal/armed vehicle resolved to %1, expected 0", capacity.ToString());
			return true;
		}

		capacity = OVT_StorageRules.ResolveAutoCapacity(true, true, true, OVT_ParkingType.PARKING_TRUCK, 300);
		if (capacity != -1)
		{
			SetFailure("A registered legal truck resolved to %1, expected -1 (unlimited)", capacity.ToString());
			return true;
		}

		capacity = OVT_StorageRules.ResolveAutoCapacity(true, true, true, OVT_ParkingType.PARKING_CAR, 300);
		if (capacity != 300)
		{
			SetFailure("A registered legal car resolved to %1, expected the supplied default 300", capacity.ToString());
			return true;
		}

		// The default is the CALLER's number, not a constant baked into the rule.
		capacity = OVT_StorageRules.ResolveAutoCapacity(true, true, true, OVT_ParkingType.PARKING_LIGHT, 42);
		if (capacity != 42)
		{
			SetFailure("A non-truck vehicle resolved to %1 when the caller supplied 42", capacity.ToString());
			return true;
		}

		Print("Storage rules auto capacity: non-vehicle -1, unregistered 0, illegal 0, truck -1, car the caller's default");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A magazine converts only when it is EXACTLY full. Strict equality, so a corrupt or unknown
//! maximum (-1) can never call a part-used magazine full.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageRules_MagazineFull : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_StorageRules.MagazineIsFull(30, 30))
		{
			SetFailure("A 30/30 magazine is not reported full");
			return true;
		}

		if (OVT_StorageRules.MagazineIsFull(29, 30))
		{
			SetFailure("A 29/30 magazine is reported full - part-used magazines must never convert");
			return true;
		}

		if (OVT_StorageRules.MagazineIsFull(31, 30))
		{
			SetFailure("A 31/30 magazine is reported full - an over-count is corrupt, not full");
			return true;
		}

		// An item with no ammo concept at all reads 0/0 and is 'full', i.e. convertible.
		if (!OVT_StorageRules.MagazineIsFull(0, 0))
		{
			SetFailure("A 0/0 item is not reported full");
			return true;
		}

		// Unknown maximum. Must answer, not throw, and must not call a loaded magazine full.
		if (OVT_StorageRules.MagazineIsFull(5, -1))
		{
			SetFailure("A 5/-1 magazine is reported full - an unknown maximum must never satisfy the test");
			return true;
		}

		if (!OVT_StorageRules.MagazineIsFull(-1, -1))
		{
			SetFailure("A -1/-1 magazine is not reported full - equal inputs are equal");
			return true;
		}

		Print("Storage rules magazine: full only on exact equality, 0/0 full, negative inputs answered not thrown");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The three base garments a character always wears - jacket (the torso garment; there is no
//! LoadoutShirtArea), pants and boots - versus everything else. Compared as typenames, which is why
//! a name that does not exist in vanilla cannot silently pass here.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageRules_BaseClothing : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_StorageRules.IsBaseClothingArea(LoadoutJacketArea))
		{
			SetFailure("LoadoutJacketArea is not treated as base clothing");
			return true;
		}

		if (!OVT_StorageRules.IsBaseClothingArea(LoadoutPantsArea))
		{
			SetFailure("LoadoutPantsArea is not treated as base clothing");
			return true;
		}

		if (!OVT_StorageRules.IsBaseClothingArea(LoadoutBootsArea))
		{
			SetFailure("LoadoutBootsArea is not treated as base clothing");
			return true;
		}

		if (OVT_StorageRules.IsBaseClothingArea(LoadoutVestArea))
		{
			SetFailure("LoadoutVestArea is treated as base clothing - a vest is lootable gear");
			return true;
		}

		if (OVT_StorageRules.IsBaseClothingArea(LoadoutHeadCoverArea))
		{
			SetFailure("LoadoutHeadCoverArea is treated as base clothing");
			return true;
		}

		if (OVT_StorageRules.IsBaseClothingArea(LoadoutBackpackArea))
		{
			SetFailure("LoadoutBackpackArea is treated as base clothing");
			return true;
		}

		if (OVT_StorageRules.IsBaseClothingArea(LoadoutHandwearSlotArea))
		{
			SetFailure("LoadoutHandwearSlotArea is treated as base clothing");
			return true;
		}

		typename unset;
		if (OVT_StorageRules.IsBaseClothingArea(unset))
		{
			SetFailure("An unset area type is treated as base clothing");
			return true;
		}

		Print("Storage rules base clothing: jacket, pants and boots only - vest, helmet, backpack, gloves and an unset type are not");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Export price: the ratio applies, the result stays STRICTLY under the cheapest shop buy price so
//! there is no shop-buy/port-sell loop, a negative shop price means the ratio stands alone, and
//! nothing is ever exported for less than a dollar.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageRules_ExportPrice : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Ratio alone: half of 100 is well under the shop's 200.
		int price = OVT_StorageRules.ExportUnitPrice(100, 0.5, 200);
		if (price != 50)
		{
			SetFailure("Export of a 100 import at ratio 0.5 under a 200 shop price is %1, expected 50", price.ToString());
			return true;
		}

		// The shop is cheaper than the ratio: the ceiling wins, and it is minShopBuyPrice - 1.
		price = OVT_StorageRules.ExportUnitPrice(100, 0.5, 40);
		if (price != 39)
		{
			SetFailure("Export capped by a 40 shop buy price is %1, expected 39 - it must be strictly under", price.ToString());
			return true;
		}

		// Exactly at the shop price is still one dollar under it.
		price = OVT_StorageRules.ExportUnitPrice(100, 0.5, 50);
		if (price != 49)
		{
			SetFailure("Export at exactly the 50 shop buy price is %1, expected 49", price.ToString());
			return true;
		}

		// Sold at no shop (illegal): the ratio stands alone, uncapped.
		price = OVT_StorageRules.ExportUnitPrice(900, 0.5, -1);
		if (price != 450)
		{
			SetFailure("Export of an item sold at no shop is %1, expected the uncapped 450", price.ToString());
			return true;
		}

		// Floor: a rounding to zero still pays a dollar.
		price = OVT_StorageRules.ExportUnitPrice(1, 0.1, -1);
		if (price != 1)
		{
			SetFailure("Export that rounds to zero paid %1, expected the floor of 1", price.ToString());
			return true;
		}

		// Floor beats the ceiling: a 1-dollar shop price would otherwise pay 0.
		price = OVT_StorageRules.ExportUnitPrice(100, 0.5, 1);
		if (price != 1)
		{
			SetFailure("Export under a 1-dollar shop price paid %1, expected the floor of 1", price.ToString());
			return true;
		}

		// A worthless import is still never free.
		price = OVT_StorageRules.ExportUnitPrice(0, 0.5, 200);
		if (price != 1)
		{
			SetFailure("Export of a zero-price import paid %1, expected the floor of 1", price.ToString());
			return true;
		}

		Print("Storage rules export price: ratio applied, held strictly under the shop, uncapped when sold nowhere, floored at 1");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Radius test for the destination picker.
//!
//! vector.Distance is NOT correctly rounded - it is off by a ULP at 1000 m and at 2000 m - so this
//! case never asserts on an exact radius boundary at range. It asserts a true zero-distance
//! inclusive hit, and at 1000 m it uses a margin comfortably wider than the tier epsilon on both
//! sides of the radius.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageRules_HolderInRange : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector player = "100 20 100";

		// Zero distance is exact in floating point, so this boundary IS safe to assert on, and it is
		// the one that proves the test is inclusive.
		if (!OVT_StorageRules.HolderIsInRange(player, player, 0))
		{
			SetFailure("A holder at the player's own position is out of range at radius 0 - the test must be inclusive");
			return true;
		}

		vector near = "105 20 100";
		if (!OVT_StorageRules.HolderIsInRange(near, player, 10))
		{
			SetFailure("A holder 5 m away is out of a 10 m radius");
			return true;
		}

		if (OVT_StorageRules.HolderIsInRange(near, player, 4))
		{
			SetFailure("A holder 5 m away is inside a 4 m radius");
			return true;
		}

		// 1000 m out, asserted with a margin far wider than the tier epsilon rather than at the radius.
		vector far = "1100 20 100";
		float margin = 0.01;

		if (margin <= OVT_TEST_LogicFixture.EPSILON)
		{
			SetFailure("The 1000 m margin is not wider than the tier epsilon - the assertion below would sit on a ULP");
			return true;
		}

		if (!OVT_StorageRules.HolderIsInRange(far, player, 1000 + margin))
		{
			SetFailure("A holder 1000 m away is out of a 1000.01 m radius");
			return true;
		}

		if (OVT_StorageRules.HolderIsInRange(far, player, 1000 - margin))
		{
			SetFailure("A holder 1000 m away is inside a 999.99 m radius");
			return true;
		}

		// Height counts: the warehouse roof is not the warehouse floor.
		vector above = "100 60 100";
		if (OVT_StorageRules.HolderIsInRange(above, player, 20))
		{
			SetFailure("A holder 40 m overhead is inside a 20 m radius - the test must be 3D");
			return true;
		}

		Print("Storage rules range: inclusive at zero, correct at 5 m and at 1000 m within a margin, and three-dimensional");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The ledger-to-ledger move's ORDERING, which is the one data-integrity rule in this feature pure
//! enough to assert without a world.
//!
//! Both cases assert CONSERVATION - the two ledgers' totals together never change - because that is
//! what "the worst a mid-transfer crash can cost is one item" reduces to for a move that spawns and
//! deletes nothing.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageRules_MoveReturnsRemainder : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string rifle = "rifle";
		string ammo = "ammo";

		OVT_StorageLedger source = new OVT_StorageLedger();
		source.Add(rifle, 20, -1);
		source.Add(ammo, 5, -1);

		OVT_StorageLedger dest = new OVT_StorageLedger();
		dest.Add(rifle, 8, -1);

		int before = source.Total() + dest.Total();

		// A destination holding 8 of a cap of 10 has room for exactly 2 of the 20 asked for.
		int shortfall;
		int moved = OVT_StorageRules.TransferLedgerLine(source, dest, rifle, 20, 10, shortfall);

		if (moved != 2)
		{
			SetFailure("Moving 20 into a destination with 2 of room moved %1, expected 2", moved.ToString());
			return true;
		}

		if (shortfall != 18)
		{
			SetFailure("Moving 20 with 2 of room reported a shortfall of %1, expected 18", shortfall.ToString());
			return true;
		}

		if (source.Count(rifle) != 18)
		{
			SetFailure("After a move that only half fitted the source holds %1, expected 18 - the un-added remainder must go BACK to the source", source.Count(rifle).ToString());
			return true;
		}

		if (dest.Count(rifle) != 10)
		{
			SetFailure("The destination holds %1 after the move, expected 10 (its whole capacity)", dest.Count(rifle).ToString());
			return true;
		}

		int after = source.Total() + dest.Total();
		if (after != before)
		{
			SetFailure("The two ledgers held %1 items before the move and %2 after - a ledger-to-ledger move must conserve", before.ToString(), after.ToString());
			return true;
		}

		// The untouched line is untouched.
		if (source.Count(ammo) != 5)
		{
			SetFailure("Moving one line changed another: ammo is %1, expected 5", source.Count(ammo).ToString());
			return true;
		}

		Print("Storage move: the un-added remainder returns to the source, the destination stops at its capacity, and nothing is created or lost");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A move can never hand the destination more than the source actually held.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageRules_MoveClampsToMembership : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string res = "grenade";

		OVT_StorageLedger source = new OVT_StorageLedger();
		source.Add(res, 3, -1);

		OVT_StorageLedger dest = new OVT_StorageLedger();

		int before = source.Total() + dest.Total();

		int shortfall;
		int moved = OVT_StorageRules.TransferLedgerLine(source, dest, res, 10, -1, shortfall);

		if (moved != 3)
		{
			SetFailure("Asking for 10 of a stock of 3 moved %1, expected 3", moved.ToString());
			return true;
		}

		if (dest.Count(res) != 3)
		{
			SetFailure("The destination gained %1 from a source holding 3 - a move must never mint", dest.Count(res).ToString());
			return true;
		}

		if (shortfall != 7)
		{
			SetFailure("Asking for 10 of a stock of 3 reported a shortfall of %1, expected 7", shortfall.ToString());
			return true;
		}

		if (source.Count(res) != 0 || source.LineCount() != 0)
		{
			SetFailure("A fully drained source still reports %1 line(s)", source.LineCount().ToString());
			return true;
		}

		int after = source.Total() + dest.Total();
		if (after != before)
		{
			SetFailure("The two ledgers held %1 items before the move and %2 after", before.ToString(), after.ToString());
			return true;
		}

		// A line that is not there at all moves nothing and mints nothing.
		moved = OVT_StorageRules.TransferLedgerLine(source, dest, "absent", 5, -1, shortfall);

		if (moved != 0 || shortfall != 5)
		{
			SetFailure("Moving a resource the source does not hold moved %1 with a shortfall of %2, expected 0 and 5", moved.ToString(), shortfall.ToString());
			return true;
		}

		if (dest.Count("absent") != 0)
		{
			SetFailure("A move of a resource the source does not hold credited the destination %1", dest.Count("absent").ToString());
			return true;
		}

		Print("Storage move: clamped to live membership, mints nothing, and drops a drained line");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! F11's headline claim, swept rather than spot-checked: a port export never pays as much as the
//! cheapest shop charges, so there is no shop-buy/port-sell loop.
//!
//! The sweep runs every ratio against every import price against every shop price, including
//! ratios ABOVE 1 - a mis-tuned m_fExportPriceRatio must still be capped by the shop price rather
//! than minting money.
//!
//! ONE EXCEPTION, ASSERTED AS IT ACTUALLY IS rather than wished away: at minShopBuyPrice == 1 the
//! ceiling is 0 and the floor is 1, so the payout EQUALS the shop price. Recorded as an open
//! question in the feature's context.md; a 1-dollar item is not a loop worth paying a free item for.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageRules_ExportBelowShopPrice : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<float> ratios = new array<float>();
		ratios.Insert(0.1);
		ratios.Insert(0.25);
		ratios.Insert(0.5);
		ratios.Insert(0.75);
		ratios.Insert(1.0);
		ratios.Insert(2.0);

		array<int> importPrices = new array<int>();
		importPrices.Insert(0);
		importPrices.Insert(1);
		importPrices.Insert(5);
		importPrices.Insert(37);
		importPrices.Insert(100);
		importPrices.Insert(500);
		importPrices.Insert(2500);

		int checked = 0;

		foreach (float ratio : ratios)
		{
			foreach (int importPrice : importPrices)
			{
				for (int shopPrice = 1; shopPrice <= 400; shopPrice++)
				{
					int price = OVT_StorageRules.ExportUnitPrice(importPrice, ratio, shopPrice);
					checked++;

					// The port never pays nothing for something it accepted.
					if (price < 1)
					{
						SetFailure("Export of a %1 import under a %2 shop price paid %3, and a payout is never below 1", importPrice.ToString(), shopPrice.ToString(), price.ToString());
						return true;
					}

					if (shopPrice == 1)
					{
						// THE DOCUMENTED EDGE: floor 1 beats ceiling 0, so it lands ON the shop price.
						if (price != 1)
						{
							SetFailure("Export under a 1-dollar shop price paid %1, expected the floor of 1", price.ToString());
							return true;
						}

						continue;
					}

					if (price >= shopPrice)
					{
						SetFailure("Export of a %1 import paid %2 where the shop charges %3 - it must be strictly under", importPrice.ToString(), price.ToString(), shopPrice.ToString());
						return true;
					}
				}
			}
		}

		// An item no shop stocks has no ceiling to be under, so it is excluded from the claim
		// deliberately rather than by omission.
		int uncapped = OVT_StorageRules.ExportUnitPrice(900, 0.5, -1);
		if (uncapped != 450)
		{
			SetFailure("An item sold at no shop paid %1, expected the uncapped 450", uncapped.ToString());
			return true;
		}

		Print("Storage export pricing: " + checked.ToString() + " priced combinations, every one under the shop buy price except the 1-dollar floor edge");

		return true;
	}
}
