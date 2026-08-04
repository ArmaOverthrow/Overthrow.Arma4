//------------------------------------------------------------------------------------------------
//! One browsable row in the shop menu - a buyable shop stock line or a sellable inventory stack.
//!
//! Deliberately dumb: it carries what a card draws and what a click needs, and nothing else. No
//! entity handles, no widgets, no manager lookups, so the browser model above it stays Logic-tier
//! testable.
//------------------------------------------------------------------------------------------------
class OVT_ShopBrowserItem : Managed
{
	int m_iResourceId;					//! Economy resource id - the wire format for buy/sell RPCs
	ResourceName m_sResource;			//! Prefab, for the preview widget
	string m_sDisplayName;				//! Already-resolved display name, what the sort orders by
	OVT_ShopCategory m_eCategory;		//! Tab this row belongs to. Never ALL
	int m_iUnitPrice;					//! Buy price in Buy mode, sell price in Sell mode
	int m_iQuantity;					//! Stock held (Buy) or copies carried (Sell). -1 = unlimited/unknown
	bool m_bEnabled;					//! False draws the card dimmed and disables its action buttons
	string m_sDisabledReasonKey;		//! #OVT- key naming why, empty when enabled

	//------------------------------------------------------------------------------------------------
	//! Builds an empty row. Every field is set explicitly - `new` applies no attribute defaults.
	void OVT_ShopBrowserItem()
	{
		m_iResourceId = -1;
		m_sResource = ResourceName.Empty;
		m_sDisplayName = "";
		m_eCategory = OVT_ShopCategory.OTHER;
		m_iUnitPrice = 0;
		m_iQuantity = 0;
		m_bEnabled = true;
		m_sDisabledReasonKey = "";
	}
}

//------------------------------------------------------------------------------------------------
//! The shop browser's row set, plus every piece of arithmetic the menu needs to draw a page of it.
//!
//! ALL PAGINATION MATHS LIVES HERE, AND THAT IS HOW BUG-024 DIES. The legacy menu computed its page
//! count as `Count() / 15` in integer arithmetic, so a 57-item shop reported 3 pages and the last 12
//! items were permanently unreachable, while NextPage on the final page could ask for a negative
//! slice index. Every one of those numbers is now a pure function with a Logic-tier case over it.
//!
//! UI-free, manager-free, world-free: build one with `new`, feed it rows, ask it questions.
//------------------------------------------------------------------------------------------------
class OVT_ShopBrowserModel : Managed
{
	protected ref array<ref OVT_ShopBrowserItem> m_aItems;

	//------------------------------------------------------------------------------------------------
	void OVT_ShopBrowserModel()
	{
		m_aItems = new array<ref OVT_ShopBrowserItem>();
	}

	//------------------------------------------------------------------------------------------------
	//! Appends a row. Null is ignored rather than stored, so no consumer has to null-check a row.
	//! \param[in] item The row to add.
	void Add(OVT_ShopBrowserItem item)
	{
		if (!item)
			return;

		m_aItems.Insert(item);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every row.
	void Clear()
	{
		m_aItems.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The number of rows held, across all categories.
	int Count()
	{
		return m_aItems.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The backing row array, in current order. Callers must not take ownership.
	array<ref OVT_ShopBrowserItem> GetItems()
	{
		return m_aItems;
	}

	//------------------------------------------------------------------------------------------------
	//! Sorts every row alphabetically by display name, case-insensitively and STABLY.
	//!
	//! Insertion sort with a strictly-greater comparison: rows whose names differ only in case, or
	//! not at all, keep the order they were added in. Stability matters because the sell model
	//! aggregates by resource id, and two variants can legitimately share a display name - a sort
	//! that shuffled them would move the player's selection between refreshes.
	void SortByDisplayName()
	{
		int count = m_aItems.Count();

		for (int i = 1; i < count; i++)
		{
			OVT_ShopBrowserItem moving = m_aItems.Get(i);
			int j = i - 1;

			while (j >= 0 && m_aItems.Get(j).m_sDisplayName.Compare(moving.m_sDisplayName, false) > 0)
			{
				m_aItems.Set(j + 1, m_aItems.Get(j));
				j--;
			}

			m_aItems.Set(j + 1, moving);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Lists the categories that actually have rows, in tab order.
	//!
	//! ALL is never included - it is the menu's own always-present first tab, not a populated
	//! category - and an empty category never earns a tab (Definition of Done F2).
	//! \param[out] categories Receives the populated categories in display order. Cleared first.
	void GetPopulatedCategories(out array<OVT_ShopCategory> categories)
	{
		if (!categories)
			return;

		categories.Clear();

		array<OVT_ShopCategory> order = new array<OVT_ShopCategory>();
		OVT_ShopCategoryHelper.GetDisplayOrder(order);

		foreach (OVT_ShopCategory candidate : order)
		{
			if (candidate == OVT_ShopCategory.ALL)
				continue;

			if (HasCategory(candidate))
				categories.Insert(candidate);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] category The category to look for.
	//! \return True when at least one row is in that category.
	bool HasCategory(OVT_ShopCategory category)
	{
		foreach (OVT_ShopBrowserItem item : m_aItems)
		{
			if (item.m_eCategory == category)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Selects the rows of one category, in current (i.e. sorted) order.
	//! \param[in] category The tab to filter by. ALL returns every row.
	//! \param[out] items Receives the matching rows. Cleared first.
	void FilterByCategory(OVT_ShopCategory category, out array<ref OVT_ShopBrowserItem> items)
	{
		if (!items)
			return;

		items.Clear();

		foreach (OVT_ShopBrowserItem item : m_aItems)
		{
			if (category == OVT_ShopCategory.ALL || item.m_eCategory == category)
				items.Insert(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Page count for a row count, clamped so there is always at least one (possibly empty) page.
	//!
	//! Math.Ceil over a FLOAT divisor. The integer division this replaces is BUG-024: 57 / 15 = 3,
	//! which hides the last 12 items of the shop.
	//! \param[in] count Number of rows in the current tab.
	//! \param[in] perPage Cards per page. Non-positive is treated as "everything on one page".
	//! \return Number of pages, always >= 1.
	static int GetPageCount(int count, int perPage)
	{
		if (perPage <= 0)
			return 1;

		if (count <= 0)
			return 1;

		float exactCount = count;
		float exactPerPage = perPage;
		int pages = Math.Ceil(exactCount / exactPerPage);

		if (pages < 1)
			return 1;

		return pages;
	}

	//------------------------------------------------------------------------------------------------
	//! Clamps a page index into [0, pageCount - 1].
	//! \param[in] page Requested page index, possibly out of range in either direction.
	//! \param[in] pageCount Number of pages, as returned by GetPageCount().
	//! \return A page index that is always safe to slice with.
	static int ClampPage(int page, int pageCount)
	{
		int safeCount = pageCount;
		if (safeCount < 1)
			safeCount = 1;

		if (page < 0)
			return 0;

		if (page > safeCount - 1)
			return safeCount - 1;

		return page;
	}

	//------------------------------------------------------------------------------------------------
	//! Slices one page out of one category. The slice bounds are clamped, so this never reads out of
	//! range no matter what page index the menu asks for.
	//! \param[in] category The tab to page through. ALL pages through everything.
	//! \param[in] page Requested page index; clamped internally.
	//! \param[in] perPage Cards per page.
	//! \param[out] items Receives at most perPage rows. Cleared first; may come back empty.
	void GetPageItems(OVT_ShopCategory category, int page, int perPage, out array<ref OVT_ShopBrowserItem> items)
	{
		if (!items)
			return;

		array<ref OVT_ShopBrowserItem> filtered = new array<ref OVT_ShopBrowserItem>();
		FilterByCategory(category, filtered);

		items.Clear();

		int count = filtered.Count();
		if (count == 0)
			return;

		int safePerPage = perPage;
		if (safePerPage <= 0)
			safePerPage = count;

		int safePage = ClampPage(page, GetPageCount(count, safePerPage));

		int start = safePage * safePerPage;
		if (start >= count)
			return;

		int end = start + safePerPage;
		if (end > count)
			end = count;

		for (int i = start; i < end; i++)
		{
			items.Insert(filtered.Get(i));
		}
	}
}
