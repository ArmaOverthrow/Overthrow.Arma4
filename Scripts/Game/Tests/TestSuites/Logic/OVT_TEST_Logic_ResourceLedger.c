//------------------------------------------------------------------------------------------------
//! TIER A cases - the resource ledger's pure arithmetic.
//!
//! Every subject is a `new` OVT_ResourceLedger fed a hand-built OVT_ResourceDefs. The ledger holds
//! no capacity of its own (capacity is passed into Add, FreeLitres and WouldFit), so nothing here
//! needs a holder, a component or anything the tier forbids.
//!
//! `new` applies no [Attribute()] defvalues; the fixture sets every definition field explicitly.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The hand-built definition table these cases share. Litres are deliberately round so every
//! capacity assertion is exact.
//------------------------------------------------------------------------------------------------
class OVT_TEST_ResourceLedgerFixture
{
	static const string TIMBER = "timber";
	static const string CEMENT = "cement";
	static const string STEEL = "steel";
	static const string HARDWARE = "hardware";
	static const string UNKNOWN = "plutonium";

	//------------------------------------------------------------------------------------------------
	//! Four definitions: 100 / 50 / 40 / 20 litres per unit.
	//! \return A fully populated table.
	static OVT_ResourceDefs MakeDefs()
	{
		OVT_ResourceDefs defs = new OVT_ResourceDefs();

		defs.AddDef(TIMBER, 100, 25, 40, 1, 0);
		defs.AddDef(CEMENT, 50, 50, 60, 1, 0);
		defs.AddDef(STEEL, 40, 90, 120, 1, 0);
		defs.AddDef(HARDWARE, 20, 10, 200, 1, 0);

		return defs;
	}
}

//------------------------------------------------------------------------------------------------
//! Add() returns the UNITS that fitted, and the litre total never passes the capacity the caller
//! supplied. A full ledger fits nothing more, reports 0 and creates no line.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceLedger_AddClampsToCapacity : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceLedgerFixture.MakeDefs();
		OVT_ResourceLedger ledger = new OVT_ResourceLedger();

		// 6 timber = 600 of the 1000 litre cap.
		int fitted = ledger.Add(OVT_TEST_ResourceLedgerFixture.TIMBER, 6, defs, 1000);
		if (fitted != 6)
		{
			SetFailure("Add(timber, 6) under a 1000 litre cap fitted %1, expected 6", fitted.ToString());
			return true;
		}

		// 10 cement asked for = 500 litres, but only 400 litres of room: 8 units fit.
		fitted = ledger.Add(OVT_TEST_ResourceLedgerFixture.CEMENT, 10, defs, 1000);
		if (fitted != 8)
		{
			SetFailure("Add(cement, 10) with 400 litres of room fitted %1, expected 8", fitted.ToString());
			return true;
		}

		if (ledger.TotalLitres(defs) != 1000)
		{
			SetFailure("TotalLitres() is %1 after a clamped add, expected exactly the cap 1000", ledger.TotalLitres(defs).ToString());
			return true;
		}

		if (ledger.Count(OVT_TEST_ResourceLedgerFixture.CEMENT) != 8)
		{
			SetFailure("The clamped line holds %1, expected only the 8 units that fitted", ledger.Count(OVT_TEST_ResourceLedgerFixture.CEMENT).ToString());
			return true;
		}

		fitted = ledger.Add(OVT_TEST_ResourceLedgerFixture.STEEL, 1, defs, 1000);
		if (fitted != 0)
		{
			SetFailure("Add() into a full ledger fitted %1, expected 0", fitted.ToString());
			return true;
		}

		if (ledger.LineCount() != 2)
		{
			SetFailure("A wholly rejected add left %1 lines, expected 2 - a zero line must never be created", ledger.LineCount().ToString());
			return true;
		}

		if (ledger.FreeLitres(defs, 1000) != 0)
		{
			SetFailure("FreeLitres() is %1 on a full ledger, expected 0", ledger.FreeLitres(defs, 1000).ToString());
			return true;
		}

		Print("Resource ledger add: returns the units that fitted and never exceeds the supplied litre cap");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A negative capacity is UNLIMITED, not a literal cap. Piles and warehouses resolve to -1, so an
//! implementation that treats it as a number rejects every add they ever make.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceLedger_AddUnlimited : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceLedgerFixture.MakeDefs();
		OVT_ResourceLedger ledger = new OVT_ResourceLedger();

		int fitted = ledger.Add(OVT_TEST_ResourceLedgerFixture.TIMBER, 10000, defs, -1);
		if (fitted != 10000)
		{
			SetFailure("Add(timber, 10000) at unlimited capacity fitted %1, expected 10000", fitted.ToString());
			return true;
		}

		if (ledger.TotalLitres(defs) != 1000000)
		{
			SetFailure("TotalLitres() is %1 after an unlimited add, expected 1000000", ledger.TotalLitres(defs).ToString());
			return true;
		}

		if (ledger.FreeLitres(defs, -1) != int.MAX)
		{
			SetFailure("FreeLitres() at unlimited capacity is %1, expected int.MAX", ledger.FreeLitres(defs, -1).ToString());
			return true;
		}

		if (!ledger.WouldFit(OVT_TEST_ResourceLedgerFixture.STEEL, 99999, defs, -1))
		{
			SetFailure("WouldFit() refused a load at unlimited capacity");
			return true;
		}

		Print("Resource ledger unlimited: -1 fits everything and reports int.MAX of free space");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Take() is clamped to what is held and REMOVES the line it empties. A zero line left standing
//! would show up as an empty row on every screen that enumerates a holder.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceLedger_TakeClampsAndDropsLine : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceLedgerFixture.MakeDefs();
		OVT_ResourceLedger ledger = new OVT_ResourceLedger();

		ledger.Add(OVT_TEST_ResourceLedgerFixture.TIMBER, 5, defs, -1);
		ledger.Add(OVT_TEST_ResourceLedgerFixture.STEEL, 3, defs, -1);

		int taken = ledger.Take(OVT_TEST_ResourceLedgerFixture.TIMBER, 9);
		if (taken != 5)
		{
			SetFailure("Take(timber, 9) from a line of 5 took %1, expected 5", taken.ToString());
			return true;
		}

		if (ledger.LineCount() != 1)
		{
			SetFailure("An emptied line left %1 lines, expected 1 - a line that reaches zero is removed", ledger.LineCount().ToString());
			return true;
		}

		if (ledger.Count(OVT_TEST_ResourceLedgerFixture.TIMBER) != 0)
		{
			SetFailure("The emptied line still reports %1 units", ledger.Count(OVT_TEST_ResourceLedgerFixture.TIMBER).ToString());
			return true;
		}

		if (ledger.TotalLitres(defs) != 120)
		{
			SetFailure("TotalLitres() is %1 after emptying the timber line, expected 120 (3 steel)", ledger.TotalLitres(defs).ToString());
			return true;
		}

		taken = ledger.Take(OVT_TEST_ResourceLedgerFixture.TIMBER, 1);
		if (taken != 0)
		{
			SetFailure("Take() from an absent line took %1, expected 0", taken.ToString());
			return true;
		}

		taken = ledger.Take(OVT_TEST_ResourceLedgerFixture.STEEL, 1);
		if (taken != 1)
		{
			SetFailure("A partial take took %1, expected 1", taken.ToString());
			return true;
		}

		if (ledger.LineCount() != 1)
		{
			SetFailure("A partial take left %1 lines, expected the line to survive", ledger.LineCount().ToString());
			return true;
		}

		Print("Resource ledger take: clamps to what is held and removes the line it empties");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! TotalLitres() is a maintained field, so it must track every add, every take and Clear() across
//! mixed ids - a stale field only shows up once two different resources are in play.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceLedger_TotalIsMaintained : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceLedgerFixture.MakeDefs();
		OVT_ResourceLedger ledger = new OVT_ResourceLedger();

		if (ledger.TotalLitres(defs) != 0)
		{
			SetFailure("A fresh ledger reports %1 litres, expected 0", ledger.TotalLitres(defs).ToString());
			return true;
		}

		ledger.Add(OVT_TEST_ResourceLedgerFixture.TIMBER, 3, defs, -1);
		ledger.Add(OVT_TEST_ResourceLedgerFixture.HARDWARE, 7, defs, -1);

		// 3 x 100 + 7 x 20.
		if (ledger.TotalLitres(defs) != 440)
		{
			SetFailure("TotalLitres() is %1 after mixed adds, expected 440", ledger.TotalLitres(defs).ToString());
			return true;
		}

		ledger.Take(OVT_TEST_ResourceLedgerFixture.HARDWARE, 2);
		if (ledger.TotalLitres(defs) != 400)
		{
			SetFailure("TotalLitres() is %1 after taking 2 hardware, expected 400", ledger.TotalLitres(defs).ToString());
			return true;
		}

		ledger.Add(OVT_TEST_ResourceLedgerFixture.TIMBER, 1, defs, -1);
		if (ledger.TotalLitres(defs) != 500)
		{
			SetFailure("TotalLitres() is %1 after topping up an existing line, expected 500", ledger.TotalLitres(defs).ToString());
			return true;
		}

		if (ledger.FreeLitres(defs, 800) != 300)
		{
			SetFailure("FreeLitres(800) is %1, expected 300", ledger.FreeLitres(defs, 800).ToString());
			return true;
		}

		ledger.Clear();
		if (ledger.TotalLitres(defs) != 0)
		{
			SetFailure("TotalLitres() is %1 after Clear(), expected 0", ledger.TotalLitres(defs).ToString());
			return true;
		}

		if (ledger.LineCount() != 0)
		{
			SetFailure("Clear() left %1 lines", ledger.LineCount().ToString());
			return true;
		}

		Print("Resource ledger total: maintained exactly across mixed adds, takes and Clear()");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Garbage in is a no-op, never a line: an empty id, a non-positive quantity, and an id the
//! definition table does not know. An unknown id has no litres per unit, so accepting one would
//! silently break the litre total.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceLedger_IgnoresGarbage : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceLedgerFixture.MakeDefs();
		OVT_ResourceLedger ledger = new OVT_ResourceLedger();

		int fitted = ledger.Add("", 5, defs, -1);
		if (fitted != 0)
		{
			SetFailure("Add() with an empty id fitted %1, expected 0", fitted.ToString());
			return true;
		}

		fitted = ledger.Add(OVT_TEST_ResourceLedgerFixture.TIMBER, 0, defs, -1);
		if (fitted != 0)
		{
			SetFailure("Add(qty 0) fitted %1, expected 0", fitted.ToString());
			return true;
		}

		fitted = ledger.Add(OVT_TEST_ResourceLedgerFixture.TIMBER, -3, defs, -1);
		if (fitted != 0)
		{
			SetFailure("Add(qty -3) fitted %1, expected 0", fitted.ToString());
			return true;
		}

		fitted = ledger.Add(OVT_TEST_ResourceLedgerFixture.UNKNOWN, 5, defs, -1);
		if (fitted != 0)
		{
			SetFailure("Add() of an id the definition table does not know fitted %1, expected 0", fitted.ToString());
			return true;
		}

		if (ledger.LineCount() != 0)
		{
			SetFailure("Garbage adds created %1 lines, expected 0", ledger.LineCount().ToString());
			return true;
		}

		if (ledger.TotalLitres(defs) != 0)
		{
			SetFailure("Garbage adds moved the litre total to %1, expected 0", ledger.TotalLitres(defs).ToString());
			return true;
		}

		int taken = ledger.Take("", 5);
		if (taken != 0)
		{
			SetFailure("Take() with an empty id took %1, expected 0", taken.ToString());
			return true;
		}

		taken = ledger.Take(OVT_TEST_ResourceLedgerFixture.TIMBER, -1);
		if (taken != 0)
		{
			SetFailure("Take(qty -1) took %1, expected 0", taken.ToString());
			return true;
		}

		if (ledger.WouldFit(OVT_TEST_ResourceLedgerFixture.UNKNOWN, 1, defs, -1))
		{
			SetFailure("WouldFit() admitted an id the definition table does not know");
			return true;
		}

		Print("Resource ledger guards: empty id, non-positive quantity and unknown id are all no-ops");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! WouldFit() is an EXACT integer comparison (D3): a load that fills the cap to the litre fits, one
//! unit more does not. No epsilon, no rounding, no judgement call at the boundary.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceLedger_WouldFitIsExact : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceLedgerFixture.MakeDefs();
		OVT_ResourceLedger ledger = new OVT_ResourceLedger();

		// 500 litres held, 500 litres of a 1000 litre cap left.
		ledger.Add(OVT_TEST_ResourceLedgerFixture.TIMBER, 5, defs, 1000);

		// 10 cement = exactly 500 litres.
		if (!ledger.WouldFit(OVT_TEST_ResourceLedgerFixture.CEMENT, 10, defs, 1000))
		{
			SetFailure("WouldFit() refused a load that fills the cap to the litre");
			return true;
		}

		if (ledger.WouldFit(OVT_TEST_ResourceLedgerFixture.CEMENT, 11, defs, 1000))
		{
			SetFailure("WouldFit() admitted a load 50 litres over the cap");
			return true;
		}

		if (ledger.WouldFit(OVT_TEST_ResourceLedgerFixture.TIMBER, 0, defs, 1000))
		{
			SetFailure("WouldFit() admitted a zero-unit load");
			return true;
		}

		// The exact-fit load really does fit when it is made.
		int fitted = ledger.Add(OVT_TEST_ResourceLedgerFixture.CEMENT, 10, defs, 1000);
		if (fitted != 10)
		{
			SetFailure("The load WouldFit() accepted only fitted %1 units, expected 10", fitted.ToString());
			return true;
		}

		if (ledger.WouldFit(OVT_TEST_ResourceLedgerFixture.HARDWARE, 1, defs, 1000))
		{
			SetFailure("WouldFit() admitted a unit into a ledger that is exactly full");
			return true;
		}

		Print("Resource ledger WouldFit: exact at the boundary in both directions");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Weight aggregates across mixed lines and is display only - it never influences a fit decision.
//! A heavy, low-volume load must still fit a cap that has the litres for it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ResourceLedger_WeightAggregates : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceDefs defs = OVT_TEST_ResourceLedgerFixture.MakeDefs();
		OVT_ResourceLedger ledger = new OVT_ResourceLedger();

		ledger.Add(OVT_TEST_ResourceLedgerFixture.TIMBER, 4, defs, -1);
		ledger.Add(OVT_TEST_ResourceLedgerFixture.HARDWARE, 3, defs, -1);

		// 4 x 25 + 3 x 10.
		float weight = ledger.TotalWeightKg(defs);
		if (Math.AbsFloat(weight - 130) > 0.001)
		{
			SetFailure("TotalWeightKg() is %1, expected 130", weight.ToString());
			return true;
		}

		ledger.Take(OVT_TEST_ResourceLedgerFixture.TIMBER, 4);
		weight = ledger.TotalWeightKg(defs);
		if (Math.AbsFloat(weight - 30) > 0.001)
		{
			SetFailure("TotalWeightKg() is %1 after emptying the timber line, expected 30", weight.ToString());
			return true;
		}

		// Steel is the heaviest resource per unit and one of the smallest by volume: 10 units is 900 kg
		// in 400 litres, and a 400 litre cap must take all of it.
		OVT_ResourceLedger heavy = new OVT_ResourceLedger();
		if (!heavy.WouldFit(OVT_TEST_ResourceLedgerFixture.STEEL, 10, defs, 400))
		{
			SetFailure("WouldFit() refused 900 kg that occupies exactly the 400 litre cap - weight must not gate a fit");
			return true;
		}

		int fitted = heavy.Add(OVT_TEST_ResourceLedgerFixture.STEEL, 10, defs, 400);
		if (fitted != 10)
		{
			SetFailure("A heavy but exactly-fitting load added %1 units, expected 10", fitted.ToString());
			return true;
		}

		Print("Resource ledger weight: sums across lines and never gates a capacity decision");

		return true;
	}
}
