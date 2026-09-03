//------------------------------------------------------------------------------------------------
//! TIER A cases - OVT_ItemSourcingRules.SplitCoverage(), shared by the High Command purchase and
//! the recruitment-tent discount. World-free by construction; see the suite header for the tier
//! rule.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Builds a one-line manifest with a given need and unit price - the shared fixture for every case
//! below that only needs a single line.
//------------------------------------------------------------------------------------------------
class OVT_TEST_ItemSourcingFixture
{
	//------------------------------------------------------------------------------------------------
	static ref array<ref OVT_ItemSourcingLine> OneLine(string resource, int needed, int unitPrice)
	{
		array<ref OVT_ItemSourcingLine> manifest = new array<ref OVT_ItemSourcingLine>();

		OVT_ItemSourcingLine line = new OVT_ItemSourcingLine();
		line.m_sResource = resource;
		line.m_iNeeded = needed;
		line.m_iUnitPrice = unitPrice;

		manifest.Insert(line);

		return manifest;
	}
}

//------------------------------------------------------------------------------------------------
//! Stock exactly meeting the need covers the whole line and charges nothing.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ItemSourcing_CoverageExactlyMeetsNeed : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<ref OVT_ItemSourcingLine> manifest = OVT_TEST_ItemSourcingFixture.OneLine("ammo_556", 10, 20);
		array<int> available = {10};

		int coveredUnits;
		int coveredValue;
		int chargedUnits;
		int chargedSubtotal = OVT_ItemSourcingRules.SplitCoverage(manifest, available, coveredUnits, coveredValue, chargedUnits);

		if (coveredUnits != 10 || coveredValue != 200 || chargedUnits != 0 || chargedSubtotal != 0)
		{
			SetFailure("Stock exactly meeting a 10-unit need at 20/unit gave covered=%1, charged=%2 - expected covered=10/200, charged=0/0", coveredUnits.ToString() + "/" + coveredValue.ToString(), chargedUnits.ToString() + "/" + chargedSubtotal.ToString());
			return true;
		}

		Print("Item sourcing: stock exactly meeting the need covers it all and charges nothing");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Stock short of the need covers what it can; the remainder is charged.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ItemSourcing_CoveragePartial : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<ref OVT_ItemSourcingLine> manifest = OVT_TEST_ItemSourcingFixture.OneLine("ammo_556", 10, 20);
		array<int> available = {4};

		int coveredUnits;
		int coveredValue;
		int chargedUnits;
		int chargedSubtotal = OVT_ItemSourcingRules.SplitCoverage(manifest, available, coveredUnits, coveredValue, chargedUnits);

		if (coveredUnits != 4 || coveredValue != 80 || chargedUnits != 6 || chargedSubtotal != 120)
		{
			SetFailure("4 units of stock against a 10-unit need at 20/unit gave covered=%1, charged=%2 - expected covered=4/80, charged=6/120", coveredUnits.ToString() + "/" + coveredValue.ToString(), chargedUnits.ToString() + "/" + chargedSubtotal.ToString());
			return true;
		}

		Print("Item sourcing: short stock covers what it can and charges exactly the remainder");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Stock exceeding the need still only covers what was needed - the surplus is not consumed here.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ItemSourcing_CoverageSurplus : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<ref OVT_ItemSourcingLine> manifest = OVT_TEST_ItemSourcingFixture.OneLine("ammo_556", 10, 20);
		array<int> available = {50};

		int coveredUnits;
		int coveredValue;
		int chargedUnits;
		int chargedSubtotal = OVT_ItemSourcingRules.SplitCoverage(manifest, available, coveredUnits, coveredValue, chargedUnits);

		if (coveredUnits != 10 || coveredValue != 200 || chargedUnits != 0 || chargedSubtotal != 0)
		{
			SetFailure("50 units of stock against a 10-unit need gave covered=%1, charged=%2 - expected covered=10/200 (only what was needed), charged=0/0", coveredUnits.ToString() + "/" + coveredValue.ToString(), chargedUnits.ToString() + "/" + chargedSubtotal.ToString());
			return true;
		}

		Print("Item sourcing: surplus stock covers only what was needed, never more");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Four lines with mixed coverage sum correctly in both the covered and charged channels.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ItemSourcing_CoverageAcrossManyLines : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<ref OVT_ItemSourcingLine> manifest = new array<ref OVT_ItemSourcingLine>();

		OVT_ItemSourcingLine line1 = new OVT_ItemSourcingLine();
		line1.m_sResource = "ammo_556";
		line1.m_iNeeded = 10;
		line1.m_iUnitPrice = 20;
		manifest.Insert(line1);

		OVT_ItemSourcingLine line2 = new OVT_ItemSourcingLine();
		line2.m_sResource = "ammo_762";
		line2.m_iNeeded = 5;
		line2.m_iUnitPrice = 30;
		manifest.Insert(line2);

		OVT_ItemSourcingLine line3 = new OVT_ItemSourcingLine();
		line3.m_sResource = "medkit";
		line3.m_iNeeded = 2;
		line3.m_iUnitPrice = 100;
		manifest.Insert(line3);

		OVT_ItemSourcingLine line4 = new OVT_ItemSourcingLine();
		line4.m_sResource = "grenade";
		line4.m_iNeeded = 8;
		line4.m_iUnitPrice = 15;
		manifest.Insert(line4);

		// Line 1: fully covered (10/10). Line 2: none covered (0/5). Line 3: surplus (99/2, caps at 2).
		// Line 4: partial (3/8).
		array<int> available = {10, 0, 99, 3};

		int coveredUnits;
		int coveredValue;
		int chargedUnits;
		int chargedSubtotal = OVT_ItemSourcingRules.SplitCoverage(manifest, available, coveredUnits, coveredValue, chargedUnits);

		// Covered: 10 + 0 + 2 + 3 = 15 units; value 200 + 0 + 200 + 45 = 445.
		// Charged: 0 + 5 + 0 + 5 = 10 units; subtotal 0 + 150 + 0 + 75 = 225.
		if (coveredUnits != 15 || coveredValue != 445 || chargedUnits != 10 || chargedSubtotal != 225)
		{
			SetFailure("Four mixed-coverage lines summed to covered=%1, charged=%2 - expected covered=15/445, charged=10/225 (only the first line summed correctly would give covered=10/200, charged=15/345)", coveredUnits.ToString() + "/" + coveredValue.ToString(), chargedUnits.ToString() + "/" + chargedSubtotal.ToString());
			return true;
		}

		Print("Item sourcing: four mixed-coverage lines sum correctly in both channels");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An unpriceable (zero unit price) line contributes no value but its units still count as charged,
//! so a caller can still refuse the purchase over it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ItemSourcing_CoverageIgnoresZeroPriceLine : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<ref OVT_ItemSourcingLine> manifest = OVT_TEST_ItemSourcingFixture.OneLine("unknown_item", 6, 0);
		array<int> available = {2};

		int coveredUnits;
		int coveredValue;
		int chargedUnits;
		int chargedSubtotal = OVT_ItemSourcingRules.SplitCoverage(manifest, available, coveredUnits, coveredValue, chargedUnits);

		if (coveredValue != 0 || chargedSubtotal != 0)
		{
			SetFailure("A zero-unit-price line produced covered value %1 / charged subtotal %2 - a line with no known price must contribute no money either way", coveredValue.ToString(), chargedSubtotal.ToString());
			return true;
		}

		if (coveredUnits != 2 || chargedUnits != 4)
		{
			SetFailure("A zero-unit-price line's units were not reported (covered=%1, charged=%2, expected 2/4) - a caller must still be able to see and refuse an unpriceable line", coveredUnits.ToString(), chargedUnits.ToString());
			return true;
		}

		Print("Item sourcing: a zero-price line contributes no money but its units are still reported");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! No combination of inputs ever produces a negative charged subtotal.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ItemSourcing_CoverageNeverNegative : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// A malformed line an upstream caller should never build, but the split itself must still
		// refuse to invent negative money from it: negative stock and a need of zero.
		array<ref OVT_ItemSourcingLine> manifest = OVT_TEST_ItemSourcingFixture.OneLine("ammo_556", 0, 20);
		array<int> available = {-5};

		int coveredUnits;
		int coveredValue;
		int chargedUnits;
		int chargedSubtotal = OVT_ItemSourcingRules.SplitCoverage(manifest, available, coveredUnits, coveredValue, chargedUnits);

		if (chargedSubtotal < 0 || coveredValue < 0 || coveredUnits < 0 || chargedUnits < 0)
		{
			SetFailure("Negative stock against a zero need produced a negative figure somewhere: covered=%1, charged=%2 - none of the four may ever be negative", coveredUnits.ToString() + "/" + coveredValue.ToString(), chargedUnits.ToString() + "/" + chargedSubtotal.ToString());
			return true;
		}

		// A line with fewer needed than available stock, index-mismatched against a shorter
		// availability array, must still resolve to zero available rather than an out-of-range read.
		array<ref OVT_ItemSourcingLine> manifest2 = OVT_TEST_ItemSourcingFixture.OneLine("ammo_556", 5, 20);
		array<int> emptyAvailable = {};

		int coveredUnits2;
		int coveredValue2;
		int chargedUnits2;
		int chargedSubtotal2 = OVT_ItemSourcingRules.SplitCoverage(manifest2, emptyAvailable, coveredUnits2, coveredValue2, chargedUnits2);

		if (chargedSubtotal2 != 100 || chargedUnits2 != 5 || coveredUnits2 != 0 || coveredValue2 != 0)
		{
			SetFailure("A manifest longer than its availability array gave covered=%1, charged=%2 - expected covered=0/0, charged=5/100 (missing stock reads as zero, not negative)", coveredUnits2.ToString() + "/" + coveredValue2.ToString(), chargedUnits2.ToString() + "/" + chargedSubtotal2.ToString());
			return true;
		}

		Print("Item sourcing: never a negative figure, whatever the inputs");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The whole money composition the High Command purchase runs: the coverage split's CHARGED subtotal
//! is what reaches the fee multiplier, so warehouse stock genuinely lowers the price and full
//! coverage collapses the total to the member cost alone.
//!
//! This is the one arithmetic in the feature where a wrong number is money, and it is the join
//! between two classes that are each correct on their own.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ItemSourcing_CoverageFeedsTheGearFee : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// A four-member group at 100 each, needing 20 magazines at 25.
		int memberCost = 400;
		float feeMultiplier = 1.0;

		array<ref OVT_ItemSourcingLine> manifest = OVT_TEST_ItemSourcingFixture.OneLine("ammo_556", 20, 25);

		int coveredUnits;
		int coveredValue;
		int chargedUnits;

		array<int> noStock = {0};
		int chargedDry = OVT_ItemSourcingRules.SplitCoverage(manifest, noStock, coveredUnits, coveredValue, chargedUnits);
		int totalDry = OVT_RecruitPurchaseRules.TotalPrice(memberCost, chargedDry, feeMultiplier);

		if (totalDry != 900)
		{
			SetFailure("With no stock in range the total was %1, expected 900 (400 members + 20 x 25 gear)", totalDry.ToString());
			return true;
		}

		array<int> halfStock = {12};
		int chargedHalf = OVT_ItemSourcingRules.SplitCoverage(manifest, halfStock, coveredUnits, coveredValue, chargedUnits);
		int totalHalf = OVT_RecruitPurchaseRules.TotalPrice(memberCost, chargedHalf, feeMultiplier);

		if (coveredValue != 300 || totalHalf != 600)
		{
			SetFailure("With 12 of 20 units in stock the covered value was %1 and the total %2 - expected 300 covered and a total of 600 (400 members + 8 x 25 charged)", coveredValue.ToString(), totalHalf.ToString());
			return true;
		}

		if (totalHalf >= totalDry)
		{
			SetFailure("Stock in range did not lower the price at all: %1 covered vs %2 dry", totalHalf.ToString(), totalDry.ToString());
			return true;
		}

		array<int> fullStock = {20};
		int chargedFull = OVT_ItemSourcingRules.SplitCoverage(manifest, fullStock, coveredUnits, coveredValue, chargedUnits);
		int totalFull = OVT_RecruitPurchaseRules.TotalPrice(memberCost, chargedFull, feeMultiplier);

		if (totalFull != memberCost)
		{
			SetFailure("With the whole manifest in stock the total was %1, expected the bare member cost of %2 - fully covered gear must be free", totalFull.ToString(), memberCost.ToString());
			return true;
		}

		// The fee multiplier applies to the CHARGED half only, never to what stock covered.
		int totalFeed = OVT_RecruitPurchaseRules.TotalPrice(memberCost, chargedHalf, 2.0);
		if (totalFeed != 800)
		{
			SetFailure("A fee multiplier of 2.0 on 8 charged units at 25 gave a total of %1, expected 800 (400 members + 200 x 2) - the multiplier must never reach the covered half", totalFeed.ToString());
			return true;
		}

		Print("Item sourcing: covered stock lowers the charged subtotal, the fee multiplies only the charged half, and full coverage leaves the member cost alone");
		return true;
	}
}
