//------------------------------------------------------------------------------------------------
//! Pure display formatting for the Game Master detail readout. Every method is a static function of
//! its arguments alone - no state, no engine singletons, no lookups of any kind - which is what makes
//! it the only part of that readout a Logic-tier case can see.
//!
//! DO NOT introduce an engine, manager or singleton reference into this file. Its test lives in
//! Scripts/Game/Tests/TestSuites/Logic/, whose guard is a directory-wide text grep, and the pattern it
//! forbids is matched in comments as readily as in code.
//!
//! COUNTDOWNS ARE NOT HERE. OVT_GMPanelFormat.FormatCountdown already formats a duration as a clock
//! string; the sabotage-downtime row calls that one directly at its call site rather than having a
//! second copy of the same maths drift away from it.
//!
//! THE STRINGS BELOW ARE DELIBERATELY NOT LOCALIZED. Every one of them names an internal value - an
//! enum member or a script class name - the same way m_sReason carries a raw class name for a reader
//! to recognise. They are diagnostic identifiers rendered readably, not prose, and a Logic case can
//! only pin an exact string if the formatter produces one. Prose around them (row labels, the note
//! line) is localized in the widget component, where it belongs.
//------------------------------------------------------------------------------------------------
class OVT_GMIconFormat
{
	//! The prefix every upgrade class name carries. What is left after it is the readable part.
	protected static const string UPGRADE_PREFIX = "OVT_BaseUpgrade";

	//! ASCII 'A' and 'Z' - the range a word break is inserted before. ToAscii() is used rather than a
	//! case conversion because the conversion methods here mutate in place and return a length.
	protected static const int ASCII_UPPER_A = 65;
	protected static const int ASCII_UPPER_Z = 90;

	//! What a percentage reads when there is nothing to take a percentage of.
	protected static const string ZERO_PERCENT = "0%";

	//------------------------------------------------------------------------------------------------
	//! Support as a whole percentage of the population: "62%".
	//!
	//! THE ZERO GUARD IS THE POINT. A population of zero is a normal state before anything has been
	//! counted, and dividing by it is the one way this method could throw. It reads "0%" instead - the
	//! same answer the record's own percentage accessor gives for the same input, so the two never
	//! disagree.
	//!
	//! The division is done in floating point on purpose: an integer division of 248 by 400 is 0, and
	//! this would then read "0%" for a town at nearly two thirds support.
	//! \param[in] support Raw support points.
	//! \param[in] population Population those points are measured against.
	//! \return The percentage with its sign, never empty, never negative.
	static string FormatSupport(int support, int population)
	{
		if (population <= 0)
			return ZERO_PERCENT;

		float ratio = support;
		ratio = ratio / population;

		int percent = Math.Round(ratio * 100);
		if (percent <= 0)
			return ZERO_PERCENT;

		return percent.ToString() + "%";
	}

	//------------------------------------------------------------------------------------------------
	//! Current against target population: "380 / 400". Both numbers are shown because either alone
	//! hides whether a settlement is growing or emptying.
	//! \param[in] population Current population.
	//! \param[in] targetPopulation Population it is heading towards.
	//! \return The pair, never empty.
	static string FormatPopulation(int population, int targetPopulation)
	{
		return population.ToString() + " / " + targetPopulation.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! An upgrade's script class name, made readable: "OVT_BaseUpgradeSniperPosition" reads
	//! "Sniper Position".
	//!
	//! ANYTHING IT DOES NOT RECOGNISE PASSES THROUGH UNCHANGED. A name that does not carry the shared
	//! prefix is returned exactly as it arrived rather than becoming an empty row - a reader seeing an
	//! unexpected class name learns something; a reader seeing a blank learns nothing.
	//!
	//! A run of capitals is spaced letter by letter ("QRFPatrol" reads "Q R F Patrol"). No class name
	//! in use today has one, and the alternative - a run-detector - is more code than the readability
	//! is worth.
	//! \param[in] className The concrete class name carried by the record.
	//! \return A readable name; never empty when the input was not.
	static string FormatUpgradeType(string className)
	{
		if (className == string.Empty)
			return className;

		if (!className.StartsWith(UPGRADE_PREFIX))
			return className;

		int prefixLength = UPGRADE_PREFIX.Length();
		string remainder = className.Substring(prefixLength, className.Length() - prefixLength);
		if (remainder == string.Empty)
			return className;

		string spaced;

		for (int i = 0; i < remainder.Length(); i++)
		{
			int code = remainder.ToAscii(i);

			if (i > 0 && code >= ASCII_UPPER_A && code <= ASCII_UPPER_Z)
				spaced = spaced + " ";

			spaced = spaced + remainder.Get(i);
		}

		return spaced;
	}

	//------------------------------------------------------------------------------------------------
	//! An OVT_EGroupOrigin value as a readable name.
	//!
	//! Written as a switch rather than typename.EnumToString() so the reader gets "Radio Tower
	//! Garrison" instead of RADIO_TOWER_GARRISON, and so a value added to the enum without a case here
	//! degrades to "Unknown" rather than to a blank.
	//! \param[in] originType An OVT_EGroupOrigin value, as int - which is what the record carries.
	//! \return A readable name; never empty.
	static string FormatOriginType(int originType)
	{
		switch (originType)
		{
			case OVT_EGroupOrigin.BASE_PATROL:
				return "Base Patrol";

			case OVT_EGroupOrigin.BASE_DEFENCE:
				return "Base Defence";

			case OVT_EGroupOrigin.BASE_SNIPER:
				return "Base Sniper";

			case OVT_EGroupOrigin.TOWER_GUARD:
				return "Tower Guard";

			case OVT_EGroupOrigin.TOWN_PATROL:
				return "Town Patrol";

			case OVT_EGroupOrigin.BASE_GARRISON:
				return "Base Garrison";

			case OVT_EGroupOrigin.RADIO_TOWER_GARRISON:
				return "Radio Tower Garrison";

			case OVT_EGroupOrigin.CAMP_GARRISON:
				return "Camp Garrison";

			case OVT_EGroupOrigin.FOB_GARRISON:
				return "FOB Garrison";

			case OVT_EGroupOrigin.QRF:
				return "QRF";

			case OVT_EGroupOrigin.DEPLOYMENT:
				return "Deployment";

			case OVT_EGroupOrigin.JOB:
				return "Job";
		}

		return "Unknown";
	}

	//------------------------------------------------------------------------------------------------
	//! One line naming where a group came from: its origin kind, the index of the thing that produced
	//! it when there is one, and the free-text reason.
	//!
	//! AN ABSENT INDEX IS NEVER PRINTED. Several origins carry -1 because no index means anything for
	//! them, and a row reading "QRF -1" is the kind of leaked sentinel a reader files a bug about. In
	//! that case the reason - which for those origins is the only specific thing known - stands alone,
	//! and the origin kind is used only when there is no reason either.
	//! \param[in] originType An OVT_EGroupOrigin value, as int.
	//! \param[in] originIndex The producing index, or -1 when the origin has none.
	//! \param[in] reason Free text naming the concrete producer; may be empty.
	//! \return One display line; never empty.
	static string FormatOrigin(int originType, int originIndex, string reason)
	{
		string typeName = FormatOriginType(originType);

		if (originIndex < 0)
		{
			if (reason == string.Empty)
				return typeName;

			return reason;
		}

		string indexed = typeName + " " + originIndex.ToString();

		if (reason == string.Empty)
			return indexed;

		return indexed + " (" + reason + ")";
	}
}
