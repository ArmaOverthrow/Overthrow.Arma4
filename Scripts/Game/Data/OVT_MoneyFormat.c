//------------------------------------------------------------------------------------------------
//! One place that turns an amount of money into text.
//!
//! Every menu in the mod currently concatenates "$" onto a raw ToString(), so 12500 reads as $12500
//! and a player has to count digits to tell it from $1250. Thousands grouping is done by hand rather
//! than by a format string because EnforceScript has no locale-aware number formatter.
//------------------------------------------------------------------------------------------------
class OVT_MoneyFormat
{
	//------------------------------------------------------------------------------------------------
	//! Formats an amount as currency with thousands separators.
	//!
	//! The minus sign goes OUTSIDE the currency symbol (-$150, not $-150), which is what every other
	//! readout in the game does.
	//! \param[in] amount Amount in game currency.
	//! \return "$12,500", "$0" or "-$150".
	static string FormatMoney(int amount)
	{
		if (amount < 0)
			return "-$" + GroupThousands(-amount);

		return "$" + GroupThousands(amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Formats a change in money as a signed string, for the HUD ticker.
	//! \param[in] delta Change in game currency.
	//! \return "+$500", "-$150", or an empty string when there is no change to show.
	static string FormatDelta(int delta)
	{
		if (delta == 0)
			return "";

		if (delta > 0)
			return "+" + FormatMoney(delta);

		return FormatMoney(delta);
	}

	//------------------------------------------------------------------------------------------------
	//! Inserts a comma every three digits, counting from the right.
	//! \param[in] value A non-negative amount.
	//! \return The digits with separators, e.g. "12,500". "0" for zero.
	protected static string GroupThousands(int value)
	{
		string digits = value.ToString();
		string result = "";
		int length = digits.Length();
		int placed = 0;

		for (int i = length - 1; i >= 0; i--)
		{
			result = digits.Substring(i, 1) + result;
			placed++;

			if (placed % 3 == 0 && i > 0)
				result = "," + result;
		}

		if (result == "")
			return "0";

		return result;
	}
}
