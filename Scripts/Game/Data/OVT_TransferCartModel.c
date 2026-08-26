//------------------------------------------------------------------------------------------------
//! One line of a transfer order: an entry id, a quantity, and COPIES of the fields a line draws.
//!
//! A line never holds an OVT_TransferEntry ref. The warehouse rebuilds its list model on every
//! inventory-changed tick, so a pointer into it would draw stale stock; Reconcile() handles that
//! explicitly instead.
//------------------------------------------------------------------------------------------------
class OVT_TransferCartLine : Managed
{
	string m_sId;								//! Entry id this line orders
	string m_sDisplayName;						//! Copied from the entry at add time
	int m_iUnitValue;							//! Copied unit price or unit stock value
	EOVT_TransferValueKind m_eValueKind;		//! Copied; drives the summary's formatting
	int m_iQuantity;							//! How many, always in [1, m_iMaxQuantity]
	int m_iMaxQuantity;							//! Cap this line clamps to

	//------------------------------------------------------------------------------------------------
	//! Builds an empty line. Every field is set explicitly - `new` applies no attribute defaults.
	void OVT_TransferCartLine()
	{
		m_sId = "";
		m_sDisplayName = "";
		m_iUnitValue = 0;
		m_eValueKind = EOVT_TransferValueKind.QUANTITY;
		m_iQuantity = 0;
		m_iMaxQuantity = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! A multi-line transfer order and all of its arithmetic: merge, clamp, drop-at-zero, totals and
//! reconciliation against a refreshed list model.
//!
//! UI-free, manager-free, world-free.
//------------------------------------------------------------------------------------------------
class OVT_TransferCartModel : Managed
{
	protected ref array<ref OVT_TransferCartLine> m_aLines;

	//------------------------------------------------------------------------------------------------
	void OVT_TransferCartModel()
	{
		m_aLines = new array<ref OVT_TransferCartLine>();
	}

	//------------------------------------------------------------------------------------------------
	//! Orders more of an entry, merging into the existing line for that id and clamping the merged
	//! total to the entry's cap. A null entry or a non-positive quantity is ignored.
	//! \param[in] entry The row being ordered.
	//! \param[in] qty How many to add.
	void Add(OVT_TransferEntry entry, int qty)
	{
		if (!entry)
			return;

		if (qty <= 0)
			return;

		int index = FindLineIndex(entry.m_sId);

		if (index < 0)
		{
			OVT_TransferCartLine line = new OVT_TransferCartLine();
			line.m_sId = entry.m_sId;
			line.m_sDisplayName = entry.m_sDisplayName;
			line.m_iUnitValue = entry.m_iValue;
			line.m_eValueKind = entry.m_eValueKind;
			line.m_iMaxQuantity = entry.m_iMaxQuantity;
			line.m_iQuantity = ClampQuantity(qty, entry.m_iMaxQuantity);

			// A cap of zero clamps the request to zero; inserting it would draw an "x0" line that
			// only Clear() can ever remove, and it would enable Accept.
			if (line.m_iQuantity <= 0)
				return;

			m_aLines.Insert(line);
			return;
		}

		OVT_TransferCartLine existing = m_aLines.Get(index);
		existing.m_iMaxQuantity = entry.m_iMaxQuantity;
		existing.m_iQuantity = ClampQuantity(existing.m_iQuantity + qty, entry.m_iMaxQuantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Tops the line for an entry up to exactly its cap.
	//! \param[in] entry The row being ordered.
	void AddAll(OVT_TransferEntry entry)
	{
		if (!entry)
			return;

		Add(entry, entry.m_iMaxQuantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes from a line, dropping the line entirely when it reaches zero or below.
	//! \param[in] id The entry id to remove.
	//! \param[in] qty How many to remove.
	void Remove(string id, int qty)
	{
		if (qty <= 0)
			return;

		int index = FindLineIndex(id);
		if (index < 0)
			return;

		OVT_TransferCartLine line = m_aLines.Get(index);
		line.m_iQuantity = line.m_iQuantity - qty;

		// RemoveOrdered, never Remove: Remove backfills the hole with the LAST line, which would
		// teleport an unrelated line under the player's remembered cart index.
		if (line.m_iQuantity <= 0)
			m_aLines.RemoveOrdered(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a line entirely.
	//! \param[in] id The entry id to remove.
	void RemoveAll(string id)
	{
		int index = FindLineIndex(id);
		if (index < 0)
			return;

		m_aLines.RemoveOrdered(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every line.
	void Clear()
	{
		m_aLines.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The number of lines held.
	int Count()
	{
		return m_aLines.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The backing line array, in add order. Callers must not take ownership.
	array<ref OVT_TransferCartLine> GetLines()
	{
		return m_aLines;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The entry id to look for.
	//! \return The ordered quantity for that id, or 0 when the cart has no line for it.
	int GetQuantity(string id)
	{
		int index = FindLineIndex(id);
		if (index < 0)
			return 0;

		return m_aLines.Get(index).m_iQuantity;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The entry id to look for.
	//! \return The line's index, or -1 when the cart has no line for it.
	int FindLineIndex(string id)
	{
		int count = m_aLines.Count();

		for (int i = 0; i < count; i++)
		{
			if (m_aLines.Get(i).m_sId == id)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The sum of every line's quantity.
	int TotalQuantity()
	{
		int total = 0;

		foreach (OVT_TransferCartLine line : m_aLines)
		{
			total = total + line.m_iQuantity;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The sum of quantity x unit value over every line.
	int TotalValue()
	{
		int total = 0;

		foreach (OVT_TransferCartLine line : m_aLines)
		{
			total = total + (line.m_iQuantity * line.m_iUnitValue);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-checks every line against a refreshed list model: a line whose entry has vanished is
	//! dropped, the copied display fields are re-read, and a line whose cap has fallen is clamped to
	//! the new cap (dropped if that leaves it at zero).
	//! \param[in] model The freshly-built list model.
	void Reconcile(OVT_TransferListModel model)
	{
		if (!model)
			return;

		for (int i = m_aLines.Count() - 1; i >= 0; i--)
		{
			OVT_TransferCartLine line = m_aLines.Get(i);
			OVT_TransferEntry entry = model.FindById(line.m_sId);

			if (!entry)
			{
				m_aLines.RemoveOrdered(i);
				continue;
			}

			// Lines COPY their display fields (D2), so this is the only place a price change reaches
			// the cart - and TotalValue() feeds the port's affordability gate.
			line.m_iUnitValue = entry.m_iValue;
			line.m_sDisplayName = entry.m_sDisplayName;

			if (entry.m_iMaxQuantity >= line.m_iMaxQuantity)
				continue;

			line.m_iMaxQuantity = entry.m_iMaxQuantity;
			line.m_iQuantity = ClampQuantity(line.m_iQuantity, entry.m_iMaxQuantity);

			if (line.m_iQuantity <= 0)
				m_aLines.RemoveOrdered(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] qty Requested quantity.
	//! \param[in] max The cap.
	//! \return qty, never above max.
	protected int ClampQuantity(int qty, int max)
	{
		if (qty > max)
			return max;

		return qty;
	}
}
