//------------------------------------------------------------------------------------------------
//! The pure resource definition table: what a resource is, in the form every rule needs.
//!
//! Parallel arrays, index-aligned, because the index is what rides the wire and the UI: the config
//! is a mod file, so index N means the same resource on every machine. Flags are 0/1 ints, never
//! array<bool>.
//!
//! Pure - no world, no manager, no engine type in any signature. A Logic case new()s one and fills
//! it by hand, which is what keeps every rule out of a component.
//------------------------------------------------------------------------------------------------
class OVT_ResourceDefs : Managed
{
	//! Stable string id, e.g. "timber". The save and the config key on this; the wire does not.
	ref array<string> m_aIds;

	//! Integer litres per unit (D3 - no capacity decision ever depends on a binary32 rounding).
	ref array<int> m_aLitresPerUnit;

	//! Display weight per unit. Never consulted by a capacity decision.
	ref array<float> m_aKgPerUnit;

	//! Config base price. The live price lives on the manager; this is only drift maths and readouts.
	ref array<int> m_aBasePrice;

	//! 0/1 - may be bought at a port.
	ref array<int> m_aImportable;

	//! 0/1 - needs the illegal-imports gate.
	ref array<int> m_aIllegal;

	//------------------------------------------------------------------------------------------------
	//! Starts an empty table. new() applies no [Attribute] defvalues, so every array is allocated here.
	void OVT_ResourceDefs()
	{
		m_aIds = new array<string>();
		m_aLitresPerUnit = new array<int>();
		m_aKgPerUnit = new array<float>();
		m_aBasePrice = new array<int>();
		m_aImportable = new array<int>();
		m_aIllegal = new array<int>();
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one definition. Every parallel array grows together or none of them does.
	//! \param[in] id Stable string id. Empty is rejected.
	//! \param[in] litresPerUnit Integer litres one unit occupies.
	//! \param[in] kgPerUnit Display weight of one unit.
	//! \param[in] basePrice Config base price.
	//! \param[in] importable 1 when it may be bought at a port.
	//! \param[in] illegal 1 when it needs the illegal-imports gate.
	//! \return The new definition index, or -1 when the id was empty or already present.
	int AddDef(string id, int litresPerUnit, float kgPerUnit, int basePrice, int importable, int illegal)
	{
		if (id == "")
			return -1;

		if (IndexOf(id) != -1)
			return -1;

		m_aIds.Insert(id);
		m_aLitresPerUnit.Insert(litresPerUnit);
		m_aKgPerUnit.Insert(kgPerUnit);
		m_aBasePrice.Insert(basePrice);
		m_aImportable.Insert(importable);
		m_aIllegal.Insert(illegal);

		return m_aIds.Count() - 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Index of a resource id. Linear over a handful of entries, so a hand-filled table needs no
	//! maintained lookup map to stay correct.
	//! \param[in] id Stable string id.
	//! \return The index, or -1 when unknown.
	int IndexOf(string id)
	{
		if (id == "")
			return -1;

		for (int i = 0; i < m_aIds.Count(); i++)
		{
			if (m_aIds[i] == id)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The id at an index.
	//! \param[in] index Definition index.
	//! \return The id, or "" when out of range.
	string IdAt(int index)
	{
		if (index < 0 || index >= m_aIds.Count())
			return "";

		return m_aIds[index];
	}

	//------------------------------------------------------------------------------------------------
	//! How many definitions the table holds.
	//! \return The count.
	int Count()
	{
		return m_aIds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Litres one unit of a definition occupies.
	//! \param[in] index Definition index.
	//! \return The litres, or 0 when out of range.
	int LitresAt(int index)
	{
		if (index < 0 || index >= m_aLitresPerUnit.Count())
			return 0;

		return m_aLitresPerUnit[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Litres one unit of a resource id occupies.
	//! \param[in] id Stable string id.
	//! \return The litres, or 0 when unknown.
	int LitresPerUnit(string id)
	{
		return LitresAt(IndexOf(id));
	}

	//------------------------------------------------------------------------------------------------
	//! Display weight of one unit.
	//! \param[in] index Definition index.
	//! \return The weight, or 0 when out of range.
	float KgAt(int index)
	{
		if (index < 0 || index >= m_aKgPerUnit.Count())
			return 0;

		return m_aKgPerUnit[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Display weight of one unit of a resource id.
	//! \param[in] id Stable string id.
	//! \return The weight, or 0 when unknown.
	float KgPerUnit(string id)
	{
		return KgAt(IndexOf(id));
	}

	//------------------------------------------------------------------------------------------------
	//! Config base price of a definition. Never the live price.
	//! \param[in] index Definition index.
	//! \return The base price, or 0 when out of range.
	int BasePriceAt(int index)
	{
		if (index < 0 || index >= m_aBasePrice.Count())
			return 0;

		return m_aBasePrice[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a definition may be bought at a port.
	//! \param[in] index Definition index.
	//! \return True only for a known index flagged 1.
	bool IsImportable(int index)
	{
		if (index < 0 || index >= m_aImportable.Count())
			return false;

		return m_aImportable[index] == 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a definition needs the illegal-imports gate.
	//! \param[in] index Definition index.
	//! \return True only for a known index flagged 1.
	bool IsIllegal(int index)
	{
		if (index < 0 || index >= m_aIllegal.Count())
			return false;

		return m_aIllegal[index] == 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an id resolves to a definition.
	//! \param[in] id Stable string id.
	//! \return True when the table knows it.
	bool Knows(string id)
	{
		return IndexOf(id) != -1;
	}
}
