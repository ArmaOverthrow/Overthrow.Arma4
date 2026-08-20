//------------------------------------------------------------------------------------------------
//! TIER A cases - the transfer screen's pure decisions.
//!
//! Every subject is built with `new` and fed hand-written values: the list model (sort, category
//! population, filtering) and the cart model (merge, clamp, drop-at-zero, totals, reconcile). None
//! of them touches a widget, a component or the world, which is why the transfer rework put them in
//! Scripts/Game/Data/ (implementation plan, D1).
//!
//! `new` applies no [Attribute()] defvalues, so every field every case depends on is set explicitly
//! by the fixture below, including ones whose declared default already matches.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Hand-built subjects for the transfer cases.
//------------------------------------------------------------------------------------------------
class OVT_TEST_TransferFixture
{
	//------------------------------------------------------------------------------------------------
	//! Builds one fully-populated transfer entry.
	//! \param[in] id Stable entry id.
	//! \param[in] displayName Name the sort orders by.
	//! \param[in] value Unit price or stock held.
	//! \param[in] maxQuantity Cap the cart clamps to.
	//! \param[in] categoryId Consumer-defined category id.
	//! \param[in] valueKind PRICE or QUANTITY.
	//! \return The entry, owned by the caller.
	static OVT_TransferEntry MakeEntry(string id, string displayName, int value, int maxQuantity, int categoryId, EOVT_TransferValueKind valueKind = EOVT_TransferValueKind.QUANTITY)
	{
		OVT_TransferEntry entry = new OVT_TransferEntry();
		entry.m_sId = id;
		entry.m_sDisplayName = displayName;
		entry.m_eImageKind = EOVT_TransferImageKind.PREFAB;
		entry.m_sImage = ResourceName.Empty;
		entry.m_iValue = value;
		entry.m_eValueKind = valueKind;
		entry.m_iMaxQuantity = maxQuantity;
		entry.m_iCategoryId = categoryId;
		entry.m_bEnabled = true;
		entry.m_sDisabledReasonKey = "";
		return entry;
	}
}

//------------------------------------------------------------------------------------------------
//! SortByDisplayName() orders case-insensitively and STABLY: two rows sharing a name (up to case)
//! keep the order they were added in, so a refresh never swaps them under the player's selection.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TransferModels_ListSortAlphabetical : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TransferListModel model = new OVT_TransferListModel();

		// Added out of order, and with two rows that differ only in case.
		model.Add(OVT_TEST_TransferFixture.MakeEntry("banana", "banana", 1, 10, 1));
		model.Add(OVT_TEST_TransferFixture.MakeEntry("apple_upper", "Apple", 1, 10, 1));
		model.Add(OVT_TEST_TransferFixture.MakeEntry("apple_lower", "apple", 1, 10, 1));
		model.Add(OVT_TEST_TransferFixture.MakeEntry("cherry", "Cherry", 1, 10, 1));

		model.SortByDisplayName();

		array<string> expected = {"apple_upper", "apple_lower", "banana", "cherry"};
		array<ref OVT_TransferEntry> entries = model.GetEntries();

		if (entries.Count() != expected.Count())
		{
			SetFailure("Sorted list holds %1 rows, expected %2", entries.Count().ToString(), expected.Count().ToString());
			return true;
		}

		for (int i = 0; i < expected.Count(); i++)
		{
			if (entries.Get(i).m_sId == expected.Get(i))
				continue;

			SetFailure("Sorted position %1 is '%2', expected '%3'", i.ToString(), entries.Get(i).m_sId, expected.Get(i));
			return true;
		}

		Print("Transfer list sort: case-insensitive alphabetical, and stable for rows sharing a name");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! GetPopulatedCategories() lists only ids that actually have rows, never lists one twice, never
//! returns CATEGORY_ALL even when a (contract-violating) row claims it, and returns them ASCENDING.
//!
//! Ascending matters twice: consumer category ids are declaration-ordered enums, so it is the tab
//! order the player already knows; and it is stable, so a refresh cannot silently reorder the tab
//! set and force the widgets to be rebuilt. The rows below are deliberately NOT added in ascending
//! category order, so a first-seen implementation returns [7, 3, 5] and fails here.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TransferModels_ListCategoryPopulation : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TransferListModel model = new OVT_TransferListModel();

		model.Add(OVT_TEST_TransferFixture.MakeEntry("a", "A", 1, 10, 7));
		model.Add(OVT_TEST_TransferFixture.MakeEntry("b", "B", 1, 10, 3));
		model.Add(OVT_TEST_TransferFixture.MakeEntry("c", "C", 1, 10, 7));
		model.Add(OVT_TEST_TransferFixture.MakeEntry("d", "D", 1, 10, OVT_TransferListModel.CATEGORY_ALL));
		model.Add(OVT_TEST_TransferFixture.MakeEntry("e", "E", 1, 10, 5));

		array<int> categories = new array<int>();
		model.GetPopulatedCategories(categories);

		if (categories.Count() != 3)
		{
			SetFailure("GetPopulatedCategories() returned %1 ids, expected 3", categories.Count().ToString());
			return true;
		}

		if (categories.Get(0) != 3 || categories.Get(1) != 5 || categories.Get(2) != 7)
		{
			SetFailure("GetPopulatedCategories() returned [%1, %2, %3], expected ascending [3, 5, 7]", categories.Get(0).ToString(), categories.Get(1).ToString(), categories.Get(2).ToString());
			return true;
		}

		if (categories.Contains(OVT_TransferListModel.CATEGORY_ALL))
		{
			SetFailure("GetPopulatedCategories() returned CATEGORY_ALL, which is the menu's own tab and never a populated id");
			return true;
		}

		// Category 9 has no rows, so it earns no tab.
		if (model.HasCategory(9))
		{
			SetFailure("HasCategory(9) is true, but no row is in category 9");
			return true;
		}

		if (!model.HasCategory(7))
		{
			SetFailure("HasCategory(7) is false, but two rows are in category 7");
			return true;
		}

		Print("Transfer list categories: populated ids only, ascending, no duplicates, CATEGORY_ALL never returned");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! FilterByCategory(): CATEGORY_ALL returns everything, a real id returns only its own rows, and
//! both preserve the model's current (sorted) order.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TransferModels_ListFilterByCategory : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TransferListModel model = new OVT_TransferListModel();

		model.Add(OVT_TEST_TransferFixture.MakeEntry("one", "One", 1, 10, 1));
		model.Add(OVT_TEST_TransferFixture.MakeEntry("two", "Two", 1, 10, 2));
		model.Add(OVT_TEST_TransferFixture.MakeEntry("three", "Three", 1, 10, 1));

		array<ref OVT_TransferEntry> filtered = new array<ref OVT_TransferEntry>();

		model.FilterByCategory(OVT_TransferListModel.CATEGORY_ALL, filtered);
		if (filtered.Count() != 3)
		{
			SetFailure("Filter by CATEGORY_ALL returned %1 rows, expected 3", filtered.Count().ToString());
			return true;
		}

		if (filtered.Get(0).m_sId != "one" || filtered.Get(2).m_sId != "three")
		{
			SetFailure("Filter by CATEGORY_ALL reordered the list: first is '%1', last is '%2'", filtered.Get(0).m_sId, filtered.Get(2).m_sId);
			return true;
		}

		model.FilterByCategory(1, filtered);
		if (filtered.Count() != 2)
		{
			SetFailure("Filter by category 1 returned %1 rows, expected 2", filtered.Count().ToString());
			return true;
		}

		if (filtered.Get(0).m_sId != "one" || filtered.Get(1).m_sId != "three")
		{
			SetFailure("Filter by category 1 returned ['%1', '%2'], expected ['one', 'three']", filtered.Get(0).m_sId, filtered.Get(1).m_sId);
			return true;
		}

		model.FilterByCategory(9, filtered);
		if (filtered.Count() != 0)
		{
			SetFailure("Filter by unpopulated category 9 returned %1 rows, expected 0", filtered.Count().ToString());
			return true;
		}

		Print("Transfer list filter: CATEGORY_ALL passes everything, a real id passes only its own, order preserved");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Adding the same entry twice yields ONE line carrying the summed quantity, and a different entry
//! gets a line of its own.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TransferModels_CartAddMerges : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TransferEntry rifle = OVT_TEST_TransferFixture.MakeEntry("rifle", "Rifle", 100, 100, 1, EOVT_TransferValueKind.PRICE);
		OVT_TransferEntry ammo = OVT_TEST_TransferFixture.MakeEntry("ammo", "Ammo", 10, 100, 2, EOVT_TransferValueKind.PRICE);

		OVT_TransferCartModel cart = new OVT_TransferCartModel();
		cart.Add(rifle, 3);
		cart.Add(rifle, 4);

		if (cart.Count() != 1)
		{
			SetFailure("Adding one id twice produced %1 lines, expected 1", cart.Count().ToString());
			return true;
		}

		if (cart.GetQuantity("rifle") != 7)
		{
			SetFailure("Merged line holds %1, expected 7", cart.GetQuantity("rifle").ToString());
			return true;
		}

		cart.Add(ammo, 5);
		if (cart.Count() != 2)
		{
			SetFailure("Adding a second id produced %1 lines, expected 2", cart.Count().ToString());
			return true;
		}

		// A null entry and a non-positive quantity are ignored, not stored.
		cart.Add(null, 5);
		cart.Add(rifle, 0);
		cart.Add(rifle, -3);

		if (cart.Count() != 2 || cart.GetQuantity("rifle") != 7)
		{
			SetFailure("Null / non-positive adds changed the cart: %1 lines, rifle holds %2", cart.Count().ToString(), cart.GetQuantity("rifle").ToString());
			return true;
		}

		// A cap of zero clamps the add to zero, and a zero-quantity line must never be inserted: only
		// Clear() could ever drop it, and it would draw "x0" while enabling Accept.
		OVT_TransferEntry outOfStock = OVT_TEST_TransferFixture.MakeEntry("out_of_stock", "Out Of Stock", 4, 0, 3);
		cart.Add(outOfStock, 5);

		if (cart.Count() != 2 || cart.FindLineIndex("out_of_stock") != -1)
		{
			SetFailure("Adding an entry whose cap is 0 inserted a zero-quantity line: %1 lines, out_of_stock sits at index %2", cart.Count().ToString(), cart.FindLineIndex("out_of_stock").ToString());
			return true;
		}

		Print("Transfer cart add: merges by id, one line per id, null / non-positive / zero-cap adds ignored");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A merged total above the entry's cap is CLAMPED, not rejected - the cart keeps the maximum the
//! player can actually take.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TransferModels_CartClampsToMax : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TransferEntry crate = OVT_TEST_TransferFixture.MakeEntry("crate", "Crate", 5, 10, 1);

		OVT_TransferCartModel cart = new OVT_TransferCartModel();
		cart.Add(crate, 6);
		cart.Add(crate, 9);

		if (cart.Count() != 1)
		{
			SetFailure("Clamped add produced %1 lines, expected 1", cart.Count().ToString());
			return true;
		}

		if (cart.GetQuantity("crate") != 10)
		{
			SetFailure("Merged total was clamped to %1, expected the cap of 10", cart.GetQuantity("crate").ToString());
			return true;
		}

		// A single oversized add clamps the same way.
		OVT_TransferCartModel single = new OVT_TransferCartModel();
		single.Add(crate, 40);

		if (single.GetQuantity("crate") != 10)
		{
			SetFailure("A single oversized add produced %1, expected the cap of 10", single.GetQuantity("crate").ToString());
			return true;
		}

		Print("Transfer cart clamp: a merged total over the cap is clamped to the cap, never rejected");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! AddAll() TOPS THE LINE UP to exactly the cap - it does not add the cap on top of what is already
//! ordered.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TransferModels_CartAddAll : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TransferEntry sandbag = OVT_TEST_TransferFixture.MakeEntry("sandbag", "Sandbag", 2, 10, 1);

		OVT_TransferCartModel cart = new OVT_TransferCartModel();
		cart.Add(sandbag, 4);
		cart.AddAll(sandbag);

		if (cart.Count() != 1)
		{
			SetFailure("AddAll produced %1 lines, expected 1", cart.Count().ToString());
			return true;
		}

		if (cart.GetQuantity("sandbag") != 10)
		{
			SetFailure("AddAll on a line of 4 with a cap of 10 gave %1, expected exactly 10", cart.GetQuantity("sandbag").ToString());
			return true;
		}

		// AddAll on an empty cart opens the line at the cap.
		OVT_TransferCartModel fresh = new OVT_TransferCartModel();
		fresh.AddAll(sandbag);

		if (fresh.GetQuantity("sandbag") != 10)
		{
			SetFailure("AddAll on an empty cart gave %1, expected the cap of 10", fresh.GetQuantity("sandbag").ToString());
			return true;
		}

		Print("Transfer cart AddAll: tops the line up to exactly the cap");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Removing to zero (or past it) DELETES the line rather than leaving a zero-quantity row, and
//! RemoveAll() deletes it outright.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TransferModels_CartRemoveDropsLine : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TransferEntry rope = OVT_TEST_TransferFixture.MakeEntry("rope", "Rope", 3, 50, 1);

		OVT_TransferCartModel cart = new OVT_TransferCartModel();
		cart.Add(rope, 5);
		cart.Remove("rope", 2);

		if (cart.GetQuantity("rope") != 3)
		{
			SetFailure("Removing 2 of 5 left %1, expected 3", cart.GetQuantity("rope").ToString());
			return true;
		}

		cart.Remove("rope", 3);
		if (cart.Count() != 0)
		{
			SetFailure("Removing down to zero left %1 lines, expected 0", cart.Count().ToString());
			return true;
		}

		if (cart.FindLineIndex("rope") != -1)
		{
			SetFailure("A line removed to zero is still findable at index %1", cart.FindLineIndex("rope").ToString());
			return true;
		}

		// Over-removing drops the line too, and never leaves a negative quantity behind.
		cart.Add(rope, 4);
		cart.Remove("rope", 99);

		if (cart.Count() != 0)
		{
			SetFailure("Over-removing left %1 lines, expected 0", cart.Count().ToString());
			return true;
		}

		cart.Add(rope, 4);
		cart.RemoveAll("rope");

		if (cart.Count() != 0)
		{
			SetFailure("RemoveAll left %1 lines, expected 0", cart.Count().ToString());
			return true;
		}

		Print("Transfer cart remove: reaching zero drops the line, over-removing drops it, RemoveAll drops it");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Dropping a line NEVER reorders the rest. array.Remove() backfills the hole with the LAST element
//! (Types.c:258-263), which would teleport an unrelated line under the menu's remembered cart index
//! and re-detail the wrong item; every drop site must use RemoveOrdered.
//!
//! Four lines, and never the second-to-last one dropped: with three lines the swap-removal and the
//! ordered removal produce the same array, so a three-line fixture could not fail.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TransferModels_CartRemoveKeepsOrder : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] cart The cart to describe.
	//! \return Its line ids, comma-separated, in cart order.
	protected string Order(OVT_TransferCartModel cart)
	{
		string joined = "";

		foreach (OVT_TransferCartLine line : cart.GetLines())
		{
			if (joined != "")
				joined = joined + ",";

			joined = joined + line.m_sId;
		}

		return joined;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A fresh four-line cart, a/b/c/d in add order.
	protected OVT_TransferCartModel Fill()
	{
		OVT_TransferCartModel cart = new OVT_TransferCartModel();
		cart.Add(OVT_TEST_TransferFixture.MakeEntry("a", "A", 1, 10, 1), 2);
		cart.Add(OVT_TEST_TransferFixture.MakeEntry("b", "B", 1, 10, 1), 2);
		cart.Add(OVT_TEST_TransferFixture.MakeEntry("c", "C", 1, 10, 1), 2);
		cart.Add(OVT_TEST_TransferFixture.MakeEntry("d", "D", 1, 10, 1), 2);
		return cart;
	}

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TransferCartModel cart = Fill();
		cart.RemoveAll("b");

		if (Order(cart) != "a,c,d")
		{
			SetFailure("RemoveAll on the second of four lines left the cart as [%1], expected [a,c,d]", Order(cart));
			return true;
		}

		cart = Fill();
		cart.Remove("b", 2);

		if (Order(cart) != "a,c,d")
		{
			SetFailure("Removing the second of four lines to zero left the cart as [%1], expected [a,c,d]", Order(cart));
			return true;
		}

		// Reconcile drops a line whose entry vanished from the refreshed list...
		cart = Fill();

		OVT_TransferListModel vanished = new OVT_TransferListModel();
		vanished.Add(OVT_TEST_TransferFixture.MakeEntry("a", "A", 1, 10, 1));
		vanished.Add(OVT_TEST_TransferFixture.MakeEntry("c", "C", 1, 10, 1));
		vanished.Add(OVT_TEST_TransferFixture.MakeEntry("d", "D", 1, 10, 1));

		cart.Reconcile(vanished);

		if (Order(cart) != "a,c,d")
		{
			SetFailure("Reconcile dropping a vanished second line left the cart as [%1], expected [a,c,d]", Order(cart));
			return true;
		}

		// ...and one whose cap fell to zero.
		cart = Fill();

		OVT_TransferListModel drained = new OVT_TransferListModel();
		drained.Add(OVT_TEST_TransferFixture.MakeEntry("a", "A", 1, 10, 1));
		drained.Add(OVT_TEST_TransferFixture.MakeEntry("b", "B", 1, 0, 1));
		drained.Add(OVT_TEST_TransferFixture.MakeEntry("c", "C", 1, 10, 1));
		drained.Add(OVT_TEST_TransferFixture.MakeEntry("d", "D", 1, 10, 1));

		cart.Reconcile(drained);

		if (Order(cart) != "a,c,d")
		{
			SetFailure("Reconcile clamping the second line to zero left the cart as [%1], expected [a,c,d]", Order(cart));
			return true;
		}

		Print("Transfer cart order: all four drop sites keep the surviving lines in add order");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! TotalQuantity() sums quantities and TotalValue() sums quantity x unit value over mixed lines;
//! an empty cart is 0 / 0.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TransferModels_CartTotals : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TransferCartModel cart = new OVT_TransferCartModel();

		if (cart.TotalQuantity() != 0 || cart.TotalValue() != 0)
		{
			SetFailure("An empty cart totals %1 items / %2 value, expected 0 / 0", cart.TotalQuantity().ToString(), cart.TotalValue().ToString());
			return true;
		}

		OVT_TransferEntry radio = OVT_TEST_TransferFixture.MakeEntry("radio", "Radio", 25, 100, 1, EOVT_TransferValueKind.PRICE);
		OVT_TransferEntry mag = OVT_TEST_TransferFixture.MakeEntry("mag", "Magazine", 10, 100, 2, EOVT_TransferValueKind.PRICE);

		cart.Add(radio, 2);
		cart.Add(mag, 3);

		if (cart.TotalQuantity() != 5)
		{
			SetFailure("TotalQuantity() is %1, expected 5", cart.TotalQuantity().ToString());
			return true;
		}

		if (cart.TotalValue() != 80)
		{
			SetFailure("TotalValue() is %1, expected 80 (2 x 25 + 3 x 10)", cart.TotalValue().ToString());
			return true;
		}

		// Removing a line takes its contribution with it.
		cart.RemoveAll("mag");

		if (cart.TotalQuantity() != 2 || cart.TotalValue() != 50)
		{
			SetFailure("After dropping a line the cart totals %1 items / %2 value, expected 2 / 50", cart.TotalQuantity().ToString(), cart.TotalValue().ToString());
			return true;
		}

		Print("Transfer cart totals: quantity sums, value is quantity x unit value, empty cart is 0/0");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Reconcile() against a refreshed list: a line whose entry vanished is dropped, a line whose cap
//! fell is clamped to the new cap, an unchanged line keeps its quantity, and every surviving line
//! re-reads the display fields it COPIED at add time (D2).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TransferModels_CartReconcile : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The cart is built against the stock the player first saw.
		OVT_TransferEntry keptOld = OVT_TEST_TransferFixture.MakeEntry("kept", "Kept", 4, 10, 1);
		OVT_TransferEntry goneOld = OVT_TEST_TransferFixture.MakeEntry("gone", "Gone", 4, 10, 1);
		OVT_TransferEntry drainedOld = OVT_TEST_TransferFixture.MakeEntry("drained", "Drained", 4, 50, 1);

		OVT_TransferCartModel cart = new OVT_TransferCartModel();
		cart.Add(keptOld, 3);
		cart.Add(goneOld, 2);
		cart.Add(drainedOld, 50);

		// Someone else emptied the warehouse between refreshes, and the kept row was repriced and
		// renamed at the same time.
		OVT_TransferListModel refreshed = new OVT_TransferListModel();
		refreshed.Add(OVT_TEST_TransferFixture.MakeEntry("kept", "Kept (repriced)", 9, 10, 1));
		refreshed.Add(OVT_TEST_TransferFixture.MakeEntry("drained", "Drained", 4, 4, 1));

		cart.Reconcile(refreshed);

		if (cart.Count() != 2)
		{
			SetFailure("After Reconcile the cart holds %1 lines, expected 2", cart.Count().ToString());
			return true;
		}

		if (cart.FindLineIndex("gone") != -1)
		{
			SetFailure("Reconcile kept a line whose entry vanished from the list");
			return true;
		}

		if (cart.GetQuantity("drained") != 4)
		{
			SetFailure("Reconcile left the drained line at %1, expected it clamped to the new cap of 4", cart.GetQuantity("drained").ToString());
			return true;
		}

		if (cart.GetQuantity("kept") != 3)
		{
			SetFailure("Reconcile changed an unaffected line's quantity to %1, expected it untouched at 3", cart.GetQuantity("kept").ToString());
			return true;
		}

		// A line copies its display fields, so Reconcile is the ONLY path by which a repriced entry
		// reaches the cart - and TotalValue() is what the port's affordability gate reads.
		OVT_TransferCartLine kept = cart.GetLines().Get(cart.FindLineIndex("kept"));

		if (kept.m_iUnitValue != 9 || kept.m_sDisplayName != "Kept (repriced)")
		{
			SetFailure("Reconcile left the kept line's copied display fields stale: unit value %1, name '%2'", kept.m_iUnitValue.ToString(), kept.m_sDisplayName);
			return true;
		}

		if (cart.TotalValue() != 43)
		{
			SetFailure("After Reconcile TotalValue() is %1, expected 43 (3 x 9 kept + 4 x 4 drained)", cart.TotalValue().ToString());
			return true;
		}

		Print("Transfer cart reconcile: vanished entry drops the line, lowered cap clamps it, quantities kept, copied display fields refreshed");

		return true;
	}
}
