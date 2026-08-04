//------------------------------------------------------------------------------------------------
//! TIER A cases - the shop browser's pure decisions.
//!
//! Every subject here is built with `new` and fed hand-written values: the category mapping helper,
//! the browser model, the sell rules, the money formatter and the HUD delta tracker. None of them
//! touches a widget, a component or the world, which is exactly why the shop rework put them in
//! Scripts/Game/Data/ in the first place (implementation plan, D1).
//!
//! Two of these cases exist because of a specific defect:
//!  - CategoryMapping pins the MODE-BEFORE-TYPE ordering. A rifle magazine is catalogued as type
//!    RIFLE with mode AMMUNITION, so a helper that checks the type first files magazines under
//!    Weapons - which is the user-visible complaint in issue #145.
//!  - Pagination pins BUG-024. The legacy menu computed its page count with integer division
//!    (57 / 15 = 3), leaving the last 12 items of a full shop permanently unreachable.
//!
//! `new` applies no [Attribute()] defvalues, so every field every case depends on is set explicitly,
//! including OVT_MoneyDeltaTracker's reset timeout (the tracker's constructor sets it, and the case
//! overrides it anyway so the timing assertions do not silently follow a future default change).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! OVT_ShopCategoryHelper.GetCategory() - the mode is read before the type, and every catalogued
//! type lands in the tab the plan's mapping table names.
//!
//! The three RIFLE rows at the top are the whole feature in miniature: one prefab type, three modes,
//! three different tabs. Everything after them is coverage of the table, plus the catch-all.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ShopUX_CategoryMapping : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// --- MODE BEATS TYPE. Same type, three modes, three tabs.
		if (!ExpectCategory(SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.AMMUNITION, OVT_ShopCategory.AMMUNITION, "a rifle magazine (RIFLE / AMMUNITION)"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON, OVT_ShopCategory.WEAPONS, "a rifle (RIFLE / WEAPON)"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.ATTACHMENT, OVT_ShopCategory.ATTACHMENTS, "a rifle optic (RIFLE / ATTACHMENT)"))
			return true;

		// Ammunition mode wins over an explosive type too, not just over a weapon type.
		if (!ExpectCategory(SCR_EArsenalItemType.MORTARS, SCR_EArsenalItemMode.AMMUNITION, OVT_ShopCategory.AMMUNITION, "a mortar shell (MORTARS / AMMUNITION)"))
			return true;

		// --- WEAPONS.
		if (!ExpectCategory(SCR_EArsenalItemType.PISTOL, SCR_EArsenalItemMode.WEAPON, OVT_ShopCategory.WEAPONS, "a pistol"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.MACHINE_GUN, SCR_EArsenalItemMode.WEAPON, OVT_ShopCategory.WEAPONS, "a machine gun"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.SNIPER_RIFLE, SCR_EArsenalItemMode.WEAPON, OVT_ShopCategory.WEAPONS, "a sniper rifle"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.ROCKET_LAUNCHER, SCR_EArsenalItemMode.WEAPON, OVT_ShopCategory.WEAPONS, "a rocket launcher"))
			return true;

		// --- ATTACHMENTS, by type as well as by mode.
		if (!ExpectCategory(SCR_EArsenalItemType.WEAPON_ATTACHMENT, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.ATTACHMENTS, "a bare attachment type"))
			return true;

		// --- MEDICAL, by type as well as by mode.
		if (!ExpectCategory(SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.MEDICAL, "a bandage (HEAL)"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.CONSUMABLE, OVT_ShopCategory.MEDICAL, "a consumable (EQUIPMENT / CONSUMABLE)"))
			return true;

		// --- EXPLOSIVES.
		if (!ExpectCategory(SCR_EArsenalItemType.LETHAL_THROWABLE, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.EXPLOSIVES, "a frag grenade"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.NON_LETHAL_THROWABLE, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.EXPLOSIVES, "a smoke grenade"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.EXPLOSIVES, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.EXPLOSIVES, "a demolition charge"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.MORTARS, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.EXPLOSIVES, "a mortar"))
			return true;

		// --- CLOTHING.
		if (!ExpectCategory(SCR_EArsenalItemType.HEADWEAR, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.CLOTHING, "a helmet"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.TORSO, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.CLOTHING, "a jacket"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.VEST_AND_WAIST, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.CLOTHING, "a vest"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.LEGS, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.CLOTHING, "trousers"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.FOOTWEAR, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.CLOTHING, "boots"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.HANDWEAR, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.CLOTHING, "gloves"))
			return true;

		// --- GEAR.
		if (!ExpectCategory(SCR_EArsenalItemType.BACKPACK, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.GEAR, "a backpack"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.RADIO_BACKPACK, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.GEAR, "a radio backpack"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.GEAR, "a piece of equipment"))
			return true;

		// --- OTHER. Vehicles deliberately do not earn a tab of their own.
		if (!ExpectCategory(SCR_EArsenalItemType.VEHICLE, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.OTHER, "a vehicle"))
			return true;

		if (!ExpectCategory(SCR_EArsenalItemType.HELICOPTER, SCR_EArsenalItemMode.DEFAULT, OVT_ShopCategory.OTHER, "a helicopter"))
			return true;

		// An item with no arsenal data at all is not a browse category question - it is the fallback.
		if (OVT_ShopCategoryHelper.GetCategoryForUncatalogued() != OVT_ShopCategory.OTHER)
		{
			SetResultFailure("An uncatalogued item mapped to %1, expected OTHER",
				typename.EnumToString(OVT_ShopCategory, OVT_ShopCategoryHelper.GetCategoryForUncatalogued()));
			return true;
		}

		// --- DISPLAY ORDER: ALL first, OTHER last, every category present exactly once.
		array<OVT_ShopCategory> order = new array<OVT_ShopCategory>();
		OVT_ShopCategoryHelper.GetDisplayOrder(order);

		if (order.Count() != 9)
		{
			SetResultFailure("GetDisplayOrder() returned %1 categories, expected 9", order.Count().ToString());
			return true;
		}

		if (order.Get(0) != OVT_ShopCategory.ALL)
		{
			SetResultFailure("GetDisplayOrder() starts with %1, expected ALL",
				typename.EnumToString(OVT_ShopCategory, order.Get(0)));
			return true;
		}

		if (order.Get(order.Count() - 1) != OVT_ShopCategory.OTHER)
		{
			SetResultFailure("GetDisplayOrder() ends with %1, expected OTHER",
				typename.EnumToString(OVT_ShopCategory, order.Get(order.Count() - 1)));
			return true;
		}

		// Every label is a distinct, non-empty #OVT- key, so no two tabs can read the same.
		array<string> seenKeys = new array<string>();
		foreach (OVT_ShopCategory category : order)
		{
			string key = OVT_ShopCategoryHelper.GetLabelKey(category);

			if (key == "" || !key.StartsWith("#OVT-"))
			{
				SetResultFailure("Category %1 has label key '%2', which is not an #OVT- key",
					typename.EnumToString(OVT_ShopCategory, category), key);
				return true;
			}

			if (seenKeys.Contains(key))
			{
				SetResultFailure("Label key '%1' is used by more than one category", key);
				return true;
			}

			seenKeys.Insert(key);
		}

		Print("Shop categories: mode beats type (RIFLE maps to AMMUNITION / WEAPONS / ATTACHMENTS by mode), full table as specified, ALL first and OTHER last");

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one row of the mapping table, naming the row in the failure message.
	//! \param[in] type Arsenal item type.
	//! \param[in] mode Arsenal item mode.
	//! \param[in] expected The category the plan's table specifies.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when the mapping matched; false after recording the failure.
	protected bool ExpectCategory(SCR_EArsenalItemType type, SCR_EArsenalItemMode mode, OVT_ShopCategory expected, string label)
	{
		OVT_ShopCategory actual = OVT_ShopCategoryHelper.GetCategory(type, mode);

		if (actual == expected)
			return true;

		SetResultFailure("%1 mapped to %2, expected %3", label,
			typename.EnumToString(OVT_ShopCategory, actual),
			typename.EnumToString(OVT_ShopCategory, expected));

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_ShopBrowserModel - alphabetical, case-insensitive, STABLE ordering, plus category filtering
//! and the populated-category list the tab row is built from.
//!
//! Stability is asserted with two rows whose names differ only in case: they must come out in the
//! order they were added. It matters because the sell model aggregates by resource id and two
//! variants can share a display name; an unstable sort would move the player's selection under their
//! cursor on every refresh.
//!
//! GetPopulatedCategories() must omit empty categories (a clothes shop shows no Weapons tab) and
//! must never include ALL, which the menu adds itself as a permanent first tab.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ShopUX_BrowserModelSortAndFilter : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_ShopBrowserModel model = new OVT_ShopBrowserModel();

		// Deliberately added out of order, with mixed casing and one case-only duplicate pair.
		model.Add(MakeItem(1, "Zebra Vest", OVT_ShopCategory.CLOTHING));
		model.Add(MakeItem(2, "apple magazine", OVT_ShopCategory.AMMUNITION));
		model.Add(MakeItem(3, "Apple magazine", OVT_ShopCategory.AMMUNITION));
		model.Add(MakeItem(4, "banana rifle", OVT_ShopCategory.WEAPONS));
		model.Add(MakeItem(5, "Cherry scope", OVT_ShopCategory.ATTACHMENTS));

		if (model.Count() != 5)
		{
			SetResultFailure("The model holds %1 rows after five Add() calls, expected 5", model.Count().ToString());
			return true;
		}

		model.SortByDisplayName();

		array<ref OVT_ShopBrowserItem> sorted = model.GetItems();

		// Case-insensitive alphabetical: apple, Apple, banana, Cherry, Zebra. A case-SENSITIVE sort
		// would put every capitalised name first and produce 3,5,1,2,4.
		array<int> expectedIds = {2, 3, 4, 5, 1};

		for (int i = 0; i < expectedIds.Count(); i++)
		{
			if (sorted.Get(i).m_iResourceId != expectedIds.Get(i))
			{
				SetResultFailure("After sorting, position %1 holds id %2, expected id %3",
					i.ToString(), sorted.Get(i).m_iResourceId.ToString(), expectedIds.Get(i).ToString());
				return true;
			}
		}

		// --- FILTERING.
		array<ref OVT_ShopBrowserItem> ammo = new array<ref OVT_ShopBrowserItem>();
		model.FilterByCategory(OVT_ShopCategory.AMMUNITION, ammo);

		if (ammo.Count() != 2)
		{
			SetResultFailure("Filtering by AMMUNITION returned %1 rows, expected 2", ammo.Count().ToString());
			return true;
		}

		foreach (OVT_ShopBrowserItem item : ammo)
		{
			if (item.m_eCategory != OVT_ShopCategory.AMMUNITION)
			{
				SetResultFailure("Filtering by AMMUNITION returned '%1', which is %2",
					item.m_sDisplayName, typename.EnumToString(OVT_ShopCategory, item.m_eCategory));
				return true;
			}
		}

		array<ref OVT_ShopBrowserItem> everything = new array<ref OVT_ShopBrowserItem>();
		model.FilterByCategory(OVT_ShopCategory.ALL, everything);

		if (everything.Count() != 5)
		{
			SetResultFailure("Filtering by ALL returned %1 rows, expected all 5", everything.Count().ToString());
			return true;
		}

		// A category nobody stocked comes back empty rather than coming back wrong.
		array<ref OVT_ShopBrowserItem> medical = new array<ref OVT_ShopBrowserItem>();
		model.FilterByCategory(OVT_ShopCategory.MEDICAL, medical);

		if (medical.Count() != 0)
		{
			SetResultFailure("Filtering by MEDICAL returned %1 rows, expected 0", medical.Count().ToString());
			return true;
		}

		// --- POPULATED CATEGORIES: four of them, in display order, no ALL, no empties.
		array<OVT_ShopCategory> populated = new array<OVT_ShopCategory>();
		model.GetPopulatedCategories(populated);

		array<OVT_ShopCategory> expectedCategories = {OVT_ShopCategory.WEAPONS, OVT_ShopCategory.AMMUNITION, OVT_ShopCategory.ATTACHMENTS, OVT_ShopCategory.CLOTHING};

		if (populated.Count() != expectedCategories.Count())
		{
			SetResultFailure("GetPopulatedCategories() returned %1 categories, expected %2",
				populated.Count().ToString(), expectedCategories.Count().ToString());
			return true;
		}

		for (int c = 0; c < expectedCategories.Count(); c++)
		{
			if (populated.Get(c) != expectedCategories.Get(c))
			{
				SetResultFailure("GetPopulatedCategories() position %1 is %2, expected %3", c.ToString(),
					typename.EnumToString(OVT_ShopCategory, populated.Get(c)),
					typename.EnumToString(OVT_ShopCategory, expectedCategories.Get(c)));
				return true;
			}
		}

		if (populated.Contains(OVT_ShopCategory.ALL))
		{
			SetResultFailure("GetPopulatedCategories() included ALL, which is the menu's own permanent tab");
			return true;
		}

		if (populated.Contains(OVT_ShopCategory.MEDICAL) || populated.Contains(OVT_ShopCategory.GEAR))
		{
			SetResultFailure("GetPopulatedCategories() included a category with no rows in it");
			return true;
		}

		Print("Shop browser model: case-insensitive stable alphabetical order, category filtering, ALL returns everything, empty categories earn no tab");

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one browser row. Every field is set explicitly - `new` applies no attribute defaults.
	//! \param[in] id Resource id.
	//! \param[in] displayName Name the sort orders by.
	//! \param[in] category Category the filter selects on.
	//! \return The row.
	protected OVT_ShopBrowserItem MakeItem(int id, string displayName, OVT_ShopCategory category)
	{
		OVT_ShopBrowserItem item = new OVT_ShopBrowserItem();
		item.m_iResourceId = id;
		item.m_sResource = ResourceName.Empty;
		item.m_sDisplayName = displayName;
		item.m_eCategory = category;
		item.m_iUnitPrice = 100;
		item.m_iQuantity = 1;
		item.m_bEnabled = true;
		item.m_sDisabledReasonKey = "";
		return item;
	}
}

//------------------------------------------------------------------------------------------------
//! PINS BUG-024. Pagination arithmetic: page counts, page clamping and slice bounds.
//!
//! The legacy shop menu divided the row count by the page size in INTEGER arithmetic, so a 57-item
//! shop reported 3 pages and its last 12 items could not be reached by any amount of clicking Next;
//! the same code path could also ask for a negative slice index from the last page. Both numbers now
//! come from this model, and this case is what keeps them honest.
//!
//! Rows are added in id order and NOT sorted, so "the 46th row" is unambiguously id 46 and a slice
//! that is off by one page or by one item is visible immediately.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ShopUX_Pagination : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		int PER_PAGE = 15;

		// --- THE BUG-024 NUMBERS, straight off the arithmetic.
		if (OVT_ShopBrowserModel.GetPageCount(57, PER_PAGE) != 4)
		{
			SetResultFailure("BUG-024: 57 items at 15 per page reported %1 pages, expected 4 (integer division gives the broken 3)",
				OVT_ShopBrowserModel.GetPageCount(57, PER_PAGE).ToString());
			return true;
		}

		if (OVT_ShopBrowserModel.GetPageCount(3, PER_PAGE) != 1)
		{
			SetResultFailure("3 items at 15 per page reported %1 pages, expected 1", OVT_ShopBrowserModel.GetPageCount(3, PER_PAGE).ToString());
			return true;
		}

		if (OVT_ShopBrowserModel.GetPageCount(0, PER_PAGE) != 1)
		{
			SetResultFailure("An empty shop reported %1 pages, expected 1 (the label must never read 1/0)",
				OVT_ShopBrowserModel.GetPageCount(0, PER_PAGE).ToString());
			return true;
		}

		if (OVT_ShopBrowserModel.GetPageCount(15, PER_PAGE) != 1)
		{
			SetResultFailure("Exactly one full page reported %1 pages, expected 1", OVT_ShopBrowserModel.GetPageCount(15, PER_PAGE).ToString());
			return true;
		}

		if (OVT_ShopBrowserModel.GetPageCount(16, PER_PAGE) != 2)
		{
			SetResultFailure("One item past a full page reported %1 pages, expected 2", OVT_ShopBrowserModel.GetPageCount(16, PER_PAGE).ToString());
			return true;
		}

		// --- PAGE INDEX CLAMPS AT BOTH ENDS.
		if (OVT_ShopBrowserModel.ClampPage(-1, 4) != 0)
		{
			SetResultFailure("Prev on the first page produced page %1, expected 0", OVT_ShopBrowserModel.ClampPage(-1, 4).ToString());
			return true;
		}

		if (OVT_ShopBrowserModel.ClampPage(9, 4) != 3)
		{
			SetResultFailure("Next on the last page produced page %1, expected 3", OVT_ShopBrowserModel.ClampPage(9, 4).ToString());
			return true;
		}

		if (OVT_ShopBrowserModel.ClampPage(2, 4) != 2)
		{
			SetResultFailure("An in-range page was moved to %1, expected 2", OVT_ShopBrowserModel.ClampPage(2, 4).ToString());
			return true;
		}

		if (OVT_ShopBrowserModel.ClampPage(0, 0) != 0)
		{
			SetResultFailure("Clamping against a zero page count produced %1, expected 0", OVT_ShopBrowserModel.ClampPage(0, 0).ToString());
			return true;
		}

		// --- THE SLICE. 57 rows, ids 1..57, unsorted so position and id line up.
		OVT_ShopBrowserModel model = new OVT_ShopBrowserModel();
		for (int i = 1; i <= 57; i++)
		{
			model.Add(MakeItem(i, OVT_ShopCategory.WEAPONS));
		}

		array<ref OVT_ShopBrowserItem> page = new array<ref OVT_ShopBrowserItem>();

		model.GetPageItems(OVT_ShopCategory.ALL, 0, PER_PAGE, page);
		if (page.Count() != 15 || page.Get(0).m_iResourceId != 1 || page.Get(14).m_iResourceId != 15)
		{
			SetResultFailure("Page 1 held %1 rows starting at id %2, expected 15 rows starting at id 1",
				page.Count().ToString(), page.Get(0).m_iResourceId.ToString());
			return true;
		}

		// THE PAGE THE OLD ARITHMETIC COULD NOT REACH: 12 rows, ids 46..57.
		model.GetPageItems(OVT_ShopCategory.ALL, 3, PER_PAGE, page);
		if (page.Count() != 12)
		{
			SetResultFailure("BUG-024: the last page held %1 rows, expected the remaining 12", page.Count().ToString());
			return true;
		}

		if (page.Get(0).m_iResourceId != 46 || page.Get(11).m_iResourceId != 57)
		{
			SetResultFailure("The last page ran from id %1 to id %2, expected 46 to 57",
				page.Get(0).m_iResourceId.ToString(), page.Get(11).m_iResourceId.ToString());
			return true;
		}

		// --- OUT-OF-RANGE REQUESTS ARE CLAMPED, NOT CRASHED.
		model.GetPageItems(OVT_ShopCategory.ALL, 99, PER_PAGE, page);
		if (page.Count() != 12 || page.Get(0).m_iResourceId != 46)
		{
			SetResultFailure("Asking for page 99 returned %1 rows starting at id %2, expected the clamped last page (12 rows from id 46)",
				page.Count().ToString(), page.Get(0).m_iResourceId.ToString());
			return true;
		}

		model.GetPageItems(OVT_ShopCategory.ALL, -5, PER_PAGE, page);
		if (page.Count() != 15 || page.Get(0).m_iResourceId != 1)
		{
			SetResultFailure("Asking for page -5 returned %1 rows starting at id %2, expected the clamped first page (15 rows from id 1)",
				page.Count().ToString(), page.Get(0).m_iResourceId.ToString());
			return true;
		}

		// A tab with nothing in it pages safely instead of slicing an empty array.
		model.GetPageItems(OVT_ShopCategory.MEDICAL, 2, PER_PAGE, page);
		if (page.Count() != 0)
		{
			SetResultFailure("An empty category returned %1 rows on page 3, expected 0", page.Count().ToString());
			return true;
		}

		// And an empty model does the same.
		OVT_ShopBrowserModel emptyModel = new OVT_ShopBrowserModel();
		emptyModel.GetPageItems(OVT_ShopCategory.ALL, 0, PER_PAGE, page);
		if (page.Count() != 0)
		{
			SetResultFailure("An empty model returned %1 rows, expected 0", page.Count().ToString());
			return true;
		}

		// Paging is PER TAB: 20 of the 57 rows are ammunition, so that tab is two pages of its own.
		OVT_ShopBrowserModel mixed = new OVT_ShopBrowserModel();
		for (int a = 1; a <= 20; a++)
		{
			mixed.Add(MakeItem(a, OVT_ShopCategory.AMMUNITION));
		}
		for (int w = 21; w <= 25; w++)
		{
			mixed.Add(MakeItem(w, OVT_ShopCategory.WEAPONS));
		}

		mixed.GetPageItems(OVT_ShopCategory.AMMUNITION, 1, PER_PAGE, page);
		if (page.Count() != 5 || page.Get(0).m_iResourceId != 16)
		{
			SetResultFailure("The ammunition tab's second page held %1 rows starting at id %2, expected 5 rows from id 16",
				page.Count().ToString(), page.Get(0).m_iResourceId.ToString());
			return true;
		}

		Print("Pagination (BUG-024 pinned): 57/15 = 4 pages with a 12-item last page, 3 and 0 items give 1 page, page index clamps at both ends, slices stay in range");

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one numbered row. Every field is set explicitly.
	//! \param[in] id Resource id, which doubles as the row's position marker.
	//! \param[in] category Category the row is filed under.
	//! \return The row.
	protected OVT_ShopBrowserItem MakeItem(int id, OVT_ShopCategory category)
	{
		OVT_ShopBrowserItem item = new OVT_ShopBrowserItem();
		item.m_iResourceId = id;
		item.m_sResource = ResourceName.Empty;
		item.m_sDisplayName = "Item " + id.ToString();
		item.m_eCategory = category;
		item.m_iUnitPrice = 100;
		item.m_iQuantity = 1;
		item.m_bEnabled = true;
		item.m_sDisabledReasonKey = "";
		return item;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_ShopSellRules - which shops buy, at what rate, and why a row cannot be sold.
//!
//! These four predicates gate the menu's Sell toggle, the greying-out of individual cards, the
//! vehicle trunk action's visibility AND the server's authority check. A wrong answer here is either
//! a sell path that silently disappears or one that credits money it should not, so every branch is
//! asserted rather than sampled.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ShopUX_SellRules : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// --- ORDINARY TOWN SHOPS BUY.
		if (!OVT_ShopSellRules.ShopBuysFromPlayers(OVT_ShopType.SHOP_GENERAL, false, 0.5))
		{
			SetResultFailure("A general store refused to buy from players");
			return true;
		}

		if (!OVT_ShopSellRules.ShopBuysFromPlayers(OVT_ShopType.SHOP_CLOTHES, false, 0))
		{
			SetResultFailure("A clothes shop refused to buy from players (the gun dealer multiplier must not apply to it)");
			return true;
		}

		// --- VEHICLE AND PROCUREMENT SHOPS NEVER BUY.
		if (OVT_ShopSellRules.ShopBuysFromPlayers(OVT_ShopType.SHOP_VEHICLE, false, 1.0))
		{
			SetResultFailure("A vehicle shop offered to buy from players");
			return true;
		}

		if (OVT_ShopSellRules.ShopBuysFromPlayers(OVT_ShopType.SHOP_GENERAL, true, 1.0))
		{
			SetResultFailure("A procurement shop offered to buy from players");
			return true;
		}

		if (OVT_ShopSellRules.ShopBuysFromPlayers(OVT_ShopType.SHOP_GUNDEALER, true, 1.0))
		{
			SetResultFailure("A procurement gun dealer offered to buy from players; procurement outranks shop type");
			return true;
		}

		// --- A GUN DEALER BUYS IFF THE MULTIPLIER IS ABOVE ZERO.
		if (!OVT_ShopSellRules.ShopBuysFromPlayers(OVT_ShopType.SHOP_GUNDEALER, false, 0.6))
		{
			SetResultFailure("A gun dealer with a 0.6 multiplier refused to buy");
			return true;
		}

		if (OVT_ShopSellRules.ShopBuysFromPlayers(OVT_ShopType.SHOP_GUNDEALER, false, 0))
		{
			SetResultFailure("A gun dealer with a zero multiplier offered to buy; the config value must disable the sell path");
			return true;
		}

		// --- THE MULTIPLIER APPLIES ONLY AT GUN DEALERS.
		float dealerRate = OVT_ShopSellRules.GetSellMultiplier(OVT_ShopType.SHOP_GUNDEALER, 0.6);
		if (!OVT_TEST_LogicFixture.FloatEquals(dealerRate, 0.6))
		{
			SetResultFailure("A gun dealer's sell multiplier came back as %1, expected 0.6", dealerRate.ToString());
			return true;
		}

		float townRate = OVT_ShopSellRules.GetSellMultiplier(OVT_ShopType.SHOP_GENERAL, 0.6);
		if (!OVT_TEST_LogicFixture.FloatEquals(townRate, 1.0))
		{
			SetResultFailure("A general store's sell multiplier came back as %1, expected 1.0 (the gun dealer rate leaked)", townRate.ToString());
			return true;
		}

		// --- PER-ITEM ELIGIBILITY.
		if (!OVT_ShopSellRules.CanSellItem(false, true, 120))
		{
			SetResultFailure("An unequipped, priced item at a buying shop could not be sold");
			return true;
		}

		if (OVT_ShopSellRules.CanSellItem(true, true, 120))
		{
			SetResultFailure("An EQUIPPED item was sellable");
			return true;
		}

		if (OVT_ShopSellRules.CanSellItem(false, false, 120))
		{
			SetResultFailure("An item was sellable at a shop that does not buy it");
			return true;
		}

		if (OVT_ShopSellRules.CanSellItem(false, true, 0))
		{
			SetResultFailure("A worthless item (price 0) was sellable");
			return true;
		}

		if (OVT_ShopSellRules.CanSellItem(false, true, -50))
		{
			SetResultFailure("An item with a negative price was sellable");
			return true;
		}

		// --- EACH BLOCKED CASE NAMES ITS OWN REASON.
		string sellableReason = OVT_ShopSellRules.GetBlockReasonKey(false, true, 120);
		if (sellableReason != "")
		{
			SetResultFailure("A sellable item reported the block reason '%1', expected none", sellableReason);
			return true;
		}

		string equippedReason = OVT_ShopSellRules.GetBlockReasonKey(true, true, 120);
		string notBoughtReason = OVT_ShopSellRules.GetBlockReasonKey(false, false, 120);
		string worthlessReason = OVT_ShopSellRules.GetBlockReasonKey(false, true, 0);

		if (!IsReasonKey(equippedReason) || !IsReasonKey(notBoughtReason) || !IsReasonKey(worthlessReason))
		{
			SetResultFailure("A blocked item reported a non-#OVT- reason: equipped '%1', not-bought '%2', worthless '%3'",
				equippedReason, notBoughtReason, worthlessReason);
			return true;
		}

		if (equippedReason == notBoughtReason || equippedReason == worthlessReason || notBoughtReason == worthlessReason)
		{
			SetResultFailure("Two blocked cases share a reason key: equipped '%1', not-bought '%2', worthless '%3'",
				equippedReason, notBoughtReason, worthlessReason);
			return true;
		}

		Print("Sell rules: vehicle and procurement shops never buy, a gun dealer buys iff its multiplier is above zero and only it applies one, and each blocked case names its own reason");

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] key The reason key to check.
	//! \return True when the key is a non-empty #OVT- localization key.
	protected bool IsReasonKey(string key)
	{
		if (key == "")
			return false;

		return key.StartsWith("#OVT-");
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_MoneyFormat and OVT_MoneyDeltaTracker - readable amounts, and a ticker that accumulates.
//!
//! The tracker is driven by a hand-written sequence of (money, timeSlice) observations rather than
//! by a clock, which is exactly why it takes the time slice as a parameter (implementation plan,
//! D11): the HUD polls it every frame, and this tier can poll it four times in a row.
//!
//! The timing claim worth naming: EVERY CHANGE RESTARTS THE COUNTDOWN. The sequence below lets 3.5 s
//! of the 4 s timeout elapse, changes the money, then lets another 3.5 s elapse and requires the
//! ticker to still be visible. Without the restart the total 7 s would have cleared it long before.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ShopUX_MoneyFormatAndDelta : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// --- FORMATTING.
		if (OVT_MoneyFormat.FormatMoney(12500) != "$12,500")
		{
			SetResultFailure("FormatMoney(12500) returned '%1', expected '$12,500'", OVT_MoneyFormat.FormatMoney(12500));
			return true;
		}

		if (OVT_MoneyFormat.FormatMoney(0) != "$0")
		{
			SetResultFailure("FormatMoney(0) returned '%1', expected '$0'", OVT_MoneyFormat.FormatMoney(0));
			return true;
		}

		if (OVT_MoneyFormat.FormatMoney(999) != "$999")
		{
			SetResultFailure("FormatMoney(999) returned '%1', expected '$999' with no separator", OVT_MoneyFormat.FormatMoney(999));
			return true;
		}

		if (OVT_MoneyFormat.FormatMoney(1000) != "$1,000")
		{
			SetResultFailure("FormatMoney(1000) returned '%1', expected '$1,000'", OVT_MoneyFormat.FormatMoney(1000));
			return true;
		}

		if (OVT_MoneyFormat.FormatMoney(-150) != "-$150")
		{
			SetResultFailure("FormatMoney(-150) returned '%1', expected '-$150' (the sign goes outside the symbol)", OVT_MoneyFormat.FormatMoney(-150));
			return true;
		}

		if (OVT_MoneyFormat.FormatMoney(-1234567) != "-$1,234,567")
		{
			SetResultFailure("FormatMoney(-1234567) returned '%1', expected '-$1,234,567'", OVT_MoneyFormat.FormatMoney(-1234567));
			return true;
		}

		if (OVT_MoneyFormat.FormatMoney(1000000) != "$1,000,000")
		{
			SetResultFailure("FormatMoney(1000000) returned '%1', expected '$1,000,000'", OVT_MoneyFormat.FormatMoney(1000000));
			return true;
		}

		if (OVT_MoneyFormat.FormatDelta(500) != "+$500")
		{
			SetResultFailure("FormatDelta(500) returned '%1', expected '+$500'", OVT_MoneyFormat.FormatDelta(500));
			return true;
		}

		if (OVT_MoneyFormat.FormatDelta(-150) != "-$150")
		{
			SetResultFailure("FormatDelta(-150) returned '%1', expected '-$150'", OVT_MoneyFormat.FormatDelta(-150));
			return true;
		}

		if (OVT_MoneyFormat.FormatDelta(0) != "")
		{
			SetResultFailure("FormatDelta(0) returned '%1', expected an empty string", OVT_MoneyFormat.FormatDelta(0));
			return true;
		}

		// --- THE TICKER.
		OVT_MoneyDeltaTracker tracker = new OVT_MoneyDeltaTracker();
		tracker.SetResetSeconds(4.0);

		// First observation seeds the baseline: opening the HUD is not a transaction.
		tracker.Update(1000, 0.1);
		if (tracker.IsVisible() || tracker.GetDelta() != 0 || tracker.GetText() != "")
		{
			SetResultFailure("The first observation produced a delta of %1 ('%2'), expected nothing at all",
				tracker.GetDelta().ToString(), tracker.GetText());
			return true;
		}

		// A sale of 500.
		tracker.Update(1500, 0.1);
		if (tracker.GetDelta() != 500 || !tracker.IsVisible() || tracker.GetText() != "+$500")
		{
			SetResultFailure("After a 500 gain the ticker read %1 ('%2'), expected 500 / '+$500'",
				tracker.GetDelta().ToString(), tracker.GetText());
			return true;
		}

		// Most of the timeout elapses with no change - still visible, but only just.
		tracker.Update(1500, 3.5);
		if (!tracker.IsVisible())
		{
			SetResultFailure("The ticker cleared after 3.5 s of a 4 s timeout");
			return true;
		}

		// A second change ACCUMULATES onto the first AND restarts the countdown.
		tracker.Update(1200, 0.1);
		if (tracker.GetDelta() != 200 || tracker.GetText() != "+$200")
		{
			SetResultFailure("A 500 gain followed by a 300 loss read %1 ('%2'), expected 200 / '+$200'",
				tracker.GetDelta().ToString(), tracker.GetText());
			return true;
		}

		// 3.5 s more with no change. Cumulative 7 s since the FIRST change, so this is only still
		// visible if the second change restarted the countdown.
		tracker.Update(1200, 3.5);
		if (!tracker.IsVisible())
		{
			SetResultFailure("The ticker cleared 3.5 s after the second change; the change did not restart the countdown");
			return true;
		}

		if (tracker.GetDelta() != 200)
		{
			SetResultFailure("The accumulated delta drifted to %1 while merely waiting, expected 200", tracker.GetDelta().ToString());
			return true;
		}

		// Past the timeout: the ticker clears and hides.
		tracker.Update(1200, 1.0);
		if (tracker.IsVisible() || tracker.GetDelta() != 0 || tracker.GetText() != "")
		{
			SetResultFailure("The ticker survived its timeout with delta %1 ('%2'), expected 0 and hidden",
				tracker.GetDelta().ToString(), tracker.GetText());
			return true;
		}

		// It keeps working afterwards: a later change starts a fresh accumulation, and a loss reads red.
		tracker.Update(1050, 0.1);
		if (tracker.GetDelta() != -150 || !tracker.IsVisible() || tracker.GetText() != "-$150")
		{
			SetResultFailure("After clearing, a 150 loss read %1 ('%2'), expected -150 / '-$150'",
				tracker.GetDelta().ToString(), tracker.GetText());
			return true;
		}

		Print("Money: thousands grouping with zero/negative/large values, and a ticker that seeds silently, accumulates, restarts its countdown on every change and clears after the timeout");

		SetResultSuccess();
		return true;
	}
}
