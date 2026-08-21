//------------------------------------------------------------------------------------------------
//! TIER A cases - the storage ledger's pure arithmetic.
//!
//! Every subject is a `new` OVT_StorageLedger fed hand-written keys and counts. The ledger holds no
//! capacity of its own (capacity is passed into Add and FreeSpace), so nothing here needs a holder,
//! a component or anything the tier forbids.
//!
//! `new` applies no [Attribute()] defvalues; the ledger's constructor sets every field it owns, and
//! each case sets every value it depends on explicitly.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Keys used by these cases. Shaped like prefab ResourceNames because that is what the ledger keys
//! on, but never resolved - the ledger treats them as opaque strings.
//------------------------------------------------------------------------------------------------
class OVT_TEST_StorageLedgerFixture
{
	static const string RES_A = "{AAAA000000000001}Prefabs/Items/A.et";
	static const string RES_B = "{AAAA000000000002}Prefabs/Items/B.et";
	static const string RES_C = "{AAAA000000000003}Prefabs/Items/C.et";
}

//------------------------------------------------------------------------------------------------
//! Add() returns what FITTED, not what was asked for, and the total never passes the capacity the
//! caller supplied. A full ledger fits nothing more and reports 0.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageLedger_AddClampsToCapacity : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_StorageLedger ledger = new OVT_StorageLedger();

		int fitted = ledger.Add(OVT_TEST_StorageLedgerFixture.RES_A, 40, 100);
		if (fitted != 40)
		{
			SetFailure("Add(40) under capacity 100 fitted %1, expected 40", fitted.ToString());
			return true;
		}

		// 80 asked for, 60 of room left.
		fitted = ledger.Add(OVT_TEST_StorageLedgerFixture.RES_B, 80, 100);
		if (fitted != 60)
		{
			SetFailure("Add(80) with 60 of room fitted %1, expected 60", fitted.ToString());
			return true;
		}

		if (ledger.Total() != 100)
		{
			SetFailure("Total() is %1 after a clamped add, expected exactly the capacity 100", ledger.Total().ToString());
			return true;
		}

		if (ledger.Count(OVT_TEST_StorageLedgerFixture.RES_B) != 60)
		{
			SetFailure("The clamped line holds %1, expected only the 60 that fitted", ledger.Count(OVT_TEST_StorageLedgerFixture.RES_B).ToString());
			return true;
		}

		fitted = ledger.Add(OVT_TEST_StorageLedgerFixture.RES_C, 5, 100);
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

		Print("Storage ledger add: returns what fitted, clamps at capacity, and creates no line when nothing fits");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A negative capacity is UNLIMITED, not a literal cap. Boxes, the warehouse building and trucks all
//! resolve to -1, so an implementation that treats it as a number rejects every add they ever make.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageLedger_AddUnlimited : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_StorageLedger ledger = new OVT_StorageLedger();

		int fitted = ledger.Add(OVT_TEST_StorageLedgerFixture.RES_A, 5000, -1);
		if (fitted != 5000)
		{
			SetFailure("Add(5000) at capacity -1 fitted %1, expected all 5000", fitted.ToString());
			return true;
		}

		fitted = ledger.Add(OVT_TEST_StorageLedgerFixture.RES_A, 1, -1);
		if (fitted != 1)
		{
			SetFailure("Add(1) into an already-large unlimited ledger fitted %1, expected 1", fitted.ToString());
			return true;
		}

		if (ledger.Total() != 5001)
		{
			SetFailure("Total() is %1 after two unlimited adds, expected 5001", ledger.Total().ToString());
			return true;
		}

		Print("Storage ledger add: capacity -1 means unlimited, never a literal cap");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Take() returns what was actually taken, and a line that reaches zero is REMOVED. The old
//! warehouse path floored at 0 and kept the key, which is how a stock map accumulates dead entries
//! that then travel to every client.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageLedger_TakeClampsAndDropsLine : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_StorageLedger ledger = new OVT_StorageLedger();
		ledger.Add(OVT_TEST_StorageLedgerFixture.RES_A, 10, -1);
		ledger.Add(OVT_TEST_StorageLedgerFixture.RES_B, 4, -1);

		int taken = ledger.Take(OVT_TEST_StorageLedgerFixture.RES_A, 3);
		if (taken != 3)
		{
			SetFailure("Take(3) of 10 held took %1, expected 3", taken.ToString());
			return true;
		}

		if (ledger.Count(OVT_TEST_StorageLedgerFixture.RES_A) != 7)
		{
			SetFailure("Partially taken line holds %1, expected 7", ledger.Count(OVT_TEST_StorageLedgerFixture.RES_A).ToString());
			return true;
		}

		if (ledger.LineCount() != 2)
		{
			SetFailure("A partial take left %1 lines, expected both to survive", ledger.LineCount().ToString());
			return true;
		}

		// Ask for more than is held: take everything, and drop the line.
		taken = ledger.Take(OVT_TEST_StorageLedgerFixture.RES_A, 99);
		if (taken != 7)
		{
			SetFailure("Take(99) of 7 held took %1, expected the 7 that were there", taken.ToString());
			return true;
		}

		if (ledger.LineCount() != 1)
		{
			SetFailure("After draining a line LineCount() is %1, expected 1 - a drained line must be removed, not kept at zero", ledger.LineCount().ToString());
			return true;
		}

		if (ledger.Count(OVT_TEST_StorageLedgerFixture.RES_A) != 0)
		{
			SetFailure("A removed line still reports %1 held, expected 0", ledger.Count(OVT_TEST_StorageLedgerFixture.RES_A).ToString());
			return true;
		}

		taken = ledger.Take(OVT_TEST_StorageLedgerFixture.RES_A, 1);
		if (taken != 0)
		{
			SetFailure("Take() from an absent line took %1, expected 0", taken.ToString());
			return true;
		}

		if (ledger.Total() != 4)
		{
			SetFailure("Total() is %1 after taking one line out entirely, expected the other line's 4", ledger.Total().ToString());
			return true;
		}

		array<string> res = new array<string>();
		array<int> counts = new array<int>();
		ledger.GetLines(res, counts);

		if (res.Count() != 1 || counts.Count() != 1)
		{
			SetFailure("GetLines() produced %1 keys and %2 counts, expected 1 of each", res.Count().ToString(), counts.Count().ToString());
			return true;
		}

		if (res.Get(0) != OVT_TEST_StorageLedgerFixture.RES_B || counts.Get(0) != 4)
		{
			SetFailure("GetLines() produced '%1' x %2, expected the surviving B line x 4", res.Get(0), counts.Get(0).ToString());
			return true;
		}

		Print("Storage ledger take: clamps to what is held, removes a drained line, and never enumerates a zero");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Total() is a MAINTAINED field, not a sum, because action labels poll it every frame. This case
//! walks it across adds, takes, a drained line and Clear() on mixed keys - a stale or recomputed
//! field diverges on the second mutation.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageLedger_TotalIsMaintained : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_StorageLedger ledger = new OVT_StorageLedger();

		if (ledger.Total() != 0)
		{
			SetFailure("A fresh ledger reports Total() %1, expected 0", ledger.Total().ToString());
			return true;
		}

		ledger.Add(OVT_TEST_StorageLedgerFixture.RES_A, 6, -1);
		if (ledger.Total() != 6)
		{
			SetFailure("Total() is %1 after adding 6, expected 6", ledger.Total().ToString());
			return true;
		}

		ledger.Add(OVT_TEST_StorageLedgerFixture.RES_B, 9, -1);
		ledger.Add(OVT_TEST_StorageLedgerFixture.RES_A, 5, -1);
		if (ledger.Total() != 20)
		{
			SetFailure("Total() is %1 after 6 + 9 + 5 across two keys, expected 20", ledger.Total().ToString());
			return true;
		}

		ledger.Take(OVT_TEST_StorageLedgerFixture.RES_B, 4);
		if (ledger.Total() != 16)
		{
			SetFailure("Total() is %1 after taking 4, expected 16", ledger.Total().ToString());
			return true;
		}

		// Overdraw: only the 5 that remain leave the total.
		ledger.Take(OVT_TEST_StorageLedgerFixture.RES_B, 500);
		if (ledger.Total() != 11)
		{
			SetFailure("Total() is %1 after draining the 5-strong B line, expected 11", ledger.Total().ToString());
			return true;
		}

		// A rejected add must not move the total.
		ledger.Add(OVT_TEST_StorageLedgerFixture.RES_C, 50, 11);
		if (ledger.Total() != 11)
		{
			SetFailure("Total() is %1 after an add that fitted nothing, expected it unchanged at 11", ledger.Total().ToString());
			return true;
		}

		ledger.Clear();
		if (ledger.Total() != 0 || ledger.LineCount() != 0)
		{
			SetFailure("After Clear() Total() is %1 and LineCount() is %2, expected 0 and 0", ledger.Total().ToString(), ledger.LineCount().ToString());
			return true;
		}

		ledger.Add(OVT_TEST_StorageLedgerFixture.RES_A, 2, -1);
		if (ledger.Total() != 2)
		{
			SetFailure("Total() is %1 after adding 2 to a cleared ledger, expected 2", ledger.Total().ToString());
			return true;
		}

		Print("Storage ledger total: maintained across mixed adds, takes, drains, rejected adds and Clear()");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! FreeSpace(): finite capacity gives cap - total and NEVER a negative number (an over-full holder
//! is a state the migration path can produce); unlimited gives a number no batch can exhaust.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageLedger_FreeSpace : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_StorageLedger ledger = new OVT_StorageLedger();

		if (ledger.FreeSpace(300) != 300)
		{
			SetFailure("An empty ledger reports %1 free of 300, expected 300", ledger.FreeSpace(300).ToString());
			return true;
		}

		ledger.Add(OVT_TEST_StorageLedgerFixture.RES_A, 120, 300);
		if (ledger.FreeSpace(300) != 180)
		{
			SetFailure("FreeSpace(300) with 120 held is %1, expected 180", ledger.FreeSpace(300).ToString());
			return true;
		}

		if (ledger.FreeSpace(-1) != int.MAX)
		{
			SetFailure("FreeSpace(-1) is %1, expected int.MAX for an unlimited holder", ledger.FreeSpace(-1).ToString());
			return true;
		}

		// 120 held against a capacity of 50: over-full, and free space is 0, never -70.
		if (ledger.FreeSpace(50) != 0)
		{
			SetFailure("FreeSpace(50) with 120 held is %1, expected 0 - free space is never negative", ledger.FreeSpace(50).ToString());
			return true;
		}

		if (ledger.FreeSpace(120) != 0)
		{
			SetFailure("FreeSpace(120) with 120 held is %1, expected exactly 0", ledger.FreeSpace(120).ToString());
			return true;
		}

		if (ledger.FreeSpace(0) != 0)
		{
			SetFailure("FreeSpace(0) is %1, expected 0 for a holder with no capacity at all", ledger.FreeSpace(0).ToString());
			return true;
		}

		Print("Storage ledger free space: cap minus total, clamped at 0, and int.MAX when unlimited");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Garbage in is a no-op, not a line. An empty key or a non-positive quantity reaches Add and Take
//! from the wire and from the migration path, and either would otherwise mint an unaddressable line
//! or move the total in the wrong direction.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_StorageLedger_IgnoresGarbage : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_StorageLedger ledger = new OVT_StorageLedger();
		ledger.Add(OVT_TEST_StorageLedgerFixture.RES_A, 10, -1);

		int fitted = ledger.Add("", 5, -1);
		if (fitted != 0)
		{
			SetFailure("Add() with an empty key fitted %1, expected 0", fitted.ToString());
			return true;
		}

		fitted = ledger.Add(OVT_TEST_StorageLedgerFixture.RES_B, 0, -1);
		if (fitted != 0)
		{
			SetFailure("Add(0) fitted %1, expected 0", fitted.ToString());
			return true;
		}

		fitted = ledger.Add(OVT_TEST_StorageLedgerFixture.RES_B, -7, -1);
		if (fitted != 0)
		{
			SetFailure("Add(-7) fitted %1, expected 0", fitted.ToString());
			return true;
		}

		if (ledger.LineCount() != 1 || ledger.Total() != 10)
		{
			SetFailure("Garbage adds left %1 lines totalling %2, expected the untouched 1 line of 10", ledger.LineCount().ToString(), ledger.Total().ToString());
			return true;
		}

		int taken = ledger.Take("", 1);
		if (taken != 0)
		{
			SetFailure("Take() with an empty key took %1, expected 0", taken.ToString());
			return true;
		}

		taken = ledger.Take(OVT_TEST_StorageLedgerFixture.RES_A, 0);
		if (taken != 0)
		{
			SetFailure("Take(0) took %1, expected 0", taken.ToString());
			return true;
		}

		taken = ledger.Take(OVT_TEST_StorageLedgerFixture.RES_A, -3);
		if (taken != 0)
		{
			SetFailure("Take(-3) took %1, expected 0", taken.ToString());
			return true;
		}

		if (ledger.Total() != 10 || ledger.Count(OVT_TEST_StorageLedgerFixture.RES_A) != 10)
		{
			SetFailure("Garbage takes left Total() %1 and line A at %2, expected 10 and 10", ledger.Total().ToString(), ledger.Count(OVT_TEST_StorageLedgerFixture.RES_A).ToString());
			return true;
		}

		Print("Storage ledger guards: empty key and non-positive quantity are no-ops on both Add and Take");

		return true;
	}
}
