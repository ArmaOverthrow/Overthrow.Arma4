//------------------------------------------------------------------------------------------------
//! The packed wire form of a resource ledger: "idx:qty|idx:qty" (D4).
//!
//! Definition INDEX, not id: resources.conf is a mod file, so index N is the same resource on every
//! machine, and the whole contents of a holder ride one RplProp string. A pure pair, so unlike an
//! RplSave bitstream it can be round-tripped in a Logic case.
//------------------------------------------------------------------------------------------------
class OVT_ResourcePack
{
	//! Between two lines.
	static const string LINE_SEPARATOR = "|";

	//! Between a definition index and its quantity.
	static const string FIELD_SEPARATOR = ":";

	//------------------------------------------------------------------------------------------------
	//! Encodes a ledger. Lines the definition table does not know are dropped - an index that does
	//! not exist on the receiver would decode as some other resource.
	//! \param[in] ledger The ledger to encode. Null encodes as "".
	//! \param[in] defs The definition table supplying indices. Null encodes as "".
	//! \return The packed string; "" for an empty ledger.
	static string Encode(OVT_ResourceLedger ledger, OVT_ResourceDefs defs)
	{
		if (!ledger || !defs)
			return "";

		array<string> ids = new array<string>();
		array<int> quantities = new array<int>();
		ledger.GetLines(ids, quantities);

		string packed = "";
		for (int i = 0; i < ids.Count(); i++)
		{
			int index = defs.IndexOf(ids[i]);
			if (index == -1)
				continue;

			int qty = quantities[i];
			if (qty <= 0)
				continue;

			if (packed != "")
				packed = packed + LINE_SEPARATOR;

			packed = packed + index.ToString() + FIELD_SEPARATOR + qty.ToString();
		}

		return packed;
	}

	//------------------------------------------------------------------------------------------------
	//! Decodes a packed string into a ledger.
	//!
	//! The whole payload is parsed and validated BEFORE the target is touched: a malformed token
	//! must leave the previous contents standing, not a half-applied mirror.
	//! \param[in] packed The packed string. "" clears the ledger and succeeds.
	//! \param[in] defs The definition table resolving indices back to ids.
	//! \param[out] ledger The ledger to overwrite on success.
	//! \return True when every token parsed; false leaves \a ledger untouched.
	static bool Decode(string packed, OVT_ResourceDefs defs, notnull OVT_ResourceLedger ledger)
	{
		if (!defs)
			return false;

		array<string> ids = new array<string>();
		array<int> quantities = new array<int>();

		if (packed != "")
		{
			array<string> tokens = new array<string>();
			packed.Split(LINE_SEPARATOR, tokens, false);

			for (int i = 0; i < tokens.Count(); i++)
			{
				array<string> fields = new array<string>();
				tokens[i].Split(FIELD_SEPARATOR, fields, false);

				if (fields.Count() != 2)
					return false;

				int index;
				if (!ParseNonNegative(fields[0], index))
					return false;

				int qty;
				if (!ParseNonNegative(fields[1], qty))
					return false;

				string id = defs.IdAt(index);
				if (id == "")
					return false;

				if (qty <= 0)
					return false;

				ids.Insert(id);
				quantities.Insert(qty);
			}
		}

		ledger.Clear();
		for (int i = 0; i < ids.Count(); i++)
		{
			ledger.Add(ids[i], quantities[i], defs, -1);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Parses a token that must be nothing but digits. ToInt() alone accepts "12abc" and "" as 12
	//! and 0, so the digits are checked first.
	//! \param[in] token The token to parse.
	//! \param[out] value Receives the parsed value; 0 on failure.
	//! \return True when the token was one or more digits.
	static bool ParseNonNegative(string token, out int value)
	{
		value = 0;

		int length = token.Length();
		if (length <= 0)
			return false;

		for (int i = 0; i < length; i++)
		{
			if (!token.IsDigitAt(i))
				return false;
		}

		value = token.ToInt();

		return true;
	}
}
