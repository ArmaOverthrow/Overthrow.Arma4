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

	//! Phase row, no objective.
	protected static const string PHASE_NONE = "#OVT-GMPanel_ObjectivePhaseNone";

	//! Phase row, phase 1 - operations at the objective softening it up.
	protected static const string PHASE_HARASSMENT = "#OVT-GMPanel_ObjectivePhaseHarassment";

	//! Phase row, phase 2 - the forward operating base.
	protected static const string PHASE_FOB = "#OVT-GMPanel_ObjectivePhaseForwardBase";

	//! Phase row, phase 3 - the counter-attack itself.
	protected static const string PHASE_COUNTER_ATTACK = "#OVT-GMPanel_ObjectivePhaseCounterAttack";

	//! Phase row, a value this build has never heard of. See FormatObjectivePhase() for why this is not
	//! folded into PHASE_NONE.
	protected static const string PHASE_UNKNOWN = "#OVT-GMPanel_ObjectivePhaseUnknown";

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
	//! The phase row's value.
	//!
	//! ⚠ AN UNRECOGNISED VALUE IS ITS OWN ANSWER, NOT "no objective". The phase crosses the wire as an
	//! integer and a newer server may name a phase this build has never heard of; reporting that as
	//! "None" would tell a Game Master the campaign has no target while it is actively being attacked.
	//! An explicit "unknown" is a symptom somebody can act on.
	//! \param[in] phase OVT_EObjectivePhase's integer, exactly as it arrived.
	//! \return A localization key. Never empty.
	static string FormatObjectivePhase(int phase)
	{
		if (phase == OVT_EObjectivePhase.IDLE)
			return PHASE_NONE;

		if (phase == OVT_EObjectivePhase.HARASSMENT)
			return PHASE_HARASSMENT;

		if (phase == OVT_EObjectivePhase.FOB)
			return PHASE_FOB;

		if (phase == OVT_EObjectivePhase.COUNTER_QRF)
			return PHASE_COUNTER_ATTACK;

		return PHASE_UNKNOWN;
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
