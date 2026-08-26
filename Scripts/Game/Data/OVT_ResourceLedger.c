//------------------------------------------------------------------------------------------------
//! One enumeration/argument record: which resource, and how many units.
//!
//! This is the currency of every resource helper signature. It is NOT the persisted record - the
//! serializers freeze their own.
//------------------------------------------------------------------------------------------------
class OVT_ResourceAmount : Managed
{
	//! Stable resource id, e.g. "timber".
	string m_sId;

	//! How many units. Never zero in a ledger-produced amount.
	int m_iQuantity;
}

//------------------------------------------------------------------------------------------------
//! A holder's resource ledger: resource id -> units held.
//!
//! Pure - no world, no manager, no engine type in any signature. Capacity is PASSED IN and never
//! held, so a pile and a warehouse are unlimited simply by applying no cap.
//!
//! Volume is integer litres throughout (D3): no epsilon, no binary32 surprise, and every fit
//! decision is an exact integer comparison.
//------------------------------------------------------------------------------------------------
class OVT_ResourceLedger : Managed
{
	//! id -> units. A line that reaches zero is REMOVED; zero entries never accumulate.
	protected ref map<string, int> m_mLines;

	//! id -> litres per unit, captured at Add time. Take() takes no definition table, so the running
	//! litre total can only stay O(1) if the per-unit volume travels with the line.
	protected ref map<string, int> m_mUnitLitres;

	//! Maintained on every mutation. The HUD and action labels poll TotalLitres() every frame.
	protected int m_iTotalLitres;

	//! (string id, int newQty). Lazily allocated - most ledgers never have a listener.
	ref ScriptInvoker m_OnChanged;

	//------------------------------------------------------------------------------------------------
	//! Starts an empty ledger.
	void OVT_ResourceLedger()
	{
		m_mLines = new map<string, int>();
		m_mUnitLitres = new map<string, int>();
		m_iTotalLitres = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds up to qty units, clamped by the caller's litre capacity.
	//! \param[in] id Stable resource id. Empty, or one defs does not know, is ignored.
	//! \param[in] qty How many units to add. Zero or negative is ignored.
	//! \param[in] defs The definition table supplying litres per unit.
	//! \param[in] capacityLitres The holder's cap in litres; negative means unlimited.
	//! \return How many units actually fitted; 0 when none did.
	int Add(string id, int qty, OVT_ResourceDefs defs, int capacityLitres)
	{
		if (id == "")
			return 0;

		if (qty <= 0)
			return 0;

		if (!defs)
			return 0;

		int index = defs.IndexOf(id);
		if (index == -1)
			return 0;

		int unitLitres = defs.LitresAt(index);
		if (unitLitres < 0)
			unitLitres = 0;

		int fitted = qty;
		if (capacityLitres >= 0 && unitLitres > 0)
		{
			int freeLitres = capacityLitres - m_iTotalLitres;
			if (freeLitres <= 0)
				return 0;

			int room = freeLitres / unitLitres;
			if (room <= 0)
				return 0;

			if (fitted > room)
				fitted = room;
		}

		int held = 0;
		if (m_mLines.Contains(id))
			held = m_mLines.Get(id);

		held = held + fitted;
		m_mLines.Set(id, held);
		m_mUnitLitres.Set(id, unitLitres);
		m_iTotalLitres = m_iTotalLitres + (fitted * unitLitres);

		if (m_OnChanged)
			m_OnChanged.Invoke(id, held);

		return fitted;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes up to qty units, clamped to what is held.
	//! \param[in] id Stable resource id. Empty is ignored.
	//! \param[in] qty How many units to take. Zero or negative is ignored.
	//! \return How many units were taken; 0 when the line is absent.
	int Take(string id, int qty)
	{
		if (id == "")
			return 0;

		if (qty <= 0)
			return 0;

		if (!m_mLines.Contains(id))
			return 0;

		int held = m_mLines.Get(id);
		int taken = qty;
		if (taken > held)
			taken = held;

		if (taken <= 0)
			return 0;

		int unitLitres = 0;
		if (m_mUnitLitres.Contains(id))
			unitLitres = m_mUnitLitres.Get(id);

		int remaining = held - taken;
		if (remaining <= 0)
		{
			m_mLines.Remove(id);
			m_mUnitLitres.Remove(id);
		}
		else
		{
			m_mLines.Set(id, remaining);
		}

		m_iTotalLitres = m_iTotalLitres - (taken * unitLitres);
		if (m_iTotalLitres < 0)
			m_iTotalLitres = 0;

		if (m_OnChanged)
			m_OnChanged.Invoke(id, remaining);

		return taken;
	}

	//------------------------------------------------------------------------------------------------
	//! How many units of a resource are held.
	//! \param[in] id Stable resource id.
	//! \return The count, or 0 when the line is absent.
	int Count(string id)
	{
		if (!m_mLines.Contains(id))
			return 0;

		return m_mLines.Get(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Total litres held across every line. O(1) - a maintained field, never a sum.
	//! \param[in] defs The definition table, for signature symmetry with the other volume calls.
	//! \return The running litre total.
	int TotalLitres(OVT_ResourceDefs defs)
	{
		return m_iTotalLitres;
	}

	//------------------------------------------------------------------------------------------------
	//! Total display weight held. Iterates; nothing polls this per frame, and no fit decision reads it.
	//! \param[in] defs The definition table supplying kg per unit.
	//! \return Kilograms across every line; 0 without a definition table.
	float TotalWeightKg(OVT_ResourceDefs defs)
	{
		if (!defs)
			return 0;

		float total = 0;
		for (int i = 0; i < m_mLines.Count(); i++)
		{
			float kg = defs.KgPerUnit(m_mLines.GetKey(i));
			total = total + (kg * m_mLines.GetElement(i));
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Room left under a caller-supplied litre capacity.
	//! \param[in] defs The definition table, for signature symmetry.
	//! \param[in] capacityLitres The holder's cap; negative means unlimited.
	//! \return int.MAX when unlimited, otherwise capacity - total, never negative.
	int FreeLitres(OVT_ResourceDefs defs, int capacityLitres)
	{
		if (capacityLitres < 0)
			return int.MAX;

		return Math.Max(0, capacityLitres - m_iTotalLitres);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a whole load would fit. Exact integer comparison; no epsilon anywhere.
	//! \param[in] id Stable resource id.
	//! \param[in] qty How many units.
	//! \param[in] defs The definition table supplying litres per unit.
	//! \param[in] capacityLitres The holder's cap; negative means unlimited.
	//! \return True when every unit of the load fits.
	bool WouldFit(string id, int qty, OVT_ResourceDefs defs, int capacityLitres)
	{
		if (id == "")
			return false;

		if (qty <= 0)
			return false;

		if (!defs)
			return false;

		int index = defs.IndexOf(id);
		if (index == -1)
			return false;

		if (capacityLitres < 0)
			return true;

		int unitLitres = defs.LitresAt(index);
		if (unitLitres <= 0)
			return true;

		return (m_iTotalLitres + (qty * unitLitres)) <= capacityLitres;
	}

	//------------------------------------------------------------------------------------------------
	//! How many distinct lines are held.
	//! \return The line count.
	int LineCount()
	{
		return m_mLines.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Enumerates every line into two parallel arrays, for the wire, the serializer and the UI.
	//! Both arrays are cleared first, so a reused buffer never accumulates.
	//! \param[out] ids Receives each resource id.
	//! \param[out] qty Receives each unit count, index-aligned with ids.
	void GetLines(out array<string> ids, out array<int> qty)
	{
		if (!ids)
			ids = new array<string>();

		if (!qty)
			qty = new array<int>();

		ids.Clear();
		qty.Clear();

		for (int i = 0; i < m_mLines.Count(); i++)
		{
			ids.Insert(m_mLines.GetKey(i));
			qty.Insert(m_mLines.GetElement(i));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the ledger. The invoker is not fired per line - callers republish once.
	void Clear()
	{
		m_mLines.Clear();
		m_mUnitLitres.Clear();
		m_iTotalLitres = 0;
	}
}
