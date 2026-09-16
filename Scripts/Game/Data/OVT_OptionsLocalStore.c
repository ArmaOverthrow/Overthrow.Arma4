//------------------------------------------------------------------------------------------------
//! The pure key-value half of the per-profile LOCAL option store.
//!
//! This class makes no engine call, touches no widget and names no manager. It mirrors
//! `OVT_MapLayerPrefsStore` (D10): the rules that can corrupt a value live here, where a Logic case
//! can pin them, and the `BaseContainer` plumbing is a separate accessor.
//------------------------------------------------------------------------------------------------
class OVT_OptionsLocalStore
{
	//! Schema version of the stored record. A stored record at any other version is discarded rather
	//! than half-trusted.
	static const int CURRENT_VERSION = 1;

	//! Hard cap on remembered ids. Reaching it means something is generating ids rather than
	//! registering LOCAL options, so the store stops growing instead of evicting.
	static const int MAX_ENTRIES = 256;

	protected ref map<string, string> m_mValues;
	protected int m_iVersion;
	protected bool m_bCapWarned;

	//------------------------------------------------------------------------------------------------
	void OVT_OptionsLocalStore()
	{
		m_mValues = new map<string, string>();
		m_iVersion = CURRENT_VERSION;
		m_bCapWarned = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Stores a value. An empty id is refused rather than remembered as a blank.
	//! \param[in] id The option id.
	//! \param[in] value The value to store, in the option's string encoding.
	//! \return True on acceptance, false when the id is empty or the cap blocks a new id.
	bool Set(string id, string value)
	{
		if (id == "")
			return false;

		if (!m_mValues.Contains(id) && m_mValues.Count() >= MAX_ENTRIES)
		{
			WarnCapOnce();
			return false;
		}

		m_mValues.Set(id, value);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The stored value, or `""` when the id is absent.
	string Get(string id)
	{
		if (!m_mValues.Contains(id))
			return "";

		return m_mValues.Get(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return True when a value is stored for the id.
	bool Has(string id)
	{
		return m_mValues.Contains(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes a stored value, if one exists.
	//! \param[in] id The option id.
	//! \return True when a value was removed, false when the id was already absent.
	bool Remove(string id)
	{
		if (!m_mValues.Contains(id))
			return false;

		m_mValues.Remove(id);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many ids are stored.
	int Count()
	{
		return m_mValues.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The schema version this record is held at.
	int GetVersion()
	{
		return m_iVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the store.
	void Clear()
	{
		m_mValues.Clear();
		m_bCapWarned = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Copies the store out as two parallel arrays, for the settings accessor to write.
	//! \param[out] ids Receives every stored id. Allocated when null and cleared otherwise.
	//! \param[out] values Receives one value for each id, in the same order. Allocated when null and
	//! cleared otherwise.
	void ToPairs(out array<string> ids, out array<string> values)
	{
		if (!ids)
			ids = new array<string>();
		else
			ids.Clear();

		if (!values)
			values = new array<string>();
		else
			values.Clear();

		for (int i = 0; i < m_mValues.Count(); i++)
		{
			ids.Insert(m_mValues.GetKey(i));
			values.Insert(m_mValues.GetElement(i));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the whole store from a stored pair of arrays.
	//!
	//! A version mismatch, a null array or a length mismatch all clear the store and adopt the
	//! current version, rather than importing a record that may no longer mean what it once meant.
	//!
	//! \param[in] version The version the pairs were stored at.
	//! \param[in] ids The stored ids.
	//! \param[in] values The stored values, one for each id, in the same order.
	//! \return True when the pairs were adopted, false when the store was cleared instead.
	bool FromPairs(int version, array<string> ids, array<string> values)
	{
		m_mValues.Clear();
		m_iVersion = CURRENT_VERSION;
		m_bCapWarned = false;

		if (version != CURRENT_VERSION)
			return false;

		if (!ids || !values)
			return false;

		if (ids.Count() != values.Count())
			return false;

		for (int i = 0; i < ids.Count(); i++)
		{
			Set(ids[i], values[i]);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs the cap warning exactly once per store.
	protected void WarnCapOnce()
	{
		if (m_bCapWarned)
			return;

		m_bCapWarned = true;

		Print("[Overthrow] OVT_OptionsLocalStore is full at " + MAX_ENTRIES.ToString() + " entries; further ids are refused and existing ones are kept", LogLevel.WARNING);
	}
}
