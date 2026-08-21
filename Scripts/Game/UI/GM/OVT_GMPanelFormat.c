//------------------------------------------------------------------------------------------------
//! Pure display formatting for the Overthrow GM panel. Every method is a static function of its
//! arguments alone - no state, no engine singletons, no world access - which is what makes it the
//! only part of the panel a Logic-tier case can see.
//!
//! DO NOT introduce a world, engine or manager reference into this file. Its test lives in
//! Scripts/Game/Tests/TestSuites/Logic/, whose guard is a directory-wide text grep, and the pattern
//! it forbids is matched in comments as readily as in code.
//------------------------------------------------------------------------------------------------
class OVT_GMPanelFormat
{
	//! Seconds in one hour; the boundary at which the countdown grows an hours field.
	protected static const int SECONDS_PER_HOUR = 3600;

	//! Seconds in one minute.
	protected static const int SECONDS_PER_MINUTE = 60;

	//! What a countdown reads when it has run out, or was handed something that is not a duration.
	protected static const string ZERO_COUNTDOWN = "0:00";

	//! What the threat readout reads at zero, or when handed something that is not a number.
	protected static const string ZERO_THREAT = "0.0";

	//------------------------------------------------------------------------------------------------
	// THE OBJECTIVE ROWS
	//
	// Two rows on the Game Master panel: what the occupying faction is working toward, and how far
	// along the ramp it is. Both are handed the raw values off the snapshot store and both answer a
	// string that is safe to hand straight to a widget.
	//
	// ⚠ THE ANSWERS ARE LOCALIZATION KEYS, NOT ENGLISH. A '#'-prefixed string is resolved by the widget
	// when it is set, and a proper noun (a town or base name) is passed through unchanged - so the two
	// cases below need no branch at the call site and no world access here.
	//------------------------------------------------------------------------------------------------

	//! Shown in the objective row when the occupying faction has no target. A normal state, not an
	//! error: an early campaign in which the resistance holds nothing has no objective by design.
	protected static const string NO_OBJECTIVE = "#OVT-GMPanel_ObjectiveNone";

	//! Phase row, no objective at all - no plan and no phase.
	protected static const string PHASE_NONE = "#OVT-GMPanel_ObjectivePhaseNone";

	//! Phase row, a plan is running but the server named no phase for it. See FormatPhaseRow()
	//! for why this is not folded into PHASE_NONE.
	protected static const string PHASE_UNKNOWN = "#OVT-GMPanel_ObjectivePhaseUnknown";

	//! What separates the plan from the phase when both are known. A colon rather than a slash or a
	//! dash because the phase belongs TO the plan; the pair reads "Town Offensive: Harassment".
	protected static const string PLAN_PHASE_SEPARATOR = ": ";

	//! What a localization key starts with. A key is only resolved when it is the WHOLE string a widget
	//! is given, which is why an authored name that looks like one is never concatenated with anything.
	protected static const string KEY_PREFIX = "#";

	//------------------------------------------------------------------------------------------------
	//! The objective row's value: the place the occupying faction is working toward.
	//! \param[in] name The display name off the snapshot store. Empty means there is no objective.
	//! \return The name, or the "no objective" key. Never empty - a blank row is indistinguishable from
	//! a row that failed to draw.
	static string FormatObjectiveName(string name)
	{
		if (name == "")
			return NO_OBJECTIVE;

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! The phase row's value: the plan the occupying faction is running and which of its phases it is in.
	//!
	//! ⚠ IT ANSWERS AUTHORED NAMES, NOT AN ENUM LABEL. The phase and the plan are what a mod author
	//! wrote in Configs/Objective/*.conf - they are the persistence keys and they cross the wire as
	//! strings - so this build has no table of them to look a number up in and needs none. Adding a
	//! doctrine is a .conf, and its phases name themselves on this row with no code change and no new
	//! localization key.
	//!
	//! ⚠ AN OBJECTIVE WITH NO PHASE NAME IS "unknown", NOT "no objective". A plan whose phase name did
	//! not arrive means the client and server builds differ, or the plan did not resolve on the server;
	//! reporting that as "-" would tell a Game Master the campaign has no target while it is actively
	//! being attacked. An explicit "unknown" is a symptom somebody can act on.
	//!
	//! ⚠ A NAME THAT LOOKS LIKE A LOCALIZATION KEY IS NEVER CONCATENATED. A '#'-prefixed string is
	//! resolved by the widget only when it is the whole string it was given, so a mod that authors its
	//! phase names as keys gets the key alone rather than a plan label and an unresolved key beside it.
	//! \param[in] planName The plan's authored name off the snapshot store, or "" when none arrived.
	//! \param[in] phaseName The phase's authored name off the snapshot store, or "" when none arrived.
	//! \return The authored pair, one authored name, or a localization key. Never empty.
	static string FormatPhaseRow(string planName, string phaseName)
	{
		if (phaseName == "")
		{
			// No plan and no phase is the ordinary "there is no objective" state; a plan with no phase
			// is a fault, and the two must not read the same.
			if (planName == "")
				return PHASE_NONE;

			return PHASE_UNKNOWN;
		}

		if (planName == "" || IsLocalizationKey(planName) || IsLocalizationKey(phaseName))
			return phaseName;

		return planName + PLAN_PHASE_SEPARATOR + phaseName;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a display string is a localization key rather than a proper noun.
	//! \param[in] value The string off the wire.
	//! \return True when a widget would resolve it, which is also when nothing may be appended to it.
	protected static bool IsLocalizationKey(string value)
	{
		return value.StartsWith(KEY_PREFIX);
	}

	//------------------------------------------------------------------------------------------------
	//! A duration as a clock string: "1:23:45" from an hour up, "12:34" below one, "0:07" below one
	//! minute. Seconds are always two digits; minutes and hours never carry a leading zero, so a
	//! reader can tell "12:34" (minutes) from "1:23:45" (hours) at a glance.
	//!
	//! Anything that is not a positive duration - zero, a negative value, or a value that fails every
	//! comparison because it is not a number - reads as "0:00". A countdown must never render a minus
	//! sign: the store clamps at zero and so does this.
	//! \param[in] seconds Duration in real seconds.
	//! \return The clock string; never empty, never negative.
	static string FormatCountdown(float seconds)
	{
		// Written as "not greater than zero" rather than "less than or equal" on purpose: a value
		// that is not a number fails BOTH comparisons, and this spelling sends it to the clamp.
		if (!(seconds > 0))
			return ZERO_COUNTDOWN;

		int total = Math.Floor(seconds);
		if (total <= 0)
			return ZERO_COUNTDOWN;

		int hours = total / SECONDS_PER_HOUR;
		int remainder = total - (hours * SECONDS_PER_HOUR);
		int minutes = remainder / SECONDS_PER_MINUTE;
		int wholeSeconds = remainder - (minutes * SECONDS_PER_MINUTE);

		if (hours > 0)
			return hours.ToString() + ":" + minutes.ToString(2) + ":" + wholeSeconds.ToString(2);

		return minutes.ToString() + ":" + wholeSeconds.ToString(2);
	}

	//------------------------------------------------------------------------------------------------
	//! Campaign threat to one decimal place, so a reader sees 3.9 where the truncating integer
	//! accessor would have shown 3. The decimal is always present: 3.0 reads "3.0", not "3".
	//! \param[in] threat Threat at full precision.
	//! \return The formatted value; never empty, never negative.
	static string FormatThreat(float threat)
	{
		// Same "not greater than zero" spelling as above, and for the same reason.
		if (!(threat > 0))
			return ZERO_THREAT;

		int tenths = Math.Round(threat * 10);
		if (tenths <= 0)
			return ZERO_THREAT;

		int whole = tenths / 10;
		int fraction = tenths - (whole * 10);

		return whole.ToString() + "." + fraction.ToString();
	}
}
