//------------------------------------------------------------------------------------------------
//! The transfer screen's row set, plus every decision the list side of the menu needs to make.
//!
//! UI-free, manager-free, world-free: build one with `new`, feed it rows, ask it questions.
//------------------------------------------------------------------------------------------------
class OVT_TransferListModel : Managed
{
	//! "No filter" / the always-present first tab. -1, NOT 0: OVT_ShopCategory.ALL is 0 and the port
	//! maps its category ids straight onto that enum, so a 0 sentinel would collide with a real id.
	static const int CATEGORY_ALL = -1;

	protected ref array<ref OVT_TransferEntry> m_aEntries;

	//------------------------------------------------------------------------------------------------
	void OVT_TransferListModel()
	{
		m_aEntries = new array<ref OVT_TransferEntry>();
	}

	//------------------------------------------------------------------------------------------------
	//! Appends a row. Null is ignored rather than stored, so no consumer has to null-check a row.
	//! \param[in] entry The row to add.
	void Add(OVT_TransferEntry entry)
	{
		if (!entry)
			return;

		m_aEntries.Insert(entry);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every row.
	void Clear()
	{
		m_aEntries.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The number of rows held, across all categories.
	int Count()
	{
		return m_aEntries.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The backing row array, in current order. Callers must not take ownership.
	array<ref OVT_TransferEntry> GetEntries()
	{
		return m_aEntries;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The entry id to look for.
	//! \return The row with that id, or null when the model no longer holds it.
	OVT_TransferEntry FindById(string id)
	{
		foreach (OVT_TransferEntry entry : m_aEntries)
		{
			if (entry.m_sId == id)
				return entry;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Sorts every row alphabetically by display name, case-insensitively and STABLY.
	//!
	//! Insertion sort with a strictly-greater comparison: rows whose names differ only in case, or
	//! not at all, keep the order they were added in, so a refresh never moves the player's
	//! selection between two rows that share a name.
	void SortByDisplayName()
	{
		int count = m_aEntries.Count();

		for (int i = 1; i < count; i++)
		{
			OVT_TransferEntry moving = m_aEntries.Get(i);
			int j = i - 1;

			while (j >= 0 && m_aEntries.Get(j).m_sDisplayName.Compare(moving.m_sDisplayName, false) > 0)
			{
				m_aEntries.Set(j + 1, m_aEntries.Get(j));
				j--;
			}

			m_aEntries.Set(j + 1, moving);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] categoryId The category to look for.
	//! \return True when at least one row is in that category.
	bool HasCategory(int categoryId)
	{
		foreach (OVT_TransferEntry entry : m_aEntries)
		{
			if (entry.m_iCategoryId == categoryId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Lists the category ids that actually have rows, in ASCENDING id order.
	//!
	//! Ascending, not first-seen: the ids are declaration-ordered enums at every consumer, so this is
	//! the order the player already knows from the shop, and it is stable across a refresh - a
	//! first-seen order changes whenever the alphabetically-first row of a category changes, which
	//! would force the tab widgets to be rebuilt and throw away gamepad focus.
	//!
	//! CATEGORY_ALL is never included - it is the menu's own always-present first tab, not a
	//! populated category - and an empty category never earns a tab.
	//! \param[out] categories Receives the populated ids, ascending. Cleared first.
	void GetPopulatedCategories(out array<int> categories)
	{
		if (!categories)
			return;

		categories.Clear();

		foreach (OVT_TransferEntry entry : m_aEntries)
		{
			if (entry.m_iCategoryId == CATEGORY_ALL)
				continue;

			if (categories.Contains(entry.m_iCategoryId))
				continue;

			int at = 0;
			while (at < categories.Count() && categories.Get(at) < entry.m_iCategoryId)
			{
				at++;
			}

			categories.InsertAt(entry.m_iCategoryId, at);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Selects the rows of one category, in current (i.e. sorted) order.
	//! \param[in] categoryId The tab to filter by. CATEGORY_ALL returns every row.
	//! \param[out] entries Receives the matching rows. Cleared first.
	void FilterByCategory(int categoryId, out array<ref OVT_TransferEntry> entries)
	{
		if (!entries)
			return;

		entries.Clear();

		foreach (OVT_TransferEntry entry : m_aEntries)
		{
			if (categoryId == CATEGORY_ALL || entry.m_iCategoryId == categoryId)
				entries.Insert(entry);
		}
	}
}
