//------------------------------------------------------------------------------------------------
//! One enumeration/serialization record: what is held, and how many.
//!
//! The ledger itself is map-backed; this is the shape it hands to the wire fan, the serializer and
//! the UI, and the shape they hand back.
//------------------------------------------------------------------------------------------------
class OVT_StorageLine : Managed
{
	//! Prefab ResourceName, never an economy id.
	string m_sRes;

	//! How many are held. Never zero in a ledger-produced line.
	int m_iCount;
}

//------------------------------------------------------------------------------------------------
//! A holder's item ledger: prefab ResourceName -> count.
//!
//! Pure - no world, no manager, no engine type in any signature, which is what keeps it Logic-tier
//! testable. Capacity is PASSED IN and never held: the component owns capacity, so a future resource
//! ledger cannot inherit an item-shaped cap.
//!
//! Keys are prefab ResourceName strings, never the economy's integer ids. GetInventoryId() resolves
//! an unregistered prefab to index 0 - some other item's identity - and looted occupying-faction
//! gear is routinely unregistered.
//------------------------------------------------------------------------------------------------
class OVT_StorageLedger : Managed
{
	//! ResourceName -> count. A line that reaches zero is REMOVED; zero entries never accumulate.
	protected ref map<string, int> m_mLines;

	//! Maintained on every mutation. Action labels poll Total() every frame, so it is never summed.
	protected int m_iTotal;

	//! (string res, int newQty). Lazily allocated - most ledgers never have a listener.
	ref ScriptInvoker m_OnChanged;

	//------------------------------------------------------------------------------------------------
	//! Starts an empty ledger.
	void OVT_StorageLedger()
	{
		m_mLines = new map<string, int>();
		m_iTotal = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds up to qty of res, clamped by the caller's capacity.
	//! \param[in] res Prefab ResourceName. Empty is ignored.
	//! \param[in] qty How many to add. Zero or negative is ignored.
	//! \param[in] capacity Total item cap for the holder; negative means unlimited.
	//! \return How many actually fitted; 0 when nothing did.
	int Add(string res, int qty, int capacity)
	{
		if (res == "")
			return 0;

		if (qty <= 0)
			return 0;

		int fitted = qty;
		if (capacity >= 0)
		{
			int free = capacity - m_iTotal;
			if (free <= 0)
				return 0;

			if (fitted > free)
				fitted = free;
		}

		int held = 0;
		if (m_mLines.Contains(res))
			held = m_mLines.Get(res);

		held = held + fitted;
		m_mLines.Set(res, held);
		m_iTotal = m_iTotal + fitted;

		if (m_OnChanged)
			m_OnChanged.Invoke(res, held);

		return fitted;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes up to qty of res, clamped to what is held.
	//! \param[in] res Prefab ResourceName. Empty is ignored.
	//! \param[in] qty How many to take. Zero or negative is ignored.
	//! \return How many were taken; 0 when the line is absent or empty.
	int Take(string res, int qty)
	{
		if (res == "")
			return 0;

		if (qty <= 0)
			return 0;

		if (!m_mLines.Contains(res))
			return 0;

		int held = m_mLines.Get(res);
		int taken = qty;
		if (taken > held)
			taken = held;

		if (taken <= 0)
			return 0;

		int remaining = held - taken;
		if (remaining <= 0)
			m_mLines.Remove(res);
		else
			m_mLines.Set(res, remaining);

		m_iTotal = m_iTotal - taken;

		if (m_OnChanged)
			m_OnChanged.Invoke(res, remaining);

		return taken;
	}

	//------------------------------------------------------------------------------------------------
	//! How many of res are held.
	//! \param[in] res Prefab ResourceName.
	//! \return The count, or 0 when the line is absent.
	int Count(string res)
	{
		if (!m_mLines.Contains(res))
			return 0;

		return m_mLines.Get(res);
	}

	//------------------------------------------------------------------------------------------------
	//! Total items held across every line. O(1) - a maintained field, never a sum.
	//! \return The running total.
	int Total()
	{
		return m_iTotal;
	}

	//------------------------------------------------------------------------------------------------
	//! Room left under a caller-supplied capacity.
	//! \param[in] capacity Total item cap; negative means unlimited.
	//! \return int.MAX when unlimited, otherwise capacity - total, never negative.
	int FreeSpace(int capacity)
	{
		if (capacity < 0)
			return int.MAX;

		return Math.Max(0, capacity - m_iTotal);
	}

	//------------------------------------------------------------------------------------------------
	//! How many distinct lines are held.
	//! \return The line count.
	int LineCount()
	{
		return m_mLines.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Enumerates every line into two parallel arrays, for the fan, the serializer and the UI.
	//! Both arrays are cleared first, so a reused buffer never accumulates.
	//! \param[out] res Receives each ResourceName.
	//! \param[out] counts Receives each count, index-aligned with res.
	void GetLines(out array<string> res, out array<int> counts)
	{
		if (!res)
			res = new array<string>();

		if (!counts)
			counts = new array<int>();

		res.Clear();
		counts.Clear();

		for (int i = 0; i < m_mLines.Count(); i++)
		{
			res.Insert(m_mLines.GetKey(i));
			counts.Insert(m_mLines.GetElement(i));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the ledger. The invoker is not fired per line - callers republish once.
	void Clear()
	{
		m_mLines.Clear();
		m_iTotal = 0;
	}
}
