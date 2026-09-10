//------------------------------------------------------------------------------------------------
//! The pure option store: definitions, stored values and every rule that can corrupt a value.
//!
//! It makes no engine call and touches no widget. Normalize is the one gate every write passes
//! through, so an out-of-range value or an undeclared choice key never leaves this class.
//!
//! A value may arrive before its definition registers, and Register re-normalizes it then.
//------------------------------------------------------------------------------------------------
class OVT_OptionsRegistry
{
	protected ref map<string, ref OVT_OptionDefinition> m_mDefinitions;
	protected ref array<string> m_aRegistrationOrder;
	protected ref map<string, string> m_mValues;
	protected ref map<string, bool> m_mPending;

	//------------------------------------------------------------------------------------------------
	void OVT_OptionsRegistry()
	{
		m_mDefinitions = new map<string, ref OVT_OptionDefinition>();
		m_aRegistrationOrder = new array<string>();
		m_mValues = new map<string, string>();
		m_mPending = new map<string, bool>();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a definition. Idempotent: the first definition for an id wins.
	//!
	//! A pending value for the same id is re-normalized and its pending mark is cleared, so a value
	//! stored before this call still lands inside the option's constraints.
	//!
	//! \param[in] def The definition. Refused when null or when `m_sId` is empty.
	//! \return True on acceptance, false when the definition is invalid or the id is already known.
	bool Register(OVT_OptionDefinition def)
	{
		if (!def)
			return false;

		if (def.m_sId == "")
			return false;

		if (m_mDefinitions.Contains(def.m_sId))
			return false;

		m_mDefinitions.Set(def.m_sId, def);
		m_aRegistrationOrder.Insert(def.m_sId);

		if (m_mPending.Contains(def.m_sId))
		{
			string raw = m_mValues.Get(def.m_sId);
			m_mValues.Set(def.m_sId, Normalize(def.m_sId, raw));
			m_mPending.Remove(def.m_sId);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The definition, or null when the id is unknown.
	OVT_OptionDefinition GetDefinition(string id)
	{
		if (!m_mDefinitions.Contains(id))
			return null;

		return m_mDefinitions.Get(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return True when a definition is registered for the id.
	bool HasDefinition(string id)
	{
		return m_mDefinitions.Contains(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Lists the ids registered for one scope, in registration order.
	//! \param[in] scope The scope to filter on.
	//! \param[out] ids Receives the ids. Allocated when null and cleared otherwise.
	void GetIdsForScope(OVT_EOptionScope scope, out array<string> ids)
	{
		if (!ids)
			ids = new array<string>();
		else
			ids.Clear();

		foreach (string id : m_aRegistrationOrder)
		{
			OVT_OptionDefinition def = m_mDefinitions.Get(id);
			if (def && def.m_eScope == scope)
				ids.Insert(id);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Clamps and snaps a raw value to what its definition allows. A `TOGGLE` becomes `"0"` or `"1"`.
	//! A `SLIDER` clamps to `[min, max]` and snaps to the nearest step measured from `min`. A `CHOICE`
	//! that names no declared key falls back to the default.
	//!
	//! \param[in] id The option id.
	//! \param[in] raw The candidate value, in the option's string encoding.
	//! \return The normalized value, or `raw` unchanged when the id has no definition.
	string Normalize(string id, string raw)
	{
		OVT_OptionDefinition def = GetDefinition(id);
		if (!def)
			return raw;

		if (def.m_eType == OVT_EOptionType.TOGGLE)
			return EncodeBool(DecodeBool(raw));

		if (def.m_eType == OVT_EOptionType.SLIDER)
			return NormalizeSlider(def, raw);

		if (def.m_eType == OVT_EOptionType.CHOICE)
			return NormalizeChoice(def, raw);

		return raw;
	}

	//------------------------------------------------------------------------------------------------
	protected string NormalizeSlider(OVT_OptionDefinition def, string raw)
	{
		float value = raw.ToFloat();

		if (value < def.m_fMin)
			value = def.m_fMin;

		if (value > def.m_fMax)
			value = def.m_fMax;

		if (def.m_fStep > 0)
		{
			float steps = Math.Round((value - def.m_fMin) / def.m_fStep);
			value = def.m_fMin + steps * def.m_fStep;

			if (value < def.m_fMin)
				value = def.m_fMin;

			if (value > def.m_fMax)
				value = def.m_fMax;
		}

		return EncodeFloat(value);
	}

	//------------------------------------------------------------------------------------------------
	protected string NormalizeChoice(OVT_OptionDefinition def, string raw)
	{
		if (def.m_aChoiceKeys && def.m_aChoiceKeys.Contains(raw))
			return raw;

		return def.m_sDefault;
	}

	//------------------------------------------------------------------------------------------------
	//! Stores a value. An id with no definition stores `raw` unclamped and marks it pending.
	//! \param[in] id The option id.
	//! \param[in] raw The candidate value, in the option's string encoding.
	void SetValue(string id, string raw)
	{
		if (!HasDefinition(id))
		{
			m_mValues.Set(id, raw);
			m_mPending.Set(id, true);
			return;
		}

		m_mValues.Set(id, Normalize(id, raw));
		m_mPending.Remove(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Overrides a definition's default value only. Never touches a stored value, so a save loaded
	//! before or after this call ends at the same result.
	//! \param[in] id The option id.
	//! \param[in] value The new default, in the option's string encoding.
	void SetDefault(string id, string value)
	{
		OVT_OptionDefinition def = GetDefinition(id);
		if (!def)
			return;

		def.m_sDefault = value;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The stored value, else the definition's default, else `""`.
	string GetValue(string id)
	{
		if (m_mValues.Contains(id))
			return m_mValues.Get(id);

		OVT_OptionDefinition def = GetDefinition(id);
		if (def)
			return def.m_sDefault;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The value read as a bool.
	bool GetBool(string id)
	{
		return DecodeBool(GetValue(id));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The value read as a float.
	float GetFloat(string id)
	{
		return DecodeFloat(GetValue(id));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The value read as an int.
	int GetInt(string id)
	{
		return GetValue(id).ToInt();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The stored choice key.
	string GetChoice(string id)
	{
		return GetValue(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Lists every non-pending value that differs from its own definition's default. The persistence
	//! serializer and the join-in-progress snapshot both read this list.
	//! \param[out] ids Receives the ids. Allocated when null and cleared otherwise.
	//! \param[out] values Receives one value for each id, in the same order.
	void GetOverrides(out array<string> ids, out array<string> values)
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
			string id = m_mValues.GetKey(i);

			if (m_mPending.Contains(id))
				continue;

			OVT_OptionDefinition def = GetDefinition(id);
			if (!def)
				continue;

			string value = m_mValues.GetElement(i);
			if (value == def.m_sDefault)
				continue;

			ids.Insert(id);
			values.Insert(value);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Lists the ids holding a value that never found a definition.
	//! \param[out] ids Receives the ids. Allocated when null and cleared otherwise.
	void GetPendingIds(out array<string> ids)
	{
		if (!ids)
			ids = new array<string>();
		else
			ids.Clear();

		for (int i = 0; i < m_mPending.Count(); i++)
		{
			ids.Insert(m_mPending.GetKey(i));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return True when the id holds a value with no definition yet.
	bool IsPending(string id)
	{
		return m_mPending.Contains(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes a stored value and its pending mark, if either exists. The definition is untouched.
	//! \param[in] id The option id.
	void ClearValue(string id)
	{
		m_mValues.Remove(id);
		m_mPending.Remove(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] scope The scope to test.
	//! \return True when the scope is edited on the server and replicated (`FACTION` or `WORLD`).
	static bool ScopeIsServerAuthoritative(OVT_EOptionScope scope)
	{
		if (scope == OVT_EOptionScope.FACTION)
			return true;

		if (scope == OVT_EOptionScope.WORLD)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] scope The scope to test.
	//! \return True when the scope needs the resistance officer permission.
	static bool ScopeRequiresOfficer(OVT_EOptionScope scope)
	{
		return scope == OVT_EOptionScope.FACTION;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] scope The scope to test.
	//! \return True when the scope needs the platform admin permission.
	static bool ScopeRequiresAdmin(OVT_EOptionScope scope)
	{
		return scope == OVT_EOptionScope.WORLD;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] value The bool to encode.
	//! \return `"1"` for true, `"0"` for false.
	static string EncodeBool(bool value)
	{
		if (value)
			return "1";

		return "0";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] value The float to encode.
	//! \return The float's string form.
	static string EncodeFloat(float value)
	{
		return value.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] raw The stored string.
	//! \return True for `"1"` or `"true"`, false for anything else, including an empty string.
	static bool DecodeBool(string raw)
	{
		if (raw == "1")
			return true;

		if (raw == "true")
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] raw The stored string.
	//! \return The parsed float, or `0` when `raw` does not start with a number.
	static float DecodeFloat(string raw)
	{
		return raw.ToFloat();
	}
}
